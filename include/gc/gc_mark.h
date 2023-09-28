/*
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 2001 by Hewlett-Packard Company. All rights reserved.
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
 *
 */

/*
 * This contains interfaces to the GC marker that are likely to be useful to
 * clients that provide detailed heap layout information to the collector.
 * This interface should not be used by normal C or C++ clients.
 * It will be useful to runtimes for other languages.
 *
 * This is an experts-only interface!  There are many ways to break the
 * collector in subtle ways by using this functionality.
 */
#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_MARK_H
#define MANAGED_STACK_ADDRESS_BOEHM_GC_MARK_H

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_H
# include "gc.h"
#endif

#ifdef __cplusplus
  extern "C" {
#endif

#define MANAGED_STACK_ADDRESS_BOEHM_GC_PROC_BYTES 100

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_BUILD) || defined(NOT_GCBUILD)
  struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry;
  struct MANAGED_STACK_ADDRESS_BOEHM_GC_hblk_s;
#else
  struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry { void *opaque; };
  struct MANAGED_STACK_ADDRESS_BOEHM_GC_hblk_s { void *opaque; };
#endif

/* A client supplied mark procedure.  Returns new mark stack pointer.   */
/* Primary effect should be to push new entries on the mark stack.      */
/* Mark stack pointer values are passed and returned explicitly.        */
/* Global variables describing mark stack are not necessarily valid.    */
/* (This usually saves a few cycles by keeping things in registers.)    */
/* Assumed to scan about MANAGED_STACK_ADDRESS_BOEHM_GC_PROC_BYTES on average.  If it needs to do   */
/* much more work than that, it should do it in smaller pieces by       */
/* pushing itself back on the mark stack.                               */
/* Note that it should always do some work (defined as marking some     */
/* objects) before pushing more than one entry on the mark stack.       */
/* This is required to ensure termination in the event of mark stack    */
/* overflows.                                                           */
/* This procedure is always called with at least one empty entry on the */
/* mark stack.                                                          */
/* Currently we require that mark procedures look for pointers in a     */
/* subset of the places the conservative marker would.  It must be safe */
/* to invoke the normal mark procedure instead.                         */
/* WARNING: Such a mark procedure may be invoked on an unused object    */
/* residing on a free list.  Such objects are cleared, except for a     */
/* free list link field in the first word.  Thus mark procedures may    */
/* not count on the presence of a type descriptor, and must handle this */
/* case correctly somehow.  Also, a mark procedure should be prepared   */
/* to be executed concurrently from the marker threads (the later ones  */
/* are created only if the client has called MANAGED_STACK_ADDRESS_BOEHM_GC_start_mark_threads()    */
/* or started a user thread previously).                                */
typedef struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry * (MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK * MANAGED_STACK_ADDRESS_BOEHM_GC_mark_proc)(MANAGED_STACK_ADDRESS_BOEHM_GC_word * /* addr */,
                                struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry * /* mark_stack_ptr */,
                                struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry * /* mark_stack_limit */,
                                MANAGED_STACK_ADDRESS_BOEHM_GC_word /* env */);

#define MANAGED_STACK_ADDRESS_BOEHM_GC_LOG_MAX_MARK_PROCS 6
#define MANAGED_STACK_ADDRESS_BOEHM_GC_MAX_MARK_PROCS (1 << MANAGED_STACK_ADDRESS_BOEHM_GC_LOG_MAX_MARK_PROCS)

/* In a few cases it's necessary to assign statically known indices to  */
/* certain mark procs.  Thus we reserve a few for well known clients.   */
/* (This is necessary if mark descriptors are compiler generated.)      */
#define MANAGED_STACK_ADDRESS_BOEHM_GC_RESERVED_MARK_PROCS 8
#define MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_RESERVED_MARK_PROC_INDEX 0

/* Object descriptors on mark stack or in objects.  Low order two       */
/* bits are tags distinguishing among the following 4 possibilities     */
/* for the rest (high order) bits.                                      */
#define MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAG_BITS 2
#define MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS   ((1U << MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAG_BITS) - 1)
#define MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH 0  /* The entire word is a length in bytes that    */
                        /* must be a multiple of 4.                     */
#define MANAGED_STACK_ADDRESS_BOEHM_GC_DS_BITMAP 1  /* The high order bits are describing pointer   */
                        /* fields.  The most significant bit is set if  */
                        /* the first word is a pointer.                 */
                        /* (This unconventional ordering sometimes      */
                        /* makes the marker slightly faster.)           */
                        /* Zeroes indicate definite nonpointers.  Ones  */
                        /* indicate possible pointers.                  */
                        /* Only usable if pointers are word aligned.    */
