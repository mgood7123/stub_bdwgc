/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1999-2004 Hewlett-Packard Development Company, L.P.
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

#include "private/gc_priv.h"

#include <string.h>

/* Allocate reclaim list for the kind.  Returns TRUE on success.        */
STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_alloc_reclaim_list(struct obj_kind *ok)
{
    struct hblk ** result;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    result = (struct hblk **)MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_alloc(
                                (MAXOBJGRANULES+1) * sizeof(struct hblk *));
    if (EXPECT(NULL == result, FALSE)) return FALSE;

    BZERO(result, (MAXOBJGRANULES+1)*sizeof(struct hblk *));
    ok -> ok_reclaim_list = result;
    return TRUE;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_alloc_large(size_t lb, int k, unsigned flags,
                              size_t align_m1)
{
    struct hblk * h;
    size_t n_blocks; /* includes alignment */
    ptr_t result = NULL;
    MANAGED_STACK_ADDRESS_BOEHM_GC_bool retry = FALSE;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    lb = ROUNDUP_GRANULE_SIZE(lb);
    n_blocks = OBJ_SZ_TO_BLOCKS_CHECKED(SIZET_SAT_ADD(lb, align_m1));
    if (!EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized, TRUE)) {
      UNLOCK(); /* just to unset MANAGED_STACK_ADDRESS_BOEHM_GC_lock_holder */
      MANAGED_STACK_ADDRESS_BOEHM_GC_init();
      LOCK();
    }
    /* Do our share of marking work.    */
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_incremental && !MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc) {
            ENTER_GC();
            MANAGED_STACK_ADDRESS_BOEHM_GC_collect_a_little_inner((int)n_blocks);
            EXIT_GC();
    }

    h = MANAGED_STACK_ADDRESS_BOEHM_GC_allochblk(lb, k, flags, align_m1);
#   ifdef USE_MUNMAP
        if (NULL == h) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_merge_unmapped();
            h = MANAGED_STACK_ADDRESS_BOEHM_GC_allochblk(lb, k, flags, align_m1);
        }
#   endif
    while (0 == h && MANAGED_STACK_ADDRESS_BOEHM_GC_collect_or_expand(n_blocks, flags, retry)) {
        h = MANAGED_STACK_ADDRESS_BOEHM_GC_allochblk(lb, k, flags, align_m1);
        retry = TRUE;
    }
    if (EXPECT(h != NULL, TRUE)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd += lb;
        if (lb > HBLKSIZE) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_large_allocd_bytes += HBLKSIZE * OBJ_SZ_TO_BLOCKS(lb);
            if (MANAGED_STACK_ADDRESS_BOEHM_GC_large_allocd_bytes > MANAGED_STACK_ADDRESS_BOEHM_GC_max_large_allocd_bytes)
                MANAGED_STACK_ADDRESS_BOEHM_GC_max_large_allocd_bytes = MANAGED_STACK_ADDRESS_BOEHM_GC_large_allocd_bytes;
        }
        /* FIXME: Do we need some way to reset MANAGED_STACK_ADDRESS_BOEHM_GC_max_large_allocd_bytes? */
        result = h -> hb_body;
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(((word)result & align_m1) == 0);
    }
    return result;
}

/* Allocate a large block of size lb bytes.  Clear if appropriate.      */
/* EXTRA_BYTES were already added to lb.  Update MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd.       */
STATIC ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_alloc_large_and_clear(size_t lb, int k, unsigned flags)
{
    ptr_t result;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    result = MANAGED_STACK_ADDRESS_BOEHM_GC_alloc_large(lb, k, flags, 0 /* align_m1 */);
    if (EXPECT(result != NULL, TRUE)
          && (MANAGED_STACK_ADDRESS_BOEHM_GC_debugging_started || MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[k].ok_init)) {
        /* Clear the whole block, in case of MANAGED_STACK_ADDRESS_BOEHM_GC_realloc call. */
        BZERO(result, HBLKSIZE * OBJ_SZ_TO_BLOCKS(lb));
    }
    return result;
}

