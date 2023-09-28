/*
 * Copyright (c) 1996-1997
 * Silicon Graphics Computer Systems, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * Copyright (c) 2002
 * Hewlett-Packard Company
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Hewlett-Packard Company makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */

/*
 * This implements standard-conforming allocators that interact with
 * the garbage collector.  Gc_allocator<T> allocates garbage-collectible
 * objects of type T.  Traceable_allocator<T> allocates objects that
 * are not themselves garbage collected, but are scanned by the
 * collector for pointers to collectible objects.  Traceable_alloc
 * should be used for explicitly managed STL containers that may
 * point to collectible objects.
 *
 * This code was derived from an earlier version of the GNU C++ standard
 * library, which itself was derived from the SGI STL implementation.
 *
 * Ignore-off-page allocator: George T. Talbot
 */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOCATOR_H
#define MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOCATOR_H

#include "gc.h"

#include <new> // for placement new and bad_alloc

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_NAMESPACE_ALLOCATOR
namespace boehmgc
{
#endif

#if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_MEMBER_TEMPLATES) && defined(_MSC_VER) && _MSC_VER <= 1200
  // MSVC++ 6.0 do not support member templates.
# define MANAGED_STACK_ADDRESS_BOEHM_GC_NO_MEMBER_TEMPLATES
#endif

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NEW_ABORTS_ON_OOM) || defined(_LIBCPP_NO_EXCEPTIONS)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOCATOR_THROW_OR_ABORT() MANAGED_STACK_ADDRESS_BOEHM_GC_abort_on_oom()
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOCATOR_THROW_OR_ABORT() throw std::bad_alloc()
#endif

#if __cplusplus >= 201103L
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_PTRDIFF_T std::ptrdiff_t
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_SIZE_T std::size_t
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_PTRDIFF_T ptrdiff_t
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_SIZE_T size_t
#endif

// First some helpers to allow us to dispatch on whether or not a type
// is known to be pointer-free.  These are private, except that the client
// may invoke the MANAGED_STACK_ADDRESS_BOEHM_GC_DECLARE_PTRFREE macro.

struct MANAGED_STACK_ADDRESS_BOEHM_GC_true_type {};
struct MANAGED_STACK_ADDRESS_BOEHM_GC_false_type {};

template <class MANAGED_STACK_ADDRESS_BOEHM_GC_tp>
struct MANAGED_STACK_ADDRESS_BOEHM_GC_type_traits {
  MANAGED_STACK_ADDRESS_BOEHM_GC_false_type MANAGED_STACK_ADDRESS_BOEHM_GC_is_ptr_free;
};

#define MANAGED_STACK_ADDRESS_BOEHM_GC_DECLARE_PTRFREE(T) \
    template<> struct MANAGED_STACK_ADDRESS_BOEHM_GC_type_traits<T> { MANAGED_STACK_ADDRESS_BOEHM_GC_true_type MANAGED_STACK_ADDRESS_BOEHM_GC_is_ptr_free; }

MANAGED_STACK_ADDRESS_BOEHM_GC_DECLARE_PTRFREE(char);
MANAGED_STACK_ADDRESS_BOEHM_GC_DECLARE_PTRFREE(signed char);
MANAGED_STACK_ADDRESS_BOEHM_GC_DECLARE_PTRFREE(unsigned char);
MANAGED_STACK_ADDRESS_BOEHM_GC_DECLARE_PTRFREE(signed short);
MANAGED_STACK_ADDRESS_BOEHM_GC_DECLARE_PTRFREE(unsigned short);
MANAGED_STACK_ADDRESS_BOEHM_GC_DECLARE_PTRFREE(signed int);
MANAGED_STACK_ADDRESS_BOEHM_GC_DECLARE_PTRFREE(unsigned int);
MANAGED_STACK_ADDRESS_BOEHM_GC_DECLARE_PTRFREE(signed long);
MANAGED_STACK_ADDRESS_BOEHM_GC_DECLARE_PTRFREE(unsigned long);
MANAGED_STACK_ADDRESS_BOEHM_GC_DECLARE_PTRFREE(float);
MANAGED_STACK_ADDRESS_BOEHM_GC_DECLARE_PTRFREE(double);
MANAGED_STACK_ADDRESS_BOEHM_GC_DECLARE_PTRFREE(long double);
// The client may want to add others.

