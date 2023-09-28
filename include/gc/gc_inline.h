/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1995 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 2005 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2008-2022 Ivan Maidanski
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

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE_H
#define MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE_H

/* WARNING:                                                             */
/* Note that for these routines, it is the clients responsibility to    */
/* add the extra byte at the end to deal with one-past-the-end pointers.*/
/* In the standard collector configuration, the collector assumes that  */
/* such a byte has been added, and hence does not trace the last word   */
/* in the resulting object.                                             */
/* This is not an issue if the collector is compiled with               */
/* DONT_ADD_BYTE_AT_END, or if MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers is not set.     */
/* This interface is most useful for compilers that generate C.         */
/* It is also used internally for thread-local allocation.              */
/* Manual use is hereby discouraged.                                    */
/* Clients should include atomic_ops.h (or similar) before this header. */
/* There is no debugging version of this allocation API.                */

#include "gc.h"
#include "gc_tiny_fl.h"

#if MANAGED_STACK_ADDRESS_BOEHM_GC_GNUC_PREREQ(3, 0)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_EXPECT(expr, outcome) __builtin_expect(expr, outcome)
  /* Equivalent to (expr), but predict that usually (expr)==outcome. */
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_EXPECT(expr, outcome) (expr)
#endif

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT
# ifdef NDEBUG
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(expr) /* empty */
# else
#   include <assert.h>
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(expr) assert(expr)
# endif
#endif

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_PREFETCH_FOR_WRITE
# if MANAGED_STACK_ADDRESS_BOEHM_GC_GNUC_PREREQ(3, 0) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PREFETCH_FOR_WRITE)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_PREFETCH_FOR_WRITE(x) __builtin_prefetch((x), 1 /* write */)
# elif defined(_MSC_VER) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PREFETCH_FOR_WRITE) \
       && (defined(_M_IX86) || defined(_M_X64)) && !defined(_CHPE_ONLY_) \
       && (_MSC_VER >= 1900) /* VS 2015+ */
#   include <intrin.h>
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_PREFETCH_FOR_WRITE(x) _m_prefetchw(x)
    /* TODO: Support also _M_ARM (__prefetchw). */
# else
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_PREFETCH_FOR_WRITE(x) (void)0
# endif
#endif

#ifdef __cplusplus
  extern "C" {
#endif

/* Object kinds (exposed to public).    */
#define MANAGED_STACK_ADDRESS_BOEHM_GC_I_PTRFREE 0
#define MANAGED_STACK_ADDRESS_BOEHM_GC_I_NORMAL  1

/* Store a pointer to a list of newly allocated objects of kind k and   */
/* size lb in *result.  The caller must make sure that *result is       */
/* traced even if objects are ptrfree.                                  */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_many(size_t /* lb */, int /* k */,
                                           void ** /* result */);

/* Generalized version of MANAGED_STACK_ADDRESS_BOEHM_GC_malloc and MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_atomic.               */
/* Uses appropriately the thread-local (if available) or the global     */
/* free-list of the specified kind.                                     */
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_ALLOC_SIZE(1) void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL
        MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind(size_t /* lb */, int /* k */);

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_THREADS
  /* Same as above but uses only the global free-list.  */
  MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_ALLOC_SIZE(1) void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL
        MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind_global(size_t /* lb */, int /* k */);
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind_global MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind
#endif

/* An internal macro to update the free list pointer atomically (if     */
/* the AO primitives are available) to avoid race with the marker.      */
#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_THREADS) && defined(AO_HAVE_store)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_FAST_M_AO_STORE(my_fl, next) \
                AO_store((volatile AO_t *)(my_fl), (AO_t)(next))
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_FAST_M_AO_STORE(my_fl, next) (void)(*(my_fl) = (next))
#endif

/* The ultimately general inline allocation macro.  Allocate an object  */
/* of size granules, putting the resulting pointer in result.  Tiny_fl  */
/* is a "tiny" free list array, which will be used first, if the size   */
/* is appropriate.  If granules argument is too large, we allocate with */
/* default_expr instead.  If we need to refill the free list, we use    */
/* MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_many with the indicated kind.                      */
/* Tiny_fl should be an array of MANAGED_STACK_ADDRESS_BOEHM_GC_TINY_FREELISTS void * pointers.     */
/* If num_direct is nonzero, and the individual free list pointers      */
/* are initialized to (void *)1, then we allocate num_direct granules   */
/* directly using generic_malloc before putting multiple objects into   */
/* the tiny_fl entry.  If num_direct is zero, then the free lists may   */
/* also be initialized to (void *)0.                                    */
/* Note that we use the zeroth free list to hold objects 1 granule in   */
/* size that are used to satisfy size 0 allocation requests.            */
/* We rely on much of this hopefully getting optimized away in the      */
/* num_direct = 0 case.                                                 */
/* Particularly, if granules argument is constant, this should generate */
/* a small amount of code.                                              */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_FAST_MALLOC_GRANS(result,granules,tiny_fl,num_direct, \
                              kind,default_expr,init) \
  do { \
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_EXPECT((granules) >= MANAGED_STACK_ADDRESS_BOEHM_GC_TINY_FREELISTS, 0)) { \
        result = (default_expr); \
    } else { \
        void **my_fl = (tiny_fl) + (granules); \
        void *my_entry = *my_fl; \
        void *next; \
    \
        for (;;) { \
            if (MANAGED_STACK_ADDRESS_BOEHM_GC_EXPECT((MANAGED_STACK_ADDRESS_BOEHM_GC_word)my_entry \
                          > (num_direct) + MANAGED_STACK_ADDRESS_BOEHM_GC_TINY_FREELISTS + 1, 1)) { \
                next = *(void **)(my_entry); \
                result = (void *)my_entry; \
                MANAGED_STACK_ADDRESS_BOEHM_GC_FAST_M_AO_STORE(my_fl, next); \
                init; \
                MANAGED_STACK_ADDRESS_BOEHM_GC_PREFETCH_FOR_WRITE(next); \
                if ((kind) != MANAGED_STACK_ADDRESS_BOEHM_GC_I_PTRFREE) { \
                    MANAGED_STACK_ADDRESS_BOEHM_GC_end_stubborn_change(my_fl); \
                    MANAGED_STACK_ADDRESS_BOEHM_GC_reachable_here(next); \
                } \
                MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_size(result) >= (granules)*MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES); \
                MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((kind) == MANAGED_STACK_ADDRESS_BOEHM_GC_I_PTRFREE \
                          || ((MANAGED_STACK_ADDRESS_BOEHM_GC_word *)result)[1] == 0); \
                break; \
            } \
            /* Entry contains counter or NULL */ \
            if ((MANAGED_STACK_ADDRESS_BOEHM_GC_signed_word)my_entry - (MANAGED_STACK_ADDRESS_BOEHM_GC_signed_word)(num_direct) <= 0 \
                    /* (MANAGED_STACK_ADDRESS_BOEHM_GC_word)my_entry <= (num_direct) */ \
                    && my_entry != 0 /* NULL */) { \
                /* Small counter value, not NULL */ \
                MANAGED_STACK_ADDRESS_BOEHM_GC_FAST_M_AO_STORE(my_fl, (char *)my_entry \
                                          + (granules) + 1); \
                result = (default_expr); \
                break; \
            } else { \
                /* Large counter or NULL */ \
                MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_many(((granules) == 0? MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES : \
                                        MANAGED_STACK_ADDRESS_BOEHM_GC_RAW_BYTES_FROM_INDEX(granules)), \
                                       kind, my_fl); \
                my_entry = *my_fl; \
                if (my_entry == 0) { \
                    result = (*MANAGED_STACK_ADDRESS_BOEHM_GC_get_oom_fn())((granules)*MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES); \
                    break; \
                } \
            } \
        } \
    } \
  } while (0)

