/*
 * Copyright (c) 1992-1994 by Xerox Corporation.  All rights reserved.
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

#ifdef CHECKSUMS

/* This is debugging code intended to verify the results of dirty bit   */
/* computations.  Works only in a single threaded environment.          */
# define NSUMS 10000
# define OFFSET 0x10000

typedef struct {
        MANAGED_STACK_ADDRESS_BOEHM_GC_bool new_valid;
        word old_sum;
        word new_sum;
        struct hblk * block;    /* Block to which this refers + OFFSET  */
                                /* to hide it from collector.           */
} page_entry;

page_entry MANAGED_STACK_ADDRESS_BOEHM_GC_sums[NSUMS];

STATIC word MANAGED_STACK_ADDRESS_BOEHM_GC_faulted[NSUMS] = { 0 };
                /* Record of pages on which we saw a write fault.       */

STATIC size_t MANAGED_STACK_ADDRESS_BOEHM_GC_n_faulted = 0;

#if defined(MPROTECT_VDB) && !defined(DARWIN)
  void MANAGED_STACK_ADDRESS_BOEHM_GC_record_fault(struct hblk * h)
  {
    word page = (word)h & ~(word)(MANAGED_STACK_ADDRESS_BOEHM_GC_page_size-1);

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_page_size != 0);
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_n_faulted >= NSUMS) ABORT("write fault log overflowed");
    MANAGED_STACK_ADDRESS_BOEHM_GC_faulted[MANAGED_STACK_ADDRESS_BOEHM_GC_n_faulted++] = page;
  }
#endif

STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_was_faulted(struct hblk *h)
{
    size_t i;
    word page = (word)h & ~(word)(MANAGED_STACK_ADDRESS_BOEHM_GC_page_size-1);

    for (i = 0; i < MANAGED_STACK_ADDRESS_BOEHM_GC_n_faulted; ++i) {
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_faulted[i] == page) return TRUE;
    }
    return FALSE;
}

STATIC word MANAGED_STACK_ADDRESS_BOEHM_GC_checksum(struct hblk *h)
{
    word *p = (word *)h;
    word *lim = (word *)(h+1);
    word result = 0;

    while ((word)p < (word)lim) {
        result += *p++;
    }
    return result | SIGNB; /* does not look like pointer */
}

int MANAGED_STACK_ADDRESS_BOEHM_GC_n_dirty_errors = 0;
int MANAGED_STACK_ADDRESS_BOEHM_GC_n_faulted_dirty_errors = 0;
unsigned long MANAGED_STACK_ADDRESS_BOEHM_GC_n_clean = 0;
unsigned long MANAGED_STACK_ADDRESS_BOEHM_GC_n_dirty = 0;

STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_update_check_page(struct hblk *h, int index)
{
    page_entry *pe = MANAGED_STACK_ADDRESS_BOEHM_GC_sums + index;
    hdr * hhdr = HDR(h);
    struct hblk *b;

    if (pe -> block != 0 && pe -> block != h + OFFSET) ABORT("goofed");
    pe -> old_sum = pe -> new_sum;
    pe -> new_sum = MANAGED_STACK_ADDRESS_BOEHM_GC_checksum(h);
#   if !defined(MSWIN32) && !defined(MSWINCE)
        if (pe -> new_sum != SIGNB && !MANAGED_STACK_ADDRESS_BOEHM_GC_page_was_ever_dirty(h)) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_page_was_ever_dirty(%p) is wrong\n", (void *)h);
        }
