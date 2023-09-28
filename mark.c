/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1995 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 2000 by Hewlett-Packard Company.  All rights reserved.
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
 *
 */

#include "private/gc_pmark.h"

/* Make arguments appear live to compiler.  Put here to minimize the    */
/* risk of inlining.  Used to minimize junk left in registers.          */
MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NOINLINE
void MANAGED_STACK_ADDRESS_BOEHM_GC_noop6(word arg1, word arg2, word arg3, word arg4, word arg5, word arg6)
{
  UNUSED_ARG(arg1);
  UNUSED_ARG(arg2);
  UNUSED_ARG(arg3);
  UNUSED_ARG(arg4);
  UNUSED_ARG(arg5);
  UNUSED_ARG(arg6);
  /* Avoid MANAGED_STACK_ADDRESS_BOEHM_GC_noop6 calls to be optimized away. */
# if defined(AO_HAVE_compiler_barrier) && !defined(BASE_ATOMIC_OPS_EMULATED)
    AO_compiler_barrier(); /* to serve as a special side-effect */
# else
    MANAGED_STACK_ADDRESS_BOEHM_GC_noop1(0);
# endif
}

#if defined(AO_HAVE_store) && defined(THREAD_SANITIZER)
  volatile AO_t MANAGED_STACK_ADDRESS_BOEHM_GC_noop_sink;
#else
  volatile word MANAGED_STACK_ADDRESS_BOEHM_GC_noop_sink;
#endif

/* Make the argument appear live to compiler.  This is similar  */
/* to MANAGED_STACK_ADDRESS_BOEHM_GC_noop6(), but with a single argument.  Robust against   */
/* whole program analysis.                                      */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_noop1(MANAGED_STACK_ADDRESS_BOEHM_GC_word x)
{
# if defined(AO_HAVE_store) && defined(THREAD_SANITIZER)
    AO_store(&MANAGED_STACK_ADDRESS_BOEHM_GC_noop_sink, (AO_t)x);
# else
    MANAGED_STACK_ADDRESS_BOEHM_GC_noop_sink = x;
# endif
}

/* Initialize MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds properly and standard free lists properly.   */
/* This must be done statically since they may be accessed before       */
/* MANAGED_STACK_ADDRESS_BOEHM_GC_init is called.                                                   */
/* It's done here, since we need to deal with mark descriptors.         */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER struct obj_kind MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[MAXOBJKINDS] = {
/* PTRFREE */ { &MANAGED_STACK_ADDRESS_BOEHM_GC_aobjfreelist[0], 0 /* filled in dynamically */,
                /* 0 | */ MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH, FALSE, FALSE
                /*, */ OK_DISCLAIM_INITZ },
/* NORMAL */  { &MANAGED_STACK_ADDRESS_BOEHM_GC_objfreelist[0], 0,
                /* 0 | */ MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH,
                                /* adjusted in MANAGED_STACK_ADDRESS_BOEHM_GC_init for EXTRA_BYTES  */
                TRUE /* add length to descr */, TRUE
                /*, */ OK_DISCLAIM_INITZ },
/* UNCOLLECTABLE */
              { &MANAGED_STACK_ADDRESS_BOEHM_GC_uobjfreelist[0], 0,
                /* 0 | */ MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH, TRUE /* add length to descr */, TRUE
                /*, */ OK_DISCLAIM_INITZ },
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ATOMIC_UNCOLLECTABLE
              { &MANAGED_STACK_ADDRESS_BOEHM_GC_auobjfreelist[0], 0,
                /* 0 | */ MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH, FALSE, FALSE
                /*, */ OK_DISCLAIM_INITZ },
# endif
};

#ifndef INITIAL_MARK_STACK_SIZE
#   define INITIAL_MARK_STACK_SIZE (1*HBLKSIZE)
                /* INITIAL_MARK_STACK_SIZE * sizeof(mse) should be a    */
                /* multiple of HBLKSIZE.                                */
                /* The incremental collector actually likes a larger    */
                /* size, since it wants to push all marked dirty        */
                /* objects before marking anything new.  Currently we   */
                /* let it grow dynamically.                             */
#endif /* !INITIAL_MARK_STACK_SIZE */

#if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL)
  STATIC word MANAGED_STACK_ADDRESS_BOEHM_GC_n_rescuing_pages = 0;
                                /* Number of dirty pages we marked from */
                                /* excludes ptrfree pages, etc.         */
                                /* Used for logging only.               */
#endif

#ifdef PARALLEL_MARK
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_parallel_mark_disabled = FALSE;
#endif

/* Is a collection in progress?  Note that this can return true in the  */
/* non-incremental case, if a collection has been abandoned and the     */
/* mark state is now MS_INVALID.                                        */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_collection_in_progress(void)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state != MS_NONE;
}

/* Clear all mark bits in the header.   */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_clear_hdr_marks(hdr *hhdr)
{
  size_t last_bit;

# ifdef AO_HAVE_load
    /* Atomic access is used to avoid racing with MANAGED_STACK_ADDRESS_BOEHM_GC_realloc.   */
    last_bit = FINAL_MARK_BIT((size_t)AO_load((volatile AO_t *)&hhdr->hb_sz));
# else
    /* No race as MANAGED_STACK_ADDRESS_BOEHM_GC_realloc holds the lock while updating hb_sz.   */
    last_bit = FINAL_MARK_BIT((size_t)hhdr->hb_sz);
# endif

    BZERO(hhdr -> hb_marks, sizeof(hhdr->hb_marks));
    set_mark_bit_from_hdr(hhdr, last_bit);
    hhdr -> hb_n_marks = 0;
}

/* Set all mark bits in the header.  Used for uncollectible blocks. */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_set_hdr_marks(hdr *hhdr)
{
    unsigned i;
    size_t sz = (size_t)hhdr->hb_sz;
    unsigned n_marks = (unsigned)FINAL_MARK_BIT(sz);

#   ifdef USE_MARK_BYTES
      for (i = 0; i <= n_marks; i += (unsigned)MARK_BIT_OFFSET(sz)) {
        hhdr -> hb_marks[i] = 1;
      }
#   else
      /* Note that all bits are set even in case of not MARK_BIT_PER_OBJ,   */
      /* instead of setting every n-th bit where n is MARK_BIT_OFFSET(sz).  */
      /* This is done for a performance reason.                             */
      for (i = 0; i < divWORDSZ(n_marks); ++i) {
        hhdr -> hb_marks[i] = MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX;
      }
      /* Set the remaining bits near the end (plus one bit past the end).   */
      hhdr -> hb_marks[i] = ((((word)1 << modWORDSZ(n_marks)) - 1) << 1) | 1;
#   endif
#   ifdef MARK_BIT_PER_OBJ
      hhdr -> hb_n_marks = n_marks;
#   else
      hhdr -> hb_n_marks = HBLK_OBJS(sz);
#   endif
}

/* Clear all mark bits associated with block h. */
static void MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK clear_marks_for_block(struct hblk *h, MANAGED_STACK_ADDRESS_BOEHM_GC_word dummy)
{
    hdr * hhdr = HDR(h);

    UNUSED_ARG(dummy);
    if (IS_UNCOLLECTABLE(hhdr -> hb_obj_kind)) return;
        /* Mark bit for these is cleared only once the object is        */
        /* explicitly deallocated.  This either frees the block, or     */
        /* the bit is cleared once the object is on the free list.      */
    MANAGED_STACK_ADDRESS_BOEHM_GC_clear_hdr_marks(hhdr);
}

/* Slow but general routines for setting/clearing/asking about mark bits. */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_mark_bit(const void *p)
{
    struct hblk *h = HBLKPTR(p);
    hdr * hhdr = HDR(h);
    word bit_no = MARK_BIT_NO((word)p - (word)h, hhdr -> hb_sz);

    if (!mark_bit_from_hdr(hhdr, bit_no)) {
      set_mark_bit_from_hdr(hhdr, bit_no);
      ++hhdr -> hb_n_marks;
    }
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_clear_mark_bit(const void *p)
{
    struct hblk *h = HBLKPTR(p);
    hdr * hhdr = HDR(h);
    word bit_no = MARK_BIT_NO((word)p - (word)h, hhdr -> hb_sz);

    if (mark_bit_from_hdr(hhdr, bit_no)) {
      size_t n_marks = hhdr -> hb_n_marks;

      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(n_marks != 0);
      clear_mark_bit_from_hdr(hhdr, bit_no);
      n_marks--;
#     ifdef PARALLEL_MARK
        if (n_marks != 0 || !MANAGED_STACK_ADDRESS_BOEHM_GC_parallel)
          hhdr -> hb_n_marks = n_marks;
        /* Don't decrement to zero.  The counts are approximate due to  */
        /* concurrency issues, but we need to ensure that a count of    */
        /* zero implies an empty block.                                 */
#     else
          hhdr -> hb_n_marks = n_marks;
#     endif
    }
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_is_marked(const void *p)
{
    struct hblk *h = HBLKPTR(p);
    hdr * hhdr = HDR(h);
    word bit_no = MARK_BIT_NO((word)p - (word)h, hhdr -> hb_sz);

    return (int)mark_bit_from_hdr(hhdr, bit_no); /* 0 or 1 */
}

/* Clear mark bits in all allocated heap blocks.  This invalidates the  */
/* marker invariant, and sets MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state to reflect this.  (This     */
/* implicitly starts marking to reestablish the invariant.)             */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_clear_marks(void)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized); /* needed for MANAGED_STACK_ADDRESS_BOEHM_GC_push_roots */
    MANAGED_STACK_ADDRESS_BOEHM_GC_apply_to_all_blocks(clear_marks_for_block, (word)0);
    MANAGED_STACK_ADDRESS_BOEHM_GC_objects_are_marked = FALSE;
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state = MS_INVALID;
    MANAGED_STACK_ADDRESS_BOEHM_GC_scan_ptr = NULL;
}

/* Initiate a garbage collection.  Initiates a full collection if the   */
/* mark state is invalid.                                               */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_initiate_gc(void)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized);
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_incremental) {
#         ifdef CHECKSUMS
            MANAGED_STACK_ADDRESS_BOEHM_GC_read_dirty(FALSE);
            MANAGED_STACK_ADDRESS_BOEHM_GC_check_dirty();
#         else
            MANAGED_STACK_ADDRESS_BOEHM_GC_read_dirty(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_INVALID);
#         endif
        }
        MANAGED_STACK_ADDRESS_BOEHM_GC_n_rescuing_pages = 0;
#   endif
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_NONE) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state = MS_PUSH_RESCUERS;
    } else {
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_INVALID);
        /* This is really a full collection, and mark bits are invalid. */
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_scan_ptr = NULL;
}

#ifdef PARALLEL_MARK
    STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_do_parallel_mark(void); /* Initiate parallel marking. */
