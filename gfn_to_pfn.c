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
static void print_gfn_to_hva(struct kvm *kvm, unsigned long full_gfn) {
  unsigned long hva;
  gfn_t gfn = (gfn_t)(full_gfn >> 12);
  unsigned long offset = full_gfn & 0xFFF;

  if (!kvm) {
    printk(KERN_ERR "No VM provided to print_gfn_to_hva\n");
    return;
  }

  hva = gfn_to_hva(kvm, gfn);
  if (kvm_is_error_hva(hva)) {
    printk(KERN_ERR "Error getting HVA for GFN 0x%lx (vm pid=%d)\n",
           (unsigned long)gfn, kvm->userspace_pid);
    return;
  }

  hva |= offset;
  get_user_page_info(kvm->mm, hva, full_gfn);
}

// handle write to proc entry
// Usage:
//   echo "0x1234"          > /proc/gfn_to_pfn   # address only → first VM
//   echo "0x1234 12345"    > /proc/gfn_to_pfn   # address + pid → explicit VM
static ssize_t gfn_write(struct file *file, const char __user *ubuf,
                         size_t count, loff_t *ppos) {
  char *kbuf, *cur, *token;
  unsigned long addr = 0;
  unsigned long vm_pid = 0;
  int got_addr = 0, got_pid = 0;
  struct kvm *kvm = NULL;

  kbuf = kmalloc(count + 1, GFP_KERNEL);
  if (!kbuf)
    return -ENOMEM;

  if (copy_from_user(kbuf, ubuf, count)) {
    kfree(kbuf);
    return -EFAULT;
  }
  kbuf[count] = '\0';
  cur = kbuf;

  // first token = address
  token = strsep(&cur, " ");
  if (token && *token) {
    if (!kstrtoul(token, 0, &addr))
      got_addr = 1;
  }

  // second token (optional) = vm pid
  token = strsep(&cur, " ");
  if (token && *token) {
    if (!kstrtoul(token, 0, &vm_pid))
      got_pid = 1;
  }

  if (!got_addr) {
    printk(KERN_ERR "gfn_write: invalid input, need '<addr> [vm_pid]'\n");
    kfree(kbuf);
    return -EINVAL;
  }

  if (got_pid) {
    bool found = false;
    list_for_each_entry(kvm, &vm_list, vm_list) {
      if (kvm->userspace_pid == vm_pid) {
        found = true;
        printk(KERN_INFO "Found VM %p (pid=%d)\n", kvm, kvm->userspace_pid);
        break;
      }
    }
    if (!found) {
      printk(KERN_ERR "No VM with pid=%lu found\n", vm_pid);
      kfree(kbuf);
      return -ESRCH;
    }
  } else {
    // default: use first VM
    kvm = list_first_entry_or_null(&vm_list, struct kvm, vm_list);
    if (!kvm) {
      printk(KERN_ERR "No VMs available (default mode)\n");
      kfree(kbuf);
      return -ESRCH;
    }
    printk(KERN_INFO "Using first VM %p (pid=%d)\n", kvm, kvm->userspace_pid);
  }

  printk(KERN_INFO "gfn_write: gfn=0x%lx, vm_pid=%s\n", addr,
         got_pid ? kasprintf(GFP_KERNEL, "%lu", vm_pid) : "first");
  print_gfn_to_hva(kvm, addr);

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
