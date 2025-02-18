/*
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1999-2000 by Hewlett-Packard Company.  All rights reserved.
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

/*
 * Some simple primitives for allocation with explicit type information.
 * Simple objects are allocated such that they contain a MANAGED_STACK_ADDRESS_BOEHM_GC_descr at the
 * end (in the last allocated word).  This descriptor may be a procedure
 * which then examines an extended descriptor passed as its environment.
 *
 * Arrays are treated as simple objects if they have sufficiently simple
 * structure.  Otherwise they are allocated from an array kind that supplies
 * a special mark procedure.  These arrays contain a pointer to a
 * complex_descriptor as their last word.
 * This is done because the environment field is too small, and the collector
 * must trace the complex_descriptor.
 *
 * Note that descriptors inside objects may appear cleared, if we encounter a
 * false reference to an object on a free list.  In the MANAGED_STACK_ADDRESS_BOEHM_GC_descr case, this
 * is OK, since a 0 descriptor corresponds to examining no fields.
 * In the complex_descriptor case, we explicitly check for that case.
 *
 * MAJOR PARTS OF THIS CODE HAVE NOT BEEN TESTED AT ALL and are not testable,
 * since they are not accessible through the current interface.
 */

#include "gc/gc_typed.h"

#define TYPD_EXTRA_BYTES (sizeof(word) - EXTRA_BYTES)

STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_explicit_kind = 0;
                        /* Object kind for objects with indirect        */
                        /* (possibly extended) descriptors.             */

STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_array_kind = 0;
                        /* Object kind for objects with complex         */
                        /* descriptors and MANAGED_STACK_ADDRESS_BOEHM_GC_array_mark_proc.          */

#define ED_INITIAL_SIZE 100

STATIC unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_typed_mark_proc_index = 0;   /* Indices of the typed */
STATIC unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_array_mark_proc_index = 0;   /* mark procedures.     */

STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_push_typed_structures_proc(void)
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ALL_SYM(MANAGED_STACK_ADDRESS_BOEHM_GC_ext_descriptors);
}

/* Add a multiword bitmap to MANAGED_STACK_ADDRESS_BOEHM_GC_ext_descriptors arrays.         */
/* Returns starting index on success, -1 otherwise.             */
STATIC signed_word MANAGED_STACK_ADDRESS_BOEHM_GC_add_ext_descriptor(const word * bm, word nbits)
{
    size_t nwords = divWORDSZ(nbits + CPP_WORDSZ-1);
    signed_word result;
    size_t i;

    LOCK();
    while (EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_avail_descr + nwords >= MANAGED_STACK_ADDRESS_BOEHM_GC_ed_size, FALSE)) {
        typed_ext_descr_t *newExtD;
        size_t new_size;
        word ed_size = MANAGED_STACK_ADDRESS_BOEHM_GC_ed_size;

        if (ed_size == 0) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((word)(&MANAGED_STACK_ADDRESS_BOEHM_GC_ext_descriptors) % sizeof(word) == 0);
            MANAGED_STACK_ADDRESS_BOEHM_GC_push_typed_structures = MANAGED_STACK_ADDRESS_BOEHM_GC_push_typed_structures_proc;
            UNLOCK();
            new_size = ED_INITIAL_SIZE;
        } else {
            UNLOCK();
            new_size = 2 * ed_size;
            if (new_size > MAX_ENV) return -1;
        }
        newExtD = (typed_ext_descr_t*)MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_atomic(new_size
                                                * sizeof(typed_ext_descr_t));
        if (NULL == newExtD)
            return -1;
        LOCK();
        if (ed_size == MANAGED_STACK_ADDRESS_BOEHM_GC_ed_size) {
            if (MANAGED_STACK_ADDRESS_BOEHM_GC_avail_descr != 0) {
                BCOPY(MANAGED_STACK_ADDRESS_BOEHM_GC_ext_descriptors, newExtD,
                      MANAGED_STACK_ADDRESS_BOEHM_GC_avail_descr * sizeof(typed_ext_descr_t));
            }
            MANAGED_STACK_ADDRESS_BOEHM_GC_ed_size = new_size;
            MANAGED_STACK_ADDRESS_BOEHM_GC_ext_descriptors = newExtD;
        }  /* else another thread already resized it in the meantime */
    }
    result = (signed_word)MANAGED_STACK_ADDRESS_BOEHM_GC_avail_descr;
    for (i = 0; i < nwords-1; i++) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_ext_descriptors[(size_t)result + i].ed_bitmap = bm[i];
        MANAGED_STACK_ADDRESS_BOEHM_GC_ext_descriptors[(size_t)result + i].ed_continued = TRUE;
    }
    /* Clear irrelevant (highest) bits for the last element.    */
    MANAGED_STACK_ADDRESS_BOEHM_GC_ext_descriptors[(size_t)result + i].ed_bitmap =
                bm[i] & (MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX >> (nwords * CPP_WORDSZ - nbits));
    MANAGED_STACK_ADDRESS_BOEHM_GC_ext_descriptors[(size_t)result + i].ed_continued = FALSE;
    MANAGED_STACK_ADDRESS_BOEHM_GC_avail_descr += nwords;
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(result >= 0);
    UNLOCK();
    return result;
}

