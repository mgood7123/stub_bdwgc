/*
 * Copyright (c) 2018-2020 Ivan Maidanski
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

// This file provides the implementation of MANAGED_STACK_ADDRESS_BOEHM_GC_throw_bad_alloc() which
// is invoked by GC operator "new" in case of an out-of-memory event.

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_BUILD
# define MANAGED_STACK_ADDRESS_BOEHM_GC_BUILD
#endif

#define MANAGED_STACK_ADDRESS_BOEHM_GC_DONT_INCL_WINDOWS_H
#include "gc/gc.h"

#include <new> // for bad_alloc, precedes include of gc_cpp.h

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NEW_ABORTS_ON_OOM) || defined(_LIBCPP_NO_EXCEPTIONS)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOCATOR_THROW_OR_ABORT() MANAGED_STACK_ADDRESS_BOEHM_GC_abort_on_oom()
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOCATOR_THROW_OR_ABORT() throw std::bad_alloc()
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_throw_bad_alloc() {
  MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOCATOR_THROW_OR_ABORT();
}