#endif /* PARALLEL_MARK */

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL
# define MANAGED_STACK_ADDRESS_BOEHM_GC_push_next_marked_dirty(h) MANAGED_STACK_ADDRESS_BOEHM_GC_push_next_marked(h)
#else
  STATIC struct hblk * MANAGED_STACK_ADDRESS_BOEHM_GC_push_next_marked_dirty(struct hblk *h);
                /* Invoke MANAGED_STACK_ADDRESS_BOEHM_GC_push_marked on next dirty block above h.   */
                /* Return a pointer just past the end of this block.    */
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL */
STATIC struct hblk * MANAGED_STACK_ADDRESS_BOEHM_GC_push_next_marked(struct hblk *h);
                /* Ditto, but also mark from clean pages.       */
STATIC struct hblk * MANAGED_STACK_ADDRESS_BOEHM_GC_push_next_marked_uncollectable(struct hblk *h);
                /* Ditto, but mark only from uncollectible pages.       */

static void alloc_mark_stack(size_t);

static void push_roots_and_advance(MANAGED_STACK_ADDRESS_BOEHM_GC_bool push_all, ptr_t cold_gc_frame)
{
  if (MANAGED_STACK_ADDRESS_BOEHM_GC_scan_ptr != NULL) return; /* not ready to push */

  MANAGED_STACK_ADDRESS_BOEHM_GC_push_roots(push_all, cold_gc_frame);
  MANAGED_STACK_ADDRESS_BOEHM_GC_objects_are_marked = TRUE;
  if (MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state != MS_INVALID)
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state = MS_ROOTS_PUSHED;
}

STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_on_mark_stack_empty_proc MANAGED_STACK_ADDRESS_BOEHM_GC_on_mark_stack_empty;

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_on_mark_stack_empty(MANAGED_STACK_ADDRESS_BOEHM_GC_on_mark_stack_empty_proc fn)
{
  LOCK();
  MANAGED_STACK_ADDRESS_BOEHM_GC_on_mark_stack_empty = fn;
  UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_on_mark_stack_empty_proc MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_on_mark_stack_empty(void)
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_on_mark_stack_empty_proc fn;

  LOCK();
  fn = MANAGED_STACK_ADDRESS_BOEHM_GC_on_mark_stack_empty;
  UNLOCK();
  return fn;
}

/* Perform a small amount of marking.                   */
/* We try to touch roughly a page of memory.            */
/* Return TRUE if we just finished a mark phase.        */
/* Cold_gc_frame is an address inside a GC frame that   */
/* remains valid until all marking is complete.         */
/* A zero value indicates that it's OK to miss some     */
/* register values.  In the case of an incremental      */
/* collection, the world may be running.                */
#ifdef WRAP_MARK_SOME
  /* For Win32, this is called after we establish a structured  */
  /* exception (or signal) handler, in case Windows unmaps one  */
  /* of our root segments.  Note that this code should never    */
  /* generate an incremental GC write fault.                    */
  STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_mark_some_inner(ptr_t cold_gc_frame)
#else
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_mark_some(ptr_t cold_gc_frame)
#endif
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    switch (MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state) {
        case MS_NONE:
            return TRUE;

        case MS_PUSH_RESCUERS:
            if ((word)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top
                >= (word)(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit - INITIAL_MARK_STACK_SIZE/2)) {
                /* Go ahead and mark, even though that might cause us to */
                /* see more marked dirty objects later on.  Avoid this   */
                /* in the future.                                        */
                MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_too_small = TRUE;
                MARK_FROM_MARK_STACK();
            } else {
                MANAGED_STACK_ADDRESS_BOEHM_GC_scan_ptr = MANAGED_STACK_ADDRESS_BOEHM_GC_push_next_marked_dirty(MANAGED_STACK_ADDRESS_BOEHM_GC_scan_ptr);
#               ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL
                  if (NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_scan_ptr) {
                    MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF("Marked from %lu dirty pages\n",
                                       (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_n_rescuing_pages);
                  }
#               endif
                push_roots_and_advance(FALSE, cold_gc_frame);
            }
            MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_PUSH_RESCUERS
                      || MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_ROOTS_PUSHED
                      || MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_INVALID);
            break;

        case MS_PUSH_UNCOLLECTABLE:
            if ((word)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top
                >= (word)(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack + MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_size/4)) {
#               ifdef PARALLEL_MARK
                  /* Avoid this, since we don't parallelize the marker  */
                  /* here.                                              */
                  if (MANAGED_STACK_ADDRESS_BOEHM_GC_parallel) MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_too_small = TRUE;
#               endif
                MARK_FROM_MARK_STACK();
            } else {
                MANAGED_STACK_ADDRESS_BOEHM_GC_scan_ptr = MANAGED_STACK_ADDRESS_BOEHM_GC_push_next_marked_uncollectable(MANAGED_STACK_ADDRESS_BOEHM_GC_scan_ptr);
                push_roots_and_advance(TRUE, cold_gc_frame);
            }
            MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_PUSH_UNCOLLECTABLE
                      || MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_ROOTS_PUSHED
                      || MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_INVALID);
            break;

        case MS_ROOTS_PUSHED:
#           ifdef PARALLEL_MARK
              /* Eventually, incremental marking should run             */
              /* asynchronously in multiple threads, without grabbing   */
              /* the allocation lock.                                   */
              /* For now, parallel marker is disabled if there is       */
              /* a chance that marking could be interrupted by          */
              /* a client-supplied time limit or custom stop function.  */
                if (MANAGED_STACK_ADDRESS_BOEHM_GC_parallel && !MANAGED_STACK_ADDRESS_BOEHM_GC_parallel_mark_disabled) {
                  MANAGED_STACK_ADDRESS_BOEHM_GC_do_parallel_mark();
                  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((word)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top < (word)MANAGED_STACK_ADDRESS_BOEHM_GC_first_nonempty);
                  MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack - 1;
                  if (MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_too_small) {
                    alloc_mark_stack(2*MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_size);
                  }
                  if (MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_ROOTS_PUSHED) {
                    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state = MS_NONE;
                    return TRUE;
                  }
                  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_INVALID);
                  break;
                }
#           endif
            if ((word)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top >= (word)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack) {
                MARK_FROM_MARK_STACK();
            } else {
                MANAGED_STACK_ADDRESS_BOEHM_GC_on_mark_stack_empty_proc on_ms_empty;

                if (MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_too_small) {
                    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state = MS_NONE;
                    alloc_mark_stack(2*MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_size);
                    return TRUE;
                }
                on_ms_empty = MANAGED_STACK_ADDRESS_BOEHM_GC_on_mark_stack_empty;
                if (on_ms_empty != 0) {
                    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top = on_ms_empty(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top,
                                                    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit);
                    /* If we pushed new items or overflowed the stack,  */
                    /* we need to continue processing.                  */
                    if ((word)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top >= (word)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack
                            || MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_too_small)
                        break;
                }

                MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state = MS_NONE;
                return TRUE;
            }
            MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_ROOTS_PUSHED
                      || MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_INVALID);
            break;

        case MS_INVALID:
        case MS_PARTIALLY_INVALID:
            if (!MANAGED_STACK_ADDRESS_BOEHM_GC_objects_are_marked) {
                MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state = MS_PUSH_UNCOLLECTABLE;
                break;
            }
            if ((word)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top >= (word)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack) {
                MARK_FROM_MARK_STACK();
                MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_PARTIALLY_INVALID
                          || MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_INVALID);
                break;
            }
            if (NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_scan_ptr && MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_INVALID) {
                /* About to start a heap scan for marked objects. */
                /* Mark stack is empty.  OK to reallocate.        */
                if (MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_too_small) {
                    alloc_mark_stack(2*MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_size);
                }
                MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state = MS_PARTIALLY_INVALID;
            }
            MANAGED_STACK_ADDRESS_BOEHM_GC_scan_ptr = MANAGED_STACK_ADDRESS_BOEHM_GC_push_next_marked(MANAGED_STACK_ADDRESS_BOEHM_GC_scan_ptr);
            if (MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_PARTIALLY_INVALID)
                push_roots_and_advance(TRUE, cold_gc_frame);
            MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_ROOTS_PUSHED
                      || MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_PARTIALLY_INVALID
                      || MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state == MS_INVALID);
            break;

        default:
            ABORT("MANAGED_STACK_ADDRESS_BOEHM_GC_mark_some: bad state");
    }
    return FALSE;
}

#ifdef WRAP_MARK_SOME
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_mark_some(ptr_t cold_gc_frame)
  {
      MANAGED_STACK_ADDRESS_BOEHM_GC_bool ret_val;

      if (MANAGED_STACK_ADDRESS_BOEHM_GC_no_dls) {
        ret_val = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_some_inner(cold_gc_frame);
      } else {
        /* Windows appears to asynchronously create and remove      */
        /* writable memory mappings, for reasons we haven't yet     */
        /* understood.  Since we look for writable regions to       */
        /* determine the root set, we may try to mark from an       */
        /* address range that disappeared since we started the      */
        /* collection.  Thus we have to recover from faults here.   */
        /* This code seems to be necessary for WinCE (at least in   */
        /* the case we'd decide to add MEM_PRIVATE sections to      */
        /* data roots in MANAGED_STACK_ADDRESS_BOEHM_GC_register_dynamic_libraries()).          */
        /* It's conceivable that this is the same issue as with     */
        /* terminating threads that we see with Linux and           */
        /* USE_PROC_FOR_LIBRARIES.                                  */
#       ifndef NO_SEH_AVAILABLE
          __try {
            ret_val = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_some_inner(cold_gc_frame);
          } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
                  EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
            goto handle_ex;
          }
#       else
#         if defined(USE_PROC_FOR_LIBRARIES) && !defined(DEFAULT_VDB)
            if (MANAGED_STACK_ADDRESS_BOEHM_GC_auto_incremental) {
              static MANAGED_STACK_ADDRESS_BOEHM_GC_bool is_warned = FALSE;

              if (!is_warned) {
                is_warned = TRUE;
                WARN("Incremental GC incompatible with /proc roots\n", 0);
              }
              /* I'm not sure if this could still work ...  */
            }
#         endif
          /* If USE_PROC_FOR_LIBRARIES, we are handling the case in     */
          /* which /proc is used for root finding, and we have threads. */
          /* We may find a stack for a thread that is in the process of */
          /* exiting, and disappears while we are marking it.           */
          /* This seems extremely difficult to avoid otherwise.         */
          MANAGED_STACK_ADDRESS_BOEHM_GC_setup_temporary_fault_handler();
          if (SETJMP(MANAGED_STACK_ADDRESS_BOEHM_GC_jmp_buf) != 0) goto handle_ex;
          ret_val = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_some_inner(cold_gc_frame);
          MANAGED_STACK_ADDRESS_BOEHM_GC_reset_fault_handler();
#       endif
      }