#define MANAGED_STACK_ADDRESS_BOEHM_GC_DS_PROC   2
                        /* The objects referenced by this object can be */
                        /* pushed on the mark stack by invoking         */
                        /* PROC(descr).  ENV(descr) is passed as the    */
                        /* last argument.                               */
#define MANAGED_STACK_ADDRESS_BOEHM_GC_MAKE_PROC(proc_index, env) \
            ((((((MANAGED_STACK_ADDRESS_BOEHM_GC_word)(env)) << MANAGED_STACK_ADDRESS_BOEHM_GC_LOG_MAX_MARK_PROCS) \
               | (unsigned)(proc_index)) << MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAG_BITS) \
             | (MANAGED_STACK_ADDRESS_BOEHM_GC_word)MANAGED_STACK_ADDRESS_BOEHM_GC_DS_PROC)
#define MANAGED_STACK_ADDRESS_BOEHM_GC_DS_PER_OBJECT 3  /* The real descriptor is at the            */
                        /* byte displacement from the beginning of the  */
                        /* object given by descr & ~MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS.         */
                        /* If the descriptor is negative, the real      */
                        /* descriptor is at (*<object_start>) -         */
                        /* (descr&~MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS) - MANAGED_STACK_ADDRESS_BOEHM_GC_INDIR_PER_OBJ_BIAS  */
                        /* The latter alternative can be used if each   */
                        /* object contains a type descriptor in the     */
                        /* first word.                                  */
                        /* Note that in the multi-threaded environments */
                        /* per-object descriptors must be located in    */
                        /* either the first two or last two words of    */
                        /* the object, since only those are guaranteed  */
                        /* to be cleared while the allocation lock is   */
                        /* held.                                        */
#define MANAGED_STACK_ADDRESS_BOEHM_GC_INDIR_PER_OBJ_BIAS 0x10

MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr;
MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr;
                        /* Bounds on the heap.  Guaranteed to be valid. */
                        /* Likely to include future heap expansion.     */
                        /* Hence usually includes not-yet-mapped        */
                        /* memory, or might overlap with other data     */
                        /* roots.  The address of any heap object is    */
                        /* larger than MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr and */
                        /* less than MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr.   */

/* Handle nested references in a custom mark procedure.                 */
/* Check if obj is a valid object. If so, ensure that it is marked.     */
/* If it was not previously marked, push its contents onto the mark     */
/* stack for future scanning.  The object will then be scanned using    */
/* its mark descriptor.                                                 */
/* Returns the new mark stack pointer.                                  */
/* Handles mark stack overflows correctly.                              */
/* Since this marks first, it makes progress even if there are mark     */
/* stack overflows.                                                     */
/* Src is the address of the pointer to obj, which is used only         */
/* for back pointer-based heap debugging.                               */
/* It is strongly recommended that most objects be handled without mark */
/* procedures, e.g. with bitmap descriptors, and that mark procedures   */
/* be reserved for exceptional cases.  That will ensure that            */
/* performance of this call is not extremely performance critical.      */
/* (Otherwise we would need to inline MANAGED_STACK_ADDRESS_BOEHM_GC_mark_and_push completely,      */
/* which would tie the client code to a fixed collector version.)       */
/* Note that mark procedures should explicitly call FIXUP_POINTER()     */
/* if required.                                                         */
MANAGED_STACK_ADDRESS_BOEHM_GC_API struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_mark_and_push(void * /* obj */,
                                struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry * /* mark_stack_ptr */,
                                struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry * /* mark_stack_limit */,
                                void ** /* src */);

#define MANAGED_STACK_ADDRESS_BOEHM_GC_MARK_AND_PUSH(obj, msp, lim, src) \
          ((MANAGED_STACK_ADDRESS_BOEHM_GC_word)(obj) > (MANAGED_STACK_ADDRESS_BOEHM_GC_word)MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr \
           && (MANAGED_STACK_ADDRESS_BOEHM_GC_word)(obj) < (MANAGED_STACK_ADDRESS_BOEHM_GC_word)MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr ? \
           MANAGED_STACK_ADDRESS_BOEHM_GC_mark_and_push(obj, msp, lim, src) : (msp))

