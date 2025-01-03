#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kvm_host.h>
#include <linux/kvm.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
static struct proc_dir_entry *proc_entry;
#include <asm/pgtable.h>




static long get_user_page_info(struct mm_struct *mm, unsigned long va)
{
	long ret;
	int locked = 0;
	struct page *pages[1] = { NULL };
	ret = get_user_pages_remote(mm, va, 1, FOLL_GET, pages, NULL);
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
	phys_addr_t phys = PFN_PHYS(pfn);
	printk(KERN_INFO "Page Found for VA");
	printk(KERN_INFO "Physical Address:0x%llx", (unsigned long long)phys);
	printk(KERN_INFO "WARNING This is using PageTransHuge and PageHuge utilities to detect a hugepage i don't trust them");
	//Note that if a page huge is declared it's probably a huge page. But I don't quite trust a non-huge page detection to detect if it isn't yet 
	if (PageTransHuge(page)) {
		printk(KERN_INFO "page is part of THP\n");
	} else if (PageHuge(page)) {
		printk(KERN_INFO "page is huge\n");
	} else {
		printk(KERN_INFO "page is not huge page\n");
	}
	put_page(page);
	return ret;
}



static void print_gfn_to_hva(unsigned long gfn_val)
{
    	struct kvm *kvm;
    	unsigned long hva;
    	gfn_t gfn = (gfn_t)gfn_val;
	//find first kvm - this needs expanding for variable # of kvms
    	kvm = list_first_entry_or_null(&vm_list, struct kvm, vm_list);
    	if (!kvm) {
        	printk(KERN_ERR "No VMs found\n");
        	return;
    	}
    	printk(KERN_INFO "Found VM\n");
    	hva = gfn_to_hva(kvm, gfn);
    	if (kvm_is_error_hva(hva)) {
        	printk(KERN_ERR "Error getting HVA for GFN 0x%lx\n", gfn_val);
        	return;
    	}
	printk(KERN_INFO "GFN 0x%lx maps to HVA 0x%lx\n", gfn_val, hva);
	get_user_page_info(kvm->mm,hva);
}


static ssize_t gfn_write(struct file *file, const char __user *ubuf,
                        size_t count, loff_t *ppos)
{
    //get gfn from buffer
    char buf[32];
    unsigned long gfn;
    if (count > sizeof(buf) - 1)
        return -EINVAL;
    if (copy_from_user(buf, ubuf, count))
        return -EFAULT;
    buf[count] = '\0';
    if (kstrtoul(buf, 0, &gfn))
        return -EINVAL;
    print_gfn_to_hva(gfn);
    return count;
}

static const struct proc_ops gfn_fops = {
    .proc_write = gfn_write,
};

static int __init gfn_module_init(void)
{
    proc_entry = proc_create("gfn_to_hva", 0666, NULL, &gfn_fops);
    if (!proc_entry)
        return -ENOMEM;
    printk(KERN_INFO "GFN to HVA module loaded\n");
    return 0;
}

static void __exit gfn_module_exit(void)
{
    proc_remove(proc_entry);
    printk(KERN_INFO "GFN to HVA module unloaded\n");
}

module_init(gfn_module_init);
module_exit(gfn_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("GFN to HVA translation module");