/* Table of bitmap descriptors for n word long all pointer objects.     */
STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_descr MANAGED_STACK_ADDRESS_BOEHM_GC_bm_table[CPP_WORDSZ / 2];

/* Return a descriptor for the concatenation of 2 nwords long objects,  */
/* each of which is described by descriptor d.  The result is known     */
/* to be short enough to fit into a bitmap descriptor.                  */
/* d is a MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH or MANAGED_STACK_ADDRESS_BOEHM_GC_DS_BITMAP descriptor.                      */
STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_descr MANAGED_STACK_ADDRESS_BOEHM_GC_double_descr(MANAGED_STACK_ADDRESS_BOEHM_GC_descr d, size_t nwords)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_bm_table[0] == MANAGED_STACK_ADDRESS_BOEHM_GC_DS_BITMAP); /* bm table is initialized */
    if ((d & MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS) == MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH) {
        d = MANAGED_STACK_ADDRESS_BOEHM_GC_bm_table[BYTES_TO_WORDS((word)d)];
    }
    d |= (d & ~(MANAGED_STACK_ADDRESS_BOEHM_GC_descr)MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS) >> nwords;
    return d;
}

STATIC mse *MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK MANAGED_STACK_ADDRESS_BOEHM_GC_typed_mark_proc(word * addr, mse * mark_stack_ptr,
                                           mse * mark_stack_limit, word env);

STATIC mse *MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK MANAGED_STACK_ADDRESS_BOEHM_GC_array_mark_proc(word * addr, mse * mark_stack_ptr,
                                           mse * mark_stack_limit, word env);

STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_init_explicit_typing(void)
{
    unsigned i;

    /* Set up object kind with simple indirect descriptor.      */
    /* Descriptor is in the last word of the object.            */
    MANAGED_STACK_ADDRESS_BOEHM_GC_typed_mark_proc_index = MANAGED_STACK_ADDRESS_BOEHM_GC_new_proc_inner(MANAGED_STACK_ADDRESS_BOEHM_GC_typed_mark_proc);
    MANAGED_STACK_ADDRESS_BOEHM_GC_explicit_kind = (int)MANAGED_STACK_ADDRESS_BOEHM_GC_new_kind_inner(MANAGED_STACK_ADDRESS_BOEHM_GC_new_free_list_inner(),
                            (WORDS_TO_BYTES((word)-1) | MANAGED_STACK_ADDRESS_BOEHM_GC_DS_PER_OBJECT),
                            TRUE, TRUE);

    /* Set up object kind with array descriptor. */
    MANAGED_STACK_ADDRESS_BOEHM_GC_array_mark_proc_index = MANAGED_STACK_ADDRESS_BOEHM_GC_new_proc_inner(MANAGED_STACK_ADDRESS_BOEHM_GC_array_mark_proc);
    MANAGED_STACK_ADDRESS_BOEHM_GC_array_kind = (int)MANAGED_STACK_ADDRESS_BOEHM_GC_new_kind_inner(MANAGED_STACK_ADDRESS_BOEHM_GC_new_free_list_inner(),
                            MANAGED_STACK_ADDRESS_BOEHM_GC_MAKE_PROC(MANAGED_STACK_ADDRESS_BOEHM_GC_array_mark_proc_index, 0),
                            FALSE, TRUE);

    MANAGED_STACK_ADDRESS_BOEHM_GC_bm_table[0] = MANAGED_STACK_ADDRESS_BOEHM_GC_DS_BITMAP;
    for (i = 1; i < CPP_WORDSZ / 2; i++) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_bm_table[i] = (((word)-1) << (CPP_WORDSZ - i)) | MANAGED_STACK_ADDRESS_BOEHM_GC_DS_BITMAP;
    }
}

