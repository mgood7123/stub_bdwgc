/*
 * Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program for any
 * purpose, provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is
 * granted, provided the above notices are retained, and a notice that
 * the code was modified is included with the above copyright notice.
 */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_CPP_H
#define MANAGED_STACK_ADDRESS_BOEHM_GC_CPP_H

/****************************************************************************
C++ Interface to the Boehm Collector

    John R. Ellis and Jesse Hull

This interface provides access to the Boehm collector.  It provides
basic facilities similar to those described in "Safe, Efficient
Garbage Collection for C++", by John R. Ellis and David L. Detlefs
(ftp://ftp.parc.xerox.com/pub/ellis/gc).

All heap-allocated objects are either "collectible" or
"uncollectible".  Programs must explicitly delete uncollectible
objects, whereas the garbage collector will automatically delete
collectible objects when it discovers them to be inaccessible.
Collectible objects may freely point at uncollectible objects and vice
versa.

Objects allocated with the built-in "::operator new" are uncollectible.

Objects derived from class "gc" are collectible.  For example:

    class A: public gc {...};
    A* a = new A;       // a is collectible.

Collectible instances of non-class types can be allocated using the GC
(or UseGC) placement:

    typedef int A[ 10 ];
    A* a = new (GC) A;

Uncollectible instances of classes derived from "gc" can be allocated
using the NoGC placement:

    class A: public gc {...};
    A* a = new (NoGC) A;   // a is uncollectible.

The new(PointerFreeGC) syntax allows the allocation of collectible
objects that are not scanned by the collector.  This useful if you
are allocating compressed data, bitmaps, or network packets.  (In
the latter case, it may remove danger of unfriendly network packets
intentionally containing values that cause spurious memory retention.)

Both uncollectible and collectible objects can be explicitly deleted
with "delete", which invokes an object's destructors and frees its
storage immediately.

A collectible object may have a clean-up function, which will be
invoked when the collector discovers the object to be inaccessible.
An object derived from "gc_cleanup" or containing a member derived
from "gc_cleanup" has a default clean-up function that invokes the
object's destructors.  Explicit clean-up functions may be specified as
an additional placement argument:

    A* a = ::new (GC, MyCleanup) A;

An object is considered "accessible" by the collector if it can be
reached by a path of pointers from static variables, automatic
variables of active functions, or from some object with clean-up
enabled; pointers from an object to itself are ignored.

Thus, if objects A and B both have clean-up functions, and A points at
B, B is considered accessible.  After A's clean-up is invoked and its
storage released, B will then become inaccessible and will have its
clean-up invoked.  If A points at B and B points to A, forming a
cycle, then that's considered a storage leak, and neither will be
collectible.  See the interface gc.h for low-level facilities for
handling such cycles of objects with clean-up.

The collector cannot guarantee that it will find all inaccessible
objects.  In practice, it finds almost all of them.

Cautions:

1. Be sure the collector is compiled with the C++ support
(e.g. --enable-cplusplus option is passed to make).

2. If the compiler does not support "operator new[]", beware that an
array of type T, where T is derived from "gc", may or may not be
allocated as a collectible object (it depends on the compiler).  Use
the explicit GC placement to make the array collectible.  For example:

    class A: public gc {...};
    A* a1 = new A[ 10 ];        // collectible or uncollectible?
    A* a2 = new (GC) A[ 10 ];   // collectible.

3. The destructors of collectible arrays of objects derived from
"gc_cleanup" will not be invoked properly.  For example:

    class A: public gc_cleanup {...};
    A* a = new (GC) A[ 10 ];    // destructors not invoked correctly

Typically, only the destructor for the first element of the array will
be invoked when the array is garbage-collected.  To get all the
destructors of any array executed, you must supply an explicit
clean-up function:

    A* a = new (GC, MyCleanUp) A[ 10 ];

(Implementing clean-up of arrays correctly, portably, and in a way
that preserves the correct exception semantics requires a language
extension, e.g. the "gc" keyword.)

4. GC name conflicts:

Many other systems seem to use the identifier "GC" as an abbreviation
for "Graphics Context".  Thus, GC placement has been replaced
by UseGC.  GC is an alias for UseGC, unless MANAGED_STACK_ADDRESS_BOEHM_GC_NAME_CONFLICT is defined.

****************************************************************************/

#include "gc.h"

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_INCLUDE_NEW
# include <new> // for std, bad_alloc
#endif

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_INCLUDE_NEW) && (__cplusplus >= 201103L)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_PTRDIFF_T std::ptrdiff_t
# define MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T std::size_t
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_PTRDIFF_T ptrdiff_t
# define MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size_t
#endif

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_NAMESPACE
# define MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(T) boehmgc::T
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(T) T
#endif

