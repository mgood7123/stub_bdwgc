/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 * Copyright 1996 Silicon Graphics.  All rights reserved.
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

/*
 * Some simple primitives for allocation with explicit type information.
 * Facilities for dynamic type inference may be added later.
 * Should be used only for extremely performance critical applications,
 * or if conservative collector leakage is otherwise a problem (unlikely).
 * Note that this is implemented completely separately from the rest
 * of the collector, and is not linked in unless referenced.
 * This does not currently support MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG in any interesting way.
 */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_TYPED_H
#define MANAGED_STACK_ADDRESS_BOEHM_GC_TYPED_H

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_H
# include "gc.h"
#endif

#ifdef __cplusplus
  extern "C" {
#endif

typedef MANAGED_STACK_ADDRESS_BOEHM_GC_word * MANAGED_STACK_ADDRESS_BOEHM_GC_bitmap;
        /* The least significant bit of the first word is one if        */
        /* the first word in the object may be a pointer.               */

#define MANAGED_STACK_ADDRESS_BOEHM_GC_WORDSZ (8 * sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_word))
#define MANAGED_STACK_ADDRESS_BOEHM_GC_get_bit(bm, index) /* index should be of unsigned type */ \
            (((bm)[(index) / MANAGED_STACK_ADDRESS_BOEHM_GC_WORDSZ] >> ((index) % MANAGED_STACK_ADDRESS_BOEHM_GC_WORDSZ)) & 1)
#define MANAGED_STACK_ADDRESS_BOEHM_GC_set_bit(bm, index) /* index should be of unsigned type */ \
            ((bm)[(index) / MANAGED_STACK_ADDRESS_BOEHM_GC_WORDSZ] |= (MANAGED_STACK_ADDRESS_BOEHM_GC_word)1 << ((index) % MANAGED_STACK_ADDRESS_BOEHM_GC_WORDSZ))
#define MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_OFFSET(t, f) (offsetof(t,f) / sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_word))
#define MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_LEN(t) (sizeof(t) / sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_word))
#define MANAGED_STACK_ADDRESS_BOEHM_GC_BITMAP_SIZE(t) ((MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_LEN(t) + MANAGED_STACK_ADDRESS_BOEHM_GC_WORDSZ - 1) / MANAGED_STACK_ADDRESS_BOEHM_GC_WORDSZ)

typedef MANAGED_STACK_ADDRESS_BOEHM_GC_word MANAGED_STACK_ADDRESS_BOEHM_GC_descr;

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_descr MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_make_descriptor(const MANAGED_STACK_ADDRESS_BOEHM_GC_word * /* MANAGED_STACK_ADDRESS_BOEHM_GC_bitmap bm */,
                                size_t /* len (number_of_bits_in_bitmap) */);
                /* Return a type descriptor for the object whose layout */
                /* is described by the argument.                        */
                /* The least significant bit of the first word is one   */
                /* if the first word in the object may be a pointer.    */
                /* The second argument specifies the number of          */
                /* meaningful bits in the bitmap.  The actual object    */
                /* may be larger (but not smaller).  Any additional     */
                /* words in the object are assumed not to contain       */
                /* pointers.                                            */
                /* Returns a conservative approximation in the          */
                /* (unlikely) case of insufficient memory to build      */
                /* the descriptor.  Calls to MANAGED_STACK_ADDRESS_BOEHM_GC_make_descriptor         */
                /* may consume some amount of a finite resource.  This  */
                /* is intended to be called once per type, not once     */
                /* per allocation.                                      */

/* It is possible to generate a descriptor for a C type T with  */
/* word aligned pointer fields f1, f2, ... as follows:                  */
/*                                                                      */
/* MANAGED_STACK_ADDRESS_BOEHM_GC_descr T_descr;                                                    */
/* MANAGED_STACK_ADDRESS_BOEHM_GC_word T_bitmap[MANAGED_STACK_ADDRESS_BOEHM_GC_BITMAP_SIZE(T)] = {0};                           */
/* MANAGED_STACK_ADDRESS_BOEHM_GC_set_bit(T_bitmap, MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_OFFSET(T,f1));                          */
/* MANAGED_STACK_ADDRESS_BOEHM_GC_set_bit(T_bitmap, MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_OFFSET(T,f2));                          */
/* ...                                                                  */
/* T_descr = MANAGED_STACK_ADDRESS_BOEHM_GC_make_descriptor(T_bitmap, MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_LEN(T));              */

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_ALLOC_SIZE(1) void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL
        MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_explicitly_typed(size_t /* size_in_bytes */,
                                   MANAGED_STACK_ADDRESS_BOEHM_GC_descr /* d */);
                /* Allocate an object whose layout is described by d.   */
                /* The size may NOT be less than the number of          */
                /* meaningful bits in the bitmap of d multiplied by     */
                /* sizeof MANAGED_STACK_ADDRESS_BOEHM_GC_word.  The returned object is cleared.     */
                /* The returned object may NOT be passed to MANAGED_STACK_ADDRESS_BOEHM_GC_realloc. */

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_ALLOC_SIZE(1) void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL
        MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_explicitly_typed_ignore_off_page(size_t /* size_in_bytes */,
                                                   MANAGED_STACK_ADDRESS_BOEHM_GC_descr /* d */);

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_CALLOC_SIZE(1, 2) void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL
        MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_explicitly_typed(size_t /* nelements */,
                                   size_t /* element_size_in_bytes */,
                                   MANAGED_STACK_ADDRESS_BOEHM_GC_descr /* d */);
        /* Allocate an array of nelements elements, each of the */
        /* given size, and with the given descriptor.           */
        /* The element size must be a multiple of the byte      */
        /* alignment required for pointers.  E.g. on a 32-bit   */
        /* machine with 16-bit aligned pointers, size_in_bytes  */
        /* must be a multiple of 2.  The element size may NOT   */
        /* be less than the number of meaningful bits in the    */
        /* bitmap of d multiplied by sizeof MANAGED_STACK_ADDRESS_BOEHM_GC_word.            */
        /* Returned object is cleared.                          */

#define MANAGED_STACK_ADDRESS_BOEHM_GC_CALLOC_TYPED_DESCR_WORDS 8

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_BUILD
  struct MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_typed_descr_s;
#else
  struct MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_typed_descr_s {
    MANAGED_STACK_ADDRESS_BOEHM_GC_word opaque[MANAGED_STACK_ADDRESS_BOEHM_GC_CALLOC_TYPED_DESCR_WORDS];
  };
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_prepare_explicitly_typed(
                        struct MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_typed_descr_s * /* pctd */,
                        size_t /* sizeof_ctd */, size_t /* nelements */,
                        size_t /* element_size_in_bytes */, MANAGED_STACK_ADDRESS_BOEHM_GC_descr);
        /* This is same as MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_explicitly_typed but more optimal  */
        /* in terms of the performance and memory usage if the client   */
        /* needs to allocate multiple typed object arrays with the      */
        /* same layout and number of elements.  The client should call  */
        /* it to be prepared for the subsequent allocations by          */
        /* MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_do_explicitly_typed, one or many.  The result of   */
        /* the preparation is stored to *pctd, even in case of          */
        /* a failure.  The prepared structure could be just dropped     */
        /* when no longer needed.  Returns 0 on failure, 1 on success;  */
        /* the result could be ignored (as it is also stored to *pctd   */
        /* and checked later by MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_do_explicitly_typed).         */

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_do_explicitly_typed(
                        const struct MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_typed_descr_s * /* pctd */,
                        size_t /* sizeof_ctd */);
        /* The actual object allocation for the prepared description.   */

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG
# define MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_EXPLICITLY_TYPED(bytes, d) ((void)(d), MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(bytes))
# define MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_EXPLICITLY_TYPED_IGNORE_OFF_PAGE(bytes, d) \
                        MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_EXPLICITLY_TYPED(bytes, d)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_CALLOC_EXPLICITLY_TYPED(n, bytes, d) \
                        ((void)(d), MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC((n) * (bytes)))
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_EXPLICITLY_TYPED(bytes, d) \
                        MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_explicitly_typed(bytes, d)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_EXPLICITLY_TYPED_IGNORE_OFF_PAGE(bytes, d) \
                        MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_explicitly_typed_ignore_off_page(bytes, d)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_CALLOC_EXPLICITLY_TYPED(n, bytes, d) \
                        MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_explicitly_typed(n, bytes, d)
#endif

#ifdef __cplusplus
  } /* extern "C" */
#endif

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_TYPED_H */