STATIC mse *MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK MANAGED_STACK_ADDRESS_BOEHM_GC_typed_mark_proc(word * addr, mse * mark_stack_ptr,
                                           mse * mark_stack_limit, word env)
{
    word bm;
    ptr_t current_p = (ptr_t)addr;
    ptr_t greatest_ha = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr;
    ptr_t least_ha = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr;
    DECLARE_HDR_CACHE;

    /* The allocation lock is held by the collection initiating thread. */
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_get_parallel() || I_HOLD_LOCK());
    bm = MANAGED_STACK_ADDRESS_BOEHM_GC_ext_descriptors[env].ed_bitmap;

    INIT_HDR_CACHE;
    for (; bm != 0; bm >>= 1, current_p += sizeof(word)) {
        if (bm & 1) {
            word current;

            LOAD_WORD_OR_CONTINUE(current, current_p);
            FIXUP_POINTER(current);
            if (current > (word)least_ha && current < (word)greatest_ha) {
                PUSH_CONTENTS((ptr_t)current, mark_stack_ptr,
                              mark_stack_limit, current_p);
            }
        }
    }
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_ext_descriptors[env].ed_continued) {
        /* Push an entry with the rest of the descriptor back onto the  */
        /* stack.  Thus we never do too much work at once.  Note that   */
        /* we also can't overflow the mark stack unless we actually     */
        /* mark something.                                              */
        mark_stack_ptr++;
        if ((word)mark_stack_ptr >= (word)mark_stack_limit) {
            mark_stack_ptr = MANAGED_STACK_ADDRESS_BOEHM_GC_signal_mark_stack_overflow(mark_stack_ptr);
        }
        mark_stack_ptr -> mse_start = (ptr_t)(addr + CPP_WORDSZ);
        mark_stack_ptr -> mse_descr.w =
                        MANAGED_STACK_ADDRESS_BOEHM_GC_MAKE_PROC(MANAGED_STACK_ADDRESS_BOEHM_GC_typed_mark_proc_index, env + 1);
    }
    return mark_stack_ptr;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_descr MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_make_descriptor(const MANAGED_STACK_ADDRESS_BOEHM_GC_word * bm, size_t len)
{
    signed_word last_set_bit = (signed_word)len - 1;
    MANAGED_STACK_ADDRESS_BOEHM_GC_descr d;

#   if defined(AO_HAVE_load_acquire) && defined(AO_HAVE_store_release)
      if (!EXPECT(AO_load_acquire(&MANAGED_STACK_ADDRESS_BOEHM_GC_explicit_typing_initialized), TRUE)) {
        LOCK();
        if (!MANAGED_STACK_ADDRESS_BOEHM_GC_explicit_typing_initialized) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_init_explicit_typing();
          AO_store_release(&MANAGED_STACK_ADDRESS_BOEHM_GC_explicit_typing_initialized, TRUE);
        }
        UNLOCK();
      }
#   else
      LOCK();
      if (!EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_explicit_typing_initialized, TRUE)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_init_explicit_typing();
        MANAGED_STACK_ADDRESS_BOEHM_GC_explicit_typing_initialized = TRUE;
      }
      UNLOCK();
#   endif

    while (last_set_bit >= 0 && !MANAGED_STACK_ADDRESS_BOEHM_GC_get_bit(bm, (word)last_set_bit))
      last_set_bit--;
    if (last_set_bit < 0) return 0; /* no pointers */

#   if ALIGNMENT == CPP_WORDSZ/8
    {
      signed_word i;

      for (i = 0; i < last_set_bit; i++) {
        if (!MANAGED_STACK_ADDRESS_BOEHM_GC_get_bit(bm, (word)i)) {
          break;
        }
      }
      if (i == last_set_bit) {
        /* An initial section contains all pointers.  Use length descriptor. */
        return WORDS_TO_BYTES((word)last_set_bit + 1) | MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH;
      }
    }