/* The size of the header added to objects allocated through the        */
/* MANAGED_STACK_ADDRESS_BOEHM_GC_debug routines.  Defined as a function so that client mark        */
/* procedures do not need to be recompiled for the collector library    */
/* version changes.                                                     */
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_CONST size_t MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_debug_header_size(void);
#define MANAGED_STACK_ADDRESS_BOEHM_GC_USR_PTR_FROM_BASE(p) \
                ((void *)((char *)(p) + MANAGED_STACK_ADDRESS_BOEHM_GC_get_debug_header_size()))

/* The same but defined as a variable.  Exists only for the backward    */
/* compatibility.  Some compilers do not accept "const" together with   */
/* deprecated or dllimport attributes, so the symbol is exported as     */
/* a non-constant one.                                                  */
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_DEPRECATED
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_BUILD
    const
# endif
  size_t MANAGED_STACK_ADDRESS_BOEHM_GC_debug_header_size;

/* Return the heap block size.  Each heap block is devoted to a single  */
/* size and kind of object.                                             */
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_CONST size_t MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_hblk_size(void);

/* Same as MANAGED_STACK_ADDRESS_BOEHM_GC_walk_hblk_fn but with index of the free list.             */
typedef void (MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK * MANAGED_STACK_ADDRESS_BOEHM_GC_walk_free_blk_fn)(struct MANAGED_STACK_ADDRESS_BOEHM_GC_hblk_s *,
                                                 int /* index */,
                                                 MANAGED_STACK_ADDRESS_BOEHM_GC_word /* client_data */);

/* Apply fn to each completely empty heap block.  It is the             */
/* responsibility of the caller to avoid data race during the function  */
/* execution (e.g. by holding the allocation lock).                     */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_iterate_free_hblks(MANAGED_STACK_ADDRESS_BOEHM_GC_walk_free_blk_fn,
                                MANAGED_STACK_ADDRESS_BOEHM_GC_word /* client_data */) MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NONNULL(1);

typedef void (MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK * MANAGED_STACK_ADDRESS_BOEHM_GC_walk_hblk_fn)(struct MANAGED_STACK_ADDRESS_BOEHM_GC_hblk_s *,
                                             MANAGED_STACK_ADDRESS_BOEHM_GC_word /* client_data */);

/* Apply fn to each allocated heap block.  It is the responsibility     */
/* of the caller to avoid data race during the function execution (e.g. */
/* by holding the allocation lock).                                     */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_apply_to_all_blocks(MANAGED_STACK_ADDRESS_BOEHM_GC_walk_hblk_fn,
                                MANAGED_STACK_ADDRESS_BOEHM_GC_word /* client_data */) MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NONNULL(1);

/* If there are likely to be false references to a block starting at h  */
/* of the indicated length, then return the next plausible starting     */
/* location for h that might avoid these false references.  Otherwise   */
/* NULL is returned.  Assumes the allocation lock is held but no        */
/* assertion about it by design.                                        */
MANAGED_STACK_ADDRESS_BOEHM_GC_API struct MANAGED_STACK_ADDRESS_BOEHM_GC_hblk_s *MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_is_black_listed(struct MANAGED_STACK_ADDRESS_BOEHM_GC_hblk_s *,
                                                    MANAGED_STACK_ADDRESS_BOEHM_GC_word /* len */);

/* Return the number of set mark bits for the heap block where object   */
/* p is located.  Defined only if the library has been compiled         */
/* without NO_DEBUGGING.                                                */
MANAGED_STACK_ADDRESS_BOEHM_GC_API unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_count_set_marks_in_hblk(const void * /* p */);

/* And some routines to support creation of new "kinds", e.g. with      */
/* custom mark procedures, by language runtimes.                        */
/* The _inner versions assume the caller holds the allocation lock.     */

/* Return a new free list array.        */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void ** MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_new_free_list(void);
MANAGED_STACK_ADDRESS_BOEHM_GC_API void ** MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_new_free_list_inner(void);

/* Return a new kind, as specified. */
MANAGED_STACK_ADDRESS_BOEHM_GC_API unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_new_kind(void ** /* free_list */,
                            MANAGED_STACK_ADDRESS_BOEHM_GC_word /* mark_descriptor_template */,
                            int /* add_size_to_descriptor */,
                            int /* clear_new_objects */) MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NONNULL(1);
                /* The last two parameters must be zero or one. */
MANAGED_STACK_ADDRESS_BOEHM_GC_API unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_new_kind_inner(void ** /* free_list */,
                            MANAGED_STACK_ADDRESS_BOEHM_GC_word /* mark_descriptor_template */,
                            int /* add_size_to_descriptor */,
                            int /* clear_new_objects */) MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NONNULL(1);