#ifndef THINK_CPLUS
# define MANAGED_STACK_ADDRESS_BOEHM_GC_cdecl MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_cdecl _cdecl
#endif

#if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_OPERATOR_NEW_ARRAY) \
    && !defined(_ENABLE_ARRAYNEW) /* Digimars */ \
    && (defined(__BORLANDC__) && (__BORLANDC__ < 0x450) \
        || (defined(__GNUC__) && !MANAGED_STACK_ADDRESS_BOEHM_GC_GNUC_PREREQ(2, 6)) \
        || (defined(_MSC_VER) && _MSC_VER <= 1020) \
        || (defined(__WATCOMC__) && __WATCOMC__ < 1050))
# define MANAGED_STACK_ADDRESS_BOEHM_GC_NO_OPERATOR_NEW_ARRAY
#endif

#if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_OPERATOR_NEW_ARRAY) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_ARRAY)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_ARRAY
#endif

#if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_INLINE_STD_NEW) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE_STD_NEW) \
    && (defined(_MSC_VER) || defined(__DMC__) \
        || ((defined(__BORLANDC__) || defined(__CYGWIN__) \
             || defined(__CYGWIN32__) || defined(__MINGW32__) \
             || defined(__WATCOMC__)) \
            && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_BUILD) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NOT_DLL)))
  // Inlining done to avoid mix up of new and delete operators by VC++ 9
  // (due to arbitrary ordering during linking).
# define MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE_STD_NEW
#endif

#if (!defined(__BORLANDC__) || __BORLANDC__ > 0x0620) \
    && !defined(__sgi) && !defined(__WATCOMC__) \
    && (!defined(_MSC_VER) || _MSC_VER > 1020)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_PLACEMENT_DELETE
#endif

#if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_SIZED_DELETE) \
    && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_OPERATOR_SIZED_DELETE) \
    && (__cplusplus >= 201402L || _MSVC_LANG >= 201402L) // C++14
# define MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_SIZED_DELETE
#endif

#if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_NOTHROW) \
    && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_OPERATOR_NEW_NOTHROW) \
    && ((defined(MANAGED_STACK_ADDRESS_BOEHM_GC_INCLUDE_NEW) \
         && (__cplusplus >= 201103L || _MSVC_LANG >= 201103L)) \
        || defined(__NOTHROW_T_DEFINED))
  // Note: this might require defining MANAGED_STACK_ADDRESS_BOEHM_GC_INCLUDE_NEW by client
  // before include gc_cpp.h (on Windows).
# define MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_NOTHROW
#endif

#if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NEW_DELETE_THROW_NOT_NEEDED) \
    && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NEW_DELETE_NEED_THROW) && MANAGED_STACK_ADDRESS_BOEHM_GC_GNUC_PREREQ(4, 2) \
    && (__cplusplus < 201103L || defined(__clang__))
# define MANAGED_STACK_ADDRESS_BOEHM_GC_NEW_DELETE_NEED_THROW
#endif

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NEW_DELETE_NEED_THROW
# define MANAGED_STACK_ADDRESS_BOEHM_GC_DECL_NEW_THROW /* empty */
#elif __cplusplus >= 201703L || _MSVC_LANG >= 201703L
  // The "dynamic exception" syntax had been deprecated in C++11
  // and was removed in C++17.
# define MANAGED_STACK_ADDRESS_BOEHM_GC_DECL_NEW_THROW noexcept(false)
#elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_INCLUDE_NEW)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_DECL_NEW_THROW throw(std::bad_alloc)
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_DECL_NEW_THROW /* empty (as bad_alloc might be undeclared) */
#endif

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NEW_ABORTS_ON_OOM) || defined(_LIBCPP_NO_EXCEPTIONS)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_OP_NEW_OOM_CHECK(obj) \
                do { if (!(obj)) MANAGED_STACK_ADDRESS_BOEHM_GC_abort_on_oom(); } while (0)
#elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_INCLUDE_NEW)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_OP_NEW_OOM_CHECK(obj) if (obj) {} else throw std::bad_alloc()
#else
  // "new" header is not included, so bad_alloc cannot be thrown directly.
  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_throw_bad_alloc();
