/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
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

#if defined(KEEP_BACK_PTRS) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS)
# include "private/dbg_mlc.h" /* for NOT_MARKED */
#endif
/*
 * This implements:
 * 1. allocation of heap block headers
 * 2. A map from addresses to heap block addresses to heap block headers
 *
 * Access speed is crucial.  We implement an index structure based on a 2
 * level tree.
 */

/* Non-macro version of header location routine */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER hdr * MANAGED_STACK_ADDRESS_BOEHM_GC_find_header(ptr_t h)
{
#   ifdef HASH_TL
        hdr * result;
        GET_HDR(h, result);
        return result;
#   else
        return HDR_INNER(h);
#   endif
}

/* Handle a header cache miss.  Returns a pointer to the        */
/* header corresponding to p, if p can possibly be a valid      */
/* object pointer, and 0 otherwise.                             */
/* GUARANTEED to return 0 for a pointer past the first page     */
/* of an object unless both MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers is set     */
/* and p is in fact a valid object pointer.                     */
/* Never returns a pointer to a free hblk.                      */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER hdr *
#ifdef PRINT_BLACK_LIST
  MANAGED_STACK_ADDRESS_BOEHM_GC_header_cache_miss(ptr_t p, hdr_cache_entry *hce, ptr_t source)
#else
  MANAGED_STACK_ADDRESS_BOEHM_GC_header_cache_miss(ptr_t p, hdr_cache_entry *hce)
#endif
{
  hdr *hhdr;
  HC_MISS();
  GET_HDR(p, hhdr);
  if (IS_FORWARDING_ADDR_OR_NIL(hhdr)) {
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers) {
      if (hhdr != 0) {
        ptr_t current = p;

        current = (ptr_t)HBLKPTR(current);
        do {
            current = current - HBLKSIZE * (word)hhdr;
            hhdr = HDR(current);
        } while(IS_FORWARDING_ADDR_OR_NIL(hhdr));
        /* current points to near the start of the large object */
        if (hhdr -> hb_flags & IGNORE_OFF_PAGE)
            return 0;
        if (HBLK_IS_FREE(hhdr)
            || p - current >= (signed_word)(hhdr -> hb_sz)) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_ADD_TO_BLACK_LIST_NORMAL(p, source);
            /* Pointer past the end of the block */
            return 0;
        }
      } else {
        MANAGED_STACK_ADDRESS_BOEHM_GC_ADD_TO_BLACK_LIST_NORMAL(p, source);
        /* And return zero: */
      }
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(hhdr == 0 || !HBLK_IS_FREE(hhdr));
      return hhdr;
      /* Pointers past the first page are probably too rare     */
      /* to add them to the cache.  We don't.                   */
      /* And correctness relies on the fact that we don't.      */
    } else {
      if (hhdr == 0) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_ADD_TO_BLACK_LIST_NORMAL(p, source);
      }
      return 0;
    }
  } else {
    if (HBLK_IS_FREE(hhdr)) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_ADD_TO_BLACK_LIST_NORMAL(p, source);
      return 0;
    } else {
      hce -> block_addr = (word)(p) >> LOG_HBLKSIZE;
      hce -> hce_hdr = hhdr;
      return hhdr;
    }
  }
}

