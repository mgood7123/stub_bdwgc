
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_IGNORE_WARN
  /* Ignore misleading "Out of Memory!" warning (which is printed on    */
  /* every MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC call below) by defining this macro before "gc.h"   */
  /* inclusion.                                                         */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_IGNORE_WARN
#endif

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_MAXIMUM_HEAP_SIZE
# define MANAGED_STACK_ADDRESS_BOEHM_GC_MAXIMUM_HEAP_SIZE (100 * 1024 * 1024)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_INITIAL_HEAP_SIZE (MANAGED_STACK_ADDRESS_BOEHM_GC_MAXIMUM_HEAP_SIZE / 20)
    /* Otherwise heap expansion aborts when deallocating large block.   */
    /* That's OK.  We test this corner case mostly to make sure that    */
    /* it fails predictably.                                            */
#endif

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_ALLOC_SIZE
  /* Omit alloc_size attribute to avoid compiler warnings about         */
  /* exceeding maximum object size when values close to MANAGED_STACK_ADDRESS_BOEHM_GC_SWORD_MAX    */
  /* are passed to MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC.                                           */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_ALLOC_SIZE(argnum) /* empty */
#endif

#include "gc.h"

/*
 * Check that very large allocation requests fail.  "Success" would usually
 * indicate that the size was somehow converted to a negative
 * number.  Clients shouldn't do this, but we should fail in the
 * expected manner.
 */

#define CHECK_ALLOC_FAILED(r, sz_str) \
  do { \
    if (NULL != (r)) { \
        fprintf(stderr, \
                "Size " sz_str " allocation unexpectedly succeeded\n"); \
        exit(1); \
    } \
  } while (0)

#define MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX ((MANAGED_STACK_ADDRESS_BOEHM_GC_word)-1)
#define MANAGED_STACK_ADDRESS_BOEHM_GC_SWORD_MAX ((MANAGED_STACK_ADDRESS_BOEHM_GC_signed_word)(MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX >> 1))

int main(void)
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_INIT();

  CHECK_ALLOC_FAILED(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(MANAGED_STACK_ADDRESS_BOEHM_GC_SWORD_MAX - 1024), "SWORD_MAX-1024");
  CHECK_ALLOC_FAILED(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(MANAGED_STACK_ADDRESS_BOEHM_GC_SWORD_MAX), "SWORD_MAX");
  /* Skip other checks to avoid "exceeds maximum object size" gcc warning. */
# if !defined(_FORTIFY_SOURCE)
    CHECK_ALLOC_FAILED(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC((MANAGED_STACK_ADDRESS_BOEHM_GC_word)MANAGED_STACK_ADDRESS_BOEHM_GC_SWORD_MAX + 1), "SWORD_MAX+1");
    CHECK_ALLOC_FAILED(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC((MANAGED_STACK_ADDRESS_BOEHM_GC_word)MANAGED_STACK_ADDRESS_BOEHM_GC_SWORD_MAX + 1024),
                       "SWORD_MAX+1024");
    CHECK_ALLOC_FAILED(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX - 1024), "WORD_MAX-1024");
    CHECK_ALLOC_FAILED(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX - 16), "WORD_MAX-16");
    CHECK_ALLOC_FAILED(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX - 8), "WORD_MAX-8");
    CHECK_ALLOC_FAILED(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX - 4), "WORD_MAX-4");
    CHECK_ALLOC_FAILED(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX), "WORD_MAX");
# endif
  printf("SUCCEEDED\n");
  return 0;
}