// In the following MANAGED_STACK_ADDRESS_BOEHM_GC_Tp is MANAGED_STACK_ADDRESS_BOEHM_GC_true_type if we are allocating a pointer-free
// object.
template <class MANAGED_STACK_ADDRESS_BOEHM_GC_Tp>
inline void * MANAGED_STACK_ADDRESS_BOEHM_GC_selective_alloc(MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_SIZE_T n, MANAGED_STACK_ADDRESS_BOEHM_GC_Tp,
                                 bool ignore_off_page) {
    void *obj = ignore_off_page ? MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_IGNORE_OFF_PAGE(n) : MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(n);
    if (0 == obj)
      MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOCATOR_THROW_OR_ABORT();
    return obj;
}

#if !defined(__WATCOMC__)
  // Note: template-id not supported in this context by Watcom compiler.
  template <>
  inline void * MANAGED_STACK_ADDRESS_BOEHM_GC_selective_alloc<MANAGED_STACK_ADDRESS_BOEHM_GC_true_type>(MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_SIZE_T n,
                                                 MANAGED_STACK_ADDRESS_BOEHM_GC_true_type,
                                                 bool ignore_off_page) {
    void *obj = ignore_off_page ? MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_ATOMIC_IGNORE_OFF_PAGE(n)
                                 : MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_ATOMIC(n);
    if (0 == obj)
      MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOCATOR_THROW_OR_ABORT();
    return obj;
  }
#endif

// Now the public gc_allocator<T> class.
template <class MANAGED_STACK_ADDRESS_BOEHM_GC_Tp>
class gc_allocator {
public:
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_SIZE_T    size_type;
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_PTRDIFF_T difference_type;
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_Tp*       pointer;
  typedef const MANAGED_STACK_ADDRESS_BOEHM_GC_Tp* const_pointer;
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_Tp&       reference;
  typedef const MANAGED_STACK_ADDRESS_BOEHM_GC_Tp& const_reference;
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_Tp        value_type;

  template <class MANAGED_STACK_ADDRESS_BOEHM_GC_Tp1> struct rebind {
    typedef gc_allocator<MANAGED_STACK_ADDRESS_BOEHM_GC_Tp1> other;
  };

  gc_allocator() MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT {}
  gc_allocator(const gc_allocator&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT {}
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_MEMBER_TEMPLATES
    template <class MANAGED_STACK_ADDRESS_BOEHM_GC_Tp1> MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_EXPLICIT
    gc_allocator(const gc_allocator<MANAGED_STACK_ADDRESS_BOEHM_GC_Tp1>&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT {}
# endif
  ~gc_allocator() MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT {}

  pointer address(reference MANAGED_STACK_ADDRESS_BOEHM_GC_x) const { return &MANAGED_STACK_ADDRESS_BOEHM_GC_x; }
  const_pointer address(const_reference MANAGED_STACK_ADDRESS_BOEHM_GC_x) const { return &MANAGED_STACK_ADDRESS_BOEHM_GC_x; }

  // MANAGED_STACK_ADDRESS_BOEHM_GC_n is permitted to be 0.  The C++ standard says nothing about what
  // the return value is when MANAGED_STACK_ADDRESS_BOEHM_GC_n == 0.
  MANAGED_STACK_ADDRESS_BOEHM_GC_Tp* allocate(size_type MANAGED_STACK_ADDRESS_BOEHM_GC_n, const void* = 0) {
    MANAGED_STACK_ADDRESS_BOEHM_GC_type_traits<MANAGED_STACK_ADDRESS_BOEHM_GC_Tp> traits;
    return static_cast<MANAGED_STACK_ADDRESS_BOEHM_GC_Tp *>(MANAGED_STACK_ADDRESS_BOEHM_GC_selective_alloc(MANAGED_STACK_ADDRESS_BOEHM_GC_n * sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_Tp),
                                                   traits.MANAGED_STACK_ADDRESS_BOEHM_GC_is_ptr_free,
                                                   false));
  }

  void deallocate(pointer __p, size_type /* MANAGED_STACK_ADDRESS_BOEHM_GC_n */) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
    { MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(__p); }

  size_type max_size() const MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
    { return static_cast<MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_SIZE_T>(-1) / sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_Tp); }