/* Fill in additional entries in MANAGED_STACK_ADDRESS_BOEHM_GC_size_map, including the i-th one.   */
/* Note that a filled in section of the array ending at n always        */
/* has the length of at least n/4.                                      */
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_extend_size_map(size_t i)
{
  size_t orig_granule_sz = ALLOC_REQUEST_GRANS(i);
  size_t granule_sz;
  size_t byte_sz = GRANULES_TO_BYTES(orig_granule_sz);
                        /* The size we try to preserve.         */
                        /* Close to i, unless this would        */
                        /* introduce too many distinct sizes.   */
  size_t smaller_than_i = byte_sz - (byte_sz >> 3);
  size_t low_limit; /* The lowest indexed entry we initialize.  */
  size_t number_of_objs;

  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(0 == MANAGED_STACK_ADDRESS_BOEHM_GC_size_map[i]);
  if (0 == MANAGED_STACK_ADDRESS_BOEHM_GC_size_map[smaller_than_i]) {
    low_limit = byte_sz - (byte_sz >> 2); /* much smaller than i */
    granule_sz = orig_granule_sz;
    while (MANAGED_STACK_ADDRESS_BOEHM_GC_size_map[low_limit] != 0)
      low_limit++;
  } else {
    low_limit = smaller_than_i + 1;
    while (MANAGED_STACK_ADDRESS_BOEHM_GC_size_map[low_limit] != 0)
      low_limit++;

    granule_sz = ALLOC_REQUEST_GRANS(low_limit);
    granule_sz += granule_sz >> 3;
    if (granule_sz < orig_granule_sz)
      granule_sz = orig_granule_sz;
  }

  /* For these larger sizes, we use an even number of granules.         */
  /* This makes it easier to, e.g., construct a 16-byte-aligned         */
  /* allocator even if MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES is 8.                           */
  granule_sz = (granule_sz + 1) & ~(size_t)1;
  if (granule_sz > MAXOBJGRANULES)
    granule_sz = MAXOBJGRANULES;

  /* If we can fit the same number of larger objects in a block, do so. */
  number_of_objs = HBLK_GRANULES / granule_sz;
  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(number_of_objs != 0);
  granule_sz = (HBLK_GRANULES / number_of_objs) & ~(size_t)1;

  byte_sz = GRANULES_TO_BYTES(granule_sz) - EXTRA_BYTES;
                        /* We may need one extra byte; do not always    */
                        /* fill in MANAGED_STACK_ADDRESS_BOEHM_GC_size_map[byte_sz].                */

  for (; low_limit <= byte_sz; low_limit++)
    MANAGED_STACK_ADDRESS_BOEHM_GC_size_map[low_limit] = granule_sz;
}

STATIC void * MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_inner_small(size_t lb, int k)
{
  struct obj_kind *ok = &MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[k];
  size_t lg = MANAGED_STACK_ADDRESS_BOEHM_GC_size_map[lb];
  void ** opp = &(ok -> ok_freelist[lg]);
  void *op = *opp;

  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
  if (EXPECT(NULL == op, FALSE)) {
    if (lg == 0) {
      if (!EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized, TRUE)) {
        UNLOCK(); /* just to unset MANAGED_STACK_ADDRESS_BOEHM_GC_lock_holder */
        MANAGED_STACK_ADDRESS_BOEHM_GC_init();
        LOCK();
        lg = MANAGED_STACK_ADDRESS_BOEHM_GC_size_map[lb];
      }
      if (0 == lg) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_extend_size_map(lb);
        lg = MANAGED_STACK_ADDRESS_BOEHM_GC_size_map[lb];
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(lg != 0);
      }
      /* Retry */
      opp = &(ok -> ok_freelist[lg]);
      op = *opp;
    }
    if (NULL == op) {
      if (NULL == ok -> ok_reclaim_list
          && !MANAGED_STACK_ADDRESS_BOEHM_GC_alloc_reclaim_list(ok))
        return NULL;
      op = MANAGED_STACK_ADDRESS_BOEHM_GC_allocobj(lg, k);
      if (NULL == op) return NULL;
    }
  }
  *opp = obj_link(op);
  obj_link(op) = NULL;
  MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd += GRANULES_TO_BYTES((word)lg);
  return op;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void * MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_inner(size_t lb, int k, unsigned flags)
{
    size_t lb_adjusted;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(k < MAXOBJKINDS);
    if (SMALL_OBJ(lb)) {
        return MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_inner_small(lb, k);
    }

#   if MAX_EXTRA_BYTES > 0
      if ((flags & IGNORE_OFF_PAGE) != 0 && lb >= HBLKSIZE) {
        /* No need to add EXTRA_BYTES.  */
        lb_adjusted = lb;
      } else
#   endif
    /* else */ {
      lb_adjusted = ADD_EXTRA_BYTES(lb);
    }
    return MANAGED_STACK_ADDRESS_BOEHM_GC_alloc_large_and_clear(lb_adjusted, k, flags);
}

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_COLLECT_AT_MALLOC
  /* Parameter to force GC at every malloc of size greater or equal to  */
  /* the given value.  This might be handy during debugging.            */
