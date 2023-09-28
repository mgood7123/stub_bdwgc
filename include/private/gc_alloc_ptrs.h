/*
 * Copyright (c) 1996-1998 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 2018-2021 Ivan Maidanski
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

/* This file is kept for a binary compatibility purpose only.   */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOC_PTRS_H
#define MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOC_PTRS_H

#include "gc/gc.h"

#ifdef __cplusplus
  extern "C" {
#endif

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_API_PRIV
# define MANAGED_STACK_ADDRESS_BOEHM_GC_API_PRIV MANAGED_STACK_ADDRESS_BOEHM_GC_API
#endif

/* Some compilers do not accept "const" together with the dllimport     */
/* attribute, so the symbols below are exported as non-constant ones.   */
#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_APIVAR_CONST
# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_BUILD) || !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DLL)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_APIVAR_CONST const
# else
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_APIVAR_CONST /* empty */
# endif
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_API_PRIV void ** MANAGED_STACK_ADDRESS_BOEHM_GC_APIVAR_CONST MANAGED_STACK_ADDRESS_BOEHM_GC_objfreelist_ptr;
MANAGED_STACK_ADDRESS_BOEHM_GC_API_PRIV void ** MANAGED_STACK_ADDRESS_BOEHM_GC_APIVAR_CONST MANAGED_STACK_ADDRESS_BOEHM_GC_aobjfreelist_ptr;
MANAGED_STACK_ADDRESS_BOEHM_GC_API_PRIV void ** MANAGED_STACK_ADDRESS_BOEHM_GC_APIVAR_CONST MANAGED_STACK_ADDRESS_BOEHM_GC_uobjfreelist_ptr;

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ATOMIC_UNCOLLECTABLE
  MANAGED_STACK_ADDRESS_BOEHM_GC_API_PRIV void ** MANAGED_STACK_ADDRESS_BOEHM_GC_APIVAR_CONST MANAGED_STACK_ADDRESS_BOEHM_GC_auobjfreelist_ptr;
#endif

/* Manually update the number of bytes allocated during the current     */
/* collection cycle and the number of explicitly deallocated bytes of   */
/* memory since the last collection, respectively.  Both functions are  */
/* unsynchronized, MANAGED_STACK_ADDRESS_BOEHM_GC_call_with_alloc_lock() should be used to avoid    */
/* data races.                                                          */
MANAGED_STACK_ADDRESS_BOEHM_GC_API_PRIV void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_incr_bytes_allocd(size_t /* bytes */);
MANAGED_STACK_ADDRESS_BOEHM_GC_API_PRIV void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_incr_bytes_freed(size_t /* bytes */);

#ifdef __cplusplus
  } /* extern "C" */
#endif

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOC_PTRS_H */
