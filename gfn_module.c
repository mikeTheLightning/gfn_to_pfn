// gfn_module.c
#include <asm/pgtable.h>
#include <linux/kernel.h>
#include <linux/kvm.h>
#include <linux/kvm_host.h>
#include <linux/mm.h>
#include <linux/huge_mm.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include "gfn_parse.h"

#define PROC_NAME "gfn_to_pfn"
#define REPLY_MAX 256

struct gfn_ctx {
  wait_queue_head_t wq;
  bool reply_ready;
  ssize_t reply_len;
  char reply[REPLY_MAX];
};

static struct proc_dir_entry *proc_entry;

static void gfn_ctx_reply(struct gfn_ctx *ctx, const char *fmt, ...)
    __printf(2, 3);

static void gfn_ctx_reply(struct gfn_ctx *ctx, const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  ctx->reply_len = vscnprintf(ctx->reply, REPLY_MAX, fmt, args);
  va_end(args);
}

/* --- helper: format info about page --- */
static ssize_t format_page_info(char *dst, size_t cap, struct mm_struct *mm,
                                unsigned long hva, unsigned long gpa) {
  long ret;
  struct page *pages[1] = {NULL};
  unsigned long base_va = hva & PAGE_MASK;
  unsigned long offset = hva & 0xFFF;

  ret = get_user_pages_remote(mm, base_va, 1, FOLL_GET, pages, NULL);
  if (ret <= 0)
    return scnprintf(dst, cap, "err:gup=%ld\n", ret);

  {
    struct page *page = pages[0];
    unsigned long pfn = page_to_pfn(page);
    phys_addr_t phys_base = PFN_PHYS(pfn);
    phys_addr_t exact_phys = phys_base | offset;

    const char *kind = "base";
    if (PageTransHuge(page))
      kind = "thp";
    else if (PageHuge(page))
      kind = "hugetlb";

    ret = scnprintf(dst, cap, "ok phys=0x%llx kind=%s gpa=0x%lx hva=0x%lx\n",
                    (unsigned long long)exact_phys, kind, gpa, hva);

    put_page(page);
  }
  return ret;
}

/* --- locate VM by pid --- */
static int find_kvm_by_pid(unsigned long vm_pid, struct kvm **out) {
  struct kvm *kvm;
  bool found = false;

  list_for_each_entry(kvm, &vm_list, vm_list) {
    if (kvm->userspace_pid == vm_pid) {
      *out = kvm;
      found = true;
      break;
    }
  }
  return found ? 0 : -ESRCH;
}

/* --- translate gfn to hva --- */
static long gfn_to_hva_safe(struct kvm *kvm, unsigned long full_gfn,
                            unsigned long *out_hva) {
  gfn_t gfn = (gfn_t)(full_gfn >> 12);
  unsigned long off = full_gfn & 0xFFF;
  unsigned long hva = gfn_to_hva(kvm, gfn);
  if (kvm_is_error_hva(hva))
    return -EFAULT;
  *out_hva = hva | off;
  return 0;
}

/* --- per-file lifecycle --- */
static int gfn_open(struct inode *ino, struct file *f) {
  struct gfn_ctx *ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
  if (!ctx)
    return -ENOMEM;
  init_waitqueue_head(&ctx->wq);
  f->private_data = ctx;
  return 0;
}

static int gfn_release(struct inode *ino, struct file *f) {
  kfree(f->private_data);
  return 0;
}

/* --- write request --- */
static ssize_t gfn_write(struct file *file, const char __user *ubuf,
                         size_t count, loff_t *ppos) {
  struct gfn_ctx *ctx = file->private_data;
  struct gfn_request req;
  struct kvm *kvm = NULL;
  unsigned long hva;
  ssize_t n;
  char kbuf[64];

  if (count >= sizeof(kbuf))
    return -E2BIG;

  if (copy_from_user(kbuf, ubuf, count))
    return -EFAULT;

  kbuf[count] = '\0';
  ctx->reply_ready = false;
  ctx->reply_len = 0;

  if (gfn_parse_request(kbuf, &req)) {
    gfn_ctx_reply(ctx, "err:invalid_input\n");
    goto out_ready;
  }

  if (req.has_pid) {
    int rc = find_kvm_by_pid(req.vm_pid, &kvm);
    if (rc) {
      gfn_ctx_reply(ctx, "err:no_vm pid=%lu\n", req.vm_pid);
      goto out_ready;
    }
  } else {
    kvm = list_first_entry_or_null(&vm_list, struct kvm, vm_list);
    if (!kvm) {
      gfn_ctx_reply(ctx, "err:no_vms\n");
      goto out_ready;
    }
  }

  if (gfn_to_hva_safe(kvm, req.raw_gfn, &hva)) {
    gfn_ctx_reply(ctx, "err:hva gfn=0x%lx\n", req.raw_gfn);
    goto out_ready;
  }

  n = format_page_info(ctx->reply, REPLY_MAX, kvm->mm, hva, req.raw_gfn);
  ctx->reply_len = clamp_t(ssize_t, n, 0, REPLY_MAX);

out_ready:
  ctx->reply_ready = true;
  wake_up_interruptible(&ctx->wq);
  return count;
}

/* --- read reply --- */
static ssize_t gfn_read(struct file *file, char __user *ubuf, size_t len,
                        loff_t *ppos) {
  struct gfn_ctx *ctx = file->private_data;

  if (!ctx->reply_ready) {
    if (file->f_flags & O_NONBLOCK)
      return -EAGAIN;
    if (wait_event_interruptible(ctx->wq, ctx->reply_ready))
      return -ERESTARTSYS;
  }

  if (!ctx->reply_len)
    return 0;

  return simple_read_from_buffer(ubuf, len, ppos, ctx->reply, ctx->reply_len);
}

static __poll_t gfn_poll(struct file *file, poll_table *pt) {
  struct gfn_ctx *ctx = file->private_data;
  __poll_t m = 0;

  poll_wait(file, &ctx->wq, pt);
  if (ctx->reply_ready)
    m |= POLLIN | POLLRDNORM;
  return m;
}

static const struct proc_ops gfn_fops = {
    .proc_open = gfn_open,
    .proc_release = gfn_release,
    .proc_read = gfn_read,
    .proc_write = gfn_write,
    .proc_poll = gfn_poll,
};

static int __init gfn_module_init(void) {
  proc_entry = proc_create(PROC_NAME, 0640, NULL, &gfn_fops);
  if (!proc_entry)
    return -ENOMEM;
  pr_info("gfn_to_pfn loaded\n");
  return 0;
}

static void __exit gfn_module_exit(void) {
  proc_remove(proc_entry);
  pr_info("gfn_to_pfn unloaded\n");
}

module_init(gfn_module_init);
module_exit(gfn_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Edward");
MODULE_DESCRIPTION("GFN→PFN translation with request/response via /proc");