# define MANAGED_STACK_ADDRESS_BOEHM_GC_OP_NEW_OOM_CHECK(obj) if (obj) {} else MANAGED_STACK_ADDRESS_BOEHM_GC_throw_bad_alloc()
#endif // !MANAGED_STACK_ADDRESS_BOEHM_GC_NEW_ABORTS_ON_OOM && !MANAGED_STACK_ADDRESS_BOEHM_GC_INCLUDE_NEW

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_NAMESPACE
namespace boehmgc
{
#endif

enum GCPlacement
{
  UseGC,
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NAME_CONFLICT
    GC = UseGC,
# endif
  NoGC,
  PointerFreeGC
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ATOMIC_UNCOLLECTABLE
    , PointerFreeNoGC
# endif
};

/**
 * Instances of classes derived from gc will be allocated in the collected
 * heap by default, unless an explicit NoGC placement is specified.
 */
class gc
{
public:
  inline void* operator new(MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T);
  inline void* operator new(MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T, GCPlacement);
  inline void* operator new(MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T, void*) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT;
    // Must be redefined here, since the other overloadings hide
    // the global definition.
  inline void operator delete(void*) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT;
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_SIZED_DELETE
    inline void operator delete(void*, MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT;
# endif

# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PLACEMENT_DELETE
    inline void operator delete(void*, GCPlacement) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT;
      // Called if construction fails.
    inline void operator delete(void*, void*) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT;
# endif // MANAGED_STACK_ADDRESS_BOEHM_GC_PLACEMENT_DELETE

# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_ARRAY
    inline void* operator new[](MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T);
    inline void* operator new[](MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T, GCPlacement);
    inline void* operator new[](MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T, void*) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT;
    inline void operator delete[](void*) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT;
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_SIZED_DELETE
      inline void operator delete[](void*, MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT;
#   endif
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PLACEMENT_DELETE
      inline void operator delete[](void*, GCPlacement) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT;
      inline void operator delete[](void*, void*) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT;
#   endif
# endif // MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_ARRAY
};

/**
 * Instances of classes derived from gc_cleanup will be allocated
 * in the collected heap by default.  When the collector discovers
 * an inaccessible object derived from gc_cleanup or containing
 * a member derived from gc_cleanup, its destructors will be invoked.
 */
class gc_cleanup: virtual public gc
{
public:
  inline gc_cleanup();
  inline virtual ~gc_cleanup();

private:
  inline static void MANAGED_STACK_ADDRESS_BOEHM_GC_cdecl cleanup(void* obj, void* clientData);
};

extern "C" {
  typedef void (MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK * GCCleanUpFunc)(void* obj, void* clientData);
}

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_NAMESPACE
}
#endif

#ifdef _MSC_VER
  // Disable warning that "no matching operator delete found; memory will
  // not be freed if initialization throws an exception"
# pragma warning(disable:4291)
  // TODO: "non-member operator new or delete may not be declared inline"
  // warning is disabled for now.
# pragma warning(disable:4595)
#endif

inline void* operator new(MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T, MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(GCPlacement),
                          MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(GCCleanUpFunc) = 0,
                          void* /* clientData */ = 0);
    // Allocates a collectible or uncollectible object, according to the
    // value of gcp.
    //
    // For collectible objects, if cleanup is non-null, then when the
    // allocated object obj becomes inaccessible, the collector will
    // invoke cleanup(obj,clientData) but will not invoke the object's
    // destructors.  It is an error to explicitly delete an object
    // allocated with a non-null cleanup.
    //
    // It is an error to specify a non-null cleanup with NoGC or for
    // classes derived from gc_cleanup or containing members derived
    // from gc_cleanup.

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PLACEMENT_DELETE
  inline void operator delete(void*, MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(GCPlacement),
                              MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(GCCleanUpFunc),
                              void*) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT;
#endif

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE_STD_NEW

# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_ARRAY
    inline void* operator new[](MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size) MANAGED_STACK_ADDRESS_BOEHM_GC_DECL_NEW_THROW
    {
      void* obj = MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_UNCOLLECTABLE(size);
      MANAGED_STACK_ADDRESS_BOEHM_GC_OP_NEW_OOM_CHECK(obj);
      return obj;
    }