/* Routines to dynamically allocate collector data structures that will */
/* never be freed.                                                      */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_alloc(size_t bytes)
{
    ptr_t result = MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_free_ptr;
    size_t bytes_to_get;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    bytes = ROUNDUP_GRANULE_SIZE(bytes);
    for (;;) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((word)MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_end_ptr >= (word)result);
        if (bytes <= (word)MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_end_ptr - (word)result) {
            /* Unallocated space of scratch buffer has enough size. */
            MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_free_ptr = result + bytes;
            return result;
        }

        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_page_size != 0);
        if (bytes >= MINHINCR * HBLKSIZE) {
            bytes_to_get = ROUNDUP_PAGESIZE_IF_MMAP(bytes);
            result = MANAGED_STACK_ADDRESS_BOEHM_GC_os_get_mem(bytes_to_get);
            if (result != NULL) {
#             if defined(KEEP_BACK_PTRS) && (MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES < 0x10)
                MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((word)result > (word)NOT_MARKED);
#             endif
              /* No update of scratch free area pointer;        */
              /* get memory directly.                           */
#             ifdef USE_SCRATCH_LAST_END_PTR
                /* Update end point of last obtained area (needed only  */
                /* by MANAGED_STACK_ADDRESS_BOEHM_GC_register_dynamic_libraries for some targets).  */
                MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_last_end_ptr = result + bytes;
#             endif
            }
            return result;
        }

        bytes_to_get = ROUNDUP_PAGESIZE_IF_MMAP(MINHINCR * HBLKSIZE);
                                                /* round up for safety */
        result = MANAGED_STACK_ADDRESS_BOEHM_GC_os_get_mem(bytes_to_get);
        if (EXPECT(NULL == result, FALSE)) {
            WARN("Out of memory - trying to allocate requested amount"
                 " (%" WARN_PRIuPTR " bytes)...\n", bytes);
            bytes_to_get = ROUNDUP_PAGESIZE_IF_MMAP(bytes);
            result = MANAGED_STACK_ADDRESS_BOEHM_GC_os_get_mem(bytes_to_get);
            if (result != NULL) {
#             ifdef USE_SCRATCH_LAST_END_PTR
                MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_last_end_ptr = result + bytes;
#             endif
            }
            return result;
        }

        /* TODO: some amount of unallocated space may remain unused forever */
        /* Update scratch area pointers and retry.      */
        MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_free_ptr = result;
        MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_end_ptr = MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_free_ptr + bytes_to_get;
#       ifdef USE_SCRATCH_LAST_END_PTR
          MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_last_end_ptr = MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_end_ptr;
#       endif
    }
}

/* Return an uninitialized header */
static hdr * alloc_hdr(void)
{
    hdr * result;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    if (NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_hdr_free_list) {
        result = (hdr *)MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_alloc(sizeof(hdr));
    } else {
        result = MANAGED_STACK_ADDRESS_BOEHM_GC_hdr_free_list;
        MANAGED_STACK_ADDRESS_BOEHM_GC_hdr_free_list = (hdr *) result -> hb_next;
    }
    return result;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE void free_hdr(hdr * hhdr)
{
    hhdr -> hb_next = (struct hblk *)MANAGED_STACK_ADDRESS_BOEHM_GC_hdr_free_list;
    MANAGED_STACK_ADDRESS_BOEHM_GC_hdr_free_list = hhdr;
}

#ifdef COUNT_HDR_CACHE_HITS
  /* Used for debugging/profiling (the symbols are externally visible). */
  word MANAGED_STACK_ADDRESS_BOEHM_GC_hdr_cache_hits = 0;
  word MANAGED_STACK_ADDRESS_BOEHM_GC_hdr_cache_misses = 0;
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_init_headers(void)
{
    unsigned i;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_all_nils);
    MANAGED_STACK_ADDRESS_BOEHM_GC_all_nils = (bottom_index *)MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_alloc(sizeof(bottom_index));
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_all_nils == NULL) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf("Insufficient memory for MANAGED_STACK_ADDRESS_BOEHM_GC_all_nils\n");
      EXIT();
    }
    BZERO(MANAGED_STACK_ADDRESS_BOEHM_GC_all_nils, sizeof(bottom_index));
    for (i = 0; i < TOP_SZ; i++) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_top_index[i] = MANAGED_STACK_ADDRESS_BOEHM_GC_all_nils;
    }
}

