#include <linux/kernel.h>
#include <linux/kvm.h>
#include <linux/kvm_host.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
static struct proc_dir_entry *proc_entry;
#include <asm/pgtable.h>

static long get_user_page_info(struct mm_struct *mm, unsigned long va,
                               unsigned long gpa) {
  long ret;
  struct page *pages[1] = {NULL};

  unsigned long base_va = va & PAGE_MASK;
  unsigned long offset = va & 0xFFF;

  ret = get_user_pages_remote(mm, base_va, 1, FOLL_GET, pages, NULL);
  if (ret < 0) {
    pr_err("get_user_pages_remote failed with %ld\n", ret);
    return ret;
  }
  if (ret == 0) {
    pr_err("No pages were returned for VA 0x%lx\n", va);
    return 0;
  }

  struct page *page = pages[0];
  unsigned long pfn = page_to_pfn(page);
  phys_addr_t phys_base = PFN_PHYS(pfn);
  phys_addr_t exact_phys = phys_base | offset;

  // page type detection
  if (PageTransHuge(page)) {
    printk(KERN_INFO "page is part of THP\n");
  } else if (PageHuge(page)) {
    printk(KERN_INFO "page is huge\n");
  } else {
    printk(KERN_INFO "page is not huge page\n");
  }

  printk(KERN_INFO "exact phys addr for gpa 0x%lx: 0x%llx\n", gpa,
         (unsigned long long)exact_phys);

  put_page(page);
  return ret;
}

// process single gfn and get hva
static void print_gfn_to_hva(unsigned long vm_id, unsigned long full_gfn) {
  struct kvm *kvm;
  unsigned long hva;
  gfn_t gfn = (gfn_t)(full_gfn >> 12);
  unsigned long offset = full_gfn & 0xFFF;
  bool found = false;

  list_for_each_entry(kvm, &vm_list, vm_list) {
    if (kvm->userspace_pid == vm_id) { // match VM by PID (from qemu)
      found = true;
      break;
    }
  }

  if (!found) {
    printk(KERN_ERR "No VM with id=%lu found\n", vm_id);
    return;
  }

  printk(KERN_INFO "Found VM %p (pid=%d) for vm_id=%lu\n", kvm,
         kvm->userspace_pid, vm_id);

  hva = gfn_to_hva(kvm, gfn);
  if (kvm_is_error_hva(hva)) {
    printk(KERN_ERR "Error getting HVA for GFN 0x%lx in VM %lu\n",
           (unsigned long)gfn, vm_id);
    return;
  }

  hva |= offset;

  // Use the VM’s memory map
  get_user_page_info(kvm->mm, hva, full_gfn);
}

// handle write to proc entry. e.g., echo "7 0x1234" > /proc/gfn_to_pfn
static ssize_t gfn_write(struct file *file, const char __user *ubuf,
                         size_t count, loff_t *ppos) {
  char *kbuf, *cur;
  char *token;
  unsigned long vm_id = 0;
  unsigned long addr = 0;
  int got_vm = 0, got_addr = 0;

  kbuf = kmalloc(count + 1, GFP_KERNEL);
  if (!kbuf)
    return -ENOMEM;

  if (copy_from_user(kbuf, ubuf, count)) {
    kfree(kbuf);
    return -EFAULT;
  }
  kbuf[count] = '\0';

  printk(KERN_INFO "gfn_write: expecting input '<vm_pid> <gfn>'\n");

  cur = kbuf;

  // first token = vm_id
  token = strsep(&cur, " ");
  if (token && *token) {
    if (!kstrtoul(token, 0, &vm_id))
      got_vm = 1;
  }

  // second token = gfn
  token = strsep(&cur, " ");
  if (token && *token) {
    if (!kstrtoul(token, 0, &addr))
      got_addr = 1;
  }

  if (!got_vm || !got_addr) {
    printk(KERN_ERR "gfn_write: invalid input, need '<vm_pid> <gfn>'\n");
    kfree(kbuf);
    return -EINVAL;
  }

  printk(KERN_INFO "gfn_write: vm_id=%lu, gfn=0x%lx\n", vm_id, addr);

  print_gfn_to_hva(vm_id, addr);

  kfree(kbuf);
  return count;
}

static const struct proc_ops gfn_fops = {
    .proc_write = gfn_write,
};

static int __init gfn_module_init(void) {
  proc_entry = proc_create("gfn_to_pfn", 0666, NULL, &gfn_fops);
  if (!proc_entry)
    return -ENOMEM;
  printk(KERN_INFO "GFN to PFN module loaded\n");
  return 0;
}

static void __exit gfn_module_exit(void) {
  proc_remove(proc_entry);
  printk(KERN_INFO "GFN to PFN module unloaded\n");
}

module_init(gfn_module_init);
module_exit(gfn_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Edward");
MODULE_DESCRIPTION("GFN to PFN translation module with multi-address support");