  void construct(pointer __p, const MANAGED_STACK_ADDRESS_BOEHM_GC_Tp& __val) { new(__p) MANAGED_STACK_ADDRESS_BOEHM_GC_Tp(__val); }
  void destroy(pointer __p) { __p->~MANAGED_STACK_ADDRESS_BOEHM_GC_Tp(); }
};

template<>
class gc_allocator<void> {
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_SIZE_T    size_type;
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_PTRDIFF_T difference_type;
  typedef void*       pointer;
  typedef const void* const_pointer;
  typedef void        value_type;

  template <class MANAGED_STACK_ADDRESS_BOEHM_GC_Tp1> struct rebind {
    typedef gc_allocator<MANAGED_STACK_ADDRESS_BOEHM_GC_Tp1> other;
  };
};

template <class MANAGED_STACK_ADDRESS_BOEHM_GC_T1, class MANAGED_STACK_ADDRESS_BOEHM_GC_T2>
inline bool operator==(const gc_allocator<MANAGED_STACK_ADDRESS_BOEHM_GC_T1>&,
                       const gc_allocator<MANAGED_STACK_ADDRESS_BOEHM_GC_T2>&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
{
  return true;
}

template <class MANAGED_STACK_ADDRESS_BOEHM_GC_T1, class MANAGED_STACK_ADDRESS_BOEHM_GC_T2>
inline bool operator!=(const gc_allocator<MANAGED_STACK_ADDRESS_BOEHM_GC_T1>&,
                       const gc_allocator<MANAGED_STACK_ADDRESS_BOEHM_GC_T2>&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
{
  return false;
}

// Now the public gc_allocator_ignore_off_page<T> class.
template <class MANAGED_STACK_ADDRESS_BOEHM_GC_Tp>
class gc_allocator_ignore_off_page {
public:
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_SIZE_T    size_type;
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_PTRDIFF_T difference_type;
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_Tp*       pointer;
  typedef const MANAGED_STACK_ADDRESS_BOEHM_GC_Tp* const_pointer;
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_Tp&       reference;
  typedef const MANAGED_STACK_ADDRESS_BOEHM_GC_Tp& const_reference;
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_Tp        value_type;

  template <class MANAGED_STACK_ADDRESS_BOEHM_GC_Tp1> struct rebind {
    typedef gc_allocator_ignore_off_page<MANAGED_STACK_ADDRESS_BOEHM_GC_Tp1> other;
  };

  gc_allocator_ignore_off_page() MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT {}
  gc_allocator_ignore_off_page(const gc_allocator_ignore_off_page&)
    MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT {}
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_MEMBER_TEMPLATES
    template <class MANAGED_STACK_ADDRESS_BOEHM_GC_Tp1> MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_EXPLICIT
    gc_allocator_ignore_off_page(const gc_allocator_ignore_off_page<MANAGED_STACK_ADDRESS_BOEHM_GC_Tp1>&)
      MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT {}
# endif
  ~gc_allocator_ignore_off_page() MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT {}

  pointer address(reference MANAGED_STACK_ADDRESS_BOEHM_GC_x) const { return &MANAGED_STACK_ADDRESS_BOEHM_GC_x; }
  const_pointer address(const_reference MANAGED_STACK_ADDRESS_BOEHM_GC_x) const { return &MANAGED_STACK_ADDRESS_BOEHM_GC_x; }

  // MANAGED_STACK_ADDRESS_BOEHM_GC_n is permitted to be 0.  The C++ standard says nothing about what
  // the return value is when MANAGED_STACK_ADDRESS_BOEHM_GC_n == 0.
  MANAGED_STACK_ADDRESS_BOEHM_GC_Tp* allocate(size_type MANAGED_STACK_ADDRESS_BOEHM_GC_n, const void* = 0) {
    MANAGED_STACK_ADDRESS_BOEHM_GC_type_traits<MANAGED_STACK_ADDRESS_BOEHM_GC_Tp> traits;
    return static_cast<MANAGED_STACK_ADDRESS_BOEHM_GC_Tp *>(MANAGED_STACK_ADDRESS_BOEHM_GC_selective_alloc(MANAGED_STACK_ADDRESS_BOEHM_GC_n * sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_Tp),
                                                   traits.MANAGED_STACK_ADDRESS_BOEHM_GC_is_ptr_free,
                                                   true));
  }

  void deallocate(pointer __p, size_type /* MANAGED_STACK_ADDRESS_BOEHM_GC_n */) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
    { MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(__p); }

  size_type max_size() const MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
    { return static_cast<MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_SIZE_T>(-1) / sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_Tp); }

  void construct(pointer __p, const MANAGED_STACK_ADDRESS_BOEHM_GC_Tp& __val) { new(__p) MANAGED_STACK_ADDRESS_BOEHM_GC_Tp(__val); }
  void destroy(pointer __p) { __p->~MANAGED_STACK_ADDRESS_BOEHM_GC_Tp(); }
};