#   endif
    if (last_set_bit < BITMAP_BITS) {
        signed_word i;

        /* Hopefully the common case.                   */
        /* Build bitmap descriptor (with bits reversed) */
        d = SIGNB;
        for (i = last_set_bit - 1; i >= 0; i--) {
            d >>= 1;
            if (MANAGED_STACK_ADDRESS_BOEHM_GC_get_bit(bm, (word)i)) d |= SIGNB;
        }
        d |= MANAGED_STACK_ADDRESS_BOEHM_GC_DS_BITMAP;
    } else {
        signed_word index = MANAGED_STACK_ADDRESS_BOEHM_GC_add_ext_descriptor(bm, (word)last_set_bit + 1);

        if (EXPECT(index == -1, FALSE)) {
            /* Out of memory: use a conservative approximation. */
            return WORDS_TO_BYTES((word)last_set_bit + 1) | MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH;
        }
        d = MANAGED_STACK_ADDRESS_BOEHM_GC_MAKE_PROC(MANAGED_STACK_ADDRESS_BOEHM_GC_typed_mark_proc_index, index);
    }
    return d;
}

#ifdef AO_HAVE_store_release
# define set_obj_descr(op, nwords, d) \
        AO_store_release((volatile AO_t *)(op) + (nwords) - 1, (AO_t)(d))
#else
# define set_obj_descr(op, nwords, d) \
        (void)(((word *)(op))[(nwords) - 1] = (word)(d))
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_explicitly_typed(size_t lb,
                                                                MANAGED_STACK_ADDRESS_BOEHM_GC_descr d)
{
    void *op;
    size_t nwords;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_explicit_typing_initialized);
    if (EXPECT(0 == lb, FALSE)) lb = 1; /* ensure nwords > 1 */
    op = MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind(SIZET_SAT_ADD(lb, TYPD_EXTRA_BYTES), MANAGED_STACK_ADDRESS_BOEHM_GC_explicit_kind);
    if (EXPECT(NULL == op, FALSE)) return NULL;

    /* It is not safe to use MANAGED_STACK_ADDRESS_BOEHM_GC_size_map to compute nwords here as      */
    /* the former might be updated asynchronously.                      */
    nwords = GRANULES_TO_WORDS(BYTES_TO_GRANULES(MANAGED_STACK_ADDRESS_BOEHM_GC_size(op)));
    set_obj_descr(op, nwords, d);
    MANAGED_STACK_ADDRESS_BOEHM_GC_dirty((word *)op + nwords - 1);
    REACHABLE_AFTER_DIRTY(d);
    return op;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL
    MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_explicitly_typed_ignore_off_page(size_t lb, MANAGED_STACK_ADDRESS_BOEHM_GC_descr d)
{
    void *op;
    size_t nwords;

    if (lb < HBLKSIZE - sizeof(word))
      return MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_explicitly_typed(lb, d);

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_explicit_typing_initialized);
    /* TYPD_EXTRA_BYTES is not used here because ignore-off-page    */
    /* objects with the requested size of at least HBLKSIZE do not  */
    /* have EXTRA_BYTES added by MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_aligned().       */
    op = MANAGED_STACK_ADDRESS_BOEHM_GC_clear_stack(MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_aligned(
                                SIZET_SAT_ADD(lb, sizeof(word)),
                                MANAGED_STACK_ADDRESS_BOEHM_GC_explicit_kind, IGNORE_OFF_PAGE, 0));
    if (EXPECT(NULL == op, FALSE)) return NULL;

    nwords = GRANULES_TO_WORDS(BYTES_TO_GRANULES(MANAGED_STACK_ADDRESS_BOEHM_GC_size(op)));
    set_obj_descr(op, nwords, d);
    MANAGED_STACK_ADDRESS_BOEHM_GC_dirty((word *)op + nwords - 1);
    REACHABLE_AFTER_DIRTY(d);
    return op;
}

/* Array descriptors.  MANAGED_STACK_ADDRESS_BOEHM_GC_array_mark_proc understands these.    */
/* We may eventually need to add provisions for headers and     */
/* trailers.  Hence we provide for tree structured descriptors, */
/* though we don't really use them currently.                   */

struct LeafDescriptor {         /* Describes simple array.      */
  word ld_tag;
# define LEAF_TAG 1
  word ld_size;                 /* Bytes per element; non-zero, */
                                /* multiple of ALIGNMENT.       */
  word ld_nelements;            /* Number of elements.          */
  MANAGED_STACK_ADDRESS_BOEHM_GC_descr ld_descriptor;       /* A simple length, bitmap,     */
                                /* or procedure descriptor.     */
};

struct ComplexArrayDescriptor {
  word ad_tag;
# define ARRAY_TAG 2
  word ad_nelements;
  union ComplexDescriptor *ad_element_descr;
};

struct SequenceDescriptor {
  word sd_tag;
# define SEQUENCE_TAG 3
  union ComplexDescriptor *sd_first;
  union ComplexDescriptor *sd_second;
};

