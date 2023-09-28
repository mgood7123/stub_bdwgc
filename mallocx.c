/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 2000 by Hewlett-Packard Company.  All rights reserved.
 * Copyright (c) 2009-2022 Ivan Maidanski
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

/*
 * These are extra allocation routines which are likely to be less
 * frequently used than those in malloc.c.  They are separate in the
 * hope that the .o file will be excluded from statically linked
 * executables.  We should probably break this up further.
 */

#include <string.h>

#ifndef MSWINCE
# include <errno.h>
#endif

/* Some externally visible but unadvertised variables to allow access to */
/* free lists from inlined allocators without including gc_priv.h        */
/* or introducing dependencies on internal data structure layouts.       */
#include "private/gc_alloc_ptrs.h"
void ** const MANAGED_STACK_ADDRESS_BOEHM_GC_objfreelist_ptr = MANAGED_STACK_ADDRESS_BOEHM_GC_objfreelist;
void ** const MANAGED_STACK_ADDRESS_BOEHM_GC_aobjfreelist_ptr = MANAGED_STACK_ADDRESS_BOEHM_GC_aobjfreelist;
void ** const MANAGED_STACK_ADDRESS_BOEHM_GC_uobjfreelist_ptr = MANAGED_STACK_ADDRESS_BOEHM_GC_uobjfreelist;
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ATOMIC_UNCOLLECTABLE
    void ** const MANAGED_STACK_ADDRESS_BOEHM_GC_auobjfreelist_ptr = MANAGED_STACK_ADDRESS_BOEHM_GC_auobjfreelist;
