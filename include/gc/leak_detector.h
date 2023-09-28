/*
 * Copyright (c) 2000-2011 by Hewlett-Packard Development Company.
 * All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose, provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_LEAK_DETECTOR_H
#define MANAGED_STACK_ADDRESS_BOEHM_GC_LEAK_DETECTOR_H

/* Include this header file (e.g., via gcc --include directive) */
/* to turn libgc into a leak detector.                          */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG
# define MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG
#endif
#include "gc.h"

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DONT_INCLUDE_STDLIB
  /* We ensure stdlib.h and string.h are included before        */
  /* redirecting malloc() and the accompanying functions.       */
# include <stdlib.h>
# include <string.h>
#endif

#undef malloc
#define malloc(n) MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(n)
#undef calloc
#define calloc(m,n) MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC((m)*(n))
#undef free
#define free(p) MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(p)
#undef realloc
#define realloc(p,n) MANAGED_STACK_ADDRESS_BOEHM_GC_REALLOC(p,n)
#undef reallocarray
#define reallocarray(p,m,n) MANAGED_STACK_ADDRESS_BOEHM_GC_REALLOC(p,(m)*(n))

#undef strdup
#define strdup(s) MANAGED_STACK_ADDRESS_BOEHM_GC_STRDUP(s)
#undef strndup
#define strndup(s,n) MANAGED_STACK_ADDRESS_BOEHM_GC_STRNDUP(s,n)

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_REQUIRE_WCSDUP
  /* The collector should be built with MANAGED_STACK_ADDRESS_BOEHM_GC_REQUIRE_WCSDUP       */
  /* defined as well to redirect wcsdup().                      */
# include <wchar.h>
# undef wcsdup
# define wcsdup(s) MANAGED_STACK_ADDRESS_BOEHM_GC_WCSDUP(s)
#endif

/* The following routines for the aligned objects allocation    */
/* (aligned_alloc, valloc, etc.) do not have their debugging    */
/* counterparts.  Note that free() called for such objects      */
/* may output a warning that the pointer has no debugging info. */

#undef aligned_alloc
#define aligned_alloc(a,n) MANAGED_STACK_ADDRESS_BOEHM_GC_memalign(a,n) /* identical to memalign */
#undef memalign
#define memalign(a,n) MANAGED_STACK_ADDRESS_BOEHM_GC_memalign(a,n)
#undef posix_memalign
#define posix_memalign(p,a,n) MANAGED_STACK_ADDRESS_BOEHM_GC_posix_memalign(p,a,n)

#undef _aligned_malloc
#define _aligned_malloc(n,a) MANAGED_STACK_ADDRESS_BOEHM_GC_memalign(a,n) /* reverse args order */
#undef _aligned_free
#define _aligned_free(p) MANAGED_STACK_ADDRESS_BOEHM_GC_free(MANAGED_STACK_ADDRESS_BOEHM_GC_base(p)) /* non-debug */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_VALLOC
# undef valloc
# define valloc(n) MANAGED_STACK_ADDRESS_BOEHM_GC_valloc(n)
# undef pvalloc
# define pvalloc(n) MANAGED_STACK_ADDRESS_BOEHM_GC_pvalloc(n) /* obsolete */
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_NO_VALLOC */

#ifndef CHECK_LEAKS
# define CHECK_LEAKS() MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect()
  /* Note 1: CHECK_LEAKS does not have GC prefix (preserved for */
  /* backward compatibility).                                   */
  /* Note 2: MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect() is also called automatically in the  */
  /* leak-finding mode at program exit.                         */
#endif

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_LEAK_DETECTOR_H */