typedef union ComplexDescriptor {
  struct LeafDescriptor ld;
  struct ComplexArrayDescriptor ad;
  struct SequenceDescriptor sd;
} complex_descriptor;

STATIC complex_descriptor *MANAGED_STACK_ADDRESS_BOEHM_GC_make_leaf_descriptor(word size, word nelements,
                                                   MANAGED_STACK_ADDRESS_BOEHM_GC_descr d)
{
  complex_descriptor *result = (complex_descriptor *)
                MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_atomic(sizeof(struct LeafDescriptor));

  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(size != 0);
  if (EXPECT(NULL == result, FALSE)) return NULL;

  result -> ld.ld_tag = LEAF_TAG;
  result -> ld.ld_size = size;
  result -> ld.ld_nelements = nelements;
  result -> ld.ld_descriptor = d;
  return result;
}

STATIC complex_descriptor *MANAGED_STACK_ADDRESS_BOEHM_GC_make_sequence_descriptor(
                                                complex_descriptor *first,
                                                complex_descriptor *second)
{
  struct SequenceDescriptor *result = (struct SequenceDescriptor *)
                MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(sizeof(struct SequenceDescriptor));
                /* Note: for a reason, the sanitizer runtime complains  */
                /* of insufficient space for complex_descriptor if the  */
                /* pointer type of result variable is changed to.       */

  if (EXPECT(NULL == result, FALSE)) return NULL;

  /* Can't result in overly conservative marking, since tags are        */
  /* very small integers. Probably faster than maintaining type info.   */
  result -> sd_tag = SEQUENCE_TAG;
  result -> sd_first = first;
  result -> sd_second = second;
  MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(result);
  REACHABLE_AFTER_DIRTY(first);
  REACHABLE_AFTER_DIRTY(second);
  return (complex_descriptor *)result;
}

#define NO_MEM  (-1)
#define SIMPLE  0
#define LEAF    1
#define COMPLEX 2

/* Build a descriptor for an array with nelements elements, each of     */
/* which can be described by a simple descriptor d.  We try to optimize */
/* some common cases.  If the result is COMPLEX, a complex_descriptor*  */
/* value is returned in *pcomplex_d.  If the result is LEAF, then a     */
/* LeafDescriptor value is built in the structure pointed to by pleaf.  */
/* The tag in the *pleaf structure is not set.  If the result is        */
/* SIMPLE, then a MANAGED_STACK_ADDRESS_BOEHM_GC_descr value is returned in *psimple_d.  If the     */
/* result is NO_MEM, then we failed to allocate the descriptor.         */
/* The implementation assumes MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH is 0.  *pleaf, *pcomplex_d   */
/* and *psimple_d may be used as temporaries during the construction.   */
STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_make_array_descriptor(size_t nelements, size_t size,
                                    MANAGED_STACK_ADDRESS_BOEHM_GC_descr d, MANAGED_STACK_ADDRESS_BOEHM_GC_descr *psimple_d,
                                    complex_descriptor **pcomplex_d,
                                    struct LeafDescriptor *pleaf)
{
# define OPT_THRESHOLD 50
        /* For larger arrays, we try to combine descriptors of adjacent */
        /* descriptors to speed up marking, and to reduce the amount    */
        /* of space needed on the mark stack.                           */

  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(size != 0);
  if ((d & MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS) == MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH) {
    if (d == (MANAGED_STACK_ADDRESS_BOEHM_GC_descr)size) {
      *psimple_d = nelements * d; /* no overflow guaranteed by caller */
      return SIMPLE;
    } else if (0 == d) {
      *psimple_d = 0;
      return SIMPLE;
    }
  }