/* Make sure that there is a bottom level index block for address addr. */
/* Return FALSE on failure.                                             */
static MANAGED_STACK_ADDRESS_BOEHM_GC_bool get_index(word addr)
{
    word hi = (word)(addr) >> (LOG_BOTTOM_SZ + LOG_HBLKSIZE);
    bottom_index * r;
    bottom_index * p;
    bottom_index ** prev;
    bottom_index *pi; /* old_p */
    word i;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
#   ifdef HASH_TL
      i = TL_HASH(hi);

      pi = MANAGED_STACK_ADDRESS_BOEHM_GC_top_index[i];
      for (p = pi; p != MANAGED_STACK_ADDRESS_BOEHM_GC_all_nils; p = p -> hash_link) {
          if (p -> key == hi) return TRUE;
      }
#   else
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_top_index[hi] != MANAGED_STACK_ADDRESS_BOEHM_GC_all_nils)
        return TRUE;
      i = hi;
#   endif
    r = (bottom_index *)MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_alloc(sizeof(bottom_index));
    if (EXPECT(NULL == r, FALSE))
      return FALSE;
    BZERO(r, sizeof(bottom_index));
    r -> key = hi;
#   ifdef HASH_TL
      r -> hash_link = pi;
#   endif

    /* Add it to the list of bottom indices */
      prev = &MANAGED_STACK_ADDRESS_BOEHM_GC_all_bottom_indices;    /* pointer to p */
      pi = 0;                           /* bottom_index preceding p */
      while ((p = *prev) != 0 && p -> key < hi) {
        pi = p;
        prev = &(p -> asc_link);
      }
      r -> desc_link = pi;
      if (0 == p) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_all_bottom_indices_end = r;
      } else {
        p -> desc_link = r;
      }
      r -> asc_link = p;
      *prev = r;

      MANAGED_STACK_ADDRESS_BOEHM_GC_top_index[i] = r;
    return TRUE;
}

/* Install a header for block h.        */
/* The header is uninitialized.         */
/* Returns the header or 0 on failure.  */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER struct hblkhdr * MANAGED_STACK_ADDRESS_BOEHM_GC_install_header(struct hblk *h)
{
    hdr * result;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    if (EXPECT(!get_index((word)h), FALSE)) return NULL;

    result = alloc_hdr();
    if (EXPECT(result != NULL, TRUE)) {
      SET_HDR(h, result);
#     ifdef USE_MUNMAP
        result -> hb_last_reclaimed = (unsigned short)MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no;
#     endif
    }
    return result;
}

/* Set up forwarding counts for block h of size sz */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_install_counts(struct hblk *h, size_t sz/* bytes */)
{
    struct hblk * hbp;

    for (hbp = h; (word)hbp < (word)h + sz; hbp += BOTTOM_SZ) {
        if (!get_index((word)hbp))
            return FALSE;
        if ((word)hbp > MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX - (word)BOTTOM_SZ * HBLKSIZE)
            break; /* overflow of hbp+=BOTTOM_SZ is expected */
    }
    if (!get_index((word)h + sz - 1))
        return FALSE;
    for (hbp = h + 1; (word)hbp < (word)h + sz; hbp += 1) {
        word i = (word)HBLK_PTR_DIFF(hbp, h);

        SET_HDR(hbp, (hdr *)(i > MAX_JUMP? MAX_JUMP : i));
    }
    return TRUE;
}

/* Remove the header for block h */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_remove_header(struct hblk *h)
{
    hdr **ha;
    GET_HDR_ADDR(h, ha);
    free_hdr(*ha);
    *ha = 0;
}

