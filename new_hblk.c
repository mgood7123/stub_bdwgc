/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 2000 by Hewlett-Packard Company.  All rights reserved.
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
 * This file contains the functions:
 *      ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_build_flXXX(h, old_fl)
 *      void MANAGED_STACK_ADDRESS_BOEHM_GC_new_hblk(size, kind)
 */

#ifndef SMALL_CONFIG
  /* Build a free list for size 2 (words) cleared objects inside        */
  /* hblk h.  Set the last link to be ofl.  Return a pointer to the     */
  /* first free list entry.                                             */
  STATIC ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_build_fl_clear2(struct hblk *h, ptr_t ofl)
  {
    word * p = (word *)(h -> hb_body);
    word * lim = (word *)(h + 1);

    p[0] = (word)ofl;
    p[1] = 0;
    p[2] = (word)p;
    p[3] = 0;
    p += 4;
    for (; (word)p < (word)lim; p += 4) {
        p[0] = (word)(p-2);
        p[1] = 0;
        p[2] = (word)p;
        p[3] = 0;
    }
    return (ptr_t)(p-2);
  }

  /* The same for size 4 cleared objects.       */
  STATIC ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_build_fl_clear4(struct hblk *h, ptr_t ofl)
  {
    word * p = (word *)(h -> hb_body);
    word * lim = (word *)(h + 1);

    p[0] = (word)ofl;
    p[1] = 0;
    p[2] = 0;
    p[3] = 0;
    p += 4;
    for (; (word)p < (word)lim; p += 4) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_PREFETCH_FOR_WRITE((ptr_t)(p + 64));
        p[0] = (word)(p-4);
        p[1] = 0;
        CLEAR_DOUBLE(p+2);
    }
    return (ptr_t)(p-4);
  }

  /* The same for size 2 uncleared objects.     */
  STATIC ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_build_fl2(struct hblk *h, ptr_t ofl)
  {
    word * p = (word *)(h -> hb_body);
    word * lim = (word *)(h + 1);

    p[0] = (word)ofl;
    p[2] = (word)p;
    p += 4;
    for (; (word)p < (word)lim; p += 4) {
        p[0] = (word)(p-2);
        p[2] = (word)p;
    }
    return (ptr_t)(p-2);
  }

  /* The same for size 4 uncleared objects.     */
  STATIC ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_build_fl4(struct hblk *h, ptr_t ofl)
  {
    word * p = (word *)(h -> hb_body);
    word * lim = (word *)(h + 1);

    p[0] = (word)ofl;
    p[4] = (word)p;
    p += 8;
    for (; (word)p < (word)lim; p += 8) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_PREFETCH_FOR_WRITE((ptr_t)(p + 64));
        p[0] = (word)(p-4);
        p[4] = (word)p;
    }
    return (ptr_t)(p-4);
  }
#endif /* !SMALL_CONFIG */

/* Build a free list for objects of size sz inside heap block h.        */
/* Clear objects inside h if clear is set.  Add list to the end of      */
/* the free list we build.  Return the new free list.                   */
/* This could be called without the allocation lock, if we ensure that  */
/* there is no concurrent collection which might reclaim objects that   */
/* we have not yet allocated.                                           */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_build_fl(struct hblk *h, size_t sz, MANAGED_STACK_ADDRESS_BOEHM_GC_bool clear,
                           ptr_t list)
{
  word *p, *prev;
  word *last_object;            /* points to last object in new hblk    */

  /* Do a few prefetches here, just because it's cheap.         */
  /* If we were more serious about it, these should go inside   */
  /* the loops.  But write prefetches usually don't seem to     */
  /* matter much.                                               */
    MANAGED_STACK_ADDRESS_BOEHM_GC_PREFETCH_FOR_WRITE((ptr_t)h);
    MANAGED_STACK_ADDRESS_BOEHM_GC_PREFETCH_FOR_WRITE((ptr_t)h + 128);
    MANAGED_STACK_ADDRESS_BOEHM_GC_PREFETCH_FOR_WRITE((ptr_t)h + 256);
    MANAGED_STACK_ADDRESS_BOEHM_GC_PREFETCH_FOR_WRITE((ptr_t)h + 378);
# ifndef SMALL_CONFIG
    /* Handle small objects sizes more efficiently.  For larger objects */
    /* the difference is less significant.                              */
    switch (sz) {
        case 2: if (clear) {
                    return MANAGED_STACK_ADDRESS_BOEHM_GC_build_fl_clear2(h, list);
                } else {
                    return MANAGED_STACK_ADDRESS_BOEHM_GC_build_fl2(h, list);
                }
        case 4: if (clear) {
                    return MANAGED_STACK_ADDRESS_BOEHM_GC_build_fl_clear4(h, list);
                } else {
                    return MANAGED_STACK_ADDRESS_BOEHM_GC_build_fl4(h, list);
                }
        default:
                break;
    }
# endif /* !SMALL_CONFIG */

  /* Clear the page if necessary. */
    if (clear) BZERO(h, HBLKSIZE);

  /* Add objects to free list */
    p = (word *)(h -> hb_body) + sz;    /* second object in *h  */
    prev = (word *)(h -> hb_body);              /* One object behind p  */
    last_object = (word *)((char *)h + HBLKSIZE);
    last_object -= sz;
                            /* Last place for last object to start */

  /* make a list of all objects in *h with head as last object */
    while ((word)p <= (word)last_object) {
      /* current object's link points to last object */
        obj_link(p) = (ptr_t)prev;
        prev = p;
        p += sz;
    }
    p -= sz;                    /* p now points to last object */

  /* Put p (which is now head of list of objects in *h) as first    */
  /* pointer in the appropriate free list for this size.            */
    *(ptr_t *)h = list;
    return (ptr_t)p;
}

/* Allocate a new heapblock for small objects of the given size in  */
/* granules and kind.  Add all of the heapblock's objects to the    */
/* free list for objects of that size.  Set all mark bits           */
/* if objects are uncollectible.  Will fail to do anything if we    */
/* are out of memory.                                               */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_new_hblk(size_t gran, int k)
{
  struct hblk *h;       /* the new heap block */

  MANAGED_STACK_ADDRESS_BOEHM_GC_STATIC_ASSERT(sizeof(struct hblk) == HBLKSIZE);
  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
  /* Allocate a new heap block. */
  h = MANAGED_STACK_ADDRESS_BOEHM_GC_allochblk(GRANULES_TO_BYTES(gran), k, 0 /* flags */, 0);
  if (EXPECT(NULL == h, FALSE)) return; /* out of memory */

  /* Mark all objects if appropriate. */
  if (IS_UNCOLLECTABLE(k)) MANAGED_STACK_ADDRESS_BOEHM_GC_set_hdr_marks(HDR(h));

  /* Build the free list */
  MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[k].ok_freelist[gran] =
        MANAGED_STACK_ADDRESS_BOEHM_GC_build_fl(h, GRANULES_TO_WORDS(gran),
                    MANAGED_STACK_ADDRESS_BOEHM_GC_debugging_started || MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[k].ok_init,
                    (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[k].ok_freelist[gran]);
}
