
#include <stdio.h>
#include <string.h>

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG
# define MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG
#endif

#include "gc.h"
#include "gc/gc_backptr.h"

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_TEST_IMPORT_API
# define MANAGED_STACK_ADDRESS_BOEHM_GC_TEST_IMPORT_API extern
#endif

/* Should match that in staticroots_lib.c.      */
struct treenode {
    struct treenode *x;
    struct treenode *y;
};

struct treenode *root[10] = { NULL };

/* Same as "root" variable but initialized to some non-zero value (to   */
/* be placed to .data section instead of .bss).                         */
struct treenode *root_nz[10] = { (struct treenode *)(MANAGED_STACK_ADDRESS_BOEHM_GC_word)1 };

static char *staticroot; /* intentionally static */

MANAGED_STACK_ADDRESS_BOEHM_GC_TEST_IMPORT_API struct treenode * libsrl_mktree(int i);
MANAGED_STACK_ADDRESS_BOEHM_GC_TEST_IMPORT_API void * libsrl_init(void);
MANAGED_STACK_ADDRESS_BOEHM_GC_TEST_IMPORT_API struct treenode ** libsrl_getpelem(int i, int j);

MANAGED_STACK_ADDRESS_BOEHM_GC_TEST_IMPORT_API struct treenode ** libsrl_getpelem2(int i, int j);

static void init_staticroot(void)
{
  /* Intentionally put staticroot initialization in a function other    */
  /* than main to prevent CSA warning that staticroot variable can be   */
  /* changed to be a local one).                                        */
  staticroot = (char *)libsrl_init();
}

int main(void)
{
  int i, j;

# ifdef STATICROOTSLIB_INIT_IN_MAIN
    MANAGED_STACK_ADDRESS_BOEHM_GC_INIT();
# endif
  init_staticroot();
  if (MANAGED_STACK_ADDRESS_BOEHM_GC_get_find_leak())
    printf("This test program is not designed for leak detection mode\n");
  if (NULL == staticroot) {
    fprintf(stderr, "MANAGED_STACK_ADDRESS_BOEHM_GC_malloc returned NULL\n");
    return 2;
  }
  memset(staticroot, 0x42, sizeof(struct treenode));
  MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect();
  for (j = 0; j < 4; j++) {
      for (i = 0; i < (int)(sizeof(root) / sizeof(root[0])); ++i) {
#       ifdef STATICROOTSLIB2
          *libsrl_getpelem2(i, j) = libsrl_mktree(12);
#       endif
        *libsrl_getpelem(i, j) = libsrl_mktree(12);
        ((j & 1) != 0 ? root_nz : root)[i] = libsrl_mktree(12);
        MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect();
      }
      for (i = 0; i < (int)sizeof(struct treenode); ++i) {
        if (staticroot[i] != 0x42) {
          fprintf(stderr, "Memory check failed\n");
          return 1;
        }
      }
  }
  printf("SUCCEEDED\n");
  return 0;
}