    inline void operator delete[](void* obj) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
    {
      MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(obj);
    }

#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_NOTHROW
      inline /* MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC */ void* operator new[](MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size,
                                            const std::nothrow_t&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
      {
        return MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_UNCOLLECTABLE(size);
      }

      inline void operator delete[](void* obj,
                                    const std::nothrow_t&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
      {
        MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(obj);
      }
#   endif // MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_NOTHROW
# endif // MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_ARRAY

  inline void* operator new(MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size) MANAGED_STACK_ADDRESS_BOEHM_GC_DECL_NEW_THROW
  {
    void* obj = MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_UNCOLLECTABLE(size);
    MANAGED_STACK_ADDRESS_BOEHM_GC_OP_NEW_OOM_CHECK(obj);
    return obj;
  }

  inline void operator delete(void* obj) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(obj);
  }

# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_NOTHROW
    inline /* MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC */ void* operator new(MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size,
                                        const std::nothrow_t&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
    {
      return MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_UNCOLLECTABLE(size);
    }

    inline void operator delete(void* obj, const std::nothrow_t&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
    {
      MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(obj);
    }
# endif // MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_NOTHROW

# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_SIZED_DELETE
    inline void operator delete(void* obj, MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
    {
      MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(obj);
    }

#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_ARRAY
      inline void operator delete[](void* obj, MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
      {
        MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(obj);
      }
#   endif
# endif // MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_SIZED_DELETE

# ifdef _MSC_VER
    // This new operator is used by VC++ in case of Debug builds.
    inline void* operator new(MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size, int /* nBlockUse */,
                              const char* szFileName, int nLine)
    {
#     ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG
        void* obj = MANAGED_STACK_ADDRESS_BOEHM_GC_debug_malloc_uncollectable(size, szFileName, nLine);
#     else
        void* obj = MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_UNCOLLECTABLE(size);
        (void)szFileName; (void)nLine;
#     endif
      MANAGED_STACK_ADDRESS_BOEHM_GC_OP_NEW_OOM_CHECK(obj);
      return obj;
    }

#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_ARRAY
      // This new operator is used by VC++ 7+ in Debug builds.
      inline void* operator new[](MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size, int nBlockUse,
                                  const char* szFileName, int nLine)
      {
        return operator new(size, nBlockUse, szFileName, nLine);
      }
#   endif
# endif // _MSC_VER

#elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_INLINE_STD_NEW) && defined(_MSC_VER)

  // The following ensures that the system default operator new[] does not
  // get undefined, which is what seems to happen on VC++ 6 for some reason
  // if we define a multi-argument operator new[].
  // There seems to be no way to redirect new in this environment without
  // including this everywhere.
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_ARRAY
    void* operator new[](MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T) MANAGED_STACK_ADDRESS_BOEHM_GC_DECL_NEW_THROW;
    void operator delete[](void*) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT;
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_NOTHROW
      /* MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC */ void* operator new[](MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T,
                                    const std::nothrow_t&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT;
      void operator delete[](void*, const std::nothrow_t&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT;
#   endif
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_SIZED_DELETE
      void operator delete[](void*, MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT;
#   endif

    void* operator new[](MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T, int /* nBlockUse */,
                         const char* /* szFileName */, int /* nLine */);
# endif // MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_ARRAY

  void* operator new(MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T) MANAGED_STACK_ADDRESS_BOEHM_GC_DECL_NEW_THROW;
  void operator delete(void*) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT;
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_NOTHROW
    /* MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC */ void* operator new(MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T,
                                    const std::nothrow_t&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT;
    void operator delete(void*, const std::nothrow_t&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT;
# endif
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_SIZED_DELETE
    void operator delete(void*, MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT;
# endif

  void* operator new(MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T, int /* nBlockUse */,
                     const char* /* szFileName */, int /* nLine */);

#endif // MANAGED_STACK_ADDRESS_BOEHM_GC_NO_INLINE_STD_NEW && _MSC_VER

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_ARRAY
  // The operator new for arrays, identical to the above.
  inline void* operator new[](MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T, MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(GCPlacement),
                              MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(GCCleanUpFunc) = 0,
                              void* /* clientData */ = 0);
#endif // MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_ARRAY

// Inline implementation.

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_NAMESPACE
namespace boehmgc
{
#endif

inline void* gc::operator new(MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size)
{
  void* obj = MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(size);
  MANAGED_STACK_ADDRESS_BOEHM_GC_OP_NEW_OOM_CHECK(obj);
  return obj;
}

inline void* gc::operator new(MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size, GCPlacement gcp)
{
  void* obj;
  switch (gcp) {
  case UseGC:
    obj = MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(size);
    break;
  case PointerFreeGC:
    obj = MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_ATOMIC(size);
    break;
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ATOMIC_UNCOLLECTABLE
    case PointerFreeNoGC:
      obj = MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_ATOMIC_UNCOLLECTABLE(size);
      break;
# endif
  case NoGC:
  default:
    obj = MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_UNCOLLECTABLE(size);
  }
  MANAGED_STACK_ADDRESS_BOEHM_GC_OP_NEW_OOM_CHECK(obj);
  return obj;
}

inline void* gc::operator new(MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T, void* p) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
{
  return p;
}

inline void gc::operator delete(void* obj) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(obj);
}

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_SIZED_DELETE
  inline void gc::operator delete(void* obj, MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(obj);
  }
#endif

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PLACEMENT_DELETE
  inline void gc::operator delete(void*, void*) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT {}

  inline void gc::operator delete(void* obj, GCPlacement) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(obj);
  }
#endif // MANAGED_STACK_ADDRESS_BOEHM_GC_PLACEMENT_DELETE

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_ARRAY
  inline void* gc::operator new[](MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size)
  {
    return gc::operator new(size);
  }

  inline void* gc::operator new[](MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size, GCPlacement gcp)
  {
    return gc::operator new(size, gcp);
  }

  inline void* gc::operator new[](MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T, void* p) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
  {
    return p;
  }

  inline void gc::operator delete[](void* obj) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
  {
    gc::operator delete(obj);
  }

# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_SIZED_DELETE
    inline void gc::operator delete[](void* obj, MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
    {
      gc::operator delete(obj, size);
    }
# endif

# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PLACEMENT_DELETE
    inline void gc::operator delete[](void*, void*) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT {}

    inline void gc::operator delete[](void* p, GCPlacement) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
    {
      gc::operator delete(p);
    }
# endif
#endif // MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_ARRAY

inline gc_cleanup::~gc_cleanup()
{
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
    void* base = MANAGED_STACK_ADDRESS_BOEHM_GC_base(this);
    if (0 == base) return; // Non-heap object.
    MANAGED_STACK_ADDRESS_BOEHM_GC_register_finalizer_ignore_self(base, 0, 0, 0, 0);
# endif
}

inline void MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK gc_cleanup::cleanup(void* obj, void* displ)
{
  reinterpret_cast<gc_cleanup*>(reinterpret_cast<char*>(obj)
                + reinterpret_cast<MANAGED_STACK_ADDRESS_BOEHM_GC_PTRDIFF_T>(displ))->~gc_cleanup();
}

inline gc_cleanup::gc_cleanup()
{
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
    MANAGED_STACK_ADDRESS_BOEHM_GC_finalization_proc oldProc = 0;
    void* oldData = 0; // to avoid "might be uninitialized" compiler warning
    void* this_ptr = reinterpret_cast<void*>(this);
    void* base = MANAGED_STACK_ADDRESS_BOEHM_GC_base(this_ptr);
    if (base != 0) {
      // Don't call the debug version, since this is a real base address.
      MANAGED_STACK_ADDRESS_BOEHM_GC_register_finalizer_ignore_self(base,
                reinterpret_cast<MANAGED_STACK_ADDRESS_BOEHM_GC_finalization_proc>(cleanup),
                reinterpret_cast<void*>(reinterpret_cast<char*>(this_ptr) -
                                        reinterpret_cast<char*>(base)),
                &oldProc, &oldData);
      if (oldProc != 0) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_register_finalizer_ignore_self(base, oldProc, oldData, 0, 0);
      }
    }
# elif defined(CPPCHECK)
    (void)cleanup;
# endif
}

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_NAMESPACE
}
#endif

inline void* operator new(MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size, MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(GCPlacement) gcp,
                          MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(GCCleanUpFunc) cleanup,
                          void* clientData)
{
  void* obj;
  switch (gcp) {
  case MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(UseGC):
    obj = MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(size);
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
      if (cleanup != 0 && obj != 0) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_REGISTER_FINALIZER_IGNORE_SELF(obj, cleanup, clientData, 0, 0);
      }
#   else
      (void)cleanup;
      (void)clientData;
#   endif
    break;
  case MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(PointerFreeGC):
    obj = MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_ATOMIC(size);
    break;
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ATOMIC_UNCOLLECTABLE
    case MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(PointerFreeNoGC):
      obj = MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_ATOMIC_UNCOLLECTABLE(size);
      break;
# endif
  case MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(NoGC):
  default:
    obj = MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_UNCOLLECTABLE(size);
  }
  MANAGED_STACK_ADDRESS_BOEHM_GC_OP_NEW_OOM_CHECK(obj);
  return obj;
}

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PLACEMENT_DELETE
  inline void operator delete(void* obj, MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(GCPlacement),
                              MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(GCCleanUpFunc),
                              void* /* clientData */) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(obj);
  }
#endif // MANAGED_STACK_ADDRESS_BOEHM_GC_PLACEMENT_DELETE

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_ARRAY
  inline void* operator new[](MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_T size,
                              MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(GCPlacement) gcp,
                              MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(GCCleanUpFunc) cleanup,
                              void* clientData)
  {
    return ::operator new(size, gcp, cleanup, clientData);
  }
#endif // MANAGED_STACK_ADDRESS_BOEHM_GC_OPERATOR_NEW_ARRAY

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_CPP_H */