/* Remove forwarding counts for h */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_remove_counts(struct hblk *h, size_t sz/* bytes */)
{
    struct hblk * hbp;

    if (sz <= HBLKSIZE) return;
    if (HDR(h+1) == 0) {
#     ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS
        for (hbp = h+2; (word)hbp < (word)h + sz; hbp++)
          MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(HDR(hbp) == 0);
#     endif
      return;
    }

    for (hbp = h+1; (word)hbp < (word)h + sz; hbp += 1) {
        SET_HDR(hbp, 0);
    }
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_apply_to_all_blocks(MANAGED_STACK_ADDRESS_BOEHM_GC_walk_hblk_fn fn,
                                           MANAGED_STACK_ADDRESS_BOEHM_GC_word client_data)
{
    signed_word j;
    bottom_index * index_p;

    for (index_p = MANAGED_STACK_ADDRESS_BOEHM_GC_all_bottom_indices; index_p != 0;
         index_p = index_p -> asc_link) {
        for (j = BOTTOM_SZ-1; j >= 0;) {
            if (!IS_FORWARDING_ADDR_OR_NIL(index_p->index[j])) {
                if (!HBLK_IS_FREE(index_p->index[j])) {
                    (*fn)(((struct hblk *)
                              (((index_p->key << LOG_BOTTOM_SZ) + (word)j)
                               << LOG_HBLKSIZE)),
                          client_data);
                }
                j--;
             } else if (index_p->index[j] == 0) {
                j--;
             } else {
                j -= (signed_word)(index_p->index[j]);
             }
         }
     }
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER struct hblk * MANAGED_STACK_ADDRESS_BOEHM_GC_next_block(struct hblk *h, MANAGED_STACK_ADDRESS_BOEHM_GC_bool allow_free)
{
    REGISTER bottom_index * bi;
    REGISTER word j = ((word)h >> LOG_HBLKSIZE) & (BOTTOM_SZ-1);

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    GET_BI(h, bi);
    if (bi == MANAGED_STACK_ADDRESS_BOEHM_GC_all_nils) {
        REGISTER word hi = (word)h >> (LOG_BOTTOM_SZ + LOG_HBLKSIZE);

        bi = MANAGED_STACK_ADDRESS_BOEHM_GC_all_bottom_indices;
        while (bi != 0 && bi -> key < hi) bi = bi -> asc_link;
        j = 0;
    }

    while (bi != 0) {
        while (j < BOTTOM_SZ) {
            hdr * hhdr = bi -> index[j];
            if (IS_FORWARDING_ADDR_OR_NIL(hhdr)) {
                j++;
            } else {
                if (allow_free || !HBLK_IS_FREE(hhdr)) {
                    return (struct hblk *)(((bi -> key << LOG_BOTTOM_SZ)
                                            + j) << LOG_HBLKSIZE);
                } else {
                    j += divHBLKSZ(hhdr -> hb_sz);
                }
            }
        }
        j = 0;
        bi = bi -> asc_link;
    }
    return NULL;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER struct hblk * MANAGED_STACK_ADDRESS_BOEHM_GC_prev_block(struct hblk *h)
{
    bottom_index * bi;
    signed_word j = ((word)h >> LOG_HBLKSIZE) & (BOTTOM_SZ-1);

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    GET_BI(h, bi);
    if (bi == MANAGED_STACK_ADDRESS_BOEHM_GC_all_nils) {
        word hi = (word)h >> (LOG_BOTTOM_SZ + LOG_HBLKSIZE);

        bi = MANAGED_STACK_ADDRESS_BOEHM_GC_all_bottom_indices_end;
        while (bi != NULL && bi -> key > hi)
            bi = bi -> desc_link;
        j = BOTTOM_SZ - 1;
    }
    for (; bi != NULL; bi = bi -> desc_link) {
        while (j >= 0) {
            hdr * hhdr = bi -> index[j];

            if (NULL == hhdr) {
                --j;
            } else if (IS_FORWARDING_ADDR_OR_NIL(hhdr)) {
                j -= (signed_word)hhdr;
            } else {
                return (struct hblk*)(((bi -> key << LOG_BOTTOM_SZ) + (word)j)
                                       << LOG_HBLKSIZE);
            }
        }
        j = BOTTOM_SZ - 1;
    }
    return NULL;
}