#     if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS)
        /* With DllMain-based thread tracking, a thread may have        */
        /* started while we were marking.  This is logically equivalent */
        /* to the exception case; our results are invalid and we have   */
        /* to start over.  This cannot be prevented since we can't      */
        /* block in DllMain.                                            */
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_started_thread_while_stopped())
          goto handle_thr_start;
#     endif
      return ret_val;

    handle_ex:
    /* Exception handler starts here for all cases. */
#     if defined(NO_SEH_AVAILABLE)
        MANAGED_STACK_ADDRESS_BOEHM_GC_reset_fault_handler();
#     endif
      {
        static word warned_gc_no;

        /* Report caught ACCESS_VIOLATION, once per collection. */
        if (warned_gc_no != MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF("Memory mapping disappeared at collection #%lu\n",
                             (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no + 1);
          warned_gc_no = MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no;
        }
      }
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS)
      handle_thr_start:
#   endif
      /* We have bad roots on the mark stack - discard it.      */
      /* Rescan from marked objects.  Redetermine roots.        */
#     ifdef REGISTER_LIBRARIES_EARLY
        START_WORLD();
        MANAGED_STACK_ADDRESS_BOEHM_GC_cond_register_dynamic_libraries();
        STOP_WORLD();
#     endif
      MANAGED_STACK_ADDRESS_BOEHM_GC_invalidate_mark_state();
      MANAGED_STACK_ADDRESS_BOEHM_GC_scan_ptr = NULL;
      return FALSE;
  }
#endif /* WRAP_MARK_SOME */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_invalidate_mark_state(void)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state = MS_INVALID;
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack-1;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER mse * MANAGED_STACK_ADDRESS_BOEHM_GC_signal_mark_stack_overflow(mse *msp)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state = MS_INVALID;
#   ifdef PARALLEL_MARK
      /* We are using a local_mark_stack in parallel mode, so   */
      /* do not signal the global mark stack to be resized.     */
      /* That will be done if required in MANAGED_STACK_ADDRESS_BOEHM_GC_return_mark_stack. */
      if (!MANAGED_STACK_ADDRESS_BOEHM_GC_parallel)
        MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_too_small = TRUE;
#   else
      MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_too_small = TRUE;
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF("Mark stack overflow; current size: %lu entries\n",
                       (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_size);
    return msp - MANAGED_STACK_ADDRESS_BOEHM_GC_MARK_STACK_DISCARDS;
}

/*
 * Mark objects pointed to by the regions described by
 * mark stack entries between mark_stack and mark_stack_top,
 * inclusive.  Assumes the upper limit of a mark stack entry
 * is never 0.  A mark stack entry never has size 0.
 * We try to traverse on the order of a hblk of memory before we return.
 * Caller is responsible for calling this until the mark stack is empty.
 * Note that this is the most performance critical routine in the
 * collector.  Hence it contains all sorts of ugly hacks to speed
 * things up.  In particular, we avoid procedure calls on the common
 * path, we take advantage of peculiarities of the mark descriptor
 * encoding, we optionally maintain a cache for the block address to
 * header mapping, we prefetch when an object is "grayed", etc.
 */
MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_ADDR MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_MEMORY MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER mse * MANAGED_STACK_ADDRESS_BOEHM_GC_mark_from(mse *mark_stack_top, mse *mark_stack,
                            mse *mark_stack_limit)
{
  signed_word credit = HBLKSIZE;  /* Remaining credit for marking work. */
  ptr_t current_p;      /* Pointer to current candidate ptr.            */
  word current;         /* Candidate pointer.                           */
  ptr_t limit = 0;      /* (Incl) limit of current candidate range.     */
  word descr;
  ptr_t greatest_ha = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr;
  ptr_t least_ha = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr;
  DECLARE_HDR_CACHE;

# define SPLIT_RANGE_WORDS 128  /* Must be power of 2.          */

  MANAGED_STACK_ADDRESS_BOEHM_GC_objects_are_marked = TRUE;
  INIT_HDR_CACHE;
# ifdef OS2 /* Use untweaked version to circumvent compiler problem.    */
    while ((word)mark_stack_top >= (word)mark_stack && credit >= 0)
# else
    while (((((word)mark_stack_top - (word)mark_stack) | (word)credit)
            & SIGNB) == 0)
# endif
  {
    current_p = mark_stack_top -> mse_start;
    descr = mark_stack_top -> mse_descr.w;
  retry:
    /* current_p and descr describe the current object.                 */
    /* (*mark_stack_top) is vacant.                                     */
    /* The following is 0 only for small objects described by a simple  */
    /* length descriptor.  For many applications this is the common     */
    /* case, so we try to detect it quickly.                            */
    if (descr & (~(word)(WORDS_TO_BYTES(SPLIT_RANGE_WORDS)-1) | MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS)) {
      word tag = descr & MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS;

      MANAGED_STACK_ADDRESS_BOEHM_GC_STATIC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS == 0x3);
      switch (tag) {
        case MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH:
          /* Large length.  Process part of the range to avoid pushing  */
          /* too much on the stack.                                     */

          /* Either it is a heap object or a region outside the heap.   */
          MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(descr < MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_real_heap_addr-MANAGED_STACK_ADDRESS_BOEHM_GC_least_real_heap_addr
                || (word)current_p + descr
                        <= MANAGED_STACK_ADDRESS_BOEHM_GC_least_real_heap_addr + sizeof(word)
                || (word)current_p >= MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_real_heap_addr);
#         ifdef PARALLEL_MARK
#           define SHARE_BYTES 2048
            if (descr > SHARE_BYTES && MANAGED_STACK_ADDRESS_BOEHM_GC_parallel
                && (word)mark_stack_top < (word)(mark_stack_limit - 1)) {
              word new_size = (descr/2) & ~(word)(sizeof(word)-1);

              mark_stack_top -> mse_start = current_p;
              mark_stack_top -> mse_descr.w =
                                (new_size + sizeof(word)) | MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH;
                                        /* Makes sure we handle         */
                                        /* misaligned pointers.         */
              mark_stack_top++;
#             ifdef ENABLE_TRACE
                if ((word)MANAGED_STACK_ADDRESS_BOEHM_GC_trace_addr >= (word)current_p
                    && (word)MANAGED_STACK_ADDRESS_BOEHM_GC_trace_addr < (word)(current_p + descr)) {
                  MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("GC #%lu: large section; start %p, len %lu,"
                                " splitting (parallel) at %p\n",
                                (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no, (void *)current_p,
                                (unsigned long)descr,
                                (void *)(current_p + new_size));
                }
#             endif
              current_p += new_size;
              descr -= new_size;
              goto retry;
            }
#         endif /* PARALLEL_MARK */
          mark_stack_top -> mse_start =
                limit = current_p + WORDS_TO_BYTES(SPLIT_RANGE_WORDS-1);
          mark_stack_top -> mse_descr.w =
                                descr - WORDS_TO_BYTES(SPLIT_RANGE_WORDS-1);
#         ifdef ENABLE_TRACE
            if ((word)MANAGED_STACK_ADDRESS_BOEHM_GC_trace_addr >= (word)current_p
                && (word)MANAGED_STACK_ADDRESS_BOEHM_GC_trace_addr < (word)(current_p + descr)) {
              MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("GC #%lu: large section; start %p, len %lu,"
                            " splitting at %p\n",
                            (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no, (void *)current_p,
                            (unsigned long)descr, (void *)limit);
            }
#         endif
          /* Make sure that pointers overlapping the two ranges are     */
          /* considered.                                                */
          limit += sizeof(word) - ALIGNMENT;
          break;
        case MANAGED_STACK_ADDRESS_BOEHM_GC_DS_BITMAP:
          mark_stack_top--;
#         ifdef ENABLE_TRACE
            if ((word)MANAGED_STACK_ADDRESS_BOEHM_GC_trace_addr >= (word)current_p
                && (word)MANAGED_STACK_ADDRESS_BOEHM_GC_trace_addr
                    < (word)(current_p + WORDS_TO_BYTES(CPP_WORDSZ-2))) {
              MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("GC #%lu: tracing from %p bitmap descr %lu\n",
                            (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no, (void *)current_p,
                            (unsigned long)descr);
            }
#         endif /* ENABLE_TRACE */
          descr &= ~(word)MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS;
          credit -= (signed_word)WORDS_TO_BYTES(CPP_WORDSZ / 2); /* guess */
          for (; descr != 0; descr <<= 1, current_p += sizeof(word)) {
            if ((descr & SIGNB) == 0) continue;
            LOAD_WORD_OR_CONTINUE(current, current_p);
            FIXUP_POINTER(current);
            if (current > (word)least_ha && current < (word)greatest_ha) {
                PREFETCH((ptr_t)current);
#               ifdef ENABLE_TRACE
                  if (MANAGED_STACK_ADDRESS_BOEHM_GC_trace_addr == current_p) {
                    MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("GC #%lu: considering(3) %p -> %p\n",
                                  (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no, (void *)current_p,
                                  (void *)current);
                  }
#               endif /* ENABLE_TRACE */
                PUSH_CONTENTS((ptr_t)current, mark_stack_top,
                              mark_stack_limit, current_p);
            }
          }
          continue;
        case MANAGED_STACK_ADDRESS_BOEHM_GC_DS_PROC:
          mark_stack_top--;
#         ifdef ENABLE_TRACE
            if ((word)MANAGED_STACK_ADDRESS_BOEHM_GC_trace_addr >= (word)current_p
                && MANAGED_STACK_ADDRESS_BOEHM_GC_base(current_p) != 0
                && MANAGED_STACK_ADDRESS_BOEHM_GC_base(current_p) == MANAGED_STACK_ADDRESS_BOEHM_GC_base(MANAGED_STACK_ADDRESS_BOEHM_GC_trace_addr)) {
              MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("GC #%lu: tracing from %p, proc descr %lu\n",
                            (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no, (void *)current_p,
                            (unsigned long)descr);
            }
#         endif /* ENABLE_TRACE */
          credit -= MANAGED_STACK_ADDRESS_BOEHM_GC_PROC_BYTES;
          mark_stack_top = (*PROC(descr))((word *)current_p, mark_stack_top,
                                          mark_stack_limit, ENV(descr));
          continue;
        case MANAGED_STACK_ADDRESS_BOEHM_GC_DS_PER_OBJECT:
          if (!(descr & SIGNB)) {
            /* Descriptor is in the object.     */
            descr = *(word *)(current_p + descr - MANAGED_STACK_ADDRESS_BOEHM_GC_DS_PER_OBJECT);
          } else {
            /* Descriptor is in type descriptor pointed to by first     */
            /* word in object.                                          */
            ptr_t type_descr = *(ptr_t *)current_p;
            /* type_descr is either a valid pointer to the descriptor   */
            /* structure, or this object was on a free list.            */
            /* If it was anything but the last object on the free list, */
            /* we will misinterpret the next object on the free list as */
            /* the type descriptor, and get a 0 GC descriptor, which    */
            /* is ideal.  Unfortunately, we need to check for the last  */
            /* object case explicitly.                                  */
            if (EXPECT(0 == type_descr, FALSE)) {
                mark_stack_top--;
                continue;
            }
            descr = *(word *)(type_descr
                              - ((signed_word)descr + (MANAGED_STACK_ADDRESS_BOEHM_GC_INDIR_PER_OBJ_BIAS
                                                       - MANAGED_STACK_ADDRESS_BOEHM_GC_DS_PER_OBJECT)));
          }
          if (0 == descr) {
              /* Can happen either because we generated a 0 descriptor  */
              /* or we saw a pointer to a free object.                  */
              mark_stack_top--;
              continue;
          }
          goto retry;
      }
    } else {
      /* Small object with length descriptor.   */
      mark_stack_top--;
#     ifndef SMALL_CONFIG
        if (descr < sizeof(word))
          continue;
#     endif
#     ifdef ENABLE_TRACE
        if ((word)MANAGED_STACK_ADDRESS_BOEHM_GC_trace_addr >= (word)current_p
            && (word)MANAGED_STACK_ADDRESS_BOEHM_GC_trace_addr < (word)(current_p + descr)) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("GC #%lu: small object; start %p, len %lu\n",
                        (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no, (void *)current_p,
                        (unsigned long)descr);
        }
#     endif
      limit = current_p + (word)descr;
    }
    /* The simple case in which we're scanning a range. */
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(!((word)current_p & (ALIGNMENT-1)));
    credit -= limit - current_p;
    limit -= sizeof(word);
    {
#     define PREF_DIST 4

#     if !defined(SMALL_CONFIG) && !defined(USE_PTR_HWTAG)
        word deferred;

        /* Try to prefetch the next pointer to be examined ASAP.        */
        /* Empirically, this also seems to help slightly without        */
        /* prefetches, at least on linux/x86.  Presumably this loop     */
        /* ends up with less register pressure, and gcc thus ends up    */
        /* generating slightly better code.  Overall gcc code quality   */
        /* for this loop is still not great.                            */
        for(;;) {
          PREFETCH(limit - PREF_DIST*CACHE_LINE_SIZE);
          MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((word)limit >= (word)current_p);
          deferred = *(word *)limit;
          FIXUP_POINTER(deferred);
          limit -= ALIGNMENT;
          if (deferred > (word)least_ha && deferred < (word)greatest_ha) {
            PREFETCH((ptr_t)deferred);
            break;
          }
          if ((word)current_p > (word)limit) goto next_object;
          /* Unroll once, so we don't do too many of the prefetches     */
          /* based on limit.                                            */
          deferred = *(word *)limit;
          FIXUP_POINTER(deferred);
          limit -= ALIGNMENT;
          if (deferred > (word)least_ha && deferred < (word)greatest_ha) {
            PREFETCH((ptr_t)deferred);
            break;
          }
          if ((word)current_p > (word)limit) goto next_object;
        }
#     endif

      for (; (word)current_p <= (word)limit; current_p += ALIGNMENT) {
        /* Empirically, unrolling this loop doesn't help a lot. */
        /* Since PUSH_CONTENTS expands to a lot of code,        */
        /* we don't.                                            */
        LOAD_WORD_OR_CONTINUE(current, current_p);
        FIXUP_POINTER(current);
        PREFETCH(current_p + PREF_DIST*CACHE_LINE_SIZE);
        if (current > (word)least_ha && current < (word)greatest_ha) {
          /* Prefetch the contents of the object we just pushed.  It's  */
          /* likely we will need them soon.                             */
          PREFETCH((ptr_t)current);
#         ifdef ENABLE_TRACE
            if (MANAGED_STACK_ADDRESS_BOEHM_GC_trace_addr == current_p) {
              MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("GC #%lu: considering(1) %p -> %p\n",
                            (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no, (void *)current_p,
                            (void *)current);
            }
#         endif /* ENABLE_TRACE */
          PUSH_CONTENTS((ptr_t)current, mark_stack_top,
                        mark_stack_limit, current_p);
        }
      }

#     if !defined(SMALL_CONFIG) && !defined(USE_PTR_HWTAG)
        /* We still need to mark the entry we previously prefetched.    */
        /* We already know that it passes the preliminary pointer       */
        /* validity test.                                               */
#       ifdef ENABLE_TRACE
            if (MANAGED_STACK_ADDRESS_BOEHM_GC_trace_addr == current_p) {
              MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("GC #%lu: considering(2) %p -> %p\n",
                            (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no, (void *)current_p,
                            (void *)deferred);
            }
#       endif /* ENABLE_TRACE */
        PUSH_CONTENTS((ptr_t)deferred, mark_stack_top,
                      mark_stack_limit, current_p);
        next_object:;
#     endif
    }
  }
  return mark_stack_top;
}

#ifdef PARALLEL_MARK

STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_help_wanted = FALSE;  /* Protected by mark lock.      */
STATIC unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_helper_count = 0;    /* Number of running helpers.   */
                                        /* Protected by mark lock.      */
STATIC unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_active_count = 0;    /* Number of active helpers.    */
                                        /* Protected by mark lock.      */
                                        /* May increase and decrease    */
                                        /* within each mark cycle.  But */
                                        /* once it returns to 0, it     */
                                        /* stays zero for the cycle.    */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER word MANAGED_STACK_ADDRESS_BOEHM_GC_mark_no = 0;

#ifdef LINT2
# define LOCAL_MARK_STACK_SIZE (HBLKSIZE / 8)
#else
# define LOCAL_MARK_STACK_SIZE HBLKSIZE
        /* Under normal circumstances, this is big enough to guarantee  */
        /* we don't overflow half of it in a single call to             */
        /* MANAGED_STACK_ADDRESS_BOEHM_GC_mark_from.                                                */
#endif

/* Wait all markers to finish initialization (i.e. store        */
/* marker_[b]sp, marker_mach_threads, MANAGED_STACK_ADDRESS_BOEHM_GC_marker_Id).            */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_wait_for_markers_init(void)
{
  signed_word count;

  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
  if (MANAGED_STACK_ADDRESS_BOEHM_GC_markers_m1 == 0)
    return;

  /* Allocate the local mark stack for the thread that holds GC lock.   */
# ifndef CAN_HANDLE_FORK
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_main_local_mark_stack);
# else
    if (NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_main_local_mark_stack)
# endif
  {
    size_t bytes_to_get =
                ROUNDUP_PAGESIZE_IF_MMAP(LOCAL_MARK_STACK_SIZE * sizeof(mse));

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_page_size != 0);
    MANAGED_STACK_ADDRESS_BOEHM_GC_main_local_mark_stack = (mse *)MANAGED_STACK_ADDRESS_BOEHM_GC_os_get_mem(bytes_to_get);
    if (NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_main_local_mark_stack)
      ABORT("Insufficient memory for main local_mark_stack");
  }

  /* Reuse marker lock and builders count to synchronize        */
  /* marker threads startup.                                    */
  MANAGED_STACK_ADDRESS_BOEHM_GC_acquire_mark_lock();
  MANAGED_STACK_ADDRESS_BOEHM_GC_fl_builder_count += MANAGED_STACK_ADDRESS_BOEHM_GC_markers_m1;
  count = MANAGED_STACK_ADDRESS_BOEHM_GC_fl_builder_count;
  MANAGED_STACK_ADDRESS_BOEHM_GC_release_mark_lock();
  if (count != 0) {
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(count > 0);
    MANAGED_STACK_ADDRESS_BOEHM_GC_wait_for_reclaim();
  }
}

/* Steal mark stack entries starting at mse low into mark stack local   */
/* until we either steal mse high, or we have max entries.              */
/* Return a pointer to the top of the local mark stack.                 */
/* (*next) is replaced by a pointer to the next unscanned mark stack    */
/* entry.                                                               */
STATIC mse * MANAGED_STACK_ADDRESS_BOEHM_GC_steal_mark_stack(mse * low, mse * high, mse * local,
                                 unsigned max, mse **next)
{
    mse *p;
    mse *top = local - 1;
    unsigned i = 0;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((word)high >= (word)(low - 1)
              && (word)(high - low + 1) <= MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_size);
    for (p = low; (word)p <= (word)high && i <= max; ++p) {
        word descr = (word)AO_load(&p->mse_descr.ao);
        if (descr != 0) {
            /* Must be ordered after read of descr: */
            AO_store_release_write(&p->mse_descr.ao, 0);
            /* More than one thread may get this entry, but that's only */
            /* a minor performance problem.                             */
            ++top;
            top -> mse_descr.w = descr;
            top -> mse_start = p -> mse_start;
            MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((descr & MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS) != MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH /* 0 */
                || descr < MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_real_heap_addr-MANAGED_STACK_ADDRESS_BOEHM_GC_least_real_heap_addr
                || (word)(p -> mse_start + descr)
                        <= MANAGED_STACK_ADDRESS_BOEHM_GC_least_real_heap_addr + sizeof(word)
                || (word)(p -> mse_start) >= MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_real_heap_addr);
            /* If this is a big object, count it as size/256 + 1 objects. */
            ++i;
            if ((descr & MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS) == MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH) i += (int)(descr >> 8);
        }
    }
    *next = p;
    return top;
}

/* Copy back a local mark stack.        */
/* low and high are inclusive bounds.   */
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_return_mark_stack(mse * low, mse * high)
{
    mse * my_top;
    mse * my_start;
    size_t stack_size;

    if ((word)high < (word)low) return;
    stack_size = high - low + 1;
    MANAGED_STACK_ADDRESS_BOEHM_GC_acquire_mark_lock();
    my_top = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top; /* Concurrent modification impossible. */
    my_start = my_top + 1;
    if ((word)(my_start - MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack + stack_size)
                > (word)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_size) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF("No room to copy back mark stack\n");
      MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state = MS_INVALID;
      MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_too_small = TRUE;
      /* We drop the local mark stack.  We'll fix things later. */
    } else {
      BCOPY(low, my_start, stack_size * sizeof(mse));
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((mse *)AO_load((volatile AO_t *)(&MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top))
                == my_top);
      AO_store_release_write((volatile AO_t *)(&MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top),
                             (AO_t)(my_top + stack_size));
                /* Ensures visibility of previously written stack contents. */
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_release_mark_lock();
    MANAGED_STACK_ADDRESS_BOEHM_GC_notify_all_marker();
}

#ifndef N_LOCAL_ITERS
# define N_LOCAL_ITERS 1
#endif

/* This function is only called when the local  */
/* and the main mark stacks are both empty.     */
static MANAGED_STACK_ADDRESS_BOEHM_GC_bool has_inactive_helpers(void)
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_bool res;

  MANAGED_STACK_ADDRESS_BOEHM_GC_acquire_mark_lock();
  res = MANAGED_STACK_ADDRESS_BOEHM_GC_active_count < MANAGED_STACK_ADDRESS_BOEHM_GC_helper_count;
  MANAGED_STACK_ADDRESS_BOEHM_GC_release_mark_lock();
  return res;
}

/* Mark from the local mark stack.              */
/* On return, the local mark stack is empty.    */
/* But this may be achieved by copying the      */
/* local mark stack back into the global one.   */
/* We do not hold the mark lock.                */
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_do_local_mark(mse *local_mark_stack, mse *local_top)
{
    unsigned n;

    for (;;) {
        for (n = 0; n < N_LOCAL_ITERS; ++n) {
            local_top = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_from(local_top, local_mark_stack,
                                     local_mark_stack + LOCAL_MARK_STACK_SIZE);
            if ((word)local_top < (word)local_mark_stack) return;
            if ((word)(local_top - local_mark_stack)
                        >= LOCAL_MARK_STACK_SIZE / 2) {
                MANAGED_STACK_ADDRESS_BOEHM_GC_return_mark_stack(local_mark_stack, local_top);
                return;
            }
        }
        if ((word)AO_load((volatile AO_t *)&MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top)
            < (word)AO_load(&MANAGED_STACK_ADDRESS_BOEHM_GC_first_nonempty)
            && (word)local_top > (word)(local_mark_stack + 1)
            && has_inactive_helpers()) {
            /* Try to share the load, since the main stack is empty,    */
            /* and helper threads are waiting for a refill.             */
            /* The entries near the bottom of the stack are likely      */
            /* to require more work.  Thus we return those, even though */
            /* it's harder.                                             */
            mse * new_bottom = local_mark_stack
                                + (local_top - local_mark_stack)/2;
            MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((word)new_bottom > (word)local_mark_stack
                      && (word)new_bottom < (word)local_top);
            MANAGED_STACK_ADDRESS_BOEHM_GC_return_mark_stack(local_mark_stack, new_bottom - 1);
            memmove(local_mark_stack, new_bottom,
                    (local_top - new_bottom + 1) * sizeof(mse));
            local_top -= (new_bottom - local_mark_stack);
        }
    }
}

#ifndef ENTRIES_TO_GET
# define ENTRIES_TO_GET 5
#endif

/* Mark using the local mark stack until the global mark stack is empty */
/* and there are no active workers. Update MANAGED_STACK_ADDRESS_BOEHM_GC_first_nonempty to reflect */
/* progress.  Caller holds the mark lock.                               */
/* Caller has already incremented MANAGED_STACK_ADDRESS_BOEHM_GC_helper_count.  We decrement it,    */
/* and maintain MANAGED_STACK_ADDRESS_BOEHM_GC_active_count.                                        */
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_mark_local(mse *local_mark_stack, int id)
{
    mse * my_first_nonempty;

    MANAGED_STACK_ADDRESS_BOEHM_GC_active_count++;
    my_first_nonempty = (mse *)AO_load(&MANAGED_STACK_ADDRESS_BOEHM_GC_first_nonempty);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((word)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack <= (word)my_first_nonempty);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((word)my_first_nonempty
        <= (word)AO_load((volatile AO_t *)&MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top) + sizeof(mse));
    MANAGED_STACK_ADDRESS_BOEHM_GC_VERBOSE_LOG_PRINTF("Starting mark helper %d\n", id);
    MANAGED_STACK_ADDRESS_BOEHM_GC_release_mark_lock();
    for (;;) {
        size_t n_on_stack;
        unsigned n_to_get;
        mse * my_top;
        mse * local_top;
        mse * global_first_nonempty = (mse *)AO_load(&MANAGED_STACK_ADDRESS_BOEHM_GC_first_nonempty);

        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((word)my_first_nonempty >= (word)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack &&
                  (word)my_first_nonempty <=
                        (word)AO_load((volatile AO_t *)&MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top)
                        + sizeof(mse));
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((word)global_first_nonempty >= (word)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack);
        if ((word)my_first_nonempty < (word)global_first_nonempty) {
            my_first_nonempty = global_first_nonempty;
        } else if ((word)global_first_nonempty < (word)my_first_nonempty) {
            (void)AO_compare_and_swap(&MANAGED_STACK_ADDRESS_BOEHM_GC_first_nonempty,
                                      (AO_t)global_first_nonempty,
                                      (AO_t)my_first_nonempty);
            /* If this fails, we just go ahead, without updating        */
            /* MANAGED_STACK_ADDRESS_BOEHM_GC_first_nonempty.                                       */
        }
        /* Perhaps we should also update MANAGED_STACK_ADDRESS_BOEHM_GC_first_nonempty, if it */
        /* is less.  But that would require using atomic updates. */
        my_top = (mse *)AO_load_acquire((volatile AO_t *)(&MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top));
        if ((word)my_top < (word)my_first_nonempty) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_acquire_mark_lock();
            my_top = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top;
                /* Asynchronous modification impossible here,   */
                /* since we hold mark lock.                     */
            n_on_stack = my_top - my_first_nonempty + 1;
            if (0 == n_on_stack) {
                MANAGED_STACK_ADDRESS_BOEHM_GC_active_count--;
                MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_active_count <= MANAGED_STACK_ADDRESS_BOEHM_GC_helper_count);
                /* Other markers may redeposit objects          */
                /* on the stack.                                */
                if (0 == MANAGED_STACK_ADDRESS_BOEHM_GC_active_count) MANAGED_STACK_ADDRESS_BOEHM_GC_notify_all_marker();
                while (MANAGED_STACK_ADDRESS_BOEHM_GC_active_count > 0
                       && (word)AO_load(&MANAGED_STACK_ADDRESS_BOEHM_GC_first_nonempty)
                                > (word)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top) {
                    /* We will be notified if either MANAGED_STACK_ADDRESS_BOEHM_GC_active_count    */
                    /* reaches zero, or if more objects are pushed on   */
                    /* the global mark stack.                           */
                    MANAGED_STACK_ADDRESS_BOEHM_GC_wait_marker();
                }
                if (MANAGED_STACK_ADDRESS_BOEHM_GC_active_count == 0
                    && (word)AO_load(&MANAGED_STACK_ADDRESS_BOEHM_GC_first_nonempty)
                        > (word)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top) {
                    MANAGED_STACK_ADDRESS_BOEHM_GC_bool need_to_notify = FALSE;
                    /* The above conditions can't be falsified while we */
                    /* hold the mark lock, since neither                */
                    /* MANAGED_STACK_ADDRESS_BOEHM_GC_active_count nor MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top can        */
                    /* change.  MANAGED_STACK_ADDRESS_BOEHM_GC_first_nonempty can only be           */
                    /* incremented asynchronously.  Thus we know that   */
                    /* both conditions actually held simultaneously.    */
                    MANAGED_STACK_ADDRESS_BOEHM_GC_helper_count--;
                    if (0 == MANAGED_STACK_ADDRESS_BOEHM_GC_helper_count) need_to_notify = TRUE;
                    MANAGED_STACK_ADDRESS_BOEHM_GC_VERBOSE_LOG_PRINTF("Finished mark helper %d\n", id);
                    if (need_to_notify) MANAGED_STACK_ADDRESS_BOEHM_GC_notify_all_marker();
                    return;
                }
                /* Else there's something on the stack again, or        */
                /* another helper may push something.                   */
                MANAGED_STACK_ADDRESS_BOEHM_GC_active_count++;
                MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_active_count > 0);
                MANAGED_STACK_ADDRESS_BOEHM_GC_release_mark_lock();
                continue;
            } else {
                MANAGED_STACK_ADDRESS_BOEHM_GC_release_mark_lock();
            }
        } else {
            n_on_stack = my_top - my_first_nonempty + 1;
        }
        n_to_get = ENTRIES_TO_GET;
        if (n_on_stack < 2 * ENTRIES_TO_GET) n_to_get = 1;
        local_top = MANAGED_STACK_ADDRESS_BOEHM_GC_steal_mark_stack(my_first_nonempty, my_top,
                                        local_mark_stack, n_to_get,
                                        &my_first_nonempty);
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((word)my_first_nonempty >= (word)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack &&
                  (word)my_first_nonempty <=
                        (word)AO_load((volatile AO_t *)&MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top)
                        + sizeof(mse));
        MANAGED_STACK_ADDRESS_BOEHM_GC_do_local_mark(local_mark_stack, local_top);
    }
}