# if defined(CPPCHECK)
    size_t MANAGED_STACK_ADDRESS_BOEHM_GC_dbg_collect_at_malloc_min_lb = 16*1024; /* e.g. */
# else
    size_t MANAGED_STACK_ADDRESS_BOEHM_GC_dbg_collect_at_malloc_min_lb = (MANAGED_STACK_ADDRESS_BOEHM_GC_COLLECT_AT_MALLOC);
# endif
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void * MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_aligned(size_t lb, int k, unsigned flags,
                                          size_t align_m1)
{
    void * result;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(k < MAXOBJKINDS);
    if (EXPECT(get_have_errors(), FALSE))
      MANAGED_STACK_ADDRESS_BOEHM_GC_print_all_errors();
    MANAGED_STACK_ADDRESS_BOEHM_GC_INVOKE_FINALIZERS();
    MANAGED_STACK_ADDRESS_BOEHM_GC_DBG_COLLECT_AT_MALLOC(lb);
    if (SMALL_OBJ(lb) && EXPECT(align_m1 < MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES, TRUE)) {
        LOCK();
        result = MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_inner_small(lb, k);
        UNLOCK();
    } else {
#       ifdef THREADS
          size_t lg;
#       endif
        size_t lb_rounded;
        MANAGED_STACK_ADDRESS_BOEHM_GC_bool init;

#       if MAX_EXTRA_BYTES > 0
          if ((flags & IGNORE_OFF_PAGE) != 0 && lb >= HBLKSIZE) {
            /* No need to add EXTRA_BYTES.      */
            lb_rounded = ROUNDUP_GRANULE_SIZE(lb);
#           ifdef THREADS
              lg = BYTES_TO_GRANULES(lb_rounded);
#           endif
          } else
#       endif
        /* else */ {
#         ifndef THREADS
            size_t lg; /* CPPCHECK */
#         endif

          if (EXPECT(0 == lb, FALSE)) lb = 1;
          lg = ALLOC_REQUEST_GRANS(lb);
          lb_rounded = GRANULES_TO_BYTES(lg);
        }

        init = MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[k].ok_init;
        if (EXPECT(align_m1 < MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES, TRUE)) {
          align_m1 = 0;
        } else if (align_m1 < HBLKSIZE) {
          align_m1 = HBLKSIZE - 1;
        }
        LOCK();
        result = MANAGED_STACK_ADDRESS_BOEHM_GC_alloc_large(lb_rounded, k, flags, align_m1);
        if (EXPECT(result != NULL, TRUE)) {
          if (MANAGED_STACK_ADDRESS_BOEHM_GC_debugging_started
#             ifndef THREADS
                || init
#             endif
             ) {
            BZERO(result, HBLKSIZE * OBJ_SZ_TO_BLOCKS(lb_rounded));
          } else {
#           ifdef THREADS
              MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(GRANULES_TO_WORDS(lg) >= 2);
              /* Clear any memory that might be used for GC descriptors */
              /* before we release the lock.                            */
                ((word *)result)[0] = 0;
                ((word *)result)[1] = 0;
                ((word *)result)[GRANULES_TO_WORDS(lg)-1] = 0;
                ((word *)result)[GRANULES_TO_WORDS(lg)-2] = 0;
#           endif
          }
        }
        UNLOCK();
#       ifdef THREADS
          if (init && !MANAGED_STACK_ADDRESS_BOEHM_GC_debugging_started && result != NULL) {
            /* Clear the rest (i.e. excluding the initial 2 words). */
            BZERO((word *)result + 2,
                  HBLKSIZE * OBJ_SZ_TO_BLOCKS(lb_rounded) - 2 * sizeof(word));
          }
#       endif
    }
    if (EXPECT(NULL == result, FALSE))
      result = (*MANAGED_STACK_ADDRESS_BOEHM_GC_get_oom_fn())(lb); /* might be misaligned */
    return result;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc(size_t lb, int k)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_aligned(lb, k, 0 /* flags */, 0 /* align_m1 */);
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind_global(size_t lb, int k)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(k < MAXOBJKINDS);
    if (SMALL_OBJ(lb)) {
        void *op;
        void **opp;
        size_t lg;

        MANAGED_STACK_ADDRESS_BOEHM_GC_DBG_COLLECT_AT_MALLOC(lb);
        LOCK();
        lg = MANAGED_STACK_ADDRESS_BOEHM_GC_size_map[lb];
        opp = &MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[k].ok_freelist[lg];
        op = *opp;
        if (EXPECT(op != NULL, TRUE)) {
            if (k == PTRFREE) {
                *opp = obj_link(op);
            } else {
                MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NULL == obj_link(op)
                          || ((word)obj_link(op) < MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_real_heap_addr
                             && (word)obj_link(op) > MANAGED_STACK_ADDRESS_BOEHM_GC_least_real_heap_addr));
                *opp = obj_link(op);
                obj_link(op) = 0;
            }
            MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd += GRANULES_TO_BYTES((word)lg);
            UNLOCK();
            return op;
        }
        UNLOCK();
    }

    /* We make the MANAGED_STACK_ADDRESS_BOEHM_GC_clear_stack() call a tail one, hoping to get more */
    /* of the stack.                                                    */
    return MANAGED_STACK_ADDRESS_BOEHM_GC_clear_stack(MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_aligned(lb, k, 0 /* flags */, 0));
}

