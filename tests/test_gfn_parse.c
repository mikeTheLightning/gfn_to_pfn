#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "../gfn_parse.h"

static void expect_success(const char *input, unsigned long gfn,
                           bool expect_pid, unsigned long pid) {
  struct gfn_request req;
  char buf[128];

  strncpy(buf, input, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  int rc = gfn_parse_request(buf, &req);
  if (rc) {
    fprintf(stderr, "expected success for '%s' but got %d\n", input, rc);
    assert(!rc);
  }

  assert(req.raw_gfn == gfn);
  assert(req.has_pid == expect_pid);
  if (expect_pid)
    assert(req.vm_pid == pid);
}

static void expect_failure(const char *input) {
  struct gfn_request req;
  char buf[128];

  strncpy(buf, input, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  int rc = gfn_parse_request(buf, &req);
  if (!rc) {
    fprintf(stderr, "expected failure for '%s' but parser succeeded\n", input);
    assert(rc);
  }
}

int main(void) {
  expect_success("0x1000 42", 0x1000, true, 42);
  expect_success("4096", 4096, false, 0);
  expect_success("   0x20   \n", 0x20, false, 0);
  expect_success("0  123", 0, true, 123);
  expect_success("0x1 0x2 0x3", 0x1, true, 0x2);

  expect_failure("");
  expect_failure("    \n");
  expect_failure("xyz");
  expect_failure("0x20 pid");

  printf("all parser tests passed\n");
  return 0;
}