/* Perform parallel mark.  We hold the GC lock, not the mark lock.      */
/* Currently runs until the mark stack is empty.                        */
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_do_parallel_mark(void)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    MANAGED_STACK_ADDRESS_BOEHM_GC_acquire_mark_lock();

    /* This could be a MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT, but it seems safer to keep it on      */
    /* all the time, especially since it's cheap.                       */
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_help_wanted || MANAGED_STACK_ADDRESS_BOEHM_GC_active_count != 0 || MANAGED_STACK_ADDRESS_BOEHM_GC_helper_count != 0)
        ABORT("Tried to start parallel mark in bad state");
    MANAGED_STACK_ADDRESS_BOEHM_GC_VERBOSE_LOG_PRINTF("Starting marking for mark phase number %lu\n",
                          (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_no);
    MANAGED_STACK_ADDRESS_BOEHM_GC_first_nonempty = (AO_t)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack;
    MANAGED_STACK_ADDRESS_BOEHM_GC_active_count = 0;
    MANAGED_STACK_ADDRESS_BOEHM_GC_helper_count = 1;
    MANAGED_STACK_ADDRESS_BOEHM_GC_help_wanted = TRUE;
    MANAGED_STACK_ADDRESS_BOEHM_GC_notify_all_marker();
        /* Wake up potential helpers.   */
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_local(MANAGED_STACK_ADDRESS_BOEHM_GC_main_local_mark_stack, 0);
    MANAGED_STACK_ADDRESS_BOEHM_GC_help_wanted = FALSE;
    /* Done; clean up.  */
    while (MANAGED_STACK_ADDRESS_BOEHM_GC_helper_count > 0) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_wait_marker();
    }
    /* MANAGED_STACK_ADDRESS_BOEHM_GC_helper_count cannot be incremented while not MANAGED_STACK_ADDRESS_BOEHM_GC_help_wanted.  */
    MANAGED_STACK_ADDRESS_BOEHM_GC_VERBOSE_LOG_PRINTF("Finished marking for mark phase number %lu\n",
                          (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_no);
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_no++;
    MANAGED_STACK_ADDRESS_BOEHM_GC_release_mark_lock();
    MANAGED_STACK_ADDRESS_BOEHM_GC_notify_all_marker();
}