#if defined(THREADS) && !defined(THREAD_LOCAL_ALLOC)
  MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind(size_t lb, int k)
  {
    return MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind_global(lb, k);
  }
#endif

/* Allocate lb bytes of atomic (pointer-free) data.     */
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_atomic(size_t lb)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind(lb, PTRFREE);
}

/* Allocate lb bytes of composite (pointerful) data.    */
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(size_t lb)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind(lb, NORMAL);
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_uncollectable(
                                                        size_t lb, int k)
{
    void *op;
    size_t lb_orig = lb;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(k < MAXOBJKINDS);
    if (EXTRA_BYTES != 0 && EXPECT(lb != 0, TRUE)) lb--;
                /* We do not need the extra byte, since this will   */
                /* not be collected anyway.                         */

    if (SMALL_OBJ(lb)) {
        void **opp;
        size_t lg;

        if (EXPECT(get_have_errors(), FALSE))
          MANAGED_STACK_ADDRESS_BOEHM_GC_print_all_errors();
        MANAGED_STACK_ADDRESS_BOEHM_GC_INVOKE_FINALIZERS();
        MANAGED_STACK_ADDRESS_BOEHM_GC_DBG_COLLECT_AT_MALLOC(lb_orig);
        LOCK();
        lg = MANAGED_STACK_ADDRESS_BOEHM_GC_size_map[lb];
        opp = &MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[k].ok_freelist[lg];
        op = *opp;
        if (EXPECT(op != NULL, TRUE)) {
            *opp = obj_link(op);
            obj_link(op) = 0;
            MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd += GRANULES_TO_BYTES((word)lg);
            /* Mark bit was already set on free list.  It will be       */
            /* cleared only temporarily during a collection, as a       */
            /* result of the normal free list mark bit clearing.        */
            MANAGED_STACK_ADDRESS_BOEHM_GC_non_gc_bytes += GRANULES_TO_BYTES((word)lg);
        } else {
            op = MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_inner_small(lb, k);
            if (NULL == op) {
              MANAGED_STACK_ADDRESS_BOEHM_GC_oom_func oom_fn = MANAGED_STACK_ADDRESS_BOEHM_GC_oom_fn;
              UNLOCK();
              return (*oom_fn)(lb_orig);
            }
            /* For small objects, the free lists are completely marked. */
        }
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_marked(op));
        UNLOCK();
    } else {
      op = MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_aligned(lb, k, 0 /* flags */, 0 /* align_m1 */);
      if (op /* != NULL */) { /* CPPCHECK */
        hdr * hhdr = HDR(op);

        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(HBLKDISPL(op) == 0); /* large block */
        /* We don't need the lock here, since we have an undisguised    */
        /* pointer.  We do need to hold the lock while we adjust        */
        /* mark bits.                                                   */
        LOCK();
        set_mark_bit_from_hdr(hhdr, 0); /* Only object. */
#       ifndef THREADS
          MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(hhdr -> hb_n_marks == 0);
                /* This is not guaranteed in the multi-threaded case    */
                /* because the counter could be updated before locking. */
#       endif
        hhdr -> hb_n_marks = 1;
        UNLOCK();
      }
    }
    return op;
}