  if (nelements <= OPT_THRESHOLD) {
    if (nelements <= 1) {
      *psimple_d = nelements == 1 ? d : 0;
      return SIMPLE;
    }
  } else if (size <= BITMAP_BITS/2
             && (d & MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS) != MANAGED_STACK_ADDRESS_BOEHM_GC_DS_PROC
             && (size & (sizeof(word)-1)) == 0) {
    complex_descriptor *one_element, *beginning;
    int result = MANAGED_STACK_ADDRESS_BOEHM_GC_make_array_descriptor(nelements / 2, 2 * size,
                                MANAGED_STACK_ADDRESS_BOEHM_GC_double_descr(d, BYTES_TO_WORDS(size)),
                                psimple_d, pcomplex_d, pleaf);

    if ((nelements & 1) == 0 || EXPECT(NO_MEM == result, FALSE))
      return result;

    one_element = MANAGED_STACK_ADDRESS_BOEHM_GC_make_leaf_descriptor(size, 1, d);
    if (EXPECT(NULL == one_element, FALSE)) return NO_MEM;

    if (COMPLEX == result) {
      beginning = *pcomplex_d;
    } else {
      beginning = SIMPLE == result ?
                        MANAGED_STACK_ADDRESS_BOEHM_GC_make_leaf_descriptor(size, 1, *psimple_d) :
                        MANAGED_STACK_ADDRESS_BOEHM_GC_make_leaf_descriptor(pleaf -> ld_size,
                                                pleaf -> ld_nelements,
                                                pleaf -> ld_descriptor);
      if (EXPECT(NULL == beginning, FALSE)) return NO_MEM;
    }
    *pcomplex_d = MANAGED_STACK_ADDRESS_BOEHM_GC_make_sequence_descriptor(beginning, one_element);
    if (EXPECT(NULL == *pcomplex_d, FALSE)) return NO_MEM;

    return COMPLEX;
  }

  pleaf -> ld_size = size;
  pleaf -> ld_nelements = nelements;
  pleaf -> ld_descriptor = d;
  return LEAF;
}

struct MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_typed_descr_s {
  struct LeafDescriptor leaf;
  MANAGED_STACK_ADDRESS_BOEHM_GC_descr simple_d;
  complex_descriptor *complex_d;
  word alloc_lb; /* size_t actually */
  signed_word descr_type; /* int actually */
};

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_prepare_explicitly_typed(
                                struct MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_typed_descr_s *pctd,
                                size_t ctd_sz,
                                size_t n, size_t lb, MANAGED_STACK_ADDRESS_BOEHM_GC_descr d)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_STATIC_ASSERT(sizeof(struct LeafDescriptor) % sizeof(word) == 0);
    MANAGED_STACK_ADDRESS_BOEHM_GC_STATIC_ASSERT(sizeof(struct MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_typed_descr_s)
                        == MANAGED_STACK_ADDRESS_BOEHM_GC_CALLOC_TYPED_DESCR_WORDS * sizeof(word));
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_explicit_typing_initialized);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(sizeof(struct MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_typed_descr_s) == ctd_sz);
    (void)ctd_sz; /* unused currently */
    if (EXPECT(0 == lb || 0 == n, FALSE)) lb = n = 1;
    if (EXPECT((lb | n) > MANAGED_STACK_ADDRESS_BOEHM_GC_SQRT_SIZE_MAX, FALSE) /* fast initial check */
        && n > MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_MAX / lb) {
      pctd -> alloc_lb = MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_MAX; /* n*lb overflow */
      pctd -> descr_type = NO_MEM;
      /* The rest of the fields are unset. */
      return 0; /* failure */
    }

    pctd -> descr_type = MANAGED_STACK_ADDRESS_BOEHM_GC_make_array_descriptor((word)n, (word)lb, d,
                                &(pctd -> simple_d), &(pctd -> complex_d),
                                &(pctd -> leaf));
    switch (pctd -> descr_type) {
    case NO_MEM:
    case SIMPLE:
      pctd -> alloc_lb = (word)lb * n;
      break;
    case LEAF:
      pctd -> alloc_lb = (word)SIZET_SAT_ADD(lb * n,
                        sizeof(struct LeafDescriptor) + TYPD_EXTRA_BYTES);
      break;
    case COMPLEX:
      pctd -> alloc_lb = (word)SIZET_SAT_ADD(lb * n, TYPD_EXTRA_BYTES);
      break;
    }
    return 1; /* success */
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_do_explicitly_typed(
                                const struct MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_typed_descr_s *pctd,
                                size_t ctd_sz)
{
    void *op;
    size_t nwords;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(sizeof(struct MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_typed_descr_s) == ctd_sz);
    (void)ctd_sz; /* unused currently */
    switch (pctd -> descr_type) {
    case NO_MEM:
      return (*MANAGED_STACK_ADDRESS_BOEHM_GC_get_oom_fn())((size_t)(pctd -> alloc_lb));
    case SIMPLE:
      return MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_explicitly_typed((size_t)(pctd -> alloc_lb),
                                        pctd -> simple_d);
    case LEAF:
    case COMPLEX:
      break;
    default:
      ABORT_RET("Bad descriptor type");
      return NULL;
    }
    op = MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind((size_t)(pctd -> alloc_lb), MANAGED_STACK_ADDRESS_BOEHM_GC_array_kind);
    if (EXPECT(NULL == op, FALSE))
      return NULL;

    nwords = GRANULES_TO_WORDS(BYTES_TO_GRANULES(MANAGED_STACK_ADDRESS_BOEHM_GC_size(op)));
    if (pctd -> descr_type == LEAF) {
      /* Set up the descriptor inside the object itself.        */
      struct LeafDescriptor *lp =
                (struct LeafDescriptor *)((word *)op + nwords -
                        (BYTES_TO_WORDS(sizeof(struct LeafDescriptor)) + 1));

      lp -> ld_tag = LEAF_TAG;
      lp -> ld_size = pctd -> leaf.ld_size;
      lp -> ld_nelements = pctd -> leaf.ld_nelements;
      lp -> ld_descriptor = pctd -> leaf.ld_descriptor;
      /* Hold the allocation lock while writing the descriptor word     */
      /* to the object to ensure that the descriptor contents are seen  */
      /* by MANAGED_STACK_ADDRESS_BOEHM_GC_array_mark_proc as expected.                             */
      /* TODO: It should be possible to replace locking with the atomic */
      /* operations (with the release barrier here) but, in this case,  */
      /* avoiding the acquire barrier in MANAGED_STACK_ADDRESS_BOEHM_GC_array_mark_proc seems to    */
      /* be tricky as MANAGED_STACK_ADDRESS_BOEHM_GC_mark_some might be invoked with the world      */
      /* running.                                                       */
      LOCK();
      ((word *)op)[nwords - 1] = (word)lp;
      UNLOCK();
    } else {
#     ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
        LOCK();
        ((word *)op)[nwords - 1] = (word)(pctd -> complex_d);
        UNLOCK();

        MANAGED_STACK_ADDRESS_BOEHM_GC_dirty((word *)op + nwords - 1);
        REACHABLE_AFTER_DIRTY(pctd -> complex_d);

        /* Make sure the descriptor is cleared once there is any danger */
        /* it may have been collected.                                  */
        if (EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_general_register_disappearing_link(
                        (void **)op + nwords - 1, op) == MANAGED_STACK_ADDRESS_BOEHM_GC_NO_MEMORY, FALSE))
#     endif
        {
            /* Couldn't register it due to lack of memory.  Punt.       */
            return (*MANAGED_STACK_ADDRESS_BOEHM_GC_get_oom_fn())((size_t)(pctd -> alloc_lb));
        }
    }
    return op;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_explicitly_typed(size_t n,
                                                                size_t lb,
                                                                MANAGED_STACK_ADDRESS_BOEHM_GC_descr d)
{
  struct MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_typed_descr_s ctd;

  (void)MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_prepare_explicitly_typed(&ctd, sizeof(ctd), n, lb, d);
  return MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_do_explicitly_typed(&ctd, sizeof(ctd));
}