# endif

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_kind_and_size(const void * p, size_t * psize)
{
    hdr * hhdr = HDR((/* no const */ void *)(word)p);

    if (psize != NULL) {
        *psize = (size_t)(hhdr -> hb_sz);
    }
    return hhdr -> hb_obj_kind;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_generic_or_special_malloc(size_t lb,
                                                                  int k)
{
    switch (k) {
        case PTRFREE:
        case NORMAL:
            return MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind(lb, k);
        case UNCOLLECTABLE:
#       ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ATOMIC_UNCOLLECTABLE
          case AUNCOLLECTABLE:
#       endif
            return MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_uncollectable(lb, k);
        default:
            return MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_aligned(lb, k, 0 /* flags */, 0);
    }
}

/* Change the size of the block pointed to by p to contain at least   */
/* lb bytes.  The object may be (and quite likely will be) moved.     */
/* The kind (e.g. atomic) is the same as that of the old.             */
/* Shrinking of large blocks is not implemented well.                 */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_realloc(void * p, size_t lb)
{
    struct hblk * h;
    hdr * hhdr;
    void * result;
#   if defined(_FORTIFY_SOURCE) && defined(__GNUC__) && !defined(__clang__)
      volatile  /* Use cleared_p instead of p as a workaround to avoid  */
                /* passing alloc_size(lb) attribute associated with p   */
                /* to memset (including memset call inside MANAGED_STACK_ADDRESS_BOEHM_GC_free).    */
#   endif
      word cleared_p = (word)p;
    size_t sz;      /* Current size in bytes    */
    size_t orig_sz; /* Original sz in bytes     */
    int obj_kind;

    if (NULL == p) return MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(lb);  /* Required by ANSI */
    if (0 == lb) /* and p != NULL */ {
#     ifndef IGNORE_FREE
        MANAGED_STACK_ADDRESS_BOEHM_GC_free(p);
#     endif
      return NULL;
    }
    h = HBLKPTR(p);
    hhdr = HDR(h);
    sz = (size_t)hhdr->hb_sz;
    obj_kind = hhdr -> hb_obj_kind;
    orig_sz = sz;

    if (sz > MAXOBJBYTES) {
        struct obj_kind * ok = &MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[obj_kind];
        word descr = ok -> ok_descriptor;

        /* Round it up to the next whole heap block.    */
        sz = (sz + HBLKSIZE-1) & ~(HBLKSIZE-1);
#       if ALIGNMENT > MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS
          /* An extra byte is not added in case of ignore-off-page  */
          /* allocated objects not smaller than HBLKSIZE.           */
          MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(sz >= HBLKSIZE);
          if (EXTRA_BYTES != 0 && (hhdr -> hb_flags & IGNORE_OFF_PAGE) != 0
              && obj_kind == NORMAL)
            descr += ALIGNMENT; /* or set to 0 */
#       endif
        if (ok -> ok_relocate_descr)
          descr += sz;
        /* MANAGED_STACK_ADDRESS_BOEHM_GC_realloc might be changing the block size while            */
        /* MANAGED_STACK_ADDRESS_BOEHM_GC_reclaim_block or MANAGED_STACK_ADDRESS_BOEHM_GC_clear_hdr_marks is examining it.      */
        /* The change to the size field is benign, in that MANAGED_STACK_ADDRESS_BOEHM_GC_reclaim   */
        /* (and MANAGED_STACK_ADDRESS_BOEHM_GC_clear_hdr_marks) would work correctly with either    */
        /* value, since we are not changing the number of objects in    */
        /* the block.  But seeing a half-updated value (though unlikely */
        /* to occur in practice) could be probably bad.                 */
        /* Using unordered atomic accesses on the size and hb_descr     */
        /* fields would solve the issue.  (The alternate solution might */
        /* be to initially overallocate large objects, so we do not     */
        /* have to adjust the size in MANAGED_STACK_ADDRESS_BOEHM_GC_realloc, if they still fit.    */
        /* But that is probably more expensive, since we may end up     */
        /* scanning a bunch of zeros during GC.)                        */
#       ifdef AO_HAVE_store
          MANAGED_STACK_ADDRESS_BOEHM_GC_STATIC_ASSERT(sizeof(hhdr->hb_sz) == sizeof(AO_t));
          AO_store((volatile AO_t *)&hhdr->hb_sz, (AO_t)sz);
          AO_store((volatile AO_t *)&hhdr->hb_descr, (AO_t)descr);
#       else
          {
            LOCK();
            hhdr -> hb_sz = sz;
            hhdr -> hb_descr = descr;
            UNLOCK();
          }
#       endif

#         ifdef MARK_BIT_PER_OBJ
            MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(hhdr -> hb_inv_sz == LARGE_INV_SZ);
#         else
            MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((hhdr -> hb_flags & LARGE_BLOCK) != 0
                        && hhdr -> hb_map[ANY_INDEX] == 1);
#         endif
          if (IS_UNCOLLECTABLE(obj_kind)) MANAGED_STACK_ADDRESS_BOEHM_GC_non_gc_bytes += (sz - orig_sz);
          /* Extra area is already cleared by MANAGED_STACK_ADDRESS_BOEHM_GC_alloc_large_and_clear. */
    }
    if (ADD_EXTRA_BYTES(lb) <= sz) {
        if (lb >= (sz >> 1)) {
            if (orig_sz > lb) {
              /* Clear unneeded part of object to avoid bogus pointer */
              /* tracing.                                             */
                BZERO((ptr_t)cleared_p + lb, orig_sz - lb);
            }
            return p;
        }
        /* shrink */
        sz = lb;
    }
    result = MANAGED_STACK_ADDRESS_BOEHM_GC_generic_or_special_malloc((word)lb, obj_kind);
    if (EXPECT(result != NULL, TRUE)) {
      /* In case of shrink, it could also return original object.       */
      /* But this gives the client warning of imminent disaster.        */
      BCOPY(p, result, sz);
#     ifndef IGNORE_FREE
        MANAGED_STACK_ADDRESS_BOEHM_GC_free((ptr_t)cleared_p);
#     endif
    }
    return result;
}

# if defined(REDIRECT_MALLOC) && !defined(REDIRECT_REALLOC)
#   define REDIRECT_REALLOC MANAGED_STACK_ADDRESS_BOEHM_GC_realloc
# endif

# ifdef REDIRECT_REALLOC

/* As with malloc, avoid two levels of extra calls here.        */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_debug_realloc_replacement(p, lb) \
        MANAGED_STACK_ADDRESS_BOEHM_GC_debug_realloc(p, lb, MANAGED_STACK_ADDRESS_BOEHM_GC_DBG_EXTRAS)

# if !defined(REDIRECT_MALLOC_IN_HEADER)
    void * realloc(void * p, size_t lb)
    {
      return REDIRECT_REALLOC(p, lb);
    }
# endif

# undef MANAGED_STACK_ADDRESS_BOEHM_GC_debug_realloc_replacement
# endif /* REDIRECT_REALLOC */

/* Allocate memory such that only pointers to near the          */
/* beginning of the object are considered.                      */
/* We avoid holding allocation lock while we clear the memory.  */
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL
    MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_ignore_off_page(size_t lb, int k)
{
  return MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_aligned(lb, k, IGNORE_OFF_PAGE, 0 /* align_m1 */);
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_ignore_off_page(size_t lb)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_aligned(lb, NORMAL, IGNORE_OFF_PAGE, 0);
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL
    MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_atomic_ignore_off_page(size_t lb)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_aligned(lb, PTRFREE, IGNORE_OFF_PAGE, 0);
}

/* Increment MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd from code that doesn't have direct access  */
/* to MANAGED_STACK_ADDRESS_BOEHM_GC_arrays.                                                        */
void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_incr_bytes_allocd(size_t n)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd += n;
}