/* Allocate lb bytes of pointerful, traced, but not collectible data.   */
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_uncollectable(size_t lb)
{
  return MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_uncollectable(lb, UNCOLLECTABLE);
}

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ATOMIC_UNCOLLECTABLE
  /* Allocate lb bytes of pointer-free, untraced, uncollectible data    */
  /* This is normally roughly equivalent to the system malloc.          */
  /* But it may be useful if malloc is redefined.                       */
  MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL
        MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_atomic_uncollectable(size_t lb)
  {
    return MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_uncollectable(lb, AUNCOLLECTABLE);
  }
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_ATOMIC_UNCOLLECTABLE */

#if defined(REDIRECT_MALLOC) && !defined(REDIRECT_MALLOC_IN_HEADER)

# ifndef MSWINCE
#   include <errno.h>
# endif

  /* Avoid unnecessary nested procedure calls here, by #defining some   */
  /* malloc replacements.  Otherwise we end up saving a meaningless     */
  /* return address in the object.  It also speeds things up, but it is */
  /* admittedly quite ugly.                                             */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_debug_malloc_replacement(lb) MANAGED_STACK_ADDRESS_BOEHM_GC_debug_malloc(lb, MANAGED_STACK_ADDRESS_BOEHM_GC_DBG_EXTRAS)

# if defined(CPPCHECK)
#   define REDIRECT_MALLOC_F MANAGED_STACK_ADDRESS_BOEHM_GC_malloc /* e.g. */
# else
#   define REDIRECT_MALLOC_F REDIRECT_MALLOC
# endif

  void * malloc(size_t lb)
  {
    /* It might help to manually inline the MANAGED_STACK_ADDRESS_BOEHM_GC_malloc call here.        */
    /* But any decent compiler should reduce the extra procedure call   */
    /* to at most a jump instruction in this case.                      */
#   if defined(I386) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_SOLARIS_THREADS)
      /* Thread initialization can call malloc before we are ready for. */
      /* It is not clear that this is enough to help matters.           */
      /* The thread implementation may well call malloc at other        */
      /* inopportune times.                                             */
      if (!EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized, TRUE)) return sbrk(lb);
#   endif
    return (void *)REDIRECT_MALLOC_F(lb);
  }

# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_LINUX_THREADS)
#   ifdef HAVE_LIBPTHREAD_SO
      STATIC ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_libpthread_start = NULL;
      STATIC ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_libpthread_end = NULL;
