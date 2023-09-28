/*
 * Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
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

/*************************************************************************
This implementation module for gc_cpp.h provides an implementation of
the global operators "new" and "delete" that calls the Boehm
allocator.  All objects allocated by this implementation will be
uncollectible but part of the root set of the collector.

You should ensure (using implementation-dependent techniques) that the
linker finds this module before the library that defines the default
built-in "new" and "delete".
**************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_BUILD
# define MANAGED_STACK_ADDRESS_BOEHM_GC_BUILD
#endif

#define MANAGED_STACK_ADDRESS_BOEHM_GC_DONT_INCL_WINDOWS_H
#include "gc/gc.h"

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_INCLUDE_NEW
# define MANAGED_STACK_ADDRESS_BOEHM_GC_INCLUDE_NEW
#endif
#include "gc/gc_cpp.h"

#if (!defined(_MSC_VER) && !defined(__DMC__) \
     || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_INLINE_STD_NEW)) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE_STD_NEW)

# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NEW_ABORTS_ON_OOM) || defined(_LIBCPP_NO_EXCEPTIONS)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOCATOR_THROW_OR_ABORT() MANAGED_STACK_ADDRESS_BOEHM_GC_abort_on_oom()
# else
    // Use bad_alloc() directly instead of MANAGED_STACK_ADDRESS_BOEHM_GC_throw_bad_alloc() call.
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOCATOR_THROW_OR_ABORT() throw std::bad_alloc()
# endif

  void* operator new(MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size) MANAGED_STACK_ADDRESS_BOEHM_GC_DECL_NEW_THROW
  {
    void* obj = MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_UNCOLLECTABLE(size);
    if (0 == obj)
      MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOCATOR_THROW_OR_ABORT();
    return obj;
  }

# ifdef _MSC_VER
    // This new operator is used by VC++ in case of Debug builds.
    void* operator new(MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size, int /* nBlockUse */,
                       const char* szFileName, int nLine)
    {
#     ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG
        void* obj = MANAGED_STACK_ADDRESS_BOEHM_GC_debug_malloc_uncollectable(size, szFileName, nLine);
#     else
        void* obj = MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_UNCOLLECTABLE(size);
        (void)szFileName; (void)nLine;
#     endif
      if (0 == obj)
        MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOCATOR_THROW_OR_ABORT();
      return obj;
    }
# endif // _MSC_VER

  void operator delete(void* obj) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(obj);
  }

# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_NOTHROW
    void* operator new(MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size, const std::nothrow_t&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
    {
      return MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_UNCOLLECTABLE(size);
    }

    void operator delete(void* obj, const std::nothrow_t&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
    {
      MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(obj);
    }
# endif // MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_NOTHROW

# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_ARRAY) && !defined(CPPCHECK)
    void* operator new[](MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size) MANAGED_STACK_ADDRESS_BOEHM_GC_DECL_NEW_THROW
    {
      void* obj = MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_UNCOLLECTABLE(size);
      if (0 == obj)
        MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOCATOR_THROW_OR_ABORT();
      return obj;
    }

#   ifdef _MSC_VER
      // This new operator is used by VC++ 7+ in Debug builds.
      void* operator new[](MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size, int nBlockUse,
                           const char* szFileName, int nLine)
      {
        return operator new(size, nBlockUse, szFileName, nLine);
      }
#   endif // _MSC_VER

    void operator delete[](void* obj) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
    {
      MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(obj);
    }

#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_NOTHROW
      void* operator new[](MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size, const std::nothrow_t&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
      {
        return MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_UNCOLLECTABLE(size);
      }

      void operator delete[](void* obj, const std::nothrow_t&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
      {
        MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(obj);
      }
#   endif // MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_NOTHROW
# endif // MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_ARRAY

# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_SIZED_DELETE
    void operator delete(void* obj, MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
    {
      MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(obj);
    }

#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_ARRAY) && !defined(CPPCHECK)
      void operator delete[](void* obj, MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
      {
        MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(obj);
      }
#   endif
# endif // MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_SIZED_DELETE

#endif // !_MSC_VER && !__DMC__ || MANAGED_STACK_ADDRESS_BOEHM_GC_NO_INLINE_STD_NEW
