#ifndef __KERNEL__
#define _GNU_SOURCE
#endif

#include "gfn_parse.h"

#ifdef __KERNEL__
#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#else
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#endif

#ifndef __KERNEL__
static int parse_ulong_token(const char *token, unsigned long *value) {
  char *end = NULL;
  unsigned long parsed;

  if (!token || !value)
    return -EINVAL;

  errno = 0;
  parsed = strtoul(token, &end, 0);
  if (errno)
    return -errno;
  if (end == token || (end && *end))
    return -EINVAL;

  *value = parsed;
  return 0;
}
#else
static int parse_ulong_token(const char *token, unsigned long *value) {
  if (!token || !value)
    return -EINVAL;
  return kstrtoul(token, 0, value);
}
#endif

static bool token_has_content(const char *token) {
  return token && *token;
}

static char *next_token(char **cursor) {
  return strsep(cursor, " \t\n");
}

int gfn_parse_request(char *buffer, struct gfn_request *req) {
  char *cursor;
  char *token;
  bool have_gfn = false;
  bool have_pid = false;
  int rc;

  if (!buffer || !req)
    return -EINVAL;

  req->raw_gfn = 0;
  req->vm_pid = 0;
  req->has_pid = false;
  cursor = buffer;

  while ((token = next_token(&cursor))) {
    if (!token_has_content(token))
      continue;

    if (!have_gfn) {
      rc = parse_ulong_token(token, &req->raw_gfn);
      if (rc)
        return rc;
      have_gfn = true;
      continue;
    }

    if (!have_pid) {
      rc = parse_ulong_token(token, &req->vm_pid);
      if (rc)
        return rc;
      have_pid = true;
      req->has_pid = true;
      break;
    }

    break;
  }

  if (!have_gfn)
    return -EINVAL;

  return 0;
}