/* Return the size of the object described by complex_d.  It would be   */
/* faster to store this directly, or to compute it as part of           */
/* MANAGED_STACK_ADDRESS_BOEHM_GC_push_complex_descriptor, but hopefully it does not matter.        */
STATIC word MANAGED_STACK_ADDRESS_BOEHM_GC_descr_obj_size(complex_descriptor *complex_d)
{
  switch(complex_d -> ad.ad_tag) {
  case LEAF_TAG:
    return complex_d -> ld.ld_nelements * complex_d -> ld.ld_size;
  case ARRAY_TAG:
    return complex_d -> ad.ad_nelements
           * MANAGED_STACK_ADDRESS_BOEHM_GC_descr_obj_size(complex_d -> ad.ad_element_descr);
  case SEQUENCE_TAG:
    return MANAGED_STACK_ADDRESS_BOEHM_GC_descr_obj_size(complex_d -> sd.sd_first)
           + MANAGED_STACK_ADDRESS_BOEHM_GC_descr_obj_size(complex_d -> sd.sd_second);
  default:
    ABORT_RET("Bad complex descriptor");
    return 0;
  }
}

/* Push descriptors for the object at addr with complex descriptor      */
/* onto the mark stack.  Return NULL if the mark stack overflowed.      */
STATIC mse *MANAGED_STACK_ADDRESS_BOEHM_GC_push_complex_descriptor(word *addr,
                                       complex_descriptor *complex_d,
                                       mse *msp, mse *msl)
{
  ptr_t current = (ptr_t)addr;
  word nelements;
  word sz;
  word i;
  MANAGED_STACK_ADDRESS_BOEHM_GC_descr d;
  complex_descriptor *element_descr;

  switch(complex_d -> ad.ad_tag) {
  case LEAF_TAG:
    d = complex_d -> ld.ld_descriptor;
    nelements = complex_d -> ld.ld_nelements;
    sz = complex_d -> ld.ld_size;

    if (EXPECT(msl - msp <= (signed_word)nelements, FALSE)) return NULL;
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(sz != 0);
    for (i = 0; i < nelements; i++) {
      msp++;
      msp -> mse_start = current;
      msp -> mse_descr.w = d;
      current += sz;
    }
    break;
  case ARRAY_TAG:
    element_descr = complex_d -> ad.ad_element_descr;
    nelements = complex_d -> ad.ad_nelements;
    sz = MANAGED_STACK_ADDRESS_BOEHM_GC_descr_obj_size(element_descr);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(sz != 0 || 0 == nelements);
    for (i = 0; i < nelements; i++) {
      msp = MANAGED_STACK_ADDRESS_BOEHM_GC_push_complex_descriptor((word *)current, element_descr,
                                       msp, msl);
      if (EXPECT(NULL == msp, FALSE)) return NULL;
      current += sz;
    }
    break;
  case SEQUENCE_TAG:
    sz = MANAGED_STACK_ADDRESS_BOEHM_GC_descr_obj_size(complex_d -> sd.sd_first);
    msp = MANAGED_STACK_ADDRESS_BOEHM_GC_push_complex_descriptor((word *)current,
                                     complex_d -> sd.sd_first, msp, msl);
    if (EXPECT(NULL == msp, FALSE)) return NULL;
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(sz != 0);
    current += sz;
    msp = MANAGED_STACK_ADDRESS_BOEHM_GC_push_complex_descriptor((word *)current,
                                     complex_d -> sd.sd_second, msp, msl);
    break;
  default:
    ABORT("Bad complex descriptor");
  }
  return msp;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD
static complex_descriptor *get_complex_descr(word *addr, size_t nwords)
{
  return (complex_descriptor *)addr[nwords - 1];
}

/* Used by MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_do_explicitly_typed via MANAGED_STACK_ADDRESS_BOEHM_GC_array_kind.     */
STATIC mse *MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK MANAGED_STACK_ADDRESS_BOEHM_GC_array_mark_proc(word *addr, mse *mark_stack_ptr,
                                           mse *mark_stack_limit, word env)
{
  hdr *hhdr = HDR(addr);
  word sz = hhdr -> hb_sz;
  size_t nwords = (size_t)BYTES_TO_WORDS(sz);
  complex_descriptor *complex_d = get_complex_descr(addr, nwords);
  mse *orig_mark_stack_ptr = mark_stack_ptr;
  mse *new_mark_stack_ptr;

  UNUSED_ARG(env);
  if (NULL == complex_d) {
    /* Found a reference to a free list entry.  Ignore it. */
    return orig_mark_stack_ptr;
  }
  /* In use counts were already updated when array descriptor was       */
  /* pushed.  Here we only replace it by subobject descriptors, so      */
  /* no update is necessary.                                            */
  new_mark_stack_ptr = MANAGED_STACK_ADDRESS_BOEHM_GC_push_complex_descriptor(addr, complex_d,
                                                  mark_stack_ptr,
                                                  mark_stack_limit-1);
  if (new_mark_stack_ptr == 0) {
    /* Explicitly instruct Clang Static Analyzer that ptr is non-null.  */
    if (NULL == mark_stack_ptr) ABORT("Bad mark_stack_ptr");

    /* Does not fit.  Conservatively push the whole array as a unit and */
    /* request a mark stack expansion.  This cannot cause a mark stack  */
    /* overflow, since it replaces the original array entry.            */
#   ifdef PARALLEL_MARK
      /* We might be using a local_mark_stack in parallel mode. */
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack + MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_size == mark_stack_limit)
#   endif
    {
      MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_too_small = TRUE;
    }
    new_mark_stack_ptr = orig_mark_stack_ptr + 1;
    new_mark_stack_ptr -> mse_start = (ptr_t)addr;
    new_mark_stack_ptr -> mse_descr.w = sz | MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH;
  } else {
    /* Push descriptor itself.  */
    new_mark_stack_ptr++;
    new_mark_stack_ptr -> mse_start = (ptr_t)(addr + nwords - 1);
    new_mark_stack_ptr -> mse_descr.w = sizeof(word) | MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH;
  }
  return new_mark_stack_ptr;
}
