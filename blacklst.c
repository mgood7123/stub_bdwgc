/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
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
 * We maintain several hash tables of hblks that have had false hits.
 * Each contains one bit per hash bucket;  If any page in the bucket
 * has had a false hit, we assume that all of them have.
 * See the definition of page_hash_table in gc_priv.h.
 * False hits from the stack(s) are much more dangerous than false hits
 * from elsewhere, since the former can pin a large object that spans the
 * block, even though it does not start on the dangerous block.
 */

/* Externally callable routines are:    */
/* - MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_black_list_normal,       */
/* - MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_black_list_stack,        */
/* - MANAGED_STACK_ADDRESS_BOEHM_GC_promote_black_lists.            */

/* Pointers to individual tables.  We replace one table by another by   */
/* switching these pointers.                                            */
STATIC word * MANAGED_STACK_ADDRESS_BOEHM_GC_old_normal_bl = NULL;
                /* Nonstack false references seen at last full          */
                /* collection.                                          */
STATIC word * MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_normal_bl = NULL;
                /* Nonstack false references seen since last            */
                /* full collection.                                     */
STATIC word * MANAGED_STACK_ADDRESS_BOEHM_GC_old_stack_bl = NULL;
STATIC word * MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_stack_bl = NULL;

STATIC word MANAGED_STACK_ADDRESS_BOEHM_GC_total_stack_black_listed = 0;
                        /* Number of bytes on stack blacklist.  */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER word MANAGED_STACK_ADDRESS_BOEHM_GC_black_list_spacing = MINHINCR * HBLKSIZE;
                        /* Initial rough guess. */

STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_clear_bl(word *);

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_default_print_heap_obj_proc(ptr_t p)
{
    ptr_t base = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_base(p);
    int kind = HDR(base)->hb_obj_kind;

    MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf("object at %p of appr. %lu bytes (%s)\n",
                  (void *)base, (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_size(base),
                  kind == PTRFREE ? "atomic" :
                    IS_UNCOLLECTABLE(kind) ? "uncollectable" : "composite");
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void (*MANAGED_STACK_ADDRESS_BOEHM_GC_print_heap_obj)(ptr_t p) = MANAGED_STACK_ADDRESS_BOEHM_GC_default_print_heap_obj_proc;

#ifdef PRINT_BLACK_LIST
  STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_print_blacklisted_ptr(word p, ptr_t source,
                                       const char *kind_str)
  {
    ptr_t base = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_base(source);

    if (0 == base) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf("Black listing (%s) %p referenced from %p in %s\n",
                      kind_str, (void *)p, (void *)source,
                      NULL != source ? "root set" : "register");
    } else {
        /* FIXME: We can't call the debug version of MANAGED_STACK_ADDRESS_BOEHM_GC_print_heap_obj  */
        /* (with PRINT_CALL_CHAIN) here because the lock is held and    */
        /* the world is stopped.                                        */
        MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf("Black listing (%s) %p referenced from %p in"
                      " object at %p of appr. %lu bytes\n",
                      kind_str, (void *)p, (void *)source,
                      (void *)base, (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_size(base));
    }
  }
#endif /* PRINT_BLACK_LIST */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_bl_init_no_interiors(void)
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
  if (MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_normal_bl == 0) {
    MANAGED_STACK_ADDRESS_BOEHM_GC_old_normal_bl = (word *)MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_alloc(sizeof(page_hash_table));
    MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_normal_bl = (word *)MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_alloc(
                                                  sizeof(page_hash_table));
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_old_normal_bl == 0 || MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_normal_bl == 0) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf("Insufficient memory for black list\n");
      EXIT();
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_clear_bl(MANAGED_STACK_ADDRESS_BOEHM_GC_old_normal_bl);
    MANAGED_STACK_ADDRESS_BOEHM_GC_clear_bl(MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_normal_bl);
  }
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_bl_init(void)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_bl_init_no_interiors();
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_old_stack_bl && NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_stack_bl);
    MANAGED_STACK_ADDRESS_BOEHM_GC_old_stack_bl = (word *)MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_alloc(sizeof(page_hash_table));
    MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_stack_bl = (word *)MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_alloc(sizeof(page_hash_table));
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_old_stack_bl == 0 || MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_stack_bl == 0) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf("Insufficient memory for black list\n");
        EXIT();
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_clear_bl(MANAGED_STACK_ADDRESS_BOEHM_GC_old_stack_bl);
    MANAGED_STACK_ADDRESS_BOEHM_GC_clear_bl(MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_stack_bl);
}

STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_clear_bl(word *doomed)
{
    BZERO(doomed, sizeof(page_hash_table));
}

STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_copy_bl(word *old, word *dest)
{
    BCOPY(old, dest, sizeof(page_hash_table));
}

static word total_stack_black_listed(void);

/* Signal the completion of a collection.  Turn the incomplete black    */
/* lists into new black lists, etc.                                     */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_promote_black_lists(void)
{
    word * very_old_normal_bl = MANAGED_STACK_ADDRESS_BOEHM_GC_old_normal_bl;
    word * very_old_stack_bl = MANAGED_STACK_ADDRESS_BOEHM_GC_old_stack_bl;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    MANAGED_STACK_ADDRESS_BOEHM_GC_old_normal_bl = MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_normal_bl;
    MANAGED_STACK_ADDRESS_BOEHM_GC_old_stack_bl = MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_stack_bl;
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_clear_bl(very_old_normal_bl);
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_clear_bl(very_old_stack_bl);
    MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_normal_bl = very_old_normal_bl;
    MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_stack_bl = very_old_stack_bl;
    MANAGED_STACK_ADDRESS_BOEHM_GC_total_stack_black_listed = total_stack_black_listed();
    MANAGED_STACK_ADDRESS_BOEHM_GC_VERBOSE_LOG_PRINTF(
                "%lu bytes in heap blacklisted for interior pointers\n",
                (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_total_stack_black_listed);
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_total_stack_black_listed != 0) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_black_list_spacing =
                HBLKSIZE*(MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize/MANAGED_STACK_ADDRESS_BOEHM_GC_total_stack_black_listed);
    }
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_black_list_spacing < 3 * HBLKSIZE) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_black_list_spacing = 3 * HBLKSIZE;
    }
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_black_list_spacing > MAXHINCR * HBLKSIZE) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_black_list_spacing = MAXHINCR * HBLKSIZE;
        /* Makes it easier to allocate really huge blocks, which otherwise */
        /* may have problems with nonuniform blacklist distributions.      */
        /* This way we should always succeed immediately after growing the */
        /* heap.                                                           */
    }
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_unpromote_black_lists(void)
{
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_copy_bl(MANAGED_STACK_ADDRESS_BOEHM_GC_old_normal_bl, MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_normal_bl);
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_copy_bl(MANAGED_STACK_ADDRESS_BOEHM_GC_old_stack_bl, MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_stack_bl);
}

#if defined(PARALLEL_MARK) && defined(THREAD_SANITIZER)
# define backlist_set_pht_entry_from_index(db, index) \
                        set_pht_entry_from_index_concurrent(db, index)
#else
  /* It is safe to set a bit in a blacklist even without        */
  /* synchronization, the only drawback is that we might have   */
  /* to redo blacklisting sometimes.                            */
# define backlist_set_pht_entry_from_index(bl, index) \
                        set_pht_entry_from_index(bl, index)
#endif

/* P is not a valid pointer reference, but it falls inside      */
/* the plausible heap bounds.                                   */
/* Add it to the normal incomplete black list if appropriate.   */
#ifdef PRINT_BLACK_LIST
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_black_list_normal(word p, ptr_t source)
#else
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_black_list_normal(word p)
#endif
{
# ifndef PARALLEL_MARK
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
# endif
  if (MANAGED_STACK_ADDRESS_BOEHM_GC_modws_valid_offsets[p & (sizeof(word)-1)]) {
    word index = PHT_HASH((word)p);

    if (HDR(p) == 0 || get_pht_entry_from_index(MANAGED_STACK_ADDRESS_BOEHM_GC_old_normal_bl, index)) {
#     ifdef PRINT_BLACK_LIST
        if (!get_pht_entry_from_index(MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_normal_bl, index)) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_print_blacklisted_ptr(p, source, "normal");
        }
#     endif
      backlist_set_pht_entry_from_index(MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_normal_bl, index);
    } /* else this is probably just an interior pointer to an allocated */
      /* object, and isn't worth black listing.                         */
  }
}

