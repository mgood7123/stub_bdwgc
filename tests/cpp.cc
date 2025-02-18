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

// This program tries to test the specific C++ functionality provided by
// gc_cpp.h that isn't tested by the more general test routines of the
// collector.

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#undef MANAGED_STACK_ADDRESS_BOEHM_GC_BUILD

#define MANAGED_STACK_ADDRESS_BOEHM_GC_DONT_INCL_WINDOWS_H
#include "gc_cpp.h"

#include <stdlib.h>
#include <string.h>

#define MANAGED_STACK_ADDRESS_BOEHM_GC_NAMESPACE_ALLOCATOR
#include "gc/gc_allocator.h"
using boehmgc::gc_allocator;
using boehmgc::gc_allocator_ignore_off_page;
using boehmgc::traceable_allocator;

# include "private/gcconfig.h"

# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_API_PRIV
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_API_PRIV MANAGED_STACK_ADDRESS_BOEHM_GC_API
# endif
extern "C" {
  MANAGED_STACK_ADDRESS_BOEHM_GC_API_PRIV void MANAGED_STACK_ADDRESS_BOEHM_GC_printf(const char * format, ...);
  /* Use GC private output to reach the same log file.  */
  /* Don't include gc_priv.h, since that may include Windows system     */
  /* header files that don't take kindly to this context.               */
}

#ifdef MSWIN32
# ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN 1
# endif
# define NOSERVICE
# include <windows.h>
#endif

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_NAME_CONFLICT
# define USE_GC MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(UseGC)
  struct foo * GC;
#else
# define USE_GC MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(GC)
#endif

#define my_assert( e ) \
    if (!(e)) { \
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf( "Assertion failure in " __FILE__ ", line %d: " #e "\n", \
                    __LINE__ ); \
        exit(1); }

#if defined(__powerpc64__) && !defined(__clang__) && MANAGED_STACK_ADDRESS_BOEHM_GC_GNUC_PREREQ(10, 0)
  /* Suppress "layout of aggregates ... has changed" GCC note. */
# define A_I_TYPE short
#else
# define A_I_TYPE int
#endif

# define LARGE_CPP_ITER_CNT 1000000

class A {public:
    /* An uncollectible class. */

    MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_EXPLICIT A(int iArg): i(static_cast<A_I_TYPE>(iArg)) {}
    void Test( int iArg ) {
        my_assert( i == iArg );}
    virtual ~A() {}
    A_I_TYPE i; };


class B: public MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(gc), public A { public:
    /* A collectible class. */

    MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_EXPLICIT B( int j ): A( j ) {}
    virtual ~B() {
        my_assert( deleting );}
    static void Deleting( int on ) {
        deleting = on;}
    static int deleting;};

int B::deleting = 0;

#define C_INIT_LEFT_RIGHT(arg_l, arg_r) \
    { \
        C *l = new C(arg_l); \
        C *r = new C(arg_r); \
        left = l; \
        right = r; \
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_is_heap_ptr(this)) { \
            MANAGED_STACK_ADDRESS_BOEHM_GC_END_STUBBORN_CHANGE(this); \
            MANAGED_STACK_ADDRESS_BOEHM_GC_reachable_here(l); \
            MANAGED_STACK_ADDRESS_BOEHM_GC_reachable_here(r); \
        } \
    }

class C: public MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(gc_cleanup), public A { public:
    /* A collectible class with cleanup and virtual multiple inheritance. */

    // The class uses dynamic memory/resource allocation, so provide both
    // a copy constructor and an assignment operator to workaround a cppcheck
    // warning.
    C(const C& c) : A(c.i), level(c.level), left(0), right(0) {
        if (level > 0)
            C_INIT_LEFT_RIGHT(*c.left, *c.right);
    }

    C& operator=(const C& c) {
        if (this != &c) {
            delete left;
            delete right;
            i = c.i;
            level = c.level;
            left = 0;
            right = 0;
            if (level > 0)
                C_INIT_LEFT_RIGHT(*c.left, *c.right);
        }
        return *this;
    }

    MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_EXPLICIT C( int levelArg ): A( levelArg ), level( levelArg ) {
        nAllocated++;
        if (level > 0) {
            C_INIT_LEFT_RIGHT(level - 1, level - 1);
        } else {
            left = right = 0;}}
    ~C() {
        this->A::Test( level );
        nFreed++;
        my_assert( level == 0 ?
                   left == 0 && right == 0 :
                   level == left->level + 1 && level == right->level + 1 );
        left = right = 0;
        level = -32456;}
    static void Test() {
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_is_incremental_mode() && nFreed < (nAllocated / 5) * 4) {
          // An explicit GC might be needed to reach the expected number
          // of the finalized objects.
          MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect();
        }
        my_assert(nFreed <= nAllocated);
#       ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
            my_assert(nFreed >= (nAllocated / 5) * 4 || MANAGED_STACK_ADDRESS_BOEHM_GC_get_find_leak());
#       endif
    }

    static int nFreed;
    static int nAllocated;
    int level;
    C* left;
    C* right;};

