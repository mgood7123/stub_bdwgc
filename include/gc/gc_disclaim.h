/*
 * Copyright (c) 2007-2011 by Hewlett-Packard Company. All rights reserved.
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

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DISCLAIM_H
#define MANAGED_STACK_ADDRESS_BOEHM_GC_DISCLAIM_H

#include "gc.h"

#ifdef __cplusplus
  extern "C" {
#endif

/* This API is defined only if the library has been suitably compiled   */
/* (i.e. with ENABLE_DISCLAIM defined).                                 */

/* Prepare the object kind used by MANAGED_STACK_ADDRESS_BOEHM_GC_finalized_malloc.  Call it from   */
/* your initialization code or, at least, at some point before using    */
/* finalized allocations.  The function is thread-safe.                 */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_init_finalized_malloc(void);

/* Type of a disclaim call-back.  Called with the allocation lock held. */
typedef int (MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK * MANAGED_STACK_ADDRESS_BOEHM_GC_disclaim_proc)(void * /* obj */);

/* Register proc to be called on each object (of given kind) ready to   */
/* be reclaimed.  If proc() returns non-zero, the collector will not    */
/* reclaim the object on this GC cycle (proc() should not try to        */
/* resurrect the object otherwise); objects reachable from proc()       */
/* (including the referred closure object) will be protected from       */
/* collection if mark_from_all is non-zero, but at the expense that     */
/* long chains of objects will take many cycles to reclaim.  Any call   */
/* to MANAGED_STACK_ADDRESS_BOEHM_GC_free() deallocates the object (pointed by the argument)        */
/* without inquiring proc().  Acquires the allocation lock.             */
/* No-op in the leak-finding mode.                                      */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_register_disclaim_proc(int /* kind */,
                                              MANAGED_STACK_ADDRESS_BOEHM_GC_disclaim_proc /* proc */,
                                              int /* mark_from_all */);

/* The finalizer closure used by MANAGED_STACK_ADDRESS_BOEHM_GC_finalized_malloc.                   */
struct MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_closure {
    MANAGED_STACK_ADDRESS_BOEHM_GC_finalization_proc proc;
    void *cd;
};

/* Allocate an object which is to be finalized by the given closure.    */
/* This uses a dedicated object kind with a disclaim procedure, and is  */
/* more efficient than MANAGED_STACK_ADDRESS_BOEHM_GC_register_finalizer and friends.               */
/* MANAGED_STACK_ADDRESS_BOEHM_GC_init_finalized_malloc must be called before using this.           */
/* The collector will reclaim the object during this GC cycle (thus,    */
/* fc->proc() should not try to resurrect the object).  The other       */
/* objects reachable from fc->proc (including the closure object in     */
/* case it is a heap-allocated one) will be protected from collection.  */
/* Note that MANAGED_STACK_ADDRESS_BOEHM_GC_size (applied to such allocated object) returns a value */
/* slightly bigger than the specified allocation size, and that MANAGED_STACK_ADDRESS_BOEHM_GC_base */
/* result points to a word prior to the start of the allocated object.  */
/* The disclaim procedure is not invoked in the leak-finding mode.      */
/* There is no debugging version of this allocation API.                */
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_ALLOC_SIZE(1) void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL
        MANAGED_STACK_ADDRESS_BOEHM_GC_finalized_malloc(size_t /* size */,
            const struct MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_closure * /* fc */) MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NONNULL(2);

#ifdef __cplusplus
  } /* extern "C" */
#endif

#endif