/* And the same for false pointers from the stack. */
#ifdef PRINT_BLACK_LIST
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_black_list_stack(word p, ptr_t source)
#else
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_black_list_stack(word p)
#endif
{
  word index = PHT_HASH((word)p);

# ifndef PARALLEL_MARK
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
# endif
  if (HDR(p) == 0 || get_pht_entry_from_index(MANAGED_STACK_ADDRESS_BOEHM_GC_old_stack_bl, index)) {
#   ifdef PRINT_BLACK_LIST
      if (!get_pht_entry_from_index(MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_stack_bl, index)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_print_blacklisted_ptr(p, source, "stack");
      }
#   endif
    backlist_set_pht_entry_from_index(MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_stack_bl, index);
  }
}

/*
 * Is the block starting at h of size len bytes black listed?  If so,
 * return the address of the next plausible r such that (r, len) might not
 * be black listed.  (R may not actually be in the heap.  We guarantee only
 * that every smaller value of r after h is also black listed.)
 * If (h,len) is not black listed, return 0.
 * Knows about the structure of the black list hash tables.
 * Assumes the allocation lock is held but no assertion about it by design.
 */
MANAGED_STACK_ADDRESS_BOEHM_GC_API struct MANAGED_STACK_ADDRESS_BOEHM_GC_hblk_s *MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_is_black_listed(struct MANAGED_STACK_ADDRESS_BOEHM_GC_hblk_s *h,
                                                    MANAGED_STACK_ADDRESS_BOEHM_GC_word len)
{
    word index = PHT_HASH((word)h);
    word i;
    word nblocks;

    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers
        && (get_pht_entry_from_index(MANAGED_STACK_ADDRESS_BOEHM_GC_old_normal_bl, index)
            || get_pht_entry_from_index(MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_normal_bl, index))) {
      return h + 1;
    }

    nblocks = divHBLKSZ(len);
    for (i = 0;;) {
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_old_stack_bl[divWORDSZ(index)] == 0
            && MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_stack_bl[divWORDSZ(index)] == 0) {
          /* An easy case. */
          i += (word)CPP_WORDSZ - modWORDSZ(index);
        } else {
          if (get_pht_entry_from_index(MANAGED_STACK_ADDRESS_BOEHM_GC_old_stack_bl, index)
              || get_pht_entry_from_index(MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_stack_bl, index)) {
            return h + (i+1);
          }
          i++;
        }
        if (i >= nblocks) break;
        index = PHT_HASH((word)(h + i));
    }
    return NULL;
}

/* Return the number of blacklisted blocks in a given range.    */
/* Used only for statistical purposes.                          */
/* Looks only at the MANAGED_STACK_ADDRESS_BOEHM_GC_incomplete_stack_bl.                    */
STATIC word MANAGED_STACK_ADDRESS_BOEHM_GC_number_stack_black_listed(struct hblk *start,
                                         struct hblk *endp1)
{
    struct hblk * h;
    word result = 0;

    for (h = start; (word)h < (word)endp1; h++) {
        word index = PHT_HASH((word)h);

        if (get_pht_entry_from_index(MANAGED_STACK_ADDRESS_BOEHM_GC_old_stack_bl, index)) result++;
    }
    return result;
}

/* Return the total number of (stack) black-listed bytes. */
static word total_stack_black_listed(void)
{
    unsigned i;
    word total = 0;

    for (i = 0; i < MANAGED_STACK_ADDRESS_BOEHM_GC_n_heap_sects; i++) {
        struct hblk * start = (struct hblk *) MANAGED_STACK_ADDRESS_BOEHM_GC_heap_sects[i].hs_start;
        struct hblk * endp1 = start + divHBLKSZ(MANAGED_STACK_ADDRESS_BOEHM_GC_heap_sects[i].hs_bytes);

        total += MANAGED_STACK_ADDRESS_BOEHM_GC_number_stack_black_listed(start, endp1);
    }
    return total * HBLKSIZE;
}
