#ifndef GFN_PARSE_H
#define GFN_PARSE_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdbool.h>
#include <stddef.h>
#endif

struct gfn_request {
  unsigned long raw_gfn;
  unsigned long vm_pid;
  bool has_pid;
};

int gfn_parse_request(char *buffer, struct gfn_request *req);

#endif /* GFN_PARSE_H */
