
/* This test file is intended to be compiled into a DLL. */

#include <stdio.h>
#include <stdlib.h>

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG
# define MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG
#endif

#include "gc.h"

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_TEST_EXPORT_API
# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_VISIBILITY_HIDDEN_SET) \
     && !defined(__CEGCC__) && !defined(__CYGWIN__) && !defined(__MINGW32__)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_TEST_EXPORT_API \
                        extern __attribute__((__visibility__("default")))
# else
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_TEST_EXPORT_API extern
# endif
#endif

#define CHECK_OUT_OF_MEMORY(p) \
    do { \
        if (NULL == (p)) { \
            fprintf(stderr, "Out of memory\n"); \
            exit(69); \
        } \
    } while (0)

struct treenode {
    struct treenode *x;
    struct treenode *y;
};

static struct treenode *root[10] = { 0 };
static struct treenode *root_nz[10] = { (struct treenode *)(MANAGED_STACK_ADDRESS_BOEHM_GC_word)2 };

/* Declare it to avoid "no previous prototype" clang warning.   */
MANAGED_STACK_ADDRESS_BOEHM_GC_TEST_EXPORT_API struct treenode ** libsrl_getpelem(int i, int j);

#ifdef STATICROOTSLIB2
# define libsrl_getpelem libsrl_getpelem2
#else

  MANAGED_STACK_ADDRESS_BOEHM_GC_TEST_EXPORT_API struct treenode * libsrl_mktree(int i);
  MANAGED_STACK_ADDRESS_BOEHM_GC_TEST_EXPORT_API void * libsrl_init(void);

  MANAGED_STACK_ADDRESS_BOEHM_GC_TEST_EXPORT_API struct treenode * libsrl_mktree(int i)
  {
    struct treenode *r = MANAGED_STACK_ADDRESS_BOEHM_GC_NEW(struct treenode);
    struct treenode *x, *y;

    CHECK_OUT_OF_MEMORY(r);
    if (0 == i)
      return 0;
    if (1 == i) {
      r = (struct treenode *)MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_ATOMIC(sizeof(struct treenode));
      CHECK_OUT_OF_MEMORY(r);
    }
    x = libsrl_mktree(i - 1);
    y = libsrl_mktree(i - 1);
    r -> x = x;
    r -> y = y;
    if (i != 1) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_END_STUBBORN_CHANGE(r);
      MANAGED_STACK_ADDRESS_BOEHM_GC_reachable_here(x);
      MANAGED_STACK_ADDRESS_BOEHM_GC_reachable_here(y);
    }
    return r;
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_TEST_EXPORT_API void * libsrl_init(void)
  {
#   ifdef TEST_MANUAL_VDB
      MANAGED_STACK_ADDRESS_BOEHM_GC_set_manual_vdb_allowed(1);
#   endif
#   ifndef STATICROOTSLIB_INIT_IN_MAIN
      MANAGED_STACK_ADDRESS_BOEHM_GC_INIT();
#   endif
#   ifndef NO_INCREMENTAL
      MANAGED_STACK_ADDRESS_BOEHM_GC_enable_incremental();
#   endif
    return MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(sizeof(struct treenode));
  }

#endif /* !STATICROOTSLIB2 */

MANAGED_STACK_ADDRESS_BOEHM_GC_TEST_EXPORT_API struct treenode ** libsrl_getpelem(int i, int j)
{
# if defined(CPPCHECK)
    struct treenode node = { 0, 0 };
    MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((MANAGED_STACK_ADDRESS_BOEHM_GC_word)node.x | (MANAGED_STACK_ADDRESS_BOEHM_GC_word)node.y);
# endif
  return &((j & 1) != 0 ? root_nz : root)[i];
}