/* The same for MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_freed.                         */
void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_incr_bytes_freed(size_t n)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_freed += n;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API size_t MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_expl_freed_bytes_since_gc(void)
{
    return (size_t)MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_freed;
}

# ifdef PARALLEL_MARK
    STATIC volatile AO_t MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd_tmp = 0;
                        /* Number of bytes of memory allocated since    */
                        /* we released the GC lock.  Instead of         */
                        /* reacquiring the GC lock just to add this in, */
                        /* we add it in the next time we reacquire      */
                        /* the lock.  (Atomically adding it doesn't     */
                        /* work, since we would have to atomically      */
                        /* update it in MANAGED_STACK_ADDRESS_BOEHM_GC_malloc, which is too         */
                        /* expensive.)                                  */
# endif /* PARALLEL_MARK */

/* Return a list of 1 or more objects of the indicated size, linked     */
/* through the first word in the object.  This has the advantage that   */
/* it acquires the allocation lock only once, and may greatly reduce    */
/* time wasted contending for the allocation lock.  Typical usage would */
/* be in a thread that requires many items of the same size.  It would  */
/* keep its own free list in thread-local storage, and call             */
/* MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_many or friends to replenish it.  (We do not round up      */
/* object sizes, since a call indicates the intention to consume many   */
/* objects of exactly this size.)                                       */
/* We assume that the size is a multiple of MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES.           */
/* We return the free-list by assigning it to *result, since it is      */
/* not safe to return, e.g. a linked list of pointer-free objects,      */
/* since the collector would not retain the entire list if it were      */
/* invoked just as we were returning.                                   */
/* Note that the client should usually clear the link field.            */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_many(size_t lb, int k, void **result)
{
    void *op;
    void *p;
    void **opp;
    size_t lw;      /* Length in words.     */
    size_t lg;      /* Length in granules.  */
    word my_bytes_allocd = 0;
    struct obj_kind * ok;
    struct hblk ** rlh;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(lb != 0 && (lb & (MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES-1)) == 0);
    /* Currently a single object is always allocated if manual VDB. */
    /* TODO: MANAGED_STACK_ADDRESS_BOEHM_GC_dirty should be called for each linked object (but  */
    /* the last one) to support multiple objects allocation.        */
    if (!SMALL_OBJ(lb) || MANAGED_STACK_ADDRESS_BOEHM_GC_manual_vdb) {
        op = MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_aligned(lb, k, 0 /* flags */, 0 /* align_m1 */);
        if (EXPECT(0 != op, TRUE))
            obj_link(op) = 0;
        *result = op;
#       ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL
          if (MANAGED_STACK_ADDRESS_BOEHM_GC_manual_vdb && MANAGED_STACK_ADDRESS_BOEHM_GC_is_heap_ptr(result)) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_dirty_inner(result);
            REACHABLE_AFTER_DIRTY(op);
          }
#       endif
        return;
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(k < MAXOBJKINDS);
    lw = BYTES_TO_WORDS(lb);
    lg = BYTES_TO_GRANULES(lb);
    if (EXPECT(get_have_errors(), FALSE))
      MANAGED_STACK_ADDRESS_BOEHM_GC_print_all_errors();
    MANAGED_STACK_ADDRESS_BOEHM_GC_INVOKE_FINALIZERS();
    MANAGED_STACK_ADDRESS_BOEHM_GC_DBG_COLLECT_AT_MALLOC(lb);
    if (!EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized, TRUE)) MANAGED_STACK_ADDRESS_BOEHM_GC_init();
    LOCK();
    /* Do our share of marking work */
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_incremental && !MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc) {
        ENTER_GC();
        MANAGED_STACK_ADDRESS_BOEHM_GC_collect_a_little_inner(1);
        EXIT_GC();
      }
    /* First see if we can reclaim a page of objects waiting to be */
    /* reclaimed.                                                  */
    ok = &MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[k];
    rlh = ok -> ok_reclaim_list;
    if (rlh != NULL) {
        struct hblk * hbp;
        hdr * hhdr;

        while ((hbp = rlh[lg]) != NULL) {
            hhdr = HDR(hbp);
            rlh[lg] = hhdr -> hb_next;
            MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(hhdr -> hb_sz == lb);
            hhdr -> hb_last_reclaimed = (unsigned short) MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no;
#           ifdef PARALLEL_MARK
              if (MANAGED_STACK_ADDRESS_BOEHM_GC_parallel) {
                  signed_word my_bytes_allocd_tmp =
                                (signed_word)AO_load(&MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd_tmp);
                  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(my_bytes_allocd_tmp >= 0);
                  /* We only decrement it while holding the GC lock.    */
                  /* Thus we can't accidentally adjust it down in more  */
                  /* than one thread simultaneously.                    */

                  if (my_bytes_allocd_tmp != 0) {
                    (void)AO_fetch_and_add(&MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd_tmp,
                                           (AO_t)(-my_bytes_allocd_tmp));
                    MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd += (word)my_bytes_allocd_tmp;
                  }
                  MANAGED_STACK_ADDRESS_BOEHM_GC_acquire_mark_lock();
                  ++ MANAGED_STACK_ADDRESS_BOEHM_GC_fl_builder_count;
                  UNLOCK();
                  MANAGED_STACK_ADDRESS_BOEHM_GC_release_mark_lock();
              }
#           endif
            op = MANAGED_STACK_ADDRESS_BOEHM_GC_reclaim_generic(hbp, hhdr, lb,
                                    ok -> ok_init, 0, &my_bytes_allocd);
            if (op != 0) {
#             ifdef PARALLEL_MARK
                if (MANAGED_STACK_ADDRESS_BOEHM_GC_parallel) {
                  *result = op;
                  (void)AO_fetch_and_add(&MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd_tmp,
                                         (AO_t)my_bytes_allocd);
                  MANAGED_STACK_ADDRESS_BOEHM_GC_acquire_mark_lock();
                  -- MANAGED_STACK_ADDRESS_BOEHM_GC_fl_builder_count;
                  if (MANAGED_STACK_ADDRESS_BOEHM_GC_fl_builder_count == 0) MANAGED_STACK_ADDRESS_BOEHM_GC_notify_all_builder();
#                 ifdef THREAD_SANITIZER
                    MANAGED_STACK_ADDRESS_BOEHM_GC_release_mark_lock();
                    LOCK();
                    MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_found += (signed_word)my_bytes_allocd;
                    UNLOCK();
#                 else
                    MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_found += (signed_word)my_bytes_allocd;
                                        /* The result may be inaccurate. */
                    MANAGED_STACK_ADDRESS_BOEHM_GC_release_mark_lock();
#                 endif
                  (void) MANAGED_STACK_ADDRESS_BOEHM_GC_clear_stack(0);
                  return;
                }
#             endif
              /* We also reclaimed memory, so we need to adjust       */
              /* that count.                                          */
              MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_found += (signed_word)my_bytes_allocd;
              MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd += my_bytes_allocd;
              goto out;
            }
#           ifdef PARALLEL_MARK
              if (MANAGED_STACK_ADDRESS_BOEHM_GC_parallel) {
                MANAGED_STACK_ADDRESS_BOEHM_GC_acquire_mark_lock();
                -- MANAGED_STACK_ADDRESS_BOEHM_GC_fl_builder_count;
                if (MANAGED_STACK_ADDRESS_BOEHM_GC_fl_builder_count == 0) MANAGED_STACK_ADDRESS_BOEHM_GC_notify_all_builder();
                MANAGED_STACK_ADDRESS_BOEHM_GC_release_mark_lock();
                LOCK();
                /* The GC lock is needed for reclaim list access.  We   */
                /* must decrement fl_builder_count before reacquiring   */
                /* the lock.  Hopefully this path is rare.              */

                rlh = ok -> ok_reclaim_list; /* reload rlh after locking */
                if (NULL == rlh) break;
              }
#           endif
        }
    }
    /* Next try to use prefix of global free list if there is one.      */
    /* We don't refill it, but we need to use it up before allocating   */
    /* a new block ourselves.                                           */
      opp = &(ok -> ok_freelist[lg]);
      if ((op = *opp) != NULL) {
        *opp = 0;
        my_bytes_allocd = 0;
        for (p = op; p != 0; p = obj_link(p)) {
          my_bytes_allocd += lb;
          if ((word)my_bytes_allocd >= HBLKSIZE) {
            *opp = obj_link(p);
            obj_link(p) = 0;
            break;
          }
        }
        MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd += my_bytes_allocd;
        goto out;
      }
    /* Next try to allocate a new block worth of objects of this size.  */
    {
        struct hblk *h = MANAGED_STACK_ADDRESS_BOEHM_GC_allochblk(lb, k, 0 /* flags */, 0 /* align_m1 */);

        if (h /* != NULL */) { /* CPPCHECK */
          if (IS_UNCOLLECTABLE(k)) MANAGED_STACK_ADDRESS_BOEHM_GC_set_hdr_marks(HDR(h));
          MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd += HBLKSIZE - HBLKSIZE % lb;
#         ifdef PARALLEL_MARK
            if (MANAGED_STACK_ADDRESS_BOEHM_GC_parallel) {
              MANAGED_STACK_ADDRESS_BOEHM_GC_acquire_mark_lock();
              ++ MANAGED_STACK_ADDRESS_BOEHM_GC_fl_builder_count;
              UNLOCK();
              MANAGED_STACK_ADDRESS_BOEHM_GC_release_mark_lock();

              op = MANAGED_STACK_ADDRESS_BOEHM_GC_build_fl(h, lw,
                        (ok -> ok_init || MANAGED_STACK_ADDRESS_BOEHM_GC_debugging_started), 0);

              *result = op;
              MANAGED_STACK_ADDRESS_BOEHM_GC_acquire_mark_lock();
              -- MANAGED_STACK_ADDRESS_BOEHM_GC_fl_builder_count;
              if (MANAGED_STACK_ADDRESS_BOEHM_GC_fl_builder_count == 0) MANAGED_STACK_ADDRESS_BOEHM_GC_notify_all_builder();
              MANAGED_STACK_ADDRESS_BOEHM_GC_release_mark_lock();
              (void) MANAGED_STACK_ADDRESS_BOEHM_GC_clear_stack(0);
              return;
            }
#         endif
          op = MANAGED_STACK_ADDRESS_BOEHM_GC_build_fl(h, lw, (ok -> ok_init || MANAGED_STACK_ADDRESS_BOEHM_GC_debugging_started), 0);
          goto out;
        }
    }

    /* As a last attempt, try allocating a single object.  Note that    */
    /* this may trigger a collection or expand the heap.                */
      op = MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_inner(lb, k, 0 /* flags */);
      if (op != NULL) obj_link(op) = NULL;

  out:
    *result = op;
    UNLOCK();
    (void) MANAGED_STACK_ADDRESS_BOEHM_GC_clear_stack(0);
}