#   endif
    STATIC ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_libld_start = NULL;
    STATIC ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_libld_end = NULL;

    STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_init_lib_bounds(void)
    {
      IF_CANCEL(int cancel_state;)

      DISABLE_CANCEL(cancel_state);
      MANAGED_STACK_ADDRESS_BOEHM_GC_init(); /* if not called yet */
#     ifdef HAVE_LIBPTHREAD_SO
        if (!MANAGED_STACK_ADDRESS_BOEHM_GC_text_mapping("libpthread-",
                             &MANAGED_STACK_ADDRESS_BOEHM_GC_libpthread_start, &MANAGED_STACK_ADDRESS_BOEHM_GC_libpthread_end)) {
          WARN("Failed to find libpthread.so text mapping: Expect crash\n", 0);
          /* This might still work with some versions of libpthread,    */
          /* so we do not abort.                                        */
        }
#     endif
      if (!MANAGED_STACK_ADDRESS_BOEHM_GC_text_mapping("ld-", &MANAGED_STACK_ADDRESS_BOEHM_GC_libld_start, &MANAGED_STACK_ADDRESS_BOEHM_GC_libld_end)) {
          WARN("Failed to find ld.so text mapping: Expect crash\n", 0);
      }
      RESTORE_CANCEL(cancel_state);
    }
# endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_LINUX_THREADS */

  void * calloc(size_t n, size_t lb)
  {
    if (EXPECT((lb | n) > MANAGED_STACK_ADDRESS_BOEHM_GC_SQRT_SIZE_MAX, FALSE) /* fast initial test */
        && lb && n > MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_MAX / lb)
      return (*MANAGED_STACK_ADDRESS_BOEHM_GC_get_oom_fn())(MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_MAX); /* n*lb overflow */
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_LINUX_THREADS)
      /* The linker may allocate some memory that is only pointed to by */
      /* mmapped thread stacks.  Make sure it is not collectible.       */
      {
        static MANAGED_STACK_ADDRESS_BOEHM_GC_bool lib_bounds_set = FALSE;
        ptr_t caller = (ptr_t)__builtin_return_address(0);

        /* This test does not need to ensure memory visibility, since   */
        /* the bounds will be set when/if we create another thread.     */
        if (!EXPECT(lib_bounds_set, TRUE)) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_init_lib_bounds();
          lib_bounds_set = TRUE;
        }
        if (((word)caller >= (word)MANAGED_STACK_ADDRESS_BOEHM_GC_libld_start
             && (word)caller < (word)MANAGED_STACK_ADDRESS_BOEHM_GC_libld_end)
#           ifdef HAVE_LIBPTHREAD_SO
              || ((word)caller >= (word)MANAGED_STACK_ADDRESS_BOEHM_GC_libpthread_start
                  && (word)caller < (word)MANAGED_STACK_ADDRESS_BOEHM_GC_libpthread_end)
                    /* The two ranges are actually usually adjacent,    */
                    /* so there may be a way to speed this up.          */
#           endif
           ) {
          return MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_uncollectable(n * lb, UNCOLLECTABLE);
        }
      }
#   endif
    return (void *)REDIRECT_MALLOC_F(n * lb);
  }

# ifndef strdup
    char *strdup(const char *s)
    {
      size_t lb = strlen(s) + 1;
      char *result = (char *)REDIRECT_MALLOC_F(lb);

      if (EXPECT(NULL == result, FALSE)) {
        errno = ENOMEM;
        return NULL;
      }
      BCOPY(s, result, lb);
      return result;
    }
# endif /* !defined(strdup) */
 /* If strdup is macro defined, we assume that it actually calls malloc, */
 /* and thus the right thing will happen even without overriding it.     */
 /* This seems to be true on most Linux systems.                         */

# ifndef strndup
    /* This is similar to strdup().     */
    char *strndup(const char *str, size_t size)
    {
      char *copy;
      size_t len = strlen(str);
      if (EXPECT(len > size, FALSE))
        len = size;
      copy = (char *)REDIRECT_MALLOC_F(len + 1);
      if (EXPECT(NULL == copy, FALSE)) {
        errno = ENOMEM;
        return NULL;
      }
      if (EXPECT(len > 0, TRUE))
        BCOPY(str, copy, len);
      copy[len] = '\0';
      return copy;
    }
# endif /* !strndup */

# undef MANAGED_STACK_ADDRESS_BOEHM_GC_debug_malloc_replacement

#endif /* REDIRECT_MALLOC */