template<>
class gc_allocator_ignore_off_page<void> {
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_SIZE_T    size_type;
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_PTRDIFF_T difference_type;
  typedef void*       pointer;
  typedef const void* const_pointer;
  typedef void        value_type;

  template <class MANAGED_STACK_ADDRESS_BOEHM_GC_Tp1> struct rebind {
    typedef gc_allocator_ignore_off_page<MANAGED_STACK_ADDRESS_BOEHM_GC_Tp1> other;
  };
};

template <class MANAGED_STACK_ADDRESS_BOEHM_GC_T1, class MANAGED_STACK_ADDRESS_BOEHM_GC_T2>
inline bool operator==(const gc_allocator_ignore_off_page<MANAGED_STACK_ADDRESS_BOEHM_GC_T1>&,
                       const gc_allocator_ignore_off_page<MANAGED_STACK_ADDRESS_BOEHM_GC_T2>&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
{
  return true;
}

template <class MANAGED_STACK_ADDRESS_BOEHM_GC_T1, class MANAGED_STACK_ADDRESS_BOEHM_GC_T2>
inline bool operator!=(const gc_allocator_ignore_off_page<MANAGED_STACK_ADDRESS_BOEHM_GC_T1>&,
                       const gc_allocator_ignore_off_page<MANAGED_STACK_ADDRESS_BOEHM_GC_T2>&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
{
  return false;
}

// And the public traceable_allocator class.

// Note that we currently do not specialize the pointer-free case,
// since a pointer-free traceable container does not make that much sense,
// though it could become an issue due to abstraction boundaries.

template <class MANAGED_STACK_ADDRESS_BOEHM_GC_Tp>
class traceable_allocator {
public:
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_SIZE_T    size_type;
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_PTRDIFF_T difference_type;
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_Tp*       pointer;
  typedef const MANAGED_STACK_ADDRESS_BOEHM_GC_Tp* const_pointer;
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_Tp&       reference;
  typedef const MANAGED_STACK_ADDRESS_BOEHM_GC_Tp& const_reference;
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_Tp        value_type;

  template <class MANAGED_STACK_ADDRESS_BOEHM_GC_Tp1> struct rebind {
    typedef traceable_allocator<MANAGED_STACK_ADDRESS_BOEHM_GC_Tp1> other;
  };

  traceable_allocator() MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT {}
  traceable_allocator(const traceable_allocator&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT {}
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_MEMBER_TEMPLATES
    template <class MANAGED_STACK_ADDRESS_BOEHM_GC_Tp1> MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_EXPLICIT
    traceable_allocator(const traceable_allocator<MANAGED_STACK_ADDRESS_BOEHM_GC_Tp1>&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT {}
# endif
  ~traceable_allocator() MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT {}

  pointer address(reference MANAGED_STACK_ADDRESS_BOEHM_GC_x) const { return &MANAGED_STACK_ADDRESS_BOEHM_GC_x; }
  const_pointer address(const_reference MANAGED_STACK_ADDRESS_BOEHM_GC_x) const { return &MANAGED_STACK_ADDRESS_BOEHM_GC_x; }

  // MANAGED_STACK_ADDRESS_BOEHM_GC_n is permitted to be 0.  The C++ standard says nothing about what
  // the return value is when MANAGED_STACK_ADDRESS_BOEHM_GC_n == 0.
  MANAGED_STACK_ADDRESS_BOEHM_GC_Tp* allocate(size_type MANAGED_STACK_ADDRESS_BOEHM_GC_n, const void* = 0) {
    void * obj = MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_UNCOLLECTABLE(MANAGED_STACK_ADDRESS_BOEHM_GC_n * sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_Tp));
    if (0 == obj)
      MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOCATOR_THROW_OR_ABORT();
    return static_cast<MANAGED_STACK_ADDRESS_BOEHM_GC_Tp*>(obj);
  }

  void deallocate(pointer __p, size_type /* MANAGED_STACK_ADDRESS_BOEHM_GC_n */) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
    { MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(__p); }

  size_type max_size() const MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
    { return static_cast<MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_SIZE_T>(-1) / sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_Tp); }

  void construct(pointer __p, const MANAGED_STACK_ADDRESS_BOEHM_GC_Tp& __val) { new(__p) MANAGED_STACK_ADDRESS_BOEHM_GC_Tp(__val); }
  void destroy(pointer __p) { __p->~MANAGED_STACK_ADDRESS_BOEHM_GC_Tp(); }
};

template<>
class traceable_allocator<void> {
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_SIZE_T    size_type;
  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_PTRDIFF_T difference_type;
  typedef void*       pointer;
  typedef const void* const_pointer;
  typedef void        value_type;

  template <class MANAGED_STACK_ADDRESS_BOEHM_GC_Tp1> struct rebind {
    typedef traceable_allocator<MANAGED_STACK_ADDRESS_BOEHM_GC_Tp1> other;
  };
};

template <class MANAGED_STACK_ADDRESS_BOEHM_GC_T1, class MANAGED_STACK_ADDRESS_BOEHM_GC_T2>
inline bool operator==(const traceable_allocator<MANAGED_STACK_ADDRESS_BOEHM_GC_T1>&,
                       const traceable_allocator<MANAGED_STACK_ADDRESS_BOEHM_GC_T2>&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
{
  return true;
}

template <class MANAGED_STACK_ADDRESS_BOEHM_GC_T1, class MANAGED_STACK_ADDRESS_BOEHM_GC_T2>
inline bool operator!=(const traceable_allocator<MANAGED_STACK_ADDRESS_BOEHM_GC_T1>&,
                       const traceable_allocator<MANAGED_STACK_ADDRESS_BOEHM_GC_T2>&) MANAGED_STACK_ADDRESS_BOEHM_GC_NOEXCEPT
{
  return false;
}

#undef MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_PTRDIFF_T
#undef MANAGED_STACK_ADDRESS_BOEHM_GC_ALCTR_SIZE_T

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_NAMESPACE_ALLOCATOR
}
#endif

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOCATOR_H */