#   endif
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_page_was_dirty(h)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_n_dirty++;
    } else {
        MANAGED_STACK_ADDRESS_BOEHM_GC_n_clean++;
    }
    b = h;
    while (IS_FORWARDING_ADDR_OR_NIL(hhdr) && hhdr != 0) {
        b -= (word)hhdr;
        hhdr = HDR(b);
    }
    if (pe -> new_valid
        && hhdr != 0 && hhdr -> hb_descr != 0 /* may contain pointers */
        && pe -> old_sum != pe -> new_sum) {
        if (!MANAGED_STACK_ADDRESS_BOEHM_GC_page_was_dirty(h) || !MANAGED_STACK_ADDRESS_BOEHM_GC_page_was_ever_dirty(h)) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_bool was_faulted = MANAGED_STACK_ADDRESS_BOEHM_GC_was_faulted(h);
            /* Set breakpoint here */MANAGED_STACK_ADDRESS_BOEHM_GC_n_dirty_errors++;
            if (was_faulted) MANAGED_STACK_ADDRESS_BOEHM_GC_n_faulted_dirty_errors++;
        }
    }
    pe -> new_valid = TRUE;
    pe -> block = h + OFFSET;
}

word MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_in_used_blocks = 0;

STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK MANAGED_STACK_ADDRESS_BOEHM_GC_add_block(struct hblk *h, MANAGED_STACK_ADDRESS_BOEHM_GC_word dummy)
{
   hdr * hhdr = HDR(h);

   UNUSED_ARG(dummy);
   MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_in_used_blocks += (hhdr->hb_sz + HBLKSIZE-1) & ~(word)(HBLKSIZE-1);
}

STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_check_blocks(void)
{
    word bytes_in_free_blocks = MANAGED_STACK_ADDRESS_BOEHM_GC_large_free_bytes;

    MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_in_used_blocks = 0;
    MANAGED_STACK_ADDRESS_BOEHM_GC_apply_to_all_blocks(MANAGED_STACK_ADDRESS_BOEHM_GC_add_block, 0);
    MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF("MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_in_used_blocks= %lu,"
                       " bytes_in_free_blocks= %lu, heapsize= %lu\n",
                       (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_in_used_blocks,
                       (unsigned long)bytes_in_free_blocks,
                       (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize);
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_in_used_blocks + bytes_in_free_blocks != MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf("LOST SOME BLOCKS!!\n");
    }
}

/* Should be called immediately after MANAGED_STACK_ADDRESS_BOEHM_GC_read_dirty.    */
void MANAGED_STACK_ADDRESS_BOEHM_GC_check_dirty(void)
{
    int index;
    unsigned i;
    struct hblk *h;
    ptr_t start;

    MANAGED_STACK_ADDRESS_BOEHM_GC_check_blocks();

    MANAGED_STACK_ADDRESS_BOEHM_GC_n_dirty_errors = 0;
    MANAGED_STACK_ADDRESS_BOEHM_GC_n_faulted_dirty_errors = 0;
    MANAGED_STACK_ADDRESS_BOEHM_GC_n_clean = 0;
    MANAGED_STACK_ADDRESS_BOEHM_GC_n_dirty = 0;

    index = 0;
    for (i = 0; i < MANAGED_STACK_ADDRESS_BOEHM_GC_n_heap_sects; i++) {
        start = MANAGED_STACK_ADDRESS_BOEHM_GC_heap_sects[i].hs_start;
        for (h = (struct hblk *)start;
             (word)h < (word)(start + MANAGED_STACK_ADDRESS_BOEHM_GC_heap_sects[i].hs_bytes); h++) {
             MANAGED_STACK_ADDRESS_BOEHM_GC_update_check_page(h, index);
             index++;
             if (index >= NSUMS) goto out;
        }
    }
out:
    MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF("Checked %lu clean and %lu dirty pages\n",
                       MANAGED_STACK_ADDRESS_BOEHM_GC_n_clean, MANAGED_STACK_ADDRESS_BOEHM_GC_n_dirty);
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_n_dirty_errors > 0) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf("Found %d dirty bit errors (%d were faulted)\n",
                      MANAGED_STACK_ADDRESS_BOEHM_GC_n_dirty_errors, MANAGED_STACK_ADDRESS_BOEHM_GC_n_faulted_dirty_errors);
    }
    for (i = 0; i < MANAGED_STACK_ADDRESS_BOEHM_GC_n_faulted; ++i) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_faulted[i] = 0; /* Don't expose block pointers to GC */
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_n_faulted = 0;
}

#endif /* CHECKSUMS */