/* Explicitly deallocate the object.  hhdr should correspond to p.      */
static void free_internal(void *p, hdr *hhdr)
{
  size_t sz = (size_t)(hhdr -> hb_sz); /* in bytes */
  size_t ngranules = BYTES_TO_GRANULES(sz); /* size in granules */
  int k = hhdr -> hb_obj_kind;

  MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_freed += sz;
  if (IS_UNCOLLECTABLE(k)) MANAGED_STACK_ADDRESS_BOEHM_GC_non_gc_bytes -= sz;
  if (EXPECT(ngranules <= MAXOBJGRANULES, TRUE)) {
    struct obj_kind *ok = &MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[k];
    void **flh;

    /* It is unnecessary to clear the mark bit.  If the object is       */
    /* reallocated, it does not matter.  Otherwise, the collector will  */
    /* do it, since it is on a free list.                               */
    if (ok -> ok_init && EXPECT(sz > sizeof(word), TRUE)) {
      BZERO((word *)p + 1, sz - sizeof(word));
    }

    flh = &(ok -> ok_freelist[ngranules]);
    obj_link(p) = *flh;
    *flh = (ptr_t)p;
  } else {
    if (sz > HBLKSIZE) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_large_allocd_bytes -= HBLKSIZE * OBJ_SZ_TO_BLOCKS(sz);
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_freehblk(HBLKPTR(p));
  }
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_free(void * p)
{
    hdr *hhdr;

    if (p /* != NULL */) {
        /* CPPCHECK */
    } else {
        /* Required by ANSI.  It's not my fault ...     */
        return;
    }

#   ifdef LOG_ALLOCS
      MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_free(%p) after GC #%lu\n",
                    p, (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no);
#   endif
    hhdr = HDR(p);
#   if defined(REDIRECT_MALLOC) && \
        ((defined(NEED_CALLINFO) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_HAVE_BUILTIN_BACKTRACE)) \
         || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_SOLARIS_THREADS) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_LINUX_THREADS) \
         || defined(MSWIN32))
        /* This might be called indirectly by MANAGED_STACK_ADDRESS_BOEHM_GC_print_callers to free  */
        /* the result of backtrace_symbols.                             */
        /* For Solaris, we have to redirect malloc calls during         */
        /* initialization.  For the others, this seems to happen        */
        /* implicitly.                                                  */
        /* Don't try to deallocate that memory.                         */
        if (EXPECT(NULL == hhdr, FALSE)) return;
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_base(p) == p);
    LOCK();
    free_internal(p, hhdr);
    UNLOCK();
}

#ifdef THREADS
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_free_inner(void * p)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    free_internal(p, HDR(p));
  }
#endif /* THREADS */

#if defined(REDIRECT_MALLOC) && !defined(REDIRECT_FREE)
# define REDIRECT_FREE MANAGED_STACK_ADDRESS_BOEHM_GC_free
#endif

#if defined(REDIRECT_FREE) && !defined(REDIRECT_MALLOC_IN_HEADER)

# if defined(CPPCHECK)
#   define REDIRECT_FREE_F MANAGED_STACK_ADDRESS_BOEHM_GC_free /* e.g. */
# else
#   define REDIRECT_FREE_F REDIRECT_FREE
# endif

  void free(void * p)
  {
#   ifndef IGNORE_FREE
#     if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_LINUX_THREADS) && !defined(USE_PROC_FOR_LIBRARIES)
        /* Don't bother with initialization checks.  If nothing         */
        /* has been initialized, the check fails, and that's safe,      */
        /* since we have not allocated uncollectible objects neither.   */
        ptr_t caller = (ptr_t)__builtin_return_address(0);
        /* This test does not need to ensure memory visibility, since   */
        /* the bounds will be set when/if we create another thread.     */
        if (((word)caller >= (word)MANAGED_STACK_ADDRESS_BOEHM_GC_libld_start
             && (word)caller < (word)MANAGED_STACK_ADDRESS_BOEHM_GC_libld_end)
#           ifdef HAVE_LIBPTHREAD_SO
              || ((word)caller >= (word)MANAGED_STACK_ADDRESS_BOEHM_GC_libpthread_start
                  && (word)caller < (word)MANAGED_STACK_ADDRESS_BOEHM_GC_libpthread_end)
#           endif
           ) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_free(p);
          return;
        }
#     endif
      REDIRECT_FREE_F(p);
#   endif
  }
#endif /* REDIRECT_FREE */