/* Try to help out the marker, if it's running.  We hold the mark lock  */
/* only, the initiating thread holds the allocation lock.               */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_help_marker(word my_mark_no)
{
#   define my_id my_id_mse.mse_descr.w
    mse my_id_mse;  /* align local_mark_stack explicitly */
    mse local_mark_stack[LOCAL_MARK_STACK_SIZE];
                /* Note: local_mark_stack is quite big (up to 128 KiB). */

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_DONT_HOLD_LOCK());
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_parallel);
    while (MANAGED_STACK_ADDRESS_BOEHM_GC_mark_no < my_mark_no
           || (!MANAGED_STACK_ADDRESS_BOEHM_GC_help_wanted && MANAGED_STACK_ADDRESS_BOEHM_GC_mark_no == my_mark_no)) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_wait_marker();
    }
    my_id = MANAGED_STACK_ADDRESS_BOEHM_GC_helper_count;
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_mark_no != my_mark_no || my_id > (unsigned)MANAGED_STACK_ADDRESS_BOEHM_GC_markers_m1) {
      /* Second test is useful only if original threads can also        */
      /* act as helpers.  Under Linux they can't.                       */
      return;
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_helper_count = (unsigned)my_id + 1;
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_local(local_mark_stack, (int)my_id);
    /* MANAGED_STACK_ADDRESS_BOEHM_GC_mark_local decrements MANAGED_STACK_ADDRESS_BOEHM_GC_helper_count. */
#   undef my_id
}

#endif /* PARALLEL_MARK */

/* Allocate or reallocate space for mark stack of size n entries.  */
/* May silently fail.                                              */
static void alloc_mark_stack(size_t n)
{
#   ifdef GWW_VDB
      static MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_incremental_at_stack_alloc = FALSE;

      MANAGED_STACK_ADDRESS_BOEHM_GC_bool recycle_old;
#   endif
    mse * new_stack;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    new_stack = (mse *)MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_alloc(n * sizeof(struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry));
#   ifdef GWW_VDB
      /* Don't recycle a stack segment obtained with the wrong flags.   */
      /* Win32 GetWriteWatch requires the right kind of memory.         */
      recycle_old = !MANAGED_STACK_ADDRESS_BOEHM_GC_auto_incremental || MANAGED_STACK_ADDRESS_BOEHM_GC_incremental_at_stack_alloc;
      MANAGED_STACK_ADDRESS_BOEHM_GC_incremental_at_stack_alloc = MANAGED_STACK_ADDRESS_BOEHM_GC_auto_incremental;
#   endif

    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_too_small = FALSE;
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack != NULL) {
        if (new_stack != 0) {
#         ifdef GWW_VDB
            if (recycle_old)
#         endif
          {
            /* Recycle old space.       */
            MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_recycle_inner(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack,
                        MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_size * sizeof(struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry));
          }
          MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack = new_stack;
          MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_size = n;
          /* FIXME: Do we need some way to reset MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_size?    */
          MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit = new_stack + n;
          MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF("Grew mark stack to %lu frames\n",
                             (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_size);
        } else {
          WARN("Failed to grow mark stack to %" WARN_PRIuPTR " frames\n", n);
        }
    } else if (NULL == new_stack) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf("No space for mark stack\n");
        EXIT();
    } else {
        MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack = new_stack;
        MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_size = n;
        MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit = new_stack + n;
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack-1;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_mark_init(void)
{
    alloc_mark_stack(INITIAL_MARK_STACK_SIZE);
}

/*
 * Push all locations between b and t onto the mark stack.
 * b is the first location to be checked. t is one past the last
 * location to be checked.
 * Should only be used if there is no possibility of mark stack
 * overflow.
 */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_push_all(void *bottom, void *top)
{
    word length;

    bottom = PTRT_ROUNDUP_BY_MASK(bottom, ALIGNMENT-1);
    top = (void *)((word)top & ~(word)(ALIGNMENT-1));
    if ((word)bottom >= (word)top) return;

    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top++;
    if ((word)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top >= (word)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit) {
        ABORT("Unexpected mark stack overflow");
    }
    length = (word)top - (word)bottom;
#   if MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS > ALIGNMENT - 1
        length = (length + MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS) & ~(word)MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS; /* round up */
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top -> mse_start = (ptr_t)bottom;
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top -> mse_descr.w = length | MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH;
}

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL

  /* Analogous to the above, but push only those pages h with           */
  /* dirty_fn(h) != 0.  We use MANAGED_STACK_ADDRESS_BOEHM_GC_push_all to actually push the block.  */
  /* Used both to selectively push dirty pages, or to push a block in   */
  /* piecemeal fashion, to allow for more marking concurrency.          */
  /* Will not overflow mark stack if MANAGED_STACK_ADDRESS_BOEHM_GC_push_all pushes a small fixed   */
  /* number of entries.  (This is invoked only if MANAGED_STACK_ADDRESS_BOEHM_GC_push_all pushes    */
  /* a single entry, or if it marks each object before pushing it, thus */
  /* ensuring progress in the event of a stack overflow.)               */
  STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_push_selected(ptr_t bottom, ptr_t top,
                               MANAGED_STACK_ADDRESS_BOEHM_GC_bool (*dirty_fn)(struct hblk *))
  {
    struct hblk * h;

    bottom = PTRT_ROUNDUP_BY_MASK(bottom, ALIGNMENT-1);
    top = (ptr_t)((word)top & ~(word)(ALIGNMENT-1));
    if ((word)bottom >= (word)top) return;

    h = HBLKPTR(bottom + HBLKSIZE);
    if ((word)top <= (word)h) {
        if ((*dirty_fn)(h-1)) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_push_all(bottom, top);
        }
        return;
    }
    if ((*dirty_fn)(h-1)) {
        if ((word)(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top - MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack)
            > 3 * MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_size / 4) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_push_all(bottom, top);
            return;
        }
        MANAGED_STACK_ADDRESS_BOEHM_GC_push_all(bottom, h);
    }

    while ((word)(h+1) <= (word)top) {
        if ((*dirty_fn)(h)) {
            if ((word)(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top - MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack)
                > 3 * MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_size / 4) {
                /* Danger of mark stack overflow.       */
                MANAGED_STACK_ADDRESS_BOEHM_GC_push_all(h, top);
                return;
            } else {
                MANAGED_STACK_ADDRESS_BOEHM_GC_push_all(h, h + 1);
            }
        }
        h++;
    }

    if ((ptr_t)h != top && (*dirty_fn)(h)) {
       MANAGED_STACK_ADDRESS_BOEHM_GC_push_all(h, top);
    }
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_push_conditional(void *bottom, void *top, int all)
  {
    if (!all) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_push_selected((ptr_t)bottom, (ptr_t)top, MANAGED_STACK_ADDRESS_BOEHM_GC_page_was_dirty);
    } else {
#     ifdef PROC_VDB
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_auto_incremental) {
          /* Pages that were never dirtied cannot contain pointers.     */
          MANAGED_STACK_ADDRESS_BOEHM_GC_push_selected((ptr_t)bottom, (ptr_t)top, MANAGED_STACK_ADDRESS_BOEHM_GC_page_was_ever_dirty);
        } else
#     endif
      /* else */ {
        MANAGED_STACK_ADDRESS_BOEHM_GC_push_all(bottom, top);
      }
    }
  }

# ifndef NO_VDB_FOR_STATIC_ROOTS
#   ifndef PROC_VDB
      /* Same as MANAGED_STACK_ADDRESS_BOEHM_GC_page_was_dirty but h is allowed to point to some    */
      /* page in the registered static roots only.  Not used if         */
      /* manual VDB is on.                                              */
      STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_static_page_was_dirty(struct hblk *h)
      {
        return get_pht_entry_from_index(MANAGED_STACK_ADDRESS_BOEHM_GC_grungy_pages, PHT_HASH(h));
      }
#   endif

    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_push_conditional_static(void *bottom, void *top,
                                             MANAGED_STACK_ADDRESS_BOEHM_GC_bool all)
    {
#     ifdef PROC_VDB
        /* Just redirect to the generic routine because PROC_VDB        */
        /* implementation gets the dirty bits map for the whole         */
        /* process memory.                                              */
        MANAGED_STACK_ADDRESS_BOEHM_GC_push_conditional(bottom, top, all);
#     else
        if (all || !MANAGED_STACK_ADDRESS_BOEHM_GC_is_vdb_for_static_roots()) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_push_all(bottom, top);
        } else {
          MANAGED_STACK_ADDRESS_BOEHM_GC_push_selected((ptr_t)bottom, (ptr_t)top,
                           MANAGED_STACK_ADDRESS_BOEHM_GC_static_page_was_dirty);
        }
#     endif
    }
# endif /* !NO_VDB_FOR_STATIC_ROOTS */

#else
  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_push_conditional(void *bottom, void *top, int all)
  {
    UNUSED_ARG(all);
    MANAGED_STACK_ADDRESS_BOEHM_GC_push_all(bottom, top);
  }
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL */

#if defined(AMIGA) || defined(MACOS) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS)
  void MANAGED_STACK_ADDRESS_BOEHM_GC_push_one(word p)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ONE_STACK(p, MARKED_FROM_REGISTER);
  }
#endif

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_push_many_regs(const word *regs, unsigned count)
  {
    unsigned i;
    for (i = 0; i < count; i++)
      MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ONE_STACK(regs[i], MARKED_FROM_REGISTER);
  }
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_API struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_mark_and_push(void *obj,
                        mse *mark_stack_ptr, mse *mark_stack_limit, void **src)
{
    hdr * hhdr;

    PREFETCH(obj);
    GET_HDR(obj, hhdr);
    if ((EXPECT(IS_FORWARDING_ADDR_OR_NIL(hhdr), FALSE)
         && (!MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers
             || NULL == (hhdr = MANAGED_STACK_ADDRESS_BOEHM_GC_find_header((ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_base(obj)))))
        || EXPECT(HBLK_IS_FREE(hhdr), FALSE)) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_ADD_TO_BLACK_LIST_NORMAL(obj, (ptr_t)src);
      return mark_stack_ptr;
    }
    return MANAGED_STACK_ADDRESS_BOEHM_GC_push_contents_hdr((ptr_t)obj, mark_stack_ptr, mark_stack_limit,
                                (ptr_t)src, hhdr, TRUE);
}