/* Note that the "atomic" version of this would be unsafe, since the    */
/* links would not be seen by the collector.                            */
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_many(size_t lb)
{
    void *result;
    size_t lg = ALLOC_REQUEST_GRANS(lb);

    MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_many(GRANULES_TO_BYTES(lg), NORMAL, &result);
    return result;
}

/* TODO: The debugging version of MANAGED_STACK_ADDRESS_BOEHM_GC_memalign and friends is tricky     */
/* and currently missing.  There are 2 major difficulties:              */
/* - MANAGED_STACK_ADDRESS_BOEHM_GC_base() should always point to the beginning of the allocated    */
/* block (thus, for small objects allocation we should probably         */
/* iterate over the list of free objects to find the one with the       */
/* suitable alignment);                                                 */
/* - store_debug_info() should return the pointer of the object with    */
/* the requested alignment (unlike the object header).                  */

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_memalign(size_t align, size_t lb)
{
    size_t offset;
    ptr_t result;
    size_t align_m1 = align - 1;

    /* Check the alignment argument.    */
    if (EXPECT(0 == align || (align & align_m1) != 0, FALSE)) return NULL;
    if (align <= MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES) return MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(lb);

    if (align >= HBLKSIZE/2 || lb >= HBLKSIZE/2) {
      return MANAGED_STACK_ADDRESS_BOEHM_GC_clear_stack(MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_aligned(lb, NORMAL,
                                        0 /* flags */, align_m1));
    }

    /* We could also try to make sure that the real rounded-up object size */
    /* is a multiple of align.  That would be correct up to HBLKSIZE.      */
    /* TODO: Not space efficient for big align values. */
    result = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(SIZET_SAT_ADD(lb, align_m1));
            /* It is OK not to check result for NULL as in that case    */
            /* MANAGED_STACK_ADDRESS_BOEHM_GC_memalign returns NULL too since (0 + 0 % align) is 0. */
    offset = (size_t)(word)result & align_m1;
    if (offset != 0) {
        offset = align - offset;
        if (!MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_STATIC_ASSERT(VALID_OFFSET_SZ <= HBLKSIZE);
            MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(offset < VALID_OFFSET_SZ);
            MANAGED_STACK_ADDRESS_BOEHM_GC_register_displacement(offset);
        }
        result += offset;
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(((word)result & align_m1) == 0);
    return result;
}

/* This one exists largely to redirect posix_memalign for leaks finding. */
MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_posix_memalign(void **memptr, size_t align, size_t lb)
{
  size_t align_minus_one = align - 1; /* to workaround a cppcheck warning */

  /* Check alignment properly.  */
  if (EXPECT(align < sizeof(void *)
             || (align_minus_one & align) != 0, FALSE)) {
#   ifdef MSWINCE
      return ERROR_INVALID_PARAMETER;
#   else
      return EINVAL;
#   endif
  }

  if ((*memptr = MANAGED_STACK_ADDRESS_BOEHM_GC_memalign(align, lb)) == NULL) {
#   ifdef MSWINCE
      return ERROR_NOT_ENOUGH_MEMORY;
#   else
      return ENOMEM;
#   endif
  }
  return 0;
}

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_VALLOC
  MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_valloc(size_t lb)
  {
    if (!EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized, TRUE)) MANAGED_STACK_ADDRESS_BOEHM_GC_init();
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_real_page_size != 0);
    return MANAGED_STACK_ADDRESS_BOEHM_GC_memalign(MANAGED_STACK_ADDRESS_BOEHM_GC_real_page_size, lb);
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_pvalloc(size_t lb)
  {
    if (!EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized, TRUE)) MANAGED_STACK_ADDRESS_BOEHM_GC_init();
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_real_page_size != 0);
    lb = SIZET_SAT_ADD(lb, MANAGED_STACK_ADDRESS_BOEHM_GC_real_page_size - 1) & ~(MANAGED_STACK_ADDRESS_BOEHM_GC_real_page_size - 1);
    return MANAGED_STACK_ADDRESS_BOEHM_GC_memalign(MANAGED_STACK_ADDRESS_BOEHM_GC_real_page_size, lb);
  }
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_NO_VALLOC */