# define MANAGED_STACK_ADDRESS_BOEHM_GC_WORDS_TO_WHOLE_GRANULES(n) \
        MANAGED_STACK_ADDRESS_BOEHM_GC_WORDS_TO_GRANULES((n) + MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_WORDS - 1)

/* Allocate n words (NOT BYTES).  X is made to point to the result.     */
/* This should really only be used if MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers is       */
/* not set, or DONT_ADD_BYTE_AT_END is set.  See above.                 */
/* Does not acquire lock.  The caller is responsible for supplying      */
/* a cleared tiny_fl free list array.  For single-threaded              */
/* applications, this may be a global array.                            */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_WORDS_KIND(result,n,tiny_fl,kind,init) \
    do { \
      size_t granules = MANAGED_STACK_ADDRESS_BOEHM_GC_WORDS_TO_WHOLE_GRANULES(n); \
      MANAGED_STACK_ADDRESS_BOEHM_GC_FAST_MALLOC_GRANS(result, granules, tiny_fl, 0, kind, \
                           MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind(granules*MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES, kind), \
                           init); \
    } while (0)

# define MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_WORDS(result,n,tiny_fl) \
        MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_WORDS_KIND(result, n, tiny_fl, MANAGED_STACK_ADDRESS_BOEHM_GC_I_NORMAL, \
                             *(void **)(result) = 0)

# define MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_ATOMIC_WORDS(result,n,tiny_fl) \
        MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_WORDS_KIND(result, n, tiny_fl, MANAGED_STACK_ADDRESS_BOEHM_GC_I_PTRFREE, (void)0)

/* And once more for two word initialized objects: */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_CONS(result, first, second, tiny_fl) \
    do { \
      void *l = (void *)(first); \
      void *r = (void *)(second); \
      MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_WORDS_KIND(result, 2, tiny_fl, MANAGED_STACK_ADDRESS_BOEHM_GC_I_NORMAL, (void)0); \
      if ((result) != 0 /* NULL */) { \
        *(void **)(result) = l; \
        MANAGED_STACK_ADDRESS_BOEHM_GC_ptr_store_and_dirty((void **)(result) + 1, r); \
        MANAGED_STACK_ADDRESS_BOEHM_GC_reachable_here(l); \
      } \
    } while (0)

/* Print address of each object in the free list.  The caller should    */
/* hold the GC lock.  Defined only if the library has been compiled     */
/* without NO_DEBUGGING.                                                */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_print_free_list(int /* kind */,
                                       size_t /* sz_in_granules */);

#ifdef __cplusplus
  } /* extern "C" */
#endif

#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE_H */