/* Mark and push (i.e. gray) a single object p onto the main    */
/* mark stack.  Consider p to be valid if it is an interior     */
/* pointer.                                                     */
/* The object p has passed a preliminary pointer validity       */
/* test, but we do not definitely know whether it is valid.     */
/* Mark bits are NOT atomically updated.  Thus this must be the */
/* only thread setting them.                                    */
MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_ADDR
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void
# if defined(PRINT_BLACK_LIST) || defined(KEEP_BACK_PTRS)
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_and_push_stack(ptr_t p, ptr_t source)
# else
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_and_push_stack(ptr_t p)
#   define source ((ptr_t)0)
# endif
{
    hdr * hhdr;
    ptr_t r = p;

    PREFETCH(p);
    GET_HDR(p, hhdr);
    if (EXPECT(IS_FORWARDING_ADDR_OR_NIL(hhdr), FALSE)) {
      if (NULL == hhdr
            || (r = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_base(p)) == NULL
            || (hhdr = HDR(r)) == NULL) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_ADD_TO_BLACK_LIST_STACK(p, source);
        return;
      }
    }
    if (EXPECT(HBLK_IS_FREE(hhdr), FALSE)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_ADD_TO_BLACK_LIST_NORMAL(p, source);
        return;
    }
#   ifdef THREADS
      /* Pointer is on the stack.  We may have dirtied the object       */
      /* it points to, but have not called MANAGED_STACK_ADDRESS_BOEHM_GC_dirty yet.                */
      MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(p); /* entire object */
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top = MANAGED_STACK_ADDRESS_BOEHM_GC_push_contents_hdr(r, MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top,
                                             MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit,
                                             source, hhdr, FALSE);
    /* We silently ignore pointers to near the end of a block,  */
    /* which is very mildly suboptimal.                         */
    /* FIXME: We should probably add a header word to address   */
    /* this.                                                    */
}
# undef source

#ifdef TRACE_BUF

# ifndef TRACE_ENTRIES
#   define TRACE_ENTRIES 1000
# endif

struct trace_entry {
    char * kind;
    word gc_no;
    word bytes_allocd;
    word arg1;
    word arg2;
} MANAGED_STACK_ADDRESS_BOEHM_GC_trace_buf[TRACE_ENTRIES] = { { NULL, 0, 0, 0, 0 } };

void MANAGED_STACK_ADDRESS_BOEHM_GC_add_trace_entry(char *kind, word arg1, word arg2)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_trace_buf[MANAGED_STACK_ADDRESS_BOEHM_GC_trace_buf_ptr].kind = kind;
    MANAGED_STACK_ADDRESS_BOEHM_GC_trace_buf[MANAGED_STACK_ADDRESS_BOEHM_GC_trace_buf_ptr].gc_no = MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no;
    MANAGED_STACK_ADDRESS_BOEHM_GC_trace_buf[MANAGED_STACK_ADDRESS_BOEHM_GC_trace_buf_ptr].bytes_allocd = MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd;
    MANAGED_STACK_ADDRESS_BOEHM_GC_trace_buf[MANAGED_STACK_ADDRESS_BOEHM_GC_trace_buf_ptr].arg1 = arg1 ^ SIGNB;
    MANAGED_STACK_ADDRESS_BOEHM_GC_trace_buf[MANAGED_STACK_ADDRESS_BOEHM_GC_trace_buf_ptr].arg2 = arg2 ^ SIGNB;
    MANAGED_STACK_ADDRESS_BOEHM_GC_trace_buf_ptr++;
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_trace_buf_ptr >= TRACE_ENTRIES) MANAGED_STACK_ADDRESS_BOEHM_GC_trace_buf_ptr = 0;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_print_trace_inner(word gc_no)
{
    int i;

    for (i = MANAGED_STACK_ADDRESS_BOEHM_GC_trace_buf_ptr-1; i != MANAGED_STACK_ADDRESS_BOEHM_GC_trace_buf_ptr; i--) {
        struct trace_entry *p;

        if (i < 0) i = TRACE_ENTRIES-1;
        p = MANAGED_STACK_ADDRESS_BOEHM_GC_trace_buf + i;
        if (p -> gc_no < gc_no || p -> kind == 0) {
            return;
        }
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Trace:%s (gc:%u, bytes:%lu) %p, %p\n",
                  p -> kind, (unsigned)p -> gc_no,
                  (unsigned long)p -> bytes_allocd,
                  (void *)(p -> arg1 ^ SIGNB), (void *)(p -> arg2 ^ SIGNB));
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Trace incomplete\n");
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_print_trace(word gc_no)
{
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_print_trace_inner(gc_no);
    UNLOCK();
}

#endif /* TRACE_BUF */

/* A version of MANAGED_STACK_ADDRESS_BOEHM_GC_push_all that treats all interior pointers as valid  */
/* and scans the entire region immediately, in case the contents change.*/
MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_ADDR MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_MEMORY MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_push_all_eager(void *bottom, void *top)
{
    REGISTER ptr_t current_p;
    REGISTER word *lim;
    REGISTER ptr_t greatest_ha = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr;
    REGISTER ptr_t least_ha = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr;
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr greatest_ha
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr least_ha

    if (top == 0) return;

    /* Check all pointers in range and push if they appear to be valid. */
    current_p = PTRT_ROUNDUP_BY_MASK(bottom, ALIGNMENT-1);
    lim = (word *)((word)top & ~(word)(ALIGNMENT-1)) - 1;
    for (; (word)current_p <= (word)lim; current_p += ALIGNMENT) {
      REGISTER word q;

      LOAD_WORD_OR_CONTINUE(q, current_p);
      MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ONE_STACK(q, current_p);
    }
#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr
#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_push_all_stack(ptr_t bottom, ptr_t top)
{
#   ifndef NEED_FIXUP_POINTER
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers
#         if defined(THREADS) && defined(MPROTECT_VDB)
            && !MANAGED_STACK_ADDRESS_BOEHM_GC_auto_incremental
#         endif
          && (word)MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top
             < (word)(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit - INITIAL_MARK_STACK_SIZE/8)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_push_all(bottom, top);
      } else
#   endif
    /* else */ {
      MANAGED_STACK_ADDRESS_BOEHM_GC_push_all_eager(bottom, top);
    }
}

#if defined(WRAP_MARK_SOME) && defined(PARALLEL_MARK)
  /* Similar to MANAGED_STACK_ADDRESS_BOEHM_GC_push_conditional but scans the whole region immediately. */
  MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_ADDR MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_MEMORY
  MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_push_conditional_eager(void *bottom, void *top,
                                          MANAGED_STACK_ADDRESS_BOEHM_GC_bool all)
  {
    REGISTER ptr_t current_p;
    REGISTER word *lim;
    REGISTER ptr_t greatest_ha = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr;
    REGISTER ptr_t least_ha = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr;
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr greatest_ha
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr least_ha

    if (top == NULL)
      return;
    (void)all; /* TODO: If !all then scan only dirty pages. */

    current_p = PTRT_ROUNDUP_BY_MASK(bottom, ALIGNMENT-1);
    lim = (word *)((word)top & ~(word)(ALIGNMENT-1)) - 1;
    for (; (word)current_p <= (word)lim; current_p += ALIGNMENT) {
      REGISTER word q;

      LOAD_WORD_OR_CONTINUE(q, current_p);
      MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ONE_HEAP(q, current_p, MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top);
    }
#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr
#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr
  }
#endif /* WRAP_MARK_SOME && PARALLEL_MARK */

#if !defined(SMALL_CONFIG) && !defined(USE_MARK_BYTES) \
    && !defined(MARK_BIT_PER_OBJ)
# if MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_WORDS == 1
#   define USE_PUSH_MARKED_ACCELERATORS
#   define PUSH_GRANULE(q) \
                do { \
                  word qcontents = (q)[0]; \
                  MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ONE_HEAP(qcontents, q, MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top); \
                } while (0)
# elif MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_WORDS == 2
#   define USE_PUSH_MARKED_ACCELERATORS
#   define PUSH_GRANULE(q) \
                do { \
                  word qcontents = (q)[0]; \
                  MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ONE_HEAP(qcontents, q, MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top); \
                  qcontents = (q)[1]; \
                  MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ONE_HEAP(qcontents, (q)+1, MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top); \
                } while (0)
# elif MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_WORDS == 4
#   define USE_PUSH_MARKED_ACCELERATORS
#   define PUSH_GRANULE(q) \
                do { \
                  word qcontents = (q)[0]; \
                  MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ONE_HEAP(qcontents, q, MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top); \
                  qcontents = (q)[1]; \
                  MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ONE_HEAP(qcontents, (q)+1, MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top); \
                  qcontents = (q)[2]; \
                  MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ONE_HEAP(qcontents, (q)+2, MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top); \
                  qcontents = (q)[3]; \
                  MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ONE_HEAP(qcontents, (q)+3, MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top); \
                } while (0)
# endif
#endif /* !USE_MARK_BYTES && !MARK_BIT_PER_OBJ && !SMALL_CONFIG */

#ifdef USE_PUSH_MARKED_ACCELERATORS
/* Push all objects reachable from marked objects in the given block */
/* containing objects of size 1 granule.                             */
MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_push_marked1(struct hblk *h, hdr *hhdr)
{
    word * mark_word_addr = &(hhdr->hb_marks[0]);
    word *p;
    word *plim;

    /* Allow registers to be used for some frequently accessed  */
    /* global variables.  Otherwise aliasing issues are likely  */
    /* to prevent that.                                         */
    ptr_t greatest_ha = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr;
    ptr_t least_ha = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr;
    mse * mark_stack_top = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top;
    mse * mark_stack_limit = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit;

#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top
#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top mark_stack_top
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit mark_stack_limit
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr greatest_ha
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr least_ha

    p = (word *)(h->hb_body);
    plim = (word *)(((word)h) + HBLKSIZE);

    /* Go through all words in block.   */
        while ((word)p < (word)plim) {
            word mark_word = *mark_word_addr++;
            word *q = p;

            while(mark_word != 0) {
              if (mark_word & 1) {
                  PUSH_GRANULE(q);
              }
              q += MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_WORDS;
              mark_word >>= 1;
            }
            p += CPP_WORDSZ * MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_WORDS;
        }

#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr
#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr
#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top
#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._mark_stack_limit
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._mark_stack_top
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top = mark_stack_top;
}


#ifndef UNALIGNED_PTRS

/* Push all objects reachable from marked objects in the given block */
/* of size 2 (granules) objects.                                     */
MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_push_marked2(struct hblk *h, hdr *hhdr)
{
    word * mark_word_addr = &(hhdr->hb_marks[0]);
    word *p;
    word *plim;

    ptr_t greatest_ha = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr;
    ptr_t least_ha = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr;
    mse * mark_stack_top = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top;
    mse * mark_stack_limit = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit;

#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top
#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top mark_stack_top
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit mark_stack_limit
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr greatest_ha
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr least_ha

    p = (word *)(h->hb_body);
    plim = (word *)(((word)h) + HBLKSIZE);

    /* Go through all words in block.   */
        while ((word)p < (word)plim) {
            word mark_word = *mark_word_addr++;
            word *q = p;

            while(mark_word != 0) {
              if (mark_word & 1) {
                  PUSH_GRANULE(q);
                  PUSH_GRANULE(q + MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_WORDS);
              }
              q += 2 * MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_WORDS;
              mark_word >>= 2;
            }
            p += CPP_WORDSZ * MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_WORDS;
        }

#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr
#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr
#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top
#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._mark_stack_limit
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._mark_stack_top
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top = mark_stack_top;
}

# if MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_WORDS < 4
/* Push all objects reachable from marked objects in the given block */
/* of size 4 (granules) objects.                                     */
/* There is a risk of mark stack overflow here.  But we handle that. */
/* And only unmarked objects get pushed, so it's not very likely.    */
MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_push_marked4(struct hblk *h, hdr *hhdr)
{
    word * mark_word_addr = &(hhdr->hb_marks[0]);
    word *p;
    word *plim;

    ptr_t greatest_ha = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr;
    ptr_t least_ha = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr;
    mse * mark_stack_top = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top;
    mse * mark_stack_limit = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit;

#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top
#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top mark_stack_top
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit mark_stack_limit
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr greatest_ha
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr least_ha

    p = (word *)(h->hb_body);
    plim = (word *)(((word)h) + HBLKSIZE);

    /* Go through all words in block.   */
        while ((word)p < (word)plim) {
            word mark_word = *mark_word_addr++;
            word *q = p;

            while(mark_word != 0) {
              if (mark_word & 1) {
                  PUSH_GRANULE(q);
                  PUSH_GRANULE(q + MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_WORDS);
                  PUSH_GRANULE(q + 2*MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_WORDS);
                  PUSH_GRANULE(q + 3*MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_WORDS);
              }
              q += 4 * MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_WORDS;
              mark_word >>= 4;
            }
            p += CPP_WORDSZ * MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_WORDS;
        }
#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr
#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr
#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top
#   undef MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._mark_stack_limit
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._mark_stack_top
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top = mark_stack_top;
}

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_WORDS < 4 */

#endif /* UNALIGNED_PTRS */

#endif /* USE_PUSH_MARKED_ACCELERATORS */

/* Push all objects reachable from marked objects in the given block.   */
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_push_marked(struct hblk *h, hdr *hhdr)
{
    word sz = hhdr -> hb_sz;
    word descr = hhdr -> hb_descr;
    ptr_t p;
    word bit_no;
    ptr_t lim;
    mse * mark_stack_top;
    mse * mark_stack_limit = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit;

    /* Some quick shortcuts: */
    if ((/* 0 | */ MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH) == descr) return;
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_block_empty(hhdr)/* nothing marked */) return;

#   if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL)
      MANAGED_STACK_ADDRESS_BOEHM_GC_n_rescuing_pages++;
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_objects_are_marked = TRUE;
    switch (BYTES_TO_GRANULES(sz)) {
#   if defined(USE_PUSH_MARKED_ACCELERATORS)
      case 1:
        MANAGED_STACK_ADDRESS_BOEHM_GC_push_marked1(h, hhdr);
        break;
#     if !defined(UNALIGNED_PTRS)
        case 2:
          MANAGED_STACK_ADDRESS_BOEHM_GC_push_marked2(h, hhdr);
          break;
#       if MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_WORDS < 4
          case 4:
            MANAGED_STACK_ADDRESS_BOEHM_GC_push_marked4(h, hhdr);
            break;
#       endif
#     endif /* !UNALIGNED_PTRS */
#   else
      case 1: /* to suppress "switch statement contains no case" warning */
#   endif
    default:
      lim = sz > MAXOBJBYTES ? h -> hb_body
                        : (ptr_t)((word)(h + 1) -> hb_body - sz);
      mark_stack_top = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top;
      for (p = h -> hb_body, bit_no = 0; (word)p <= (word)lim;
           p += sz, bit_no += MARK_BIT_OFFSET(sz)) {
        /* Mark from fields inside the object.  */
        if (mark_bit_from_hdr(hhdr, bit_no)) {
          mark_stack_top = MANAGED_STACK_ADDRESS_BOEHM_GC_push_obj(p, hhdr, mark_stack_top,
                                       mark_stack_limit);
        }
      }
      MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top = mark_stack_top;
    }
}