int C::nFreed = 0;
int C::nAllocated = 0;


class D: public MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(gc) { public:
    /* A collectible class with a static member function to be used as
    an explicit clean-up function supplied to ::new. */

    MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_EXPLICIT D( int iArg ): i( iArg ) {
        nAllocated++;}
    static void CleanUp( void* obj, void* data ) {
        D* self = static_cast<D*>(obj);
        nFreed++;
        my_assert(static_cast<MANAGED_STACK_ADDRESS_BOEHM_GC_word>(self->i)
                    == reinterpret_cast<MANAGED_STACK_ADDRESS_BOEHM_GC_word>(data));
    }
    static void Test() {
#       ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
            my_assert(nFreed >= (nAllocated / 5) * 4 || MANAGED_STACK_ADDRESS_BOEHM_GC_get_find_leak());
#       endif
    }

    int i;
    static int nFreed;
    static int nAllocated;};

int D::nFreed = 0;
int D::nAllocated = 0;


class E: public MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(gc_cleanup) { public:
    /* A collectible class with clean-up for use by F. */

    E() {
        nAllocated++;}
    ~E() {
        nFreed++;}

    static int nFreed;
    static int nAllocated;};

int E::nFreed = 0;
int E::nAllocated = 0;


class F: public E {public:
    /* A collectible class with clean-up, a base with clean-up, and a
    member with clean-up. */

    F() {
        nAllocatedF++;
    }

    ~F() {
        nFreedF++;
    }

    static void Test() {
#       ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
            my_assert(nFreedF >= (nAllocatedF / 5) * 4 || MANAGED_STACK_ADDRESS_BOEHM_GC_get_find_leak());
#       endif
        my_assert(2 * nFreedF == nFreed);
    }

    E e;
    static int nFreedF;
    static int nAllocatedF;
};

int F::nFreedF = 0;
int F::nAllocatedF = 0;


MANAGED_STACK_ADDRESS_BOEHM_GC_word Disguise( void* p ) {
    return MANAGED_STACK_ADDRESS_BOEHM_GC_HIDE_NZ_POINTER(p);
}

void* Undisguise( MANAGED_STACK_ADDRESS_BOEHM_GC_word i ) {
    return MANAGED_STACK_ADDRESS_BOEHM_GC_REVEAL_NZ_POINTER(i);
}

#define MANAGED_STACK_ADDRESS_BOEHM_GC_CHECKED_DELETE(p) \
    { \
      size_t freed_before = MANAGED_STACK_ADDRESS_BOEHM_GC_get_expl_freed_bytes_since_gc(); \
      delete p; /* the operator should invoke MANAGED_STACK_ADDRESS_BOEHM_GC_FREE() */ \
      size_t freed_after = MANAGED_STACK_ADDRESS_BOEHM_GC_get_expl_freed_bytes_since_gc(); \
      my_assert(freed_before != freed_after); \
    }

#define N_TESTS 7