/* Return a new mark procedure identifier, suitable for use as  */
/* the first argument in MANAGED_STACK_ADDRESS_BOEHM_GC_MAKE_PROC.                          */
MANAGED_STACK_ADDRESS_BOEHM_GC_API unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_new_proc(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_proc);
MANAGED_STACK_ADDRESS_BOEHM_GC_API unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_new_proc_inner(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_proc);

/* Similar to MANAGED_STACK_ADDRESS_BOEHM_GC_init_gcj_malloc() described in gc_gcj.h but with the   */
/* proper types of the arguments.                                       */
/* Defined only if the library has been compiled with MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT.   */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_init_gcj_malloc_mp(unsigned /* mp_index */,
                                          MANAGED_STACK_ADDRESS_BOEHM_GC_mark_proc /* mp */);

/* Allocate an object of a given kind.  By default, there are only      */
/* a few kinds: composite (pointerful), atomic, uncollectible, etc.     */
/* We claim it is possible for clever client code that understands the  */
/* GC internals to add more, e.g. to communicate object layout          */
/* information to the collector.  Note that in the multi-threaded       */
/* contexts, this is usually unsafe for kinds that have the descriptor  */
/* in the object itself, since there is otherwise a window in which     */
/* the descriptor is not correct.  Even in the single-threaded case,    */
/* we need to be sure that cleared objects on a free list don't         */
/* cause a GC crash if they are accidentally traced.                    */
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_ALLOC_SIZE(1) void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc(
                                                            size_t /* lb */,
                                                            int /* knd */);

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_ALLOC_SIZE(1) void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL
                                        MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_ignore_off_page(
                                            size_t /* lb */, int /* knd */);
                                /* As above, but pointers to past the   */
                                /* first hblk of the resulting object   */
                                /* are ignored.                         */

/* Generalized version of MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_[atomic_]uncollectable.     */
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_ALLOC_SIZE(1) void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL
                                        MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_uncollectable(
                                            size_t /* lb */, int /* knd */);

/* Same as above but primary for allocating an object of the same kind  */
/* as an existing one (kind obtained by MANAGED_STACK_ADDRESS_BOEHM_GC_get_kind_and_size).          */
/* Not suitable for GCJ and typed-malloc kinds.                         */
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_ALLOC_SIZE(1) void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL
                                        MANAGED_STACK_ADDRESS_BOEHM_GC_generic_or_special_malloc(
                                            size_t /* size */, int /* knd */);
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_ALLOC_SIZE(1) void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL
                                        MANAGED_STACK_ADDRESS_BOEHM_GC_debug_generic_or_special_malloc(
                                            size_t /* size */, int /* knd */,
                                            MANAGED_STACK_ADDRESS_BOEHM_GC_EXTRA_PARAMS);

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG
# define MANAGED_STACK_ADDRESS_BOEHM_GC_GENERIC_OR_SPECIAL_MALLOC(sz, knd) \
                MANAGED_STACK_ADDRESS_BOEHM_GC_debug_generic_or_special_malloc(sz, knd, MANAGED_STACK_ADDRESS_BOEHM_GC_EXTRAS)
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_GENERIC_OR_SPECIAL_MALLOC(sz, knd) \
                MANAGED_STACK_ADDRESS_BOEHM_GC_generic_or_special_malloc(sz, knd)
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG */

/* Similar to MANAGED_STACK_ADDRESS_BOEHM_GC_size but returns object kind.  Size is returned too    */
/* if psize is not NULL.                                                */
MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_kind_and_size(const void *, size_t * /* psize */)
                                                        MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NONNULL(1);

typedef void (MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK * MANAGED_STACK_ADDRESS_BOEHM_GC_describe_type_fn)(void * /* p */,
                                                 char * /* out_buf */);
                                /* A procedure which                    */
                                /* produces a human-readable            */
                                /* description of the "type" of object  */
                                /* p into the buffer out_buf of length  */
                                /* MANAGED_STACK_ADDRESS_BOEHM_GC_TYPE_DESCR_LEN.  This is used by  */
                                /* the debug support when printing      */
                                /* objects.                             */
                                /* These functions should be as robust  */
                                /* as possible, though we do avoid      */
                                /* invoking them on objects on the      */
                                /* global free list.                    */
#define MANAGED_STACK_ADDRESS_BOEHM_GC_TYPE_DESCR_LEN 40

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_register_describe_type_fn(int /* kind */,
                                                 MANAGED_STACK_ADDRESS_BOEHM_GC_describe_type_fn);
                                /* Register a describe_type function    */
                                /* to be used when printing objects     */
                                /* of a particular kind.                */