#ifdef ENABLE_DISCLAIM
/* Unconditionally mark from all objects which have not been reclaimed. */
/* This is useful in order to retain pointers which are reachable from  */
/* the disclaim notifiers.                                              */
/* To determine whether an object has been reclaimed, we require that   */
/* any live object has a non-zero as one of the two least significant   */
/* bits of the first word.  On the other hand, a reclaimed object is    */
/* a members of free-lists, and thus contains a word-aligned            */
/* next-pointer as the first word.                                      */
 MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD
 STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_push_unconditionally(struct hblk *h, hdr *hhdr)
 {
    word sz = hhdr -> hb_sz;
    word descr = hhdr -> hb_descr;
    ptr_t p;
    ptr_t lim;
    mse * mark_stack_top;
    mse * mark_stack_limit = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit;

    if ((/* 0 | */ MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH) == descr) return;

#   if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL)
      MANAGED_STACK_ADDRESS_BOEHM_GC_n_rescuing_pages++;
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_objects_are_marked = TRUE;
    lim = sz > MAXOBJBYTES ? h -> hb_body
                        : (ptr_t)((word)(h + 1) -> hb_body - sz);
    mark_stack_top = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top;
    for (p = h -> hb_body; (word)p <= (word)lim; p += sz) {
      if ((*(word *)p & 0x3) != 0) {
        mark_stack_top = MANAGED_STACK_ADDRESS_BOEHM_GC_push_obj(p, hhdr, mark_stack_top,
                                     mark_stack_limit);
      }
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top = mark_stack_top;
  }
#endif /* ENABLE_DISCLAIM */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL
  /* Test whether any page in the given block is dirty.   */
  STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_block_was_dirty(struct hblk *h, hdr *hhdr)
  {
    word sz;
    ptr_t p;

#   ifdef AO_HAVE_load
      /* Atomic access is used to avoid racing with MANAGED_STACK_ADDRESS_BOEHM_GC_realloc. */
      sz = (word)AO_load((volatile AO_t *)&(hhdr -> hb_sz));
#   else
      sz = hhdr -> hb_sz;
#   endif
    if (sz <= MAXOBJBYTES) {
      return MANAGED_STACK_ADDRESS_BOEHM_GC_page_was_dirty(h);
    }

    for (p = (ptr_t)h; (word)p < (word)h + sz; p += HBLKSIZE) {
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_page_was_dirty((struct hblk *)p)) return TRUE;
    }
    return FALSE;
  }
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL */

/* Similar to MANAGED_STACK_ADDRESS_BOEHM_GC_push_marked, but skip over unallocated blocks  */
/* and return address of next plausible block.                  */
STATIC struct hblk * MANAGED_STACK_ADDRESS_BOEHM_GC_push_next_marked(struct hblk *h)
{
    hdr * hhdr = HDR(h);

    if (EXPECT(IS_FORWARDING_ADDR_OR_NIL(hhdr) || HBLK_IS_FREE(hhdr), FALSE)) {
      h = MANAGED_STACK_ADDRESS_BOEHM_GC_next_block(h, FALSE);
      if (NULL == h) return NULL;
      hhdr = MANAGED_STACK_ADDRESS_BOEHM_GC_find_header((ptr_t)h);
    } else {
#     ifdef LINT2
        if (NULL == h) ABORT("Bad HDR() definition");
#     endif
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_push_marked(h, hhdr);
    return h + OBJ_SZ_TO_BLOCKS(hhdr -> hb_sz);
}

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL
  /* Identical to above, but mark only from dirty pages.        */
  STATIC struct hblk * MANAGED_STACK_ADDRESS_BOEHM_GC_push_next_marked_dirty(struct hblk *h)
  {
    hdr * hhdr = HDR(h);

    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_incremental) ABORT("Dirty bits not set up");
    for (;;) {
        if (EXPECT(IS_FORWARDING_ADDR_OR_NIL(hhdr)
                   || HBLK_IS_FREE(hhdr), FALSE)) {
          h = MANAGED_STACK_ADDRESS_BOEHM_GC_next_block(h, FALSE);
          if (NULL == h) return NULL;
          hhdr = MANAGED_STACK_ADDRESS_BOEHM_GC_find_header((ptr_t)h);
        } else {
#         ifdef LINT2
            if (NULL == h) ABORT("Bad HDR() definition");
#         endif
        }
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_block_was_dirty(h, hhdr))
          break;
        h += OBJ_SZ_TO_BLOCKS(hhdr -> hb_sz);
        hhdr = HDR(h);
    }
#   ifdef ENABLE_DISCLAIM
      if ((hhdr -> hb_flags & MARK_UNCONDITIONALLY) != 0) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_push_unconditionally(h, hhdr);

        /* Then we may ask, why not also add the MARK_UNCONDITIONALLY   */
        /* case to MANAGED_STACK_ADDRESS_BOEHM_GC_push_next_marked, which is also applied to        */
        /* uncollectible blocks?  But it seems to me that the function  */
        /* does not need to scan uncollectible (and unconditionally     */
        /* marked) blocks since those are already handled in the        */
        /* MS_PUSH_UNCOLLECTABLE phase.                                 */
      } else
#   endif
    /* else */ {
      MANAGED_STACK_ADDRESS_BOEHM_GC_push_marked(h, hhdr);
    }
    return h + OBJ_SZ_TO_BLOCKS(hhdr -> hb_sz);
  }
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL */

/* Similar to above, but for uncollectible pages.  Needed since we      */
/* do not clear marks for such pages, even for full collections.        */
STATIC struct hblk * MANAGED_STACK_ADDRESS_BOEHM_GC_push_next_marked_uncollectable(struct hblk *h)
{
    hdr * hhdr = HDR(h);

    for (;;) {
        if (EXPECT(IS_FORWARDING_ADDR_OR_NIL(hhdr)
                   || HBLK_IS_FREE(hhdr), FALSE)) {
          h = MANAGED_STACK_ADDRESS_BOEHM_GC_next_block(h, FALSE);
          if (NULL == h) return NULL;
          hhdr = MANAGED_STACK_ADDRESS_BOEHM_GC_find_header((ptr_t)h);
        } else {
#         ifdef LINT2
            if (NULL == h) ABORT("Bad HDR() definition");
#         endif
        }
        if (hhdr -> hb_obj_kind == UNCOLLECTABLE) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_push_marked(h, hhdr);
            break;
        }
#       ifdef ENABLE_DISCLAIM
            if ((hhdr -> hb_flags & MARK_UNCONDITIONALLY) != 0) {
                MANAGED_STACK_ADDRESS_BOEHM_GC_push_unconditionally(h, hhdr);
                break;
            }
#       endif
        h += OBJ_SZ_TO_BLOCKS(hhdr -> hb_sz);
        hhdr = HDR(h);
    }
    return h + OBJ_SZ_TO_BLOCKS(hhdr -> hb_sz);
}