#if ((defined(MSWIN32) && !defined(__MINGW32__)) || defined(MSWINCE)) \
    && !defined(NO_WINMAIN_ENTRY)
  int APIENTRY WinMain(HINSTANCE /* instance */, HINSTANCE /* prev */,
                       LPSTR cmd, int /* cmdShow */)
  {
    int argc = 0;
    char* argv[ 3 ];

#   if defined(CPPCHECK)
      MANAGED_STACK_ADDRESS_BOEHM_GC_noop1(reinterpret_cast<MANAGED_STACK_ADDRESS_BOEHM_GC_word>(&WinMain));
#   endif
    if (cmd != 0)
      for (argc = 1; argc < static_cast<int>(sizeof(argv) / sizeof(argv[0]));
           argc++) {
        // Parse the command-line string.  Non-reentrant strtok() is not used
        // to avoid complains of static analysis tools.  (And, strtok_r() is
        // not available on some platforms.)  The code is equivalent to:
        //   if (!(argv[argc] = strtok(argc == 1 ? cmd : 0, " \t"))) break;
        if (NULL == cmd) {
          argv[argc] = NULL;
          break;
        }
        for (; *cmd != '\0'; cmd++) {
          if (*cmd != ' ' && *cmd != '\t')
            break;
        }
        if ('\0' == *cmd) {
          argv[argc] = NULL;
          break;
        }
        argv[argc] = cmd;
        while (*(++cmd) != '\0') {
          if (*cmd == ' ' || *cmd == '\t')
            break;
        }
        if (*cmd != '\0') {
          *(cmd++) = '\0';
        } else {
          cmd = NULL;
        }
      }
#elif defined(MACOS)
  int main() {
    char* argv_[] = {"cpptest", "7"}; // MacOS doesn't have a command line
    argv = argv_;
    argc = sizeof(argv_) / sizeof(argv_[0]);
#else
  int main(int argc, char* argv[]) {
#endif

    MANAGED_STACK_ADDRESS_BOEHM_GC_set_all_interior_pointers(1);
                        /* needed due to C++ multiple inheritance used  */

#   ifdef TEST_MANUAL_VDB
      MANAGED_STACK_ADDRESS_BOEHM_GC_set_manual_vdb_allowed(1);
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_INIT();
#   ifndef NO_INCREMENTAL
      MANAGED_STACK_ADDRESS_BOEHM_GC_enable_incremental();
#   endif
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_get_find_leak())
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("This test program is not designed for leak detection mode\n");

    int i, iters, n;
    int *x = gc_allocator<int>().allocate(1);
    int *xio;
    xio = gc_allocator_ignore_off_page<int>().allocate(1);
    MANAGED_STACK_ADDRESS_BOEHM_GC_reachable_here(xio);
    int **xptr = traceable_allocator<int *>().allocate(1);
    *x = 29;
    if (!xptr) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Out of memory!\n");
      exit(69);
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE_AND_DIRTY(xptr, x);
    x = 0;
    if (argc != 2
        || (n = atoi(argv[1])) <= 0) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("usage: cpptest <number-of-iterations>\n"
                "Assuming %d iterations\n", N_TESTS);
      n = N_TESTS;
    }
#   ifdef LINT2
      if (n > 30 * 1000) n = 30 * 1000;
#   endif

    for (iters = 1; iters <= n; iters++) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf( "Starting iteration %d\n", iters );

            /* Allocate some uncollectible As and disguise their pointers.
            Later we'll check to see if the objects are still there.  We're
            checking to make sure these objects really are uncollectible. */
        MANAGED_STACK_ADDRESS_BOEHM_GC_word as[ 1000 ];
        MANAGED_STACK_ADDRESS_BOEHM_GC_word bs[ 1000 ];
        for (i = 0; i < 1000; i++) {
            as[ i ] = Disguise( new (MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(NoGC)) A(i) );
            bs[ i ] = Disguise( new (MANAGED_STACK_ADDRESS_BOEHM_GC_NS_QUALIFY(NoGC)) B(i) ); }

            /* Allocate a fair number of finalizable Cs, Ds, and Fs.
            Later we'll check to make sure they've gone away. */
        for (i = 0; i < 1000; i++) {
            C* c = new C( 2 );
            C c1( 2 );           /* stack allocation should work too */
            D* d;
            F* f;
            d = ::new (USE_GC, D::CleanUp,
                       reinterpret_cast<void*>(static_cast<MANAGED_STACK_ADDRESS_BOEHM_GC_word>(i))) D(i);
            MANAGED_STACK_ADDRESS_BOEHM_GC_reachable_here(d);
            f = new F;
            F** fa = new F*[1];
            fa[0] = f;
            (void)fa;
            delete[] fa;
            if (0 == i % 10)
                MANAGED_STACK_ADDRESS_BOEHM_GC_CHECKED_DELETE(c);
        }

            /* Allocate a very large number of collectible As and Bs and
            drop the references to them immediately, forcing many
            collections. */
        for (i = 0; i < LARGE_CPP_ITER_CNT; i++) {
            A* a;
            a = new (USE_GC) A( i );
            MANAGED_STACK_ADDRESS_BOEHM_GC_reachable_here(a);
            B* b;
            b = new B( i );
            (void)b;
            b = new (USE_GC) B( i );
            if (0 == i % 10) {
                B::Deleting( 1 );
                MANAGED_STACK_ADDRESS_BOEHM_GC_CHECKED_DELETE(b);
                B::Deleting( 0 );}
#           if defined(FINALIZE_ON_DEMAND) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION)
              MANAGED_STACK_ADDRESS_BOEHM_GC_invoke_finalizers();
#           endif
            }

            /* Make sure the uncollectible As and Bs are still there. */
        for (i = 0; i < 1000; i++) {
            A* a = static_cast<A*>(Undisguise(as[i]));
            B* b = static_cast<B*>(Undisguise(bs[i]));
            a->Test( i );
#           if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
              // Workaround for ASan/MSan: the linker uses operator delete
              // implementation from libclang_rt instead of gc_cpp (thus
              // causing incompatible alloc/free).
              MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(a);
#           else
              MANAGED_STACK_ADDRESS_BOEHM_GC_CHECKED_DELETE(a);
#           endif
            b->Test( i );
            B::Deleting( 1 );
            MANAGED_STACK_ADDRESS_BOEHM_GC_CHECKED_DELETE(b);
            B::Deleting( 0 );
#           if defined(FINALIZE_ON_DEMAND) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION)
                 MANAGED_STACK_ADDRESS_BOEHM_GC_invoke_finalizers();
#           endif
            }

            /* Make sure most of the finalizable Cs, Ds, and Fs have
            gone away. */
        C::Test();
        D::Test();
        F::Test();}

    x = *xptr;
    my_assert(29 == x[0]);
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("The test appears to have succeeded.\n");
    return 0;
}