/* Provide a version of strdup() that uses the collector to allocate    */
/* the copy of the string.                                              */
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC char * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_strdup(const char *s)
{
  char *copy;
  size_t lb;
  if (s == NULL) return NULL;
  lb = strlen(s) + 1;
  copy = (char *)MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_atomic(lb);
  if (EXPECT(NULL == copy, FALSE)) {
#   ifndef MSWINCE
      errno = ENOMEM;
#   endif
    return NULL;
  }
  BCOPY(s, copy, lb);
  return copy;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC char * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_strndup(const char *str, size_t size)
{
  char *copy;
  size_t len = strlen(str); /* str is expected to be non-NULL  */
  if (EXPECT(len > size, FALSE))
    len = size;
  copy = (char *)MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_atomic(len + 1);
  if (EXPECT(NULL == copy, FALSE)) {
#   ifndef MSWINCE
      errno = ENOMEM;
#   endif
    return NULL;
  }
  if (EXPECT(len > 0, TRUE))
    BCOPY(str, copy, len);
  copy[len] = '\0';
  return copy;
}

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_REQUIRE_WCSDUP
# include <wchar.h> /* for wcslen() */

  MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC wchar_t * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_wcsdup(const wchar_t *str)
  {
    size_t lb = (wcslen(str) + 1) * sizeof(wchar_t);
    wchar_t *copy = (wchar_t *)MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_atomic(lb);

    if (EXPECT(NULL == copy, FALSE)) {
#     ifndef MSWINCE
        errno = ENOMEM;
#     endif
      return NULL;
    }
    BCOPY(str, copy, lb);
    return copy;
  }
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_REQUIRE_WCSDUP */

#ifndef CPPCHECK
  MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_stubborn(size_t lb)
  {
    return MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(lb);
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_change_stubborn(const void *p)
  {
    UNUSED_ARG(p);
  }
#endif /* !CPPCHECK */

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_end_stubborn_change(const void *p)
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(p); /* entire object */
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_ptr_store_and_dirty(void *p, const void *q)
{
  *(const void **)p = q;
  MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(p);
  REACHABLE_AFTER_DIRTY(q);
}