/* Clear some of the inaccessible part of the stack.  Returns its       */
/* argument, so it can be used in a tail call position, hence clearing  */
/* another frame.  Argument may be NULL.                                */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_clear_stack(void *);

/* Set and get the client notifier on collections.  The client function */
/* is called at the start of every full GC (called with the allocation  */
/* lock held).  May be 0.  This is a really tricky interface to use     */
/* correctly.  Unless you really understand the collector internals,    */
/* the callback should not, directly or indirectly, make any MANAGED_STACK_ADDRESS_BOEHM_GC_ or     */
/* potentially blocking calls.  In particular, it is not safe to        */
/* allocate memory using the garbage collector from within the callback */
/* function.  Both the setter and getter acquire the GC lock.           */
typedef void (MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK * MANAGED_STACK_ADDRESS_BOEHM_GC_start_callback_proc)(void);
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_start_callback(MANAGED_STACK_ADDRESS_BOEHM_GC_start_callback_proc);
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_start_callback_proc MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_start_callback(void);

/* Slow/general mark bit manipulation.  The caller should hold the      */
/* allocation lock.  MANAGED_STACK_ADDRESS_BOEHM_GC_is_marked returns 1 (true) or 0.  The argument  */
/* should be the real address of an object (i.e. the address of the     */
/* debug header if there is one).                                       */
MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_is_marked(const void *) MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NONNULL(1);
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_clear_mark_bit(const void *) MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NONNULL(1);
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_mark_bit(const void *) MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NONNULL(1);

/* Push everything in the given range onto the mark stack.              */
/* (MANAGED_STACK_ADDRESS_BOEHM_GC_push_conditional pushes either all or only dirty pages depending */
/* on the third argument.)  MANAGED_STACK_ADDRESS_BOEHM_GC_push_all_eager also ensures that stack   */
/* is scanned immediately, not just scheduled for scanning.             */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_push_all(void * /* bottom */, void * /* top */);
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_push_all_eager(void * /* bottom */, void * /* top */);
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_push_conditional(void * /* bottom */, void * /* top */,
                                        int /* bool all */);
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_push_finalizer_structures(void);

/* Set and get the client push-other-roots procedure.  A client         */
/* supplied procedure should also call the original procedure.          */
/* Note that both the setter and getter require some external           */
/* synchronization to avoid data race.                                  */
typedef void (MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK * MANAGED_STACK_ADDRESS_BOEHM_GC_push_other_roots_proc)(void);
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_push_other_roots(MANAGED_STACK_ADDRESS_BOEHM_GC_push_other_roots_proc);
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_push_other_roots_proc MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_push_other_roots(void);

/* Walk the GC heap visiting all reachable objects.  Assume the caller  */
/* holds the allocation lock.  Object base pointer, object size and     */
/* client custom data are passed to the callback (holding the lock).    */
typedef void (MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK * MANAGED_STACK_ADDRESS_BOEHM_GC_reachable_object_proc)(void * /* obj */,
                                                size_t /* bytes */,
                                                void * /* client_data */);
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_enumerate_reachable_objects_inner(
                                MANAGED_STACK_ADDRESS_BOEHM_GC_reachable_object_proc,
                                void * /* client_data */) MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NONNULL(1);

/* Is the given address in one of the temporary static root sections?   */
/* Acquires the GC lock.                                                */
MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_is_tmp_root(void *);

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_print_trace(MANAGED_STACK_ADDRESS_BOEHM_GC_word /* gc_no */);
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_print_trace_inner(MANAGED_STACK_ADDRESS_BOEHM_GC_word /* gc_no */);

/* Set the client for when mark stack is empty.  A client can use       */
/* this callback to process (un)marked objects and push additional      */
/* work onto the stack.  Useful for implementing ephemerons.            */
/* Both the setter and getter acquire the GC lock.                      */
typedef struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry * (MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK * MANAGED_STACK_ADDRESS_BOEHM_GC_on_mark_stack_empty_proc)(
                                struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry * /* mark_stack_ptr */,
                                struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry * /* mark_stack_limit */);
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_on_mark_stack_empty(MANAGED_STACK_ADDRESS_BOEHM_GC_on_mark_stack_empty_proc);
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_on_mark_stack_empty_proc MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_on_mark_stack_empty(void);

#ifdef __cplusplus
  } /* extern "C" */
#endif

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_MARK_H */
