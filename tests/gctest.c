/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
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
 */

/* An incomplete test for the garbage collector.                */
/* Some more obscure entry points are not tested at all.        */
/* This must be compiled with the same flags used to build the  */
/* GC.  It uses GC internals to allow more precise results      */
/* checking for some of the tests.                              */

# ifdef HAVE_CONFIG_H
#   include "config.h"
# endif

# undef MANAGED_STACK_ADDRESS_BOEHM_GC_BUILD

#if (defined(DBG_HDRS_ALL) || defined(MAKE_BACK_GRAPH)) \
    && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG) && !defined(CPPCHECK)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG
#endif

#ifdef DEFAULT_VDB /* specified manually (e.g. passed to CFLAGS) */
# define TEST_DEFAULT_VDB
#endif

#if defined(CPPCHECK) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS) && !defined(_GNU_SOURCE)
# define _GNU_SOURCE 1
#endif
#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_THREADS_DISCOVERY
# undef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_THREAD_REDIRECTS
#endif
#include "gc.h"
#include "gc/javaxfc.h"

#ifndef NTHREADS /* Number of additional threads to fork. */
# define NTHREADS 5 /* Excludes main thread, which also runs a test. */
        /* In the single-threaded case, the number of times to rerun it. */
#endif

# if defined(_WIN32_WCE) && !defined(__GNUC__)
#   include <winbase.h>
/* #   define assert ASSERT */
# else
#   include <assert.h>  /* Not normally used, but handy for debugging.  */
# endif

#if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS) \
    && defined(_DEBUG) && (_MSC_VER >= 1900) /* VS 2015+ */
# ifndef _CRTDBG_MAP_ALLOC
#   define _CRTDBG_MAP_ALLOC
# endif
  /* This should be included before gc_priv.h (see the note about   */
  /* _malloca redefinition bug in gcconfig.h).                      */
# include <crtdbg.h> /* for _CrtDumpMemoryLeaks, _CrtSetDbgFlag */
#endif

#if (defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION) || defined(DBG_HDRS_ALL)) \
    && !defined(NO_TYPED_TEST)
# define NO_TYPED_TEST
#endif

#ifndef NO_TYPED_TEST
# include "gc/gc_typed.h"
#endif

#define NOT_GCBUILD
#include "private/gc_priv.h"    /* For output, locking,                 */
                                /* some statistics and gcconfig.h.      */

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PRINT_VERBOSE_STATS) || defined(GCTEST_PRINT_VERBOSE)
# define print_stats VERBOSE
# define INIT_PRINT_STATS /* empty */
#else
  /* Use own variable as MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats might not be visible.   */
  static int print_stats = 0;
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_READ_ENV_FILE
    /* GETENV uses GC internal function in this case.   */
#   define INIT_PRINT_STATS /* empty */
# else
#   define INIT_PRINT_STATS \
        { \
          if (0 != GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_PRINT_VERBOSE_STATS")) \
            print_stats = VERBOSE; \
          else if (0 != GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_PRINT_STATS")) \
            print_stats = 1; \
        }
# endif
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_PRINT_VERBOSE_STATS */

# ifdef PCR
#   include "th/PCR_ThCrSec.h"
#   include "th/PCR_Th.h"
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_printf printf
# endif

# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_PTHREADS)
#   include <pthread.h>
# endif

# if ((defined(DARWIN) && defined(MPROTECT_VDB) \
       && !defined(MAKE_BACK_GRAPH) && !defined(TEST_HANDLE_FORK)) \
      || (defined(THREADS) && !defined(CAN_HANDLE_FORK)) \
      || defined(HAVE_NO_FORK) || defined(USE_WINALLOC)) \
     && !defined(NO_TEST_HANDLE_FORK)
#   define NO_TEST_HANDLE_FORK
# endif

# ifndef NO_TEST_HANDLE_FORK
#   include <unistd.h>
#   include <sys/types.h>
#   include <sys/wait.h>
#   if defined(HANDLE_FORK) && defined(CAN_CALL_ATFORK)
#     define INIT_FORK_SUPPORT MANAGED_STACK_ADDRESS_BOEHM_GC_set_handle_fork(1)
                /* Causes abort in MANAGED_STACK_ADDRESS_BOEHM_GC_init on pthread_atfork failure.   */
#   elif !defined(TEST_FORK_WITHOUT_ATFORK)
#     define INIT_FORK_SUPPORT MANAGED_STACK_ADDRESS_BOEHM_GC_set_handle_fork(-1)
                /* Passing -1 implies fork() should be as well manually */
                /* surrounded with MANAGED_STACK_ADDRESS_BOEHM_GC_atfork_prepare/parent/child.      */
#   endif
# endif

# ifndef INIT_FORK_SUPPORT
#   define INIT_FORK_SUPPORT /* empty */
# endif

#ifdef PCR
# define FINALIZER_LOCK() PCR_ThCrSec_EnterSys()
# define FINALIZER_UNLOCK() PCR_ThCrSec_ExitSys()
#elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS)
  static pthread_mutex_t incr_lock = PTHREAD_MUTEX_INITIALIZER;
# define FINALIZER_LOCK() pthread_mutex_lock(&incr_lock)
# define FINALIZER_UNLOCK() pthread_mutex_unlock(&incr_lock)
#elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS)
  static CRITICAL_SECTION incr_cs;
# define FINALIZER_LOCK() EnterCriticalSection(&incr_cs)
# define FINALIZER_UNLOCK() LeaveCriticalSection(&incr_cs)
#else
# define FINALIZER_LOCK() (void)0
# define FINALIZER_UNLOCK() (void)0
#endif /* !THREADS */

#include <stdarg.h>

#ifdef TEST_MANUAL_VDB
# define INIT_MANUAL_VDB_ALLOWED MANAGED_STACK_ADDRESS_BOEHM_GC_set_manual_vdb_allowed(1)
#elif !defined(SMALL_CONFIG)
# define INIT_MANUAL_VDB_ALLOWED MANAGED_STACK_ADDRESS_BOEHM_GC_set_manual_vdb_allowed(0)
#else
# define INIT_MANUAL_VDB_ALLOWED /* empty */
#endif

#ifdef TEST_PAGES_EXECUTABLE
# define INIT_PAGES_EXECUTABLE MANAGED_STACK_ADDRESS_BOEHM_GC_set_pages_executable(1)
#else
# define INIT_PAGES_EXECUTABLE (void)0
#endif

#define CHECK_GCLIB_VERSION \
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_get_version() != (((MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_VAL_T)MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MAJOR << 16) \
                             | (MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MINOR << 8) | MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MICRO)) { \
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("libgc version mismatch\n"); \
      exit(1); \
    }

/* Call MANAGED_STACK_ADDRESS_BOEHM_GC_INIT only on platforms on which we think we really need it,  */
/* so that we can test automatic initialization on the rest.            */
#if defined(TEST_EXPLICIT_MANAGED_STACK_ADDRESS_BOEHM_GC_INIT) || defined(AIX) || defined(CYGWIN32) \
        || defined(DARWIN) || defined(HOST_ANDROID) \
        || (defined(MSWINCE) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WINMAIN_REDIRECT))
# define MANAGED_STACK_ADDRESS_BOEHM_GC_OPT_INIT MANAGED_STACK_ADDRESS_BOEHM_GC_INIT()
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_OPT_INIT /* empty */
#endif

#define INIT_FIND_LEAK \
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_get_find_leak()) {} else \
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("This test program is not designed for leak detection mode\n")

#ifdef NO_CLOCK
# define INIT_PERF_MEASUREMENT (void)0
#else
# define INIT_PERF_MEASUREMENT MANAGED_STACK_ADDRESS_BOEHM_GC_start_performance_measurement()
#endif

#define MANAGED_STACK_ADDRESS_BOEHM_GC_COND_INIT() \
    INIT_FORK_SUPPORT; INIT_MANUAL_VDB_ALLOWED; INIT_PAGES_EXECUTABLE; \
    MANAGED_STACK_ADDRESS_BOEHM_GC_OPT_INIT; CHECK_GCLIB_VERSION; \
    INIT_PRINT_STATS; INIT_FIND_LEAK; INIT_PERF_MEASUREMENT

#define CHECK_OUT_OF_MEMORY(p) \
            if (NULL == (p)) { \
              MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Out of memory\n"); \
              exit(69); \
            }

static void *checkOOM(void *p)
{
  CHECK_OUT_OF_MEMORY(p);
  return p;
}

/* Define AO primitives for a single-threaded mode. */
#ifndef AO_HAVE_compiler_barrier
  /* AO_t not defined. */
# define AO_t MANAGED_STACK_ADDRESS_BOEHM_GC_word
#endif
#ifndef AO_HAVE_load_acquire
  static AO_t AO_load_acquire(const volatile AO_t *addr)
  {
    AO_t result;

    FINALIZER_LOCK();
    result = *addr;
    FINALIZER_UNLOCK();
    return result;
  }
#endif
#ifndef AO_HAVE_store_release
  /* Not a macro as new_val argument should be evaluated before the lock. */
  static void AO_store_release(volatile AO_t *addr, AO_t new_val)
  {
    FINALIZER_LOCK();
    *addr = new_val;
    FINALIZER_UNLOCK();
  }
#endif
#ifndef AO_HAVE_fetch_and_add1
# define AO_fetch_and_add1(p) ((*(p))++)
                /* This is used only to update counters.        */
#endif

/* Allocation Statistics.  Synchronization is not strictly necessary.   */
static volatile AO_t uncollectable_count = 0;
static volatile AO_t collectable_count = 0;
static volatile AO_t atomic_count = 0;
static volatile AO_t realloc_count = 0;

static volatile AO_t extra_count = 0;
                                /* Amount of space wasted in cons node; */
                                /* also used in gcj_cons, mktree and    */
                                /* chktree (for other purposes).        */

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_FASTALLOC) && defined(AMIGA)
  EXTERN_C_BEGIN
  void MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_free_all_mem(void);
  EXTERN_C_END

  void Amiga_Fail(void){MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_free_all_mem();abort();}
# define FAIL Amiga_Fail()
#ifndef NO_TYPED_TEST
  void *MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_gctest_malloc_explicitly_typed(size_t lb, MANAGED_STACK_ADDRESS_BOEHM_GC_descr d){
    void *ret=MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_explicitly_typed(lb,d);
    if(ret==NULL){
              MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect();
              ret=MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_explicitly_typed(lb,d);
      if(ret==NULL){
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Out of memory, (typed allocations are not directly "
                      "supported with the MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_FASTALLOC option.)\n");
        FAIL;
      }
    }
    return ret;
  }
  void *MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_gctest_calloc_explicitly_typed(size_t a,size_t lb, MANAGED_STACK_ADDRESS_BOEHM_GC_descr d){
    void *ret=MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_explicitly_typed(a,lb,d);
    if(ret==NULL){
              MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect();
              ret=MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_explicitly_typed(a,lb,d);
      if(ret==NULL){
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Out of memory, (typed allocations are not directly "
                      "supported with the MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_FASTALLOC option.)\n");
        FAIL;
      }
    }
    return ret;
  }
# define MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_explicitly_typed(a,b) MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_gctest_malloc_explicitly_typed(a,b)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_explicitly_typed(a,b,c) MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_gctest_calloc_explicitly_typed(a,b,c)
#endif /* !NO_TYPED_TEST */

#else /* !AMIGA_FASTALLOC */

# if defined(PCR) || defined(LINT2)
#   define FAIL abort()
# else
#   define FAIL ABORT("Test failed")
# endif

#endif /* !AMIGA_FASTALLOC */

/* AT_END may be defined to exercise the interior pointer test  */
/* if the collector is configured with ALL_INTERIOR_POINTERS.   */
/* As it stands, this test should succeed with either           */
/* configuration.  In the FIND_LEAK configuration, it should    */
/* find lots of leaks, since we free almost nothing.            */

struct SEXPR {
    struct SEXPR * sexpr_car;
    struct SEXPR * sexpr_cdr;
};


typedef struct SEXPR * sexpr;

# define INT_TO_SEXPR(x) ((sexpr)(MANAGED_STACK_ADDRESS_BOEHM_GC_word)(x))
# define SEXPR_TO_INT(x) ((int)(MANAGED_STACK_ADDRESS_BOEHM_GC_word)(x))

# undef nil
# define nil (INT_TO_SEXPR(0))
# define car(x) ((x) -> sexpr_car)
# define cdr(x) ((x) -> sexpr_cdr)
# define is_nil(x) ((x) == nil)

/* Silly implementation of Lisp cons.  Intentionally wastes lots of     */
/* space to test collector.                                             */
# ifdef VERY_SMALL_CONFIG
#   define cons small_cons
# else
static sexpr cons(sexpr x, sexpr y)
{
    sexpr r;
    int *p;
    unsigned my_extra = (unsigned)AO_fetch_and_add1(&extra_count) % 5000;

    r = (sexpr)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(sizeof(struct SEXPR) + my_extra));
    AO_fetch_and_add1(&collectable_count);
    for (p = (int *)r;
         (MANAGED_STACK_ADDRESS_BOEHM_GC_word)p < (MANAGED_STACK_ADDRESS_BOEHM_GC_word)r + my_extra + sizeof(struct SEXPR); p++) {
        if (*p) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Found nonzero at %p - allocator is broken\n",
                      (void *)p);
            FAIL;
        }
        *p = (int)((13 << 11) + ((p - (int *)r) & 0xfff));
    }
#   ifdef AT_END
        r = (sexpr)((char *)r + (my_extra & ~7U));
#   endif
    r -> sexpr_car = x;
    MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE_AND_DIRTY(&r->sexpr_cdr, y);
    MANAGED_STACK_ADDRESS_BOEHM_GC_reachable_here(x);
    return r;
}
# endif

#include "gc/gc_mark.h"

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT

#include "gc/gc_gcj.h"

/* The following struct emulates the vtable in gcj.     */
/* This assumes the default value of MARK_DESCR_OFFSET. */
struct fake_vtable {
  void * dummy;         /* class pointer in real gcj.   */
  MANAGED_STACK_ADDRESS_BOEHM_GC_word descr;
};

struct fake_vtable gcj_class_struct1 = { 0, sizeof(struct SEXPR)
                                            + sizeof(struct fake_vtable *) };
                        /* length based descriptor.     */
struct fake_vtable gcj_class_struct2 =
                        { 0, ((MANAGED_STACK_ADDRESS_BOEHM_GC_word)3 << (CPP_WORDSZ - 3)) | MANAGED_STACK_ADDRESS_BOEHM_GC_DS_BITMAP};
                        /* Bitmap based descriptor.     */

static struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry *MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK fake_gcj_mark_proc(MANAGED_STACK_ADDRESS_BOEHM_GC_word *addr,
                                        struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry *mark_stack_ptr,
                                        struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry *mark_stack_limit,
                                        MANAGED_STACK_ADDRESS_BOEHM_GC_word env)
{
    sexpr x;

    if (1 == env) {
        /* Object allocated with debug allocator.       */
        addr = (MANAGED_STACK_ADDRESS_BOEHM_GC_word *)MANAGED_STACK_ADDRESS_BOEHM_GC_USR_PTR_FROM_BASE(addr);
    }
    x = (sexpr)(addr + 1); /* Skip the vtable pointer. */
    mark_stack_ptr = MANAGED_STACK_ADDRESS_BOEHM_GC_MARK_AND_PUSH(
                              (void *)(x -> sexpr_cdr), mark_stack_ptr,
                              mark_stack_limit, (void * *)&(x -> sexpr_cdr));
    mark_stack_ptr = MANAGED_STACK_ADDRESS_BOEHM_GC_MARK_AND_PUSH(
                              (void *)(x -> sexpr_car), mark_stack_ptr,
                              mark_stack_limit, (void * *)&(x -> sexpr_car));
    return mark_stack_ptr;
}

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT */

static sexpr small_cons(sexpr x, sexpr y)
{
    sexpr r = MANAGED_STACK_ADDRESS_BOEHM_GC_NEW(struct SEXPR);

    CHECK_OUT_OF_MEMORY(r);
    AO_fetch_and_add1(&collectable_count);
    r -> sexpr_car = x;
    MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE_AND_DIRTY(&r->sexpr_cdr, y);
    MANAGED_STACK_ADDRESS_BOEHM_GC_reachable_here(x);
    return r;
}

#ifdef NO_CONS_ATOMIC_LEAF
# define small_cons_leaf(x) small_cons(INT_TO_SEXPR(x), nil)
#else
  static sexpr small_cons_leaf(int x)
  {
    sexpr r = (sexpr)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_ATOMIC(sizeof(struct SEXPR)));

    AO_fetch_and_add1(&atomic_count);
    r -> sexpr_car = INT_TO_SEXPR(x);
    r -> sexpr_cdr = nil;
    return r;
  }
#endif

static sexpr small_cons_uncollectable(sexpr x, sexpr y)
{
    sexpr r = (sexpr)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_UNCOLLECTABLE(sizeof(struct SEXPR)));

    AO_fetch_and_add1(&uncollectable_count);
    r -> sexpr_cdr = (sexpr)(~(MANAGED_STACK_ADDRESS_BOEHM_GC_word)y);
    MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE_AND_DIRTY(&r->sexpr_car, x);
    return r;
}

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT
  static sexpr gcj_cons(sexpr x, sexpr y)
  {
    sexpr result;
    MANAGED_STACK_ADDRESS_BOEHM_GC_word cnt = (MANAGED_STACK_ADDRESS_BOEHM_GC_word)AO_fetch_and_add1(&extra_count);
    void *d = (cnt & 1) != 0 ? &gcj_class_struct1 : &gcj_class_struct2;
    size_t lb = sizeof(struct SEXPR) + sizeof(struct fake_vtable*);
    void *r = (cnt & 2) != 0 ? MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_MALLOC_IGNORE_OFF_PAGE(lb
                                        + (cnt <= HBLKSIZE / 2 ? cnt : 0), d)
                             : MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_MALLOC(lb, d);

    CHECK_OUT_OF_MEMORY(r);
    AO_fetch_and_add1(&collectable_count);
    result = (sexpr)((MANAGED_STACK_ADDRESS_BOEHM_GC_word *)r + 1);
    result -> sexpr_car = x;
    MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE_AND_DIRTY(&result->sexpr_cdr, y);
    MANAGED_STACK_ADDRESS_BOEHM_GC_reachable_here(x);
    return result;
  }
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT */

/* Return reverse(x) concatenated with y */
static sexpr reverse1(sexpr x, sexpr y)
{
    if (is_nil(x)) {
        return y;
    } else {
        return reverse1(cdr(x), cons(car(x), y));
    }
}

static sexpr reverse(sexpr x)
{
#   ifdef TEST_WITH_SYSTEM_MALLOC
      MANAGED_STACK_ADDRESS_BOEHM_GC_noop1(MANAGED_STACK_ADDRESS_BOEHM_GC_HIDE_NZ_POINTER(checkOOM(malloc(100000))));
#   endif
    return reverse1(x, nil);
}

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS
  /* TODO: Implement for Win32 */

  static void *do_gcollect(void *arg)
  {
    if (print_stats)
      MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Collect from a standalone thread\n");
    MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect();
    return arg;
  }

  static void collect_from_other_thread(void)
  {
    pthread_t t;
    int code = pthread_create(&t, NULL, do_gcollect, NULL /* arg */);

    if (code != 0) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("gcollect thread creation failed, errno= %d\n", code);
      FAIL;
    }
    code = pthread_join(t, NULL);
    if (code != 0) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("gcollect thread join failed, errno= %d\n", code);
      FAIL;
    }
  }

# define MAX_GCOLLECT_THREADS ((NTHREADS+2)/3)
  static volatile AO_t gcollect_threads_cnt = 0;
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS */

static sexpr ints(int low, int up)
{
    if (up < 0 ? low > -up : low > up) {
        if (up < 0) {
#           ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS
                if (AO_fetch_and_add1(&gcollect_threads_cnt) + 1
                        <= MAX_GCOLLECT_THREADS) {
                    collect_from_other_thread();
                    return nil;
                }
#           endif
            MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect_and_unmap();
        }
        return nil;
    } else {
        return small_cons(small_cons_leaf(low), ints(low + 1, up));
    }
}

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT
/* Return reverse(x) concatenated with y */
static sexpr gcj_reverse1(sexpr x, sexpr y)
{
    if (is_nil(x)) {
        return y;
    } else {
        return gcj_reverse1(cdr(x), gcj_cons(car(x), y));
    }
}

static sexpr gcj_reverse(sexpr x)
{
    return gcj_reverse1(x, nil);
}

static sexpr gcj_ints(int low, int up)
{
    if (low > up) {
        return nil;
    } else {
        return gcj_cons(gcj_cons(INT_TO_SEXPR(low), nil), gcj_ints(low+1, up));
    }
}
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT */

/* To check uncollectible allocation we build lists with disguised cdr  */
/* pointers, and make sure they don't go away.                          */
static sexpr uncollectable_ints(int low, int up)
{
    if (low > up) {
        return nil;
    } else {
        return small_cons_uncollectable(small_cons_leaf(low),
                                        uncollectable_ints(low+1, up));
    }
}

static void check_ints(sexpr list, int low, int up)
{
    if (is_nil(list)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("list is nil\n");
        FAIL;
    }
    if (SEXPR_TO_INT(car(car(list))) != low) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf(
           "List reversal produced incorrect list - collector is broken\n");
        FAIL;
    }
    if (low == up) {
        if (cdr(list) != nil) {
           MANAGED_STACK_ADDRESS_BOEHM_GC_printf("List too long - collector is broken\n");
           FAIL;
        }
    } else {
        check_ints(cdr(list), low+1, up);
    }
}

# define UNCOLLECTABLE_CDR(x) (sexpr)(~(MANAGED_STACK_ADDRESS_BOEHM_GC_word)cdr(x))

static void check_uncollectable_ints(sexpr list, int low, int up)
{
    if (SEXPR_TO_INT(car(car(list))) != low) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Uncollectable list corrupted - collector is broken\n");
        FAIL;
    }
    if (low == up) {
      if (UNCOLLECTABLE_CDR(list) != nil) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Uncollectable list too long - collector is broken\n");
        FAIL;
      }
    } else {
        check_uncollectable_ints(UNCOLLECTABLE_CDR(list), low+1, up);
    }
}

#ifdef PRINT_AND_CHECK_INT_LIST
  /* The following might be useful for debugging. */

  static void print_int_list(sexpr x)
  {
    if (is_nil(x)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("NIL\n");
    } else {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("(%d)", SEXPR_TO_INT(car(car(x))));
        if (!is_nil(cdr(x))) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf(", ");
            print_int_list(cdr(x));
        } else {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("\n");
        }
    }
  }

  static void check_marks_int_list(sexpr x)
  {
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_is_marked(x)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("[unm:%p]", (void *)x);
    } else {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("[mkd:%p]", (void *)x);
    }
    if (is_nil(x)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("NIL\n");
    } else {
        if (!MANAGED_STACK_ADDRESS_BOEHM_GC_is_marked(car(x)))
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("[unm car:%p]", (void *)car(x));
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("(%d)", SEXPR_TO_INT(car(car(x))));
        if (!is_nil(cdr(x))) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf(", ");
            check_marks_int_list(cdr(x));
        } else {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("\n");
        }
    }
  }
#endif /* PRINT_AND_CHECK_INT_LIST */

/* A tiny list reversal test to check thread creation.  */
#ifdef THREADS
# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ENABLE_SUSPEND_THREAD)
#   include "gc/javaxfc.h"
# endif

# ifdef VERY_SMALL_CONFIG
#   define TINY_REVERSE_UPPER_VALUE 4
# else
#   define TINY_REVERSE_UPPER_VALUE 10
# endif

  static void tiny_reverse_test_inner(void)
  {
    int i;

    for (i = 0; i < 5; ++i) {
      check_ints(reverse(reverse(ints(1, TINY_REVERSE_UPPER_VALUE))),
                 1, TINY_REVERSE_UPPER_VALUE);
    }
  }

# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS)
    static void*
# elif !defined(MSWINCE) && !defined(MSWIN_XBOX1) && !defined(NO_CRT) \
       && !defined(NO_TEST_ENDTHREADEX)
#   define TEST_ENDTHREADEX
    static unsigned __stdcall
# else
    static DWORD __stdcall
# endif
  tiny_reverse_test(void *p_resumed)
  {
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ENABLE_SUSPEND_THREAD) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_OSF1_THREADS) \
       && defined(SIGNAL_BASED_STOP_WORLD)
      if (p_resumed != NULL) {
        /* Test self-suspend.   */
        MANAGED_STACK_ADDRESS_BOEHM_GC_suspend_thread(pthread_self());
        AO_store_release((volatile AO_t *)p_resumed, (AO_t)TRUE);
      }
#   else
      (void)p_resumed;
#   endif
    tiny_reverse_test_inner();
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ENABLE_SUSPEND_THREAD)
      /* Force collection from a thread. */
      MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect();
#   endif
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_CANCEL)
      {
        static volatile AO_t tiny_cancel_cnt = 0;

        if (AO_fetch_and_add1(&tiny_cancel_cnt) % 3 == 0
            && MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_cancel(pthread_self()) != 0) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("pthread_cancel failed\n");
          FAIL;
        }
      }
#   endif
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_HAVE_PTHREAD_EXIT) \
       || (defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS))
      {
        static volatile AO_t tiny_exit_cnt = 0;

        if ((AO_fetch_and_add1(&tiny_exit_cnt) & 1) == 0) {
#         ifdef TEST_ENDTHREADEX
            MANAGED_STACK_ADDRESS_BOEHM_GC_endthreadex(0);
#         elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS)
            MANAGED_STACK_ADDRESS_BOEHM_GC_ExitThread(0);
#         else
            MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_exit(p_resumed);
#         endif
        }
      }
#   endif
    return 0;
  }

# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS)
    static void fork_a_thread(void)
    {
      pthread_t t;
      int code;
#     ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ENABLE_SUSPEND_THREAD
        static volatile AO_t forked_cnt = 0;
        volatile AO_t *p_resumed = NULL;

        if (AO_fetch_and_add1(&forked_cnt) % 2 == 0) {
          p_resumed = MANAGED_STACK_ADDRESS_BOEHM_GC_NEW(AO_t);
          CHECK_OUT_OF_MEMORY(p_resumed);
          AO_fetch_and_add1(&collectable_count);
        }
#     else
#       define p_resumed NULL
#     endif
      code = pthread_create(&t, NULL, tiny_reverse_test, (void*)p_resumed);
      if (code != 0) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Small thread creation failed %d\n", code);
        FAIL;
      }
#     if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ENABLE_SUSPEND_THREAD) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_OSF1_THREADS) \
         && defined(SIGNAL_BASED_STOP_WORLD)
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_is_thread_suspended(t) && NULL == p_resumed) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Running thread should be not suspended\n");
          FAIL;
        }
        MANAGED_STACK_ADDRESS_BOEHM_GC_suspend_thread(t); /* might be already self-suspended */
        if (!MANAGED_STACK_ADDRESS_BOEHM_GC_is_thread_suspended(t)) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Thread expected to be suspended\n");
          FAIL;
        }
        MANAGED_STACK_ADDRESS_BOEHM_GC_suspend_thread(t); /* should be no-op */
        for (;;) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_resume_thread(t);
          if (NULL == p_resumed || AO_load_acquire(p_resumed))
            break;
          MANAGED_STACK_ADDRESS_BOEHM_GC_collect_a_little();
        }
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_is_thread_suspended(t)) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Resumed thread should be not suspended\n");
          FAIL;
        }
        MANAGED_STACK_ADDRESS_BOEHM_GC_resume_thread(t); /* should be no-op */
        if (NULL == p_resumed)
          MANAGED_STACK_ADDRESS_BOEHM_GC_collect_a_little();
        /* Thread could be running or already terminated (but not joined). */
        MANAGED_STACK_ADDRESS_BOEHM_GC_suspend_thread(t);
        MANAGED_STACK_ADDRESS_BOEHM_GC_collect_a_little();
        if (!MANAGED_STACK_ADDRESS_BOEHM_GC_is_thread_suspended(t)) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Thread expected to be suspended\n");
          FAIL;
        }
        MANAGED_STACK_ADDRESS_BOEHM_GC_resume_thread(t);
#     endif
      if ((code = pthread_join(t, 0)) != 0) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Small thread join failed, errno= %d\n", code);
        FAIL;
      }
    }

# elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS)
    static void fork_a_thread(void)
    {
        HANDLE h;
#       ifdef TEST_ENDTHREADEX
          unsigned thread_id;

          h = (HANDLE)MANAGED_STACK_ADDRESS_BOEHM_GC_beginthreadex(NULL /* security */,
                                       0 /* stack_size */, tiny_reverse_test,
                                       NULL /* arglist */, 0 /* initflag */,
                                       &thread_id);
#       else
          DWORD thread_id;

          h = CreateThread((SECURITY_ATTRIBUTES *)NULL, (MANAGED_STACK_ADDRESS_BOEHM_GC_word)0,
                           tiny_reverse_test, NULL, (DWORD)0, &thread_id);
                                /* Explicitly specify types of the      */
                                /* arguments to test the prototype.     */
#       endif
        if (h == (HANDLE)NULL) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Small thread creation failed, errcode= %d\n",
                      (int)GetLastError());
            FAIL;
        }
        if (WaitForSingleObject(h, INFINITE) != WAIT_OBJECT_0) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Small thread wait failed, errcode= %d\n",
                      (int)GetLastError());
            FAIL;
        }
    }
# endif

#endif

static void test_generic_malloc_or_special(void *p) {
  size_t size;
  int kind;
  void *p2;

  kind = MANAGED_STACK_ADDRESS_BOEHM_GC_get_kind_and_size(p, &size);
  if (size != MANAGED_STACK_ADDRESS_BOEHM_GC_size(p)) {
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_get_kind_and_size returned size not matching MANAGED_STACK_ADDRESS_BOEHM_GC_size\n");
    FAIL;
  }
  p2 = checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_GENERIC_OR_SPECIAL_MALLOC(10, kind));
  if (MANAGED_STACK_ADDRESS_BOEHM_GC_get_kind_and_size(p2, NULL) != kind) {
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_generic_or_special_malloc:"
              " unexpected kind of returned object\n");
    FAIL;
  }
  MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(p2);
}

/* Try to force a to be strangely aligned */
volatile struct A_s {
  char dummy;
  AO_t aa;
} A;
#define a_set(p) AO_store_release(&A.aa, (AO_t)(p))
#define a_get() (sexpr)AO_load_acquire(&A.aa)

/*
 * Repeatedly reverse lists built out of very different sized cons cells.
 * Check that we didn't lose anything.
 */
static void *MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK reverse_test_inner(void *data)
{
    int i;
    sexpr b;
    sexpr c;
    sexpr d;
    sexpr e;
    sexpr *f, *g, *h;

    if (data == 0) {
      /* This stack frame is not guaranteed to be scanned. */
      return MANAGED_STACK_ADDRESS_BOEHM_GC_call_with_gc_active(reverse_test_inner, (void*)(MANAGED_STACK_ADDRESS_BOEHM_GC_word)1);
    }

# ifndef BIG
#   if defined(MACOS) \
       || (defined(UNIX_LIKE) && defined(NO_GETCONTEXT)) /* e.g. musl */
      /* Assume 128 KB stacks at least. */
#     if defined(__aarch64__) || defined(__s390x__)
#       define BIG 600
#     else
#       define BIG 1000
#     endif
#   elif defined(PCR)
      /* PCR default stack is 100 KB.  Stack frames are up to 120 bytes. */
#     define BIG 700
#   elif defined(MSWINCE) || defined(EMBOX) || defined(RTEMS)
      /* WinCE only allows 64 KB stacks. */
#     define BIG 500
#   elif defined(EMSCRIPTEN) || defined(OSF1)
      /* Wasm reports "Maximum call stack size exceeded" error otherwise. */
      /* OSF has limited stack space by default, and large frames. */
#     define BIG 200
#   elif defined(__MACH__) && defined(__ppc64__)
#     define BIG 2500
#   else
#     define BIG 4500
#   endif
# endif

    a_set(ints(1, 49));
    b = ints(1, 50);
    c = ints(1, -BIG); /* force garbage collection inside */
    d = uncollectable_ints(1, 100);
    test_generic_malloc_or_special(d);
    e = uncollectable_ints(1, 1);
    /* Check that realloc updates object descriptors correctly */
    f = (sexpr *)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(4 * sizeof(sexpr)));
    AO_fetch_and_add1(&collectable_count);
    f = (sexpr *)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_REALLOC((void *)f, 6 * sizeof(sexpr)));
    AO_fetch_and_add1(&realloc_count);
    MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE_AND_DIRTY(f + 5, ints(1, 17));
    g = (sexpr *)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(513 * sizeof(sexpr)));
    AO_fetch_and_add1(&collectable_count);
    test_generic_malloc_or_special(g);
    g = (sexpr *)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_REALLOC((void *)g, 800 * sizeof(sexpr)));
    AO_fetch_and_add1(&realloc_count);
    MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE_AND_DIRTY(g + 799, ints(1, 18));
    h = (sexpr *)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(1025 * sizeof(sexpr)));
    AO_fetch_and_add1(&collectable_count);
    h = (sexpr *)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_REALLOC((void *)h, 2000 * sizeof(sexpr)));
    AO_fetch_and_add1(&realloc_count);
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT
      MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE_AND_DIRTY(h + 1999, gcj_ints(1, 200));
      for (i = 0; i < 51; ++i) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE_AND_DIRTY(h + 1999, gcj_reverse(h[1999]));
      }
      /* Leave it as the reversed list for now. */
#   else
      MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE_AND_DIRTY(h + 1999, ints(1, 200));
#   endif
    /* Try to force some collections and reuse of small list elements */
    for (i = 0; i < 10; i++) {
      (void)ints(1, BIG);
    }
    /* Superficially test interior pointer recognition on stack */
    c = (sexpr)((char *)c + sizeof(char *));
    d = (sexpr)((char *)d + sizeof(char *));

    MANAGED_STACK_ADDRESS_BOEHM_GC_FREE((void *)e);

    check_ints(b,1,50);
# ifdef PRINT_AND_CHECK_INT_LIST
    print_int_list(b);
    check_marks_int_list(b);
# endif
# ifndef EMSCRIPTEN
    check_ints(a_get(),1,49);
# else
    /* FIXME: gctest fails unless check_ints(a_get(), ...) are skipped. */
# endif
    for (i = 0; i < 50; i++) {
        check_ints(b,1,50);
        b = reverse(reverse(b));
    }
    check_ints(b,1,50);
# ifndef EMSCRIPTEN
    check_ints(a_get(),1,49);
# endif
    for (i = 0; i < 10 * (NTHREADS+1); i++) {
#       if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS)
#         if NTHREADS > 0
            if (i % 10 == 0) fork_a_thread();
#         else
            MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((MANAGED_STACK_ADDRESS_BOEHM_GC_word)(MANAGED_STACK_ADDRESS_BOEHM_GC_funcptr_uint)&fork_a_thread);
#         endif
#       endif
        /* This maintains the invariant that a always points to a list  */
        /* of 49 integers.  Thus, this is thread safe without locks,    */
        /* assuming acquire/release barriers in a_get/set() and atomic  */
        /* pointer assignments (otherwise, e.g., check_ints() may see   */
        /* an uninitialized object returned by MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC).              */
        a_set(reverse(reverse(a_get())));
#       if !defined(AT_END) && !defined(THREADS)
          /* This is not thread safe, since realloc explicitly deallocates */
          a_set(checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_REALLOC(a_get(), (i & 1) != 0 ? 500 : 8200)));
          AO_fetch_and_add1(&realloc_count);
#       endif
    }
# ifndef EMSCRIPTEN
    check_ints(a_get(),1,49);
# endif
    check_ints(b,1,50);

    /* Restore c and d values. */
    c = (sexpr)((char *)c - sizeof(char *));
    d = (sexpr)((char *)d - sizeof(char *));

    check_ints(c,1,BIG);
    check_uncollectable_ints(d, 1, 100);
    check_ints(f[5], 1,17);
    check_ints(g[799], 1,18);
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT
      MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE_AND_DIRTY(h + 1999, gcj_reverse(h[1999]));
#   endif
    check_ints(h[1999], 1,200);
#   ifndef THREADS
      a_set(NULL);
#   endif
    *(sexpr volatile *)(MANAGED_STACK_ADDRESS_BOEHM_GC_word)&b = 0;
    *(sexpr volatile *)(MANAGED_STACK_ADDRESS_BOEHM_GC_word)&c = 0;
    return 0;
}

static void reverse_test(void)
{
    /* Test MANAGED_STACK_ADDRESS_BOEHM_GC_do_blocking/MANAGED_STACK_ADDRESS_BOEHM_GC_call_with_gc_active. */
    (void)MANAGED_STACK_ADDRESS_BOEHM_GC_do_blocking(reverse_test_inner, 0);
}

/*
 * The rest of this builds balanced binary trees, checks that they don't
 * disappear, and tests finalization.
 */
typedef struct treenode {
    int level;
    struct treenode * lchild;
    struct treenode * rchild;
} tn;

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
  int finalizable_count = 0;
#endif

int finalized_count = 0;
int dropped_something = 0;

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
  static void MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK finalizer(void *obj, void *client_data)
  {
    tn *t = (tn *)obj;

    FINALIZER_LOCK();
    if ((int)(MANAGED_STACK_ADDRESS_BOEHM_GC_word)client_data != t -> level) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Wrong finalization data - collector is broken\n");
      FAIL;
    }
    finalized_count++;
    t -> level = -1;    /* detect duplicate finalization immediately */
    FINALIZER_UNLOCK();
  }

  static void MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK dummy_finalizer(void *obj, void *client_data)
  {
    UNUSED_ARG(obj);
    UNUSED_ARG(client_data);
  }
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION */

# define MAX_FINALIZED_PER_THREAD 4000

#define MAX_FINALIZED ((NTHREADS+1) * MAX_FINALIZED_PER_THREAD)

#if !defined(MACOS)
  MANAGED_STACK_ADDRESS_BOEHM_GC_FAR MANAGED_STACK_ADDRESS_BOEHM_GC_word live_indicators[MAX_FINALIZED] = {0};
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_LONG_REFS_NOT_NEEDED
    MANAGED_STACK_ADDRESS_BOEHM_GC_FAR void *live_long_refs[MAX_FINALIZED] = { NULL };
# endif
#else
  /* Too big for THINK_C. have to allocate it dynamically. */
  MANAGED_STACK_ADDRESS_BOEHM_GC_word *live_indicators = 0;
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_LONG_REFS_NOT_NEEDED
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_LONG_REFS_NOT_NEEDED
# endif
#endif

int live_indicators_count = 0;

static tn * mktree(int n)
{
    tn * result = MANAGED_STACK_ADDRESS_BOEHM_GC_NEW(tn);
    tn * left, * right;

    CHECK_OUT_OF_MEMORY(result);
    AO_fetch_and_add1(&collectable_count);
#   if defined(MACOS)
        /* get around static data limitations. */
        if (!live_indicators) {
          live_indicators =
                    (MANAGED_STACK_ADDRESS_BOEHM_GC_word*)NewPtrClear(MAX_FINALIZED * sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_word));
          CHECK_OUT_OF_MEMORY(live_indicators);
        }
#   endif
    if (0 == n) return NULL;
    result -> level = n;
    result -> lchild = left = mktree(n - 1);
    result -> rchild = right = mktree(n - 1);
    if (AO_fetch_and_add1(&extra_count) % 17 == 0 && n >= 2) {
        tn * tmp;

        CHECK_OUT_OF_MEMORY(left);
        tmp = left -> rchild;
        CHECK_OUT_OF_MEMORY(right);
        MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE_AND_DIRTY(&left->rchild, right->lchild);
        MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE_AND_DIRTY(&right->lchild, tmp);
    }
    if (AO_fetch_and_add1(&extra_count) % 119 == 0) {
#       ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
          int my_index;
          void **new_link = MANAGED_STACK_ADDRESS_BOEHM_GC_NEW(void *);

          CHECK_OUT_OF_MEMORY(new_link);
          AO_fetch_and_add1(&collectable_count);
#       endif
        {
          FINALIZER_LOCK();
                /* Losing a count here causes erroneous report of failure. */
#         ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
            finalizable_count++;
            my_index = live_indicators_count++;
#         endif
          FINALIZER_UNLOCK();
        }

#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
      if (!MANAGED_STACK_ADDRESS_BOEHM_GC_get_find_leak()) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_REGISTER_FINALIZER((void *)result, finalizer, (void *)(MANAGED_STACK_ADDRESS_BOEHM_GC_word)n,
                              (MANAGED_STACK_ADDRESS_BOEHM_GC_finalization_proc *)0, (void * *)0);
        if (my_index >= MAX_FINALIZED) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("live_indicators overflowed\n");
            FAIL;
        }
        live_indicators[my_index] = 13;
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_GENERAL_REGISTER_DISAPPEARING_LINK(
                    (void **)(&(live_indicators[my_index])), result) != 0) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_general_register_disappearing_link failed\n");
            FAIL;
        }
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_move_disappearing_link((void **)(&(live_indicators[my_index])),
                    (void **)(&(live_indicators[my_index]))) != MANAGED_STACK_ADDRESS_BOEHM_GC_SUCCESS) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_move_disappearing_link(link,link) failed\n");
            FAIL;
        }
        *new_link = (void *)live_indicators[my_index];
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_move_disappearing_link((void **)(&(live_indicators[my_index])),
                                      new_link) != MANAGED_STACK_ADDRESS_BOEHM_GC_SUCCESS) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_move_disappearing_link(new_link) failed\n");
            FAIL;
        }
        /* Note: if other thread is performing fork at this moment,     */
        /* then the stack of the current thread is dropped (together    */
        /* with new_link variable) in the child process, and            */
        /* MANAGED_STACK_ADDRESS_BOEHM_GC_dl_hashtbl entry with the link equal to new_link will be  */
        /* removed when a collection occurs (as expected).              */
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_unregister_disappearing_link(new_link) == 0) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_unregister_disappearing_link failed\n");
            FAIL;
        }
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_move_disappearing_link((void **)(&(live_indicators[my_index])),
                                      new_link) != MANAGED_STACK_ADDRESS_BOEHM_GC_NOT_FOUND) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_move_disappearing_link(new_link) failed 2\n");
            FAIL;
        }
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_GENERAL_REGISTER_DISAPPEARING_LINK(
                    (void **)(&(live_indicators[my_index])), result) != 0) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_general_register_disappearing_link failed 2\n");
            FAIL;
        }
#       ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_LONG_REFS_NOT_NEEDED
          if (MANAGED_STACK_ADDRESS_BOEHM_GC_REGISTER_LONG_LINK(&live_long_refs[my_index], result) != 0) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_register_long_link failed\n");
            FAIL;
          }
          if (MANAGED_STACK_ADDRESS_BOEHM_GC_move_long_link(&live_long_refs[my_index],
                                &live_long_refs[my_index]) != MANAGED_STACK_ADDRESS_BOEHM_GC_SUCCESS) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_move_long_link(link,link) failed\n");
            FAIL;
          }
          *new_link = live_long_refs[my_index];
          if (MANAGED_STACK_ADDRESS_BOEHM_GC_move_long_link(&live_long_refs[my_index],
                                new_link) != MANAGED_STACK_ADDRESS_BOEHM_GC_SUCCESS) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_move_long_link(new_link) failed\n");
            FAIL;
          }
          if (MANAGED_STACK_ADDRESS_BOEHM_GC_unregister_long_link(new_link) == 0) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_unregister_long_link failed\n");
            FAIL;
          }
          if (MANAGED_STACK_ADDRESS_BOEHM_GC_move_long_link(&live_long_refs[my_index],
                                new_link) != MANAGED_STACK_ADDRESS_BOEHM_GC_NOT_FOUND) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_move_long_link(new_link) failed 2\n");
            FAIL;
          }
          if (MANAGED_STACK_ADDRESS_BOEHM_GC_REGISTER_LONG_LINK(&live_long_refs[my_index], result) != 0) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_register_long_link failed 2\n");
            FAIL;
          }
#       endif
      }
#   endif
      MANAGED_STACK_ADDRESS_BOEHM_GC_reachable_here(result);
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_END_STUBBORN_CHANGE(result);
    MANAGED_STACK_ADDRESS_BOEHM_GC_reachable_here(left);
    MANAGED_STACK_ADDRESS_BOEHM_GC_reachable_here(right);
    return result;
}

static void chktree(tn *t, int n)
{
    if (0 == n) {
        if (NULL == t) /* is a leaf? */
            return;
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Clobbered a leaf - collector is broken\n");
        FAIL;
    }
    if (t -> level != n) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Lost a node at level %d - collector is broken\n", n);
        FAIL;
    }
    if (AO_fetch_and_add1(&extra_count) % 373 == 0) {
        (void)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(
                        (unsigned)AO_fetch_and_add1(&extra_count) % 5001));
        AO_fetch_and_add1(&collectable_count);
    }
    chktree(t -> lchild, n-1);
    if (AO_fetch_and_add1(&extra_count) % 73 == 0) {
        (void)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(
                        (unsigned)AO_fetch_and_add1(&extra_count) % 373));
        AO_fetch_and_add1(&collectable_count);
    }
    chktree(t -> rchild, n-1);
}

#ifndef VERY_SMALL_CONFIG
# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS)
    pthread_key_t fl_key;
# endif

  static void * alloc8bytes(void)
  {
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS
      AO_fetch_and_add1(&atomic_count);
      return MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_ATOMIC(8);
#   elif defined(SMALL_CONFIG) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG)
      AO_fetch_and_add1(&collectable_count);
      return MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(8);
#   else
      void ** my_free_list_ptr;
      void * my_free_list;
      void * next;

      my_free_list_ptr = (void **)pthread_getspecific(fl_key);
      if (NULL == my_free_list_ptr) {
        my_free_list_ptr = MANAGED_STACK_ADDRESS_BOEHM_GC_NEW_UNCOLLECTABLE(void *);
        if (NULL == my_free_list_ptr) return NULL;
        AO_fetch_and_add1(&uncollectable_count);
        if (pthread_setspecific(fl_key, my_free_list_ptr) != 0) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("pthread_setspecific failed\n");
          FAIL;
        }
      }
      my_free_list = *my_free_list_ptr;
      if (NULL == my_free_list) {
        my_free_list = MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_many(8);
        if (NULL == my_free_list) return NULL;
      }
      next = MANAGED_STACK_ADDRESS_BOEHM_GC_NEXT(my_free_list);
      MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE_AND_DIRTY(my_free_list_ptr, next);
      MANAGED_STACK_ADDRESS_BOEHM_GC_NEXT(my_free_list) = NULL;
      AO_fetch_and_add1(&collectable_count);
      return my_free_list;
#   endif
  }

  static void alloc_small(int n)
  {
    int i;

    for (i = 0; i < n; i += 8) {
      void *p = alloc8bytes();

      CHECK_OUT_OF_MEMORY(p);
    }
  }
#endif /* !VERY_SMALL_CONFIG */

#include "gc/gc_inline.h"

static void test_tinyfl(void)
{
  void *results[3];
  void *tfls[3][MANAGED_STACK_ADDRESS_BOEHM_GC_TINY_FREELISTS];

# ifndef DONT_ADD_BYTE_AT_END
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_get_all_interior_pointers()) return; /* skip */
# endif
  BZERO(tfls, sizeof(tfls));
  /* TODO: Improve testing of FAST_MALLOC functionality. */
  MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_WORDS(results[0], 11, tfls[0]);
  CHECK_OUT_OF_MEMORY(results[0]);
  MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_ATOMIC_WORDS(results[1], 20, tfls[1]);
  CHECK_OUT_OF_MEMORY(results[1]);
  MANAGED_STACK_ADDRESS_BOEHM_GC_CONS(results[2], results[0], results[1], tfls[2]);
  CHECK_OUT_OF_MEMORY(results[2]);
}

# if defined(THREADS) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG)
#   ifdef VERY_SMALL_CONFIG
#     define TREE_HEIGHT 12
#   else
#     define TREE_HEIGHT 15
#   endif
# else
#   ifdef VERY_SMALL_CONFIG
#     define TREE_HEIGHT 13
#   else
#     define TREE_HEIGHT 16
#   endif
# endif
static void tree_test(void)
{
    tn * root;
    int i;

    root = mktree(TREE_HEIGHT);
#   ifndef VERY_SMALL_CONFIG
      alloc_small(5000000);
#   endif
    chktree(root, TREE_HEIGHT);
    FINALIZER_LOCK();
    if (finalized_count && !dropped_something) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Premature finalization - collector is broken\n");
        FAIL;
    }
    dropped_something = 1;
    FINALIZER_UNLOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_reachable_here(root);    /* Root needs to remain live until      */
                                /* dropped_something is set.            */
    root = mktree(TREE_HEIGHT);
    chktree(root, TREE_HEIGHT);
    for (i = TREE_HEIGHT; i >= 0; i--) {
        root = mktree(i);
        chktree(root, i);
    }
#   ifndef VERY_SMALL_CONFIG
      alloc_small(5000000);
#   endif
}

unsigned n_tests = 0;

#ifndef NO_TYPED_TEST
const MANAGED_STACK_ADDRESS_BOEHM_GC_word bm_huge[320 / CPP_WORDSZ] = {
# if CPP_WORDSZ == 32
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
    0xffffffff,
# endif
    (MANAGED_STACK_ADDRESS_BOEHM_GC_word)((MANAGED_STACK_ADDRESS_BOEHM_GC_signed_word)-1),
    (MANAGED_STACK_ADDRESS_BOEHM_GC_word)((MANAGED_STACK_ADDRESS_BOEHM_GC_signed_word)-1),
    (MANAGED_STACK_ADDRESS_BOEHM_GC_word)((MANAGED_STACK_ADDRESS_BOEHM_GC_signed_word)-1),
    (MANAGED_STACK_ADDRESS_BOEHM_GC_word)((MANAGED_STACK_ADDRESS_BOEHM_GC_signed_word)-1),
    ((MANAGED_STACK_ADDRESS_BOEHM_GC_word)((MANAGED_STACK_ADDRESS_BOEHM_GC_signed_word)-1)) >> 8 /* highest byte is zero */
};

/* A very simple test of explicitly typed allocation.   */
static void typed_test(void)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_word * old, * newP;
    MANAGED_STACK_ADDRESS_BOEHM_GC_word bm3[1] = {0};
    MANAGED_STACK_ADDRESS_BOEHM_GC_word bm2[1] = {0};
    MANAGED_STACK_ADDRESS_BOEHM_GC_word bm_large[1] = { 0xf7ff7fff };
    MANAGED_STACK_ADDRESS_BOEHM_GC_descr d1;
    MANAGED_STACK_ADDRESS_BOEHM_GC_descr d2;
    MANAGED_STACK_ADDRESS_BOEHM_GC_descr d3 = MANAGED_STACK_ADDRESS_BOEHM_GC_make_descriptor(bm_large, 32);
    MANAGED_STACK_ADDRESS_BOEHM_GC_descr d4 = MANAGED_STACK_ADDRESS_BOEHM_GC_make_descriptor(bm_huge, 320);
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG
      struct MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_typed_descr_s ctd_l;
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_word *x = (MANAGED_STACK_ADDRESS_BOEHM_GC_word *)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_EXPLICITLY_TYPED(
                                        320 * sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_word) + 123, d4));
    int i;

    AO_fetch_and_add1(&collectable_count);
    (void)MANAGED_STACK_ADDRESS_BOEHM_GC_make_descriptor(bm_large, 32);
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_get_bit(bm_huge, 32) == 0 || MANAGED_STACK_ADDRESS_BOEHM_GC_get_bit(bm_huge, 311) == 0
        || MANAGED_STACK_ADDRESS_BOEHM_GC_get_bit(bm_huge, 319) != 0) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Bad MANAGED_STACK_ADDRESS_BOEHM_GC_get_bit() or bm_huge initialization\n");
      FAIL;
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_bit(bm3, 0);
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_bit(bm3, 1);
    d1 = MANAGED_STACK_ADDRESS_BOEHM_GC_make_descriptor(bm3, 2);
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_bit(bm2, 1);
    d2 = MANAGED_STACK_ADDRESS_BOEHM_GC_make_descriptor(bm2, 2);
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_prepare_explicitly_typed(&ctd_l, sizeof(ctd_l), 1001,
                                             3 * sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_word), d2) != 1) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Out of memory in calloc typed prepare\n");
        exit(69);
      }
#   endif
    old = 0;
    for (i = 0; i < 4000; i++) {
        if ((i & 0xff) != 0) {
          newP = (MANAGED_STACK_ADDRESS_BOEHM_GC_word*)MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_EXPLICITLY_TYPED(4 * sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_word), d1);
        } else {
          newP = (MANAGED_STACK_ADDRESS_BOEHM_GC_word*)MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_EXPLICITLY_TYPED_IGNORE_OFF_PAGE(
                                                      4 * sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_word), d1);
        }
        CHECK_OUT_OF_MEMORY(newP);
        AO_fetch_and_add1(&collectable_count);
        if (newP[0] != 0 || newP[1] != 0) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Bad initialization by MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_explicitly_typed\n");
            FAIL;
        }
        newP[0] = 17;
        MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE_AND_DIRTY(newP + 1, old);
        old = newP;
        AO_fetch_and_add1(&collectable_count);
        newP = (MANAGED_STACK_ADDRESS_BOEHM_GC_word *)MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_EXPLICITLY_TYPED(4 * sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_word), d2);
        CHECK_OUT_OF_MEMORY(newP);
        newP[0] = 17;
        MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE_AND_DIRTY(newP + 1, old);
        old = newP;
        AO_fetch_and_add1(&collectable_count);
        newP = (MANAGED_STACK_ADDRESS_BOEHM_GC_word*)MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_EXPLICITLY_TYPED(33 * sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_word), d3);
        CHECK_OUT_OF_MEMORY(newP);
        newP[0] = 17;
        MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE_AND_DIRTY(newP + 1, old);
        old = newP;
        AO_fetch_and_add1(&collectable_count);
        newP = (MANAGED_STACK_ADDRESS_BOEHM_GC_word *)MANAGED_STACK_ADDRESS_BOEHM_GC_CALLOC_EXPLICITLY_TYPED(4, 2 * sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_word),
                                                     d1);
        CHECK_OUT_OF_MEMORY(newP);
        newP[0] = 17;
        MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE_AND_DIRTY(newP + 1, old);
        old = newP;
        AO_fetch_and_add1(&collectable_count);
        if (i & 0xff) {
          newP = (MANAGED_STACK_ADDRESS_BOEHM_GC_word *)MANAGED_STACK_ADDRESS_BOEHM_GC_CALLOC_EXPLICITLY_TYPED(7, 3 * sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_word),
                                                       d2);
        } else {
#         ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG
            newP = (MANAGED_STACK_ADDRESS_BOEHM_GC_word *)MANAGED_STACK_ADDRESS_BOEHM_GC_CALLOC_EXPLICITLY_TYPED(1001,
                                                3 * sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_word), d2);
#         else
            newP = (MANAGED_STACK_ADDRESS_BOEHM_GC_word *)MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_do_explicitly_typed(&ctd_l,
                                                            sizeof(ctd_l));
#         endif
          if (newP != NULL && (newP[0] != 0 || newP[1] != 0)) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Bad initialization by MANAGED_STACK_ADDRESS_BOEHM_GC_calloc_explicitly_typed\n");
            FAIL;
          }
        }
        CHECK_OUT_OF_MEMORY(newP);
        newP[0] = 17;
        MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE_AND_DIRTY(newP + 1, old);
        old = newP;
    }
    for (i = 0; i < 20000; i++) {
        if (newP[0] != 17) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Typed alloc failed at %d\n", i);
            FAIL;
        }
        newP[0] = 0;
        old = newP;
        newP = (MANAGED_STACK_ADDRESS_BOEHM_GC_word *)old[1];
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect();
    MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((MANAGED_STACK_ADDRESS_BOEHM_GC_word)x);
}
#endif /* !NO_TYPED_TEST */

#ifdef DBG_HDRS_ALL
# define set_print_procs() (void)(A.dummy = 17)
#else
  static volatile AO_t fail_count = 0;

  static void MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK fail_proc1(void *arg)
  {
    UNUSED_ARG(arg);
    AO_fetch_and_add1(&fail_count);
  }

  static void set_print_procs(void)
  {
    /* Set these global variables just once to avoid TSan false positives. */
    A.dummy = 17;
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_is_valid_displacement_print_proc(fail_proc1);
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_is_visible_print_proc(fail_proc1);
  }

# ifdef THREADS
#   define TEST_FAIL_COUNT(n) 1
# else
#   define TEST_FAIL_COUNT(n) (fail_count >= (AO_t)(n))
# endif
#endif /* !DBG_HDRS_ALL */

static void uniq(void *p, ...) {
  va_list a;
  void *q[100];
  int n = 0, i, j;
  q[n++] = p;
  va_start(a,p);
  for (;(q[n] = va_arg(a,void *)) != NULL;n++) ;
  va_end(a);
  for (i=0; i<n; i++)
    for (j=0; j<i; j++)
      if (q[i] == q[j]) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf(
              "Apparently failed to mark from some function arguments.\n"
              "Perhaps MANAGED_STACK_ADDRESS_BOEHM_GC_push_regs was configured incorrectly?\n"
        );
        FAIL;
      }
}

#include "private/gc_alloc_ptrs.h"

static void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK inc_int_counter(void *pcounter)
{
  ++(*(int *)pcounter);

  /* Dummy checking of API functions while GC lock is held.     */
  MANAGED_STACK_ADDRESS_BOEHM_GC_incr_bytes_allocd(0);
  MANAGED_STACK_ADDRESS_BOEHM_GC_incr_bytes_freed(0);

  return NULL;
}

struct thr_hndl_sb_s {
  void *gc_thread_handle;
  struct MANAGED_STACK_ADDRESS_BOEHM_GC_stack_base sb;
};

static void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK set_stackbottom(void *cd)
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_set_stackbottom(((struct thr_hndl_sb_s *)cd)->gc_thread_handle,
                     &((struct thr_hndl_sb_s *)cd)->sb);
  return NULL;
}

#ifndef MIN_WORDS
# define MIN_WORDS 2
#endif

static void run_one_test(void)
{
    char *x;
#   ifndef DBG_HDRS_ALL
        char *y;
        char **z;
#   endif
#   ifndef NO_CLOCK
      CLOCK_TYPE start_time;
      CLOCK_TYPE reverse_time;
      unsigned long time_diff;
#   endif
#   ifndef NO_TEST_HANDLE_FORK
      pid_t pid;
      int wstatus;
#   endif
    struct thr_hndl_sb_s thr_hndl_sb;

    MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(0);
#   ifdef THREADS
      if (!MANAGED_STACK_ADDRESS_BOEHM_GC_thread_is_registered() && MANAGED_STACK_ADDRESS_BOEHM_GC_is_init_called()) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Current thread is not registered with GC\n");
        FAIL;
      }
#   endif
    test_tinyfl();
#   ifndef DBG_HDRS_ALL
      x = (char *)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(7));
      AO_fetch_and_add1(&collectable_count);
      y = (char *)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(7));
      AO_fetch_and_add1(&collectable_count);
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_size(x) != 8 && MANAGED_STACK_ADDRESS_BOEHM_GC_size(y) != MIN_WORDS * sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_word)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_size produced unexpected results\n");
        FAIL;
      }
      x = (char *)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(15));
      AO_fetch_and_add1(&collectable_count);
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_size(x) != 16) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_size produced unexpected results 2\n");
        FAIL;
      }
      x = (char *)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(0));
      AO_fetch_and_add1(&collectable_count);
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_size(x) != MIN_WORDS * sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_word)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(0) failed: MANAGED_STACK_ADDRESS_BOEHM_GC_size returns %lu\n",
                      (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_size(x));
        FAIL;
      }
      x = (char *)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_uncollectable(0));
      AO_fetch_and_add1(&uncollectable_count);
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_size(x) != MIN_WORDS * sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_word)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_uncollectable(0) failed\n");
        FAIL;
      }
      x = (char *)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(16));
      AO_fetch_and_add1(&collectable_count);
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_base(MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_ADD(x, 13)) != x) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_base(heap ptr) produced incorrect result\n");
        FAIL;
      }
      if (!MANAGED_STACK_ADDRESS_BOEHM_GC_is_heap_ptr(x)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_is_heap_ptr(heap_ptr) produced incorrect result\n");
        FAIL;
      }
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_is_heap_ptr(&x)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_is_heap_ptr(&local_var) produced incorrect result\n");
        FAIL;
      }
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_is_heap_ptr((void *)(MANAGED_STACK_ADDRESS_BOEHM_GC_word)&fail_count)
          || MANAGED_STACK_ADDRESS_BOEHM_GC_is_heap_ptr(NULL)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_is_heap_ptr(&global_var) produced incorrect result\n");
        FAIL;
      }
      (void)MANAGED_STACK_ADDRESS_BOEHM_GC_PRE_INCR(x, 0);
      (void)MANAGED_STACK_ADDRESS_BOEHM_GC_POST_INCR(x);
      (void)MANAGED_STACK_ADDRESS_BOEHM_GC_POST_DECR(x);
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_base(x) != x) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Bad INCR/DECR result\n");
        FAIL;
      }
      y = (char *)(MANAGED_STACK_ADDRESS_BOEHM_GC_word)fail_proc1;
#     ifndef PCR
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_base(y) != 0) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_base(fn_ptr) produced incorrect result\n");
          FAIL;
        }
#     endif
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_same_obj(x+5, x) != x + 5) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_same_obj produced incorrect result\n");
        FAIL;
      }
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_is_visible(y) != y || MANAGED_STACK_ADDRESS_BOEHM_GC_is_visible(x) != x) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_is_visible produced incorrect result\n");
        FAIL;
      }
      z = (char **)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(8));
      AO_fetch_and_add1(&collectable_count);
      MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE(z, x);
      MANAGED_STACK_ADDRESS_BOEHM_GC_end_stubborn_change(z);
      if (*z != x) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_PTR_STORE failed: %p != %p\n", (void *)(*z), (void *)x);
        FAIL;
      }
#     if !defined(IA64) && !defined(POWERPC)
        if (!TEST_FAIL_COUNT(1)) {
          /* On POWERPCs function pointers point to a descriptor in the */
          /* data segment, so there should have been no failures.       */
          /* The same applies to IA64.                                  */
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_is_visible produced wrong failure indication\n");
          FAIL;
        }
#     endif
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_is_valid_displacement(y) != y
        || MANAGED_STACK_ADDRESS_BOEHM_GC_is_valid_displacement(x) != x
        || MANAGED_STACK_ADDRESS_BOEHM_GC_is_valid_displacement(x + 3) != x + 3) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_is_valid_displacement produced incorrect result\n");
        FAIL;
      }

      {
          size_t i;
          void *p;

          p = MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(17);
          CHECK_OUT_OF_MEMORY(p);
          AO_fetch_and_add1(&collectable_count);

          /* TODO: MANAGED_STACK_ADDRESS_BOEHM_GC_memalign and friends are not tested well. */
          for (i = sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_word); i <= HBLKSIZE * 4; i *= 2) {
            p = checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_memalign(i, 17));
            AO_fetch_and_add1(&collectable_count);
            if ((MANAGED_STACK_ADDRESS_BOEHM_GC_word)p % i != 0 || *(int *)p != 0) {
              MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_memalign(%u,17) produced incorrect result: %p\n",
                        (unsigned)i, p);
              FAIL;
            }
          }
          (void)MANAGED_STACK_ADDRESS_BOEHM_GC_posix_memalign(&p, 64, 1);
          CHECK_OUT_OF_MEMORY(p);
          AO_fetch_and_add1(&collectable_count);
      }
#     ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_VALLOC
        {
          void *p = checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_valloc(78));

          AO_fetch_and_add1(&collectable_count);
          if (((MANAGED_STACK_ADDRESS_BOEHM_GC_word)p & 0x1ff /* at least */) != 0 || *(int *)p != 0) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_valloc() produced incorrect result: %p\n", p);
            FAIL;
          }

          p = checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_pvalloc(123));
          AO_fetch_and_add1(&collectable_count);
          /* Note: cannot check MANAGED_STACK_ADDRESS_BOEHM_GC_size() result. */
          if (((MANAGED_STACK_ADDRESS_BOEHM_GC_word)p & 0x1ff) != 0 || *(int *)p != 0) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_pvalloc() produced incorrect result: %p\n", p);
            FAIL;
          }
        }
#     endif
#     ifndef ALL_INTERIOR_POINTERS
#       if defined(POWERPC)
          if (!TEST_FAIL_COUNT(1))
#       else
          if (!TEST_FAIL_COUNT(MANAGED_STACK_ADDRESS_BOEHM_GC_get_all_interior_pointers() ? 1 : 2))
#       endif
        {
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf(
              "MANAGED_STACK_ADDRESS_BOEHM_GC_is_valid_displacement produced wrong failure indication\n");
          FAIL;
        }
#     endif
#   endif /* DBG_HDRS_ALL */
    x = MANAGED_STACK_ADDRESS_BOEHM_GC_STRNDUP("abc", 1);
    CHECK_OUT_OF_MEMORY(x);
    AO_fetch_and_add1(&atomic_count);
    if (strlen(x) != 1) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_strndup unexpected result\n");
      FAIL;
    }
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_REQUIRE_WCSDUP
      {
        static const wchar_t ws[] = { 'a', 'b', 'c', 0 };
        void *p = MANAGED_STACK_ADDRESS_BOEHM_GC_WCSDUP(ws);

        CHECK_OUT_OF_MEMORY(p);
        AO_fetch_and_add1(&atomic_count);
      }
#   endif
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
      if (!MANAGED_STACK_ADDRESS_BOEHM_GC_get_find_leak()) {
        void **p = (void **)MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_ATOMIC(sizeof(void *));

        CHECK_OUT_OF_MEMORY(p); /* LINT2: do not use checkOOM() */
        AO_fetch_and_add1(&atomic_count);
        *p = x;
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_register_disappearing_link(p) != 0) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_register_disappearing_link failed\n");
          FAIL;
        }
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_get_java_finalization()) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_finalization_proc ofn = 0;
          void *ocd = NULL;

          MANAGED_STACK_ADDRESS_BOEHM_GC_REGISTER_FINALIZER_UNREACHABLE(p, dummy_finalizer, NULL,
                                            &ofn, &ocd);
          if (ofn != 0 || ocd != NULL) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_register_finalizer_unreachable unexpected result\n");
            FAIL;
          }
        }
#       ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_TOGGLE_REFS_NOT_NEEDED
          if (MANAGED_STACK_ADDRESS_BOEHM_GC_TOGGLEREF_ADD(p, 1) == MANAGED_STACK_ADDRESS_BOEHM_GC_NO_MEMORY) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Out of memory in MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_add\n");
            exit(69);
          }
#       endif
      }
#   endif
    /* Test floating point alignment */
        {
          double *dp = MANAGED_STACK_ADDRESS_BOEHM_GC_NEW(double);

          CHECK_OUT_OF_MEMORY(dp);
          AO_fetch_and_add1(&collectable_count);
          *dp = 1.0;
          dp = MANAGED_STACK_ADDRESS_BOEHM_GC_NEW(double);
          CHECK_OUT_OF_MEMORY(dp);
          AO_fetch_and_add1(&collectable_count);
          *dp = 1.0;
#         ifndef NO_DEBUGGING
            (void)MANAGED_STACK_ADDRESS_BOEHM_GC_count_set_marks_in_hblk(dp);
#         endif
        }
    /* Test size 0 allocation a bit more */
        {
           size_t i;
           for (i = 0; i < 10000; ++i) {
             (void)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(0));
             AO_fetch_and_add1(&collectable_count);
             MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(0)));
             (void)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_ATOMIC(0));
             AO_fetch_and_add1(&atomic_count);
             MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_ATOMIC(0)));
             test_generic_malloc_or_special(checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_atomic(1)));
             AO_fetch_and_add1(&atomic_count);
             MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_ATOMIC_IGNORE_OFF_PAGE(1)));
             MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_IGNORE_OFF_PAGE(2)));
             (void)checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_ignore_off_page(2 * HBLKSIZE,
                                                              NORMAL));
             AO_fetch_and_add1(&collectable_count);
           }
         }
    thr_hndl_sb.gc_thread_handle = MANAGED_STACK_ADDRESS_BOEHM_GC_get_my_stackbottom(&thr_hndl_sb.sb);
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT
      MANAGED_STACK_ADDRESS_BOEHM_GC_REGISTER_DISPLACEMENT(sizeof(struct fake_vtable *));
      MANAGED_STACK_ADDRESS_BOEHM_GC_init_gcj_malloc_mp(0U, fake_gcj_mark_proc);
#   endif
    /* Make sure that fn arguments are visible to the collector.        */
      uniq(
        MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12), MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12), MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12),
        (MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect(),MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12)),
        MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12), MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12), MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12),
        (MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect(),MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12)),
        MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12), MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12), MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12),
        (MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect(),MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12)),
        MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12), MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12), MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12),
        (MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect(),MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12)),
        MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12), MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12), MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12),
        (MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect(),MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(12)),
        (void *)0);
    /* MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(0) must return NULL or something we can deallocate. */
        MANAGED_STACK_ADDRESS_BOEHM_GC_free(checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(0)));
        MANAGED_STACK_ADDRESS_BOEHM_GC_free(checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_atomic(0)));
        MANAGED_STACK_ADDRESS_BOEHM_GC_free(checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(0)));
        MANAGED_STACK_ADDRESS_BOEHM_GC_free(checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_atomic(0)));
#   ifndef NO_TEST_HANDLE_FORK
        MANAGED_STACK_ADDRESS_BOEHM_GC_atfork_prepare();
        pid = fork();
        if (pid != 0) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_atfork_parent();
          if (pid == -1) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Process fork failed\n");
            FAIL;
          }
          if (print_stats)
            MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Forked child process, pid= %ld\n", (long)pid);
          if (waitpid(pid, &wstatus, 0) == -1) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Wait for child process failed\n");
            FAIL;
          }
          if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Child process failed, pid= %ld, status= 0x%x\n",
                      (long)pid, wstatus);
            FAIL;
          }
        } else {
          pid_t child_pid = getpid();

          MANAGED_STACK_ADDRESS_BOEHM_GC_atfork_child();
          if (print_stats)
            MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Started a child process, pid= %ld\n",
                          (long)child_pid);
#         ifdef PARALLEL_MARK
            MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect(); /* no parallel markers */
#         endif
          MANAGED_STACK_ADDRESS_BOEHM_GC_start_mark_threads();
          MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect();
#         ifdef THREADS
            /* Skip "Premature finalization" check in the       */
            /* child process because there could be a chance    */
            /* that some other thread of the parent was         */
            /* executing mktree at the moment of fork.          */
            dropped_something = 1;
#         endif
          tree_test();
#         ifndef NO_TYPED_TEST
            typed_test();
#         endif
#         ifdef THREADS
            if (print_stats)
              MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Starting tiny reverse test, pid= %ld\n",
                            (long)child_pid);
            tiny_reverse_test_inner();
            MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect();
#         endif
          if (print_stats)
            MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Finished a child process, pid= %ld\n",
                          (long)child_pid);
          exit(0);
        }
#   endif
    (void)MANAGED_STACK_ADDRESS_BOEHM_GC_call_with_alloc_lock(set_stackbottom, &thr_hndl_sb);

    /* Repeated list reversal test. */
#   ifndef NO_CLOCK
        GET_TIME(start_time);
#   endif
        reverse_test();
#   ifndef NO_CLOCK
        if (print_stats) {
          GET_TIME(reverse_time);
          time_diff = MS_TIME_DIFF(reverse_time, start_time);
          MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Finished reverse_test at time %u (%p)\n",
                        (unsigned) time_diff, (void *)&start_time);
        }
#   endif
#   ifndef NO_TYPED_TEST
      typed_test();
#     ifndef NO_CLOCK
        if (print_stats) {
          CLOCK_TYPE typed_time;

          GET_TIME(typed_time);
          time_diff = MS_TIME_DIFF(typed_time, start_time);
          MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Finished typed_test at time %u (%p)\n",
                        (unsigned) time_diff, (void *)&start_time);
        }
#     endif
#   endif /* !NO_TYPED_TEST */
    tree_test();
#   ifdef TEST_WITH_SYSTEM_MALLOC
      free(checkOOM(calloc(1, 1)));
      free(checkOOM(realloc(NULL, 64)));
#   endif
#   ifndef NO_CLOCK
      if (print_stats) {
        CLOCK_TYPE tree_time;

        GET_TIME(tree_time);
        time_diff = MS_TIME_DIFF(tree_time, start_time);
        MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Finished tree_test at time %u (%p)\n",
                      (unsigned) time_diff, (void *)&start_time);
      }
#   endif
    /* Run reverse_test a second time, so we hopefully notice corruption. */
    reverse_test();
#   ifndef NO_DEBUGGING
      (void)MANAGED_STACK_ADDRESS_BOEHM_GC_is_tmp_root((/* no volatile */ void *)(MANAGED_STACK_ADDRESS_BOEHM_GC_word)&atomic_count);
#   endif
#   ifndef NO_CLOCK
      if (print_stats) {
        GET_TIME(reverse_time);
        time_diff = MS_TIME_DIFF(reverse_time, start_time);
        MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Finished second reverse_test at time %u (%p)\n",
                      (unsigned)time_diff, (void *)&start_time);
      }
#   endif
    /* MANAGED_STACK_ADDRESS_BOEHM_GC_allocate_ml and MANAGED_STACK_ADDRESS_BOEHM_GC_need_to_lock are no longer exported, and   */
    /* AO_fetch_and_add1() may be unavailable to update a counter.      */
    (void)MANAGED_STACK_ADDRESS_BOEHM_GC_call_with_alloc_lock(inc_int_counter, &n_tests);
#   ifndef NO_CLOCK
      if (print_stats)
        MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Finished %p\n", (void *)&start_time);
#   endif
}

/* Execute some tests after termination of other test threads (if any). */
static void run_single_threaded_test(void) {
    MANAGED_STACK_ADDRESS_BOEHM_GC_disable();
    MANAGED_STACK_ADDRESS_BOEHM_GC_FREE(checkOOM(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(100)));
    MANAGED_STACK_ADDRESS_BOEHM_GC_expand_hp(0); /* add a block to heap */
    MANAGED_STACK_ADDRESS_BOEHM_GC_enable();
}

static void MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK reachable_objs_counter(void *obj, size_t size,
                                               void *pcounter)
{
  if (0 == size) {
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Reachable object has zero size\n");
    FAIL;
  }
  if (MANAGED_STACK_ADDRESS_BOEHM_GC_base(obj) != obj) {
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Invalid reachable object base passed by enumerator: %p\n",
              obj);
    FAIL;
  }
  if (MANAGED_STACK_ADDRESS_BOEHM_GC_size(obj) != size) {
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Invalid reachable object size passed by enumerator: %lu\n",
              (unsigned long)size);
    FAIL;
  }
  (*(unsigned *)pcounter)++;
}

/* A minimal testing of LONG_MULT().    */
static void test_long_mult(void)
{
#if !defined(CPPCHECK) || !defined(NO_LONGLONG64)
  unsigned32 hp, lp;

  LONG_MULT(hp, lp, (unsigned32)0x1234567UL, (unsigned32)0xfedcba98UL);
  if (hp != (unsigned32)0x121fa00UL || lp != (unsigned32)0x23e20b28UL) {
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("LONG_MULT gives wrong result\n");
    FAIL;
  }
  LONG_MULT(hp, lp, (unsigned32)0xdeadbeefUL, (unsigned32)0xefcdab12UL);
  if (hp != (unsigned32)0xd0971b30UL || lp != (unsigned32)0xbd2411ceUL) {
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("LONG_MULT gives wrong result (2)\n");
    FAIL;
  }
#endif
}

#define NUMBER_ROUND_UP(v, bound) ((((v) + (bound) - 1) / (bound)) * (bound))

static void check_heap_stats(void)
{
    size_t max_heap_sz;
    int i;
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
#     ifdef FINALIZE_ON_DEMAND
        int late_finalize_count = 0;
#     endif
#   endif
    unsigned obj_count = 0;

    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_is_init_called()) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("GC should be initialized!\n");
      FAIL;
    }

    /* The upper bounds are a guess, which has been empirically */
    /* adjusted.  On low-end uniprocessors with incremental GC  */
    /* these may be particularly dubious, since empirically the */
    /* heap tends to grow largely as a result of the GC not     */
    /* getting enough cycles.                                   */
#   if CPP_WORDSZ == 64
      max_heap_sz = 26000000;
#   else
      max_heap_sz = 16000000;
#   endif
#   ifdef VERY_SMALL_CONFIG
      max_heap_sz /= 4;
#   endif
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG
        max_heap_sz *= 2;
#       ifdef SAVE_CALL_CHAIN
            max_heap_sz *= 3;
#           ifdef SAVE_CALL_COUNT
                max_heap_sz += max_heap_sz * NFRAMES / 4;
#           endif
#       endif
#   endif
#   if defined(ADDRESS_SANITIZER) && !defined(__clang__)
        max_heap_sz = max_heap_sz * 2 - max_heap_sz / 3;
#   endif
#   ifdef MEMORY_SANITIZER
        max_heap_sz += max_heap_sz / 4;
#   endif
    max_heap_sz *= n_tests;
#   if defined(USE_MMAP) || defined(MSWIN32)
      max_heap_sz = NUMBER_ROUND_UP(max_heap_sz, 4 * 1024 * 1024);
#   endif

    /* Garbage collect repeatedly so that all inaccessible objects      */
    /* can be finalized.                                                */
      while (MANAGED_STACK_ADDRESS_BOEHM_GC_collect_a_little()) { } /* should work even if disabled GC */
      for (i = 0; i < 16; i++) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect();
#       ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
#         ifdef FINALIZE_ON_DEMAND
            late_finalize_count +=
#         endif
                MANAGED_STACK_ADDRESS_BOEHM_GC_invoke_finalizers();
#       endif
      }
      if (print_stats) {
        struct MANAGED_STACK_ADDRESS_BOEHM_GC_stack_base sb;
        int res = MANAGED_STACK_ADDRESS_BOEHM_GC_get_stack_base(&sb);

        if (res == MANAGED_STACK_ADDRESS_BOEHM_GC_SUCCESS) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Primordial thread stack bottom: %p\n", sb.mem_base);
        } else if (res == MANAGED_STACK_ADDRESS_BOEHM_GC_UNIMPLEMENTED) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_get_stack_base() unimplemented\n");
        } else {
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_get_stack_base() failed: %d\n", res);
          FAIL;
        }
      }
    MANAGED_STACK_ADDRESS_BOEHM_GC_alloc_lock();
    MANAGED_STACK_ADDRESS_BOEHM_GC_enumerate_reachable_objects_inner(reachable_objs_counter, &obj_count);
    MANAGED_STACK_ADDRESS_BOEHM_GC_alloc_unlock();
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Completed %u tests\n", n_tests);
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Allocated %d collectable objects\n", (int)collectable_count);
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Allocated %d uncollectable objects\n",
                  (int)uncollectable_count);
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Allocated %d atomic objects\n", (int)atomic_count);
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Reallocated %d objects\n", (int)realloc_count);
#   ifndef NO_TEST_HANDLE_FORK
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Garbage collection after fork is tested too\n");
#   endif
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_get_find_leak()) {
      int still_live = 0;
#     ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_LONG_REFS_NOT_NEEDED
        int still_long_live = 0;
#     endif

#     ifdef FINALIZE_ON_DEMAND
        if (finalized_count != late_finalize_count) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Finalized %d/%d objects - demand finalization error\n",
                    finalized_count, finalizable_count);
          FAIL;
        }
#     endif
      if (finalized_count > finalizable_count
          || finalized_count < finalizable_count/2) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Finalized %d/%d objects - "
                  "finalization is probably broken\n",
                  finalized_count, finalizable_count);
        FAIL;
      } else {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Finalized %d/%d objects - finalization is probably OK\n",
                  finalized_count, finalizable_count);
      }
      for (i = 0; i < MAX_FINALIZED; i++) {
        if (live_indicators[i] != 0) {
            still_live++;
        }
#       ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_LONG_REFS_NOT_NEEDED
          if (live_long_refs[i] != NULL) {
              still_long_live++;
          }
#       endif
      }
      i = finalizable_count - finalized_count - still_live;
      if (0 != i) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("%d disappearing links remain and %d more objects "
                      "were not finalized\n", still_live, i);
        if (i > 10) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("\tVery suspicious!\n");
        } else {
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("\tSlightly suspicious, but probably OK\n");
        }
      }
#     ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_LONG_REFS_NOT_NEEDED
        if (0 != still_long_live) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("%d 'long' links remain\n", still_long_live);
        }
#     endif
    }
# endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Total number of bytes allocated is %lu\n",
                  (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_get_total_bytes());
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Total memory use by allocated blocks is %lu bytes\n",
              (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_get_memory_use());
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Final heap size is %lu bytes\n",
                  (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_get_heap_size());
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_get_total_bytes() < (size_t)n_tests *
#   ifdef VERY_SMALL_CONFIG
        2700000
#   else
        33500000
#   endif
        ) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Incorrect execution - missed some allocations\n");
      FAIL;
    }
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_get_heap_size() + MANAGED_STACK_ADDRESS_BOEHM_GC_get_unmapped_bytes() > max_heap_sz
            && !MANAGED_STACK_ADDRESS_BOEHM_GC_get_find_leak()) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Unexpected heap growth - collector may be broken"
                  " (heapsize: %lu, expected: %lu)\n",
            (unsigned long)(MANAGED_STACK_ADDRESS_BOEHM_GC_get_heap_size() + MANAGED_STACK_ADDRESS_BOEHM_GC_get_unmapped_bytes()),
            (unsigned long)max_heap_sz);
        FAIL;
    }
#   ifdef USE_MUNMAP
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Obtained %lu bytes from OS (of which %lu bytes unmapped)\n",
                (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_get_obtained_from_os_bytes(),
                (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_get_unmapped_bytes());
#   else
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Obtained %lu bytes from OS\n",
                (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_get_obtained_from_os_bytes());
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Final number of reachable objects is %u\n", obj_count);

#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_GET_HEAP_USAGE_NOT_NEEDED
      /* Get global counters (just to check the functions work).  */
      MANAGED_STACK_ADDRESS_BOEHM_GC_get_heap_usage_safe(NULL, NULL, NULL, NULL, NULL);
      {
        struct MANAGED_STACK_ADDRESS_BOEHM_GC_prof_stats_s stats;
        (void)MANAGED_STACK_ADDRESS_BOEHM_GC_get_prof_stats(&stats, sizeof(stats));
#       ifdef THREADS
          (void)MANAGED_STACK_ADDRESS_BOEHM_GC_get_prof_stats_unsafe(&stats, sizeof(stats));
#       endif
      }
      (void)MANAGED_STACK_ADDRESS_BOEHM_GC_get_size_map_at(-1);
      (void)MANAGED_STACK_ADDRESS_BOEHM_GC_get_size_map_at(1);
#   endif
    test_long_mult();

#   ifndef NO_CLOCK
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Full/world-stopped collections took %lu/%lu ms\n",
                MANAGED_STACK_ADDRESS_BOEHM_GC_get_full_gc_total_time(), MANAGED_STACK_ADDRESS_BOEHM_GC_get_stopped_mark_total_time());
#   endif
#   ifdef PARALLEL_MARK
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Completed %u collections (using %d marker threads)\n",
                (unsigned)MANAGED_STACK_ADDRESS_BOEHM_GC_get_gc_no(), MANAGED_STACK_ADDRESS_BOEHM_GC_get_parallel() + 1);
#   else
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Completed %u collections\n", (unsigned)MANAGED_STACK_ADDRESS_BOEHM_GC_get_gc_no());
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Collector appears to work\n");
}

#if defined(MACOS)
void SetMinimumStack(long minSize)
{
        if (minSize > LMGetDefltStack())
        {
                long newApplLimit = (long) GetApplLimit()
                                        - (minSize - LMGetDefltStack());
                SetApplLimit((Ptr) newApplLimit);
                MaxApplZone();
        }
}

#define cMinStackSpace (512L * 1024L)

#endif

static void MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK warn_proc(char *msg, MANAGED_STACK_ADDRESS_BOEHM_GC_word p)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf(msg, (unsigned long)p);
    /*FAIL;*/
}

static void enable_incremental_mode(void)
{
# if (defined(TEST_DEFAULT_VDB) || defined(TEST_MANUAL_VDB) \
      || !defined(DEFAULT_VDB)) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL)
#   if !defined(MAKE_BACK_GRAPH) && !defined(NO_INCREMENTAL) \
       && !defined(REDIRECT_MALLOC) && !defined(USE_PROC_FOR_LIBRARIES)
      MANAGED_STACK_ADDRESS_BOEHM_GC_enable_incremental();
#   endif
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_is_incremental_mode()) {
#     ifndef SMALL_CONFIG
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_get_manual_vdb_allowed()) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Switched to incremental mode (manual VDB)\n");
        } else
#     endif
      /* else */ {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Switched to incremental mode\n");
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_incremental_protection_needs() == MANAGED_STACK_ADDRESS_BOEHM_GC_PROTECTS_NONE) {
#         if defined(PROC_VDB) || defined(SOFT_VDB)
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Reading dirty bits from /proc\n");
#         elif defined(GWW_VDB)
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Using GetWriteWatch-based implementation\n");
#         endif
        } else {
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Emulating dirty bits with mprotect/signals\n");
        }
      }
    }
# endif
}

#if defined(CPPCHECK)
# define UNTESTED(sym) MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((MANAGED_STACK_ADDRESS_BOEHM_GC_word)(MANAGED_STACK_ADDRESS_BOEHM_GC_funcptr_uint)&sym)
#endif

#if defined(MSWINCE) && defined(UNDER_CE)
# define WINMAIN_LPTSTR LPWSTR
#else
# define WINMAIN_LPTSTR LPSTR
#endif

#if !defined(PCR) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS)

#if defined(_DEBUG) && (_MSC_VER >= 1900) /* VS 2015+ */
  /* Ensure that there is no system-malloc-allocated objects at normal  */
  /* exit (i.e. no such memory leaked).                                 */
# define CRTMEM_CHECK_INIT() \
        (void)_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF)
# define CRTMEM_DUMP_LEAKS() \
        do { \
          if (_CrtDumpMemoryLeaks()) { \
            MANAGED_STACK_ADDRESS_BOEHM_GC_printf("System-malloc-allocated memory leaked\n"); \
            FAIL; \
          } \
        } while (0)
#else
# define CRTMEM_CHECK_INIT() (void)0
# define CRTMEM_DUMP_LEAKS() (void)0
#endif /* !_MSC_VER || !_DEBUG */

#if ((defined(MSWIN32) && !defined(__MINGW32__)) || defined(MSWINCE)) \
    && !defined(NO_WINMAIN_ENTRY)
  int APIENTRY WinMain(HINSTANCE instance, HINSTANCE prev, WINMAIN_LPTSTR cmd,
                       int n)
#elif defined(RTEMS)
# include <bsp.h>
# define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
# define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
# define CONFIGURE_RTEMS_INIT_TASKS_TABLE
# define CONFIGURE_MAXIMUM_TASKS 1
# define CONFIGURE_INIT
# define CONFIGURE_INIT_TASK_STACK_SIZE (64*1024)
# include <rtems/confdefs.h>
  rtems_task Init(rtems_task_argument ignord)
#else
  int main(void)
#endif
{
    CRTMEM_CHECK_INIT();
#   if ((defined(MSWIN32) && !defined(__MINGW32__)) || defined(MSWINCE)) \
       && !defined(NO_WINMAIN_ENTRY)
      UNUSED_ARG(instance);
      UNUSED_ARG(prev);
      UNUSED_ARG(cmd);
      UNUSED_ARG(n);
#     if defined(CPPCHECK)
        MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((MANAGED_STACK_ADDRESS_BOEHM_GC_word)(MANAGED_STACK_ADDRESS_BOEHM_GC_funcptr_uint)&WinMain);
#     endif
#   elif defined(RTEMS)
      UNUSED_ARG(ignord);
#     if defined(CPPCHECK)
        MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((MANAGED_STACK_ADDRESS_BOEHM_GC_word)(MANAGED_STACK_ADDRESS_BOEHM_GC_funcptr_uint)&Init);
#     endif
#   endif
    n_tests = 0;
    MANAGED_STACK_ADDRESS_BOEHM_GC_clear_exclusion_table(); /* no-op as called before GC init */
#   if defined(MACOS)
        /* Make sure we have lots and lots of stack space.      */
        SetMinimumStack(cMinStackSpace);
        /* Cheat and let stdio initialize toolbox for us.       */
        printf("Testing GC Macintosh port\n");
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_COND_INIT();
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_warn_proc(warn_proc);
    enable_incremental_mode();
    set_print_procs();
    MANAGED_STACK_ADDRESS_BOEHM_GC_start_incremental_collection();
    run_one_test();
#   if NTHREADS > 0
      {
        int i;
        for (i = 0; i < NTHREADS; i++)
          run_one_test();
      }
#   endif
    run_single_threaded_test();
    check_heap_stats();
#   ifndef MSWINCE
      fflush(stdout);
#   endif
#   if defined(CPPCHECK)
       /* Entry points we should be testing, but aren't.        */
#     ifdef AMIGA
#       ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_FASTALLOC
          UNTESTED(MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_get_mem);
#       endif
#       ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_ONLYFAST
          UNTESTED(MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_set_toany);
#       endif
#     endif
#     if defined(MACOS) && defined(USE_TEMPORARY_MEMORY)
        UNTESTED(MANAGED_STACK_ADDRESS_BOEHM_GC_MacTemporaryNewPtr);
#     endif
      UNTESTED(MANAGED_STACK_ADDRESS_BOEHM_GC_abort_on_oom);
      UNTESTED(MANAGED_STACK_ADDRESS_BOEHM_GC_deinit);
#     ifndef NO_DEBUGGING
        UNTESTED(MANAGED_STACK_ADDRESS_BOEHM_GC_dump);
        UNTESTED(MANAGED_STACK_ADDRESS_BOEHM_GC_dump_regions);
        UNTESTED(MANAGED_STACK_ADDRESS_BOEHM_GC_print_free_list);
#     endif
#   endif
#   if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ANDROID_LOG) && !defined(KOS) && !defined(MACOS) \
       && !defined(OS2) && !defined(MSWIN32) && !defined(MSWINCE)
      MANAGED_STACK_ADDRESS_BOEHM_GC_set_log_fd(2);
#   endif
#   ifdef ANY_MSWIN
      MANAGED_STACK_ADDRESS_BOEHM_GC_win32_free_heap();
#   endif
    CRTMEM_DUMP_LEAKS();
#   ifdef RTEMS
      exit(0);
#   else
      return 0;
#   endif
}
# endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS && !MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS */

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS)

static DWORD __stdcall thr_run_one_test(void *arg)
{
  UNUSED_ARG(arg);
  run_one_test();
  return 0;
}

#ifdef MSWINCE
HANDLE win_created_h;
HWND win_handle;

static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                    LPARAM lParam)
{
  LRESULT ret = 0;
  switch (uMsg) {
    case WM_HIBERNATE:
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Received WM_HIBERNATE, calling MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect\n");
      /* Force "unmap as much memory as possible" mode. */
      MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect_and_unmap();
      break;
    case WM_CLOSE:
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Received WM_CLOSE, closing window\n");
      DestroyWindow(hwnd);
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    default:
      ret = DefWindowProc(hwnd, uMsg, wParam, lParam);
      break;
  }
  return ret;
}

static DWORD __stdcall thr_window(void *arg)
{
  WNDCLASS win_class = {
    CS_NOCLOSE,
    window_proc,
    0,
    0,
    GetModuleHandle(NULL),
    NULL,
    NULL,
    (HBRUSH)(COLOR_APPWORKSPACE+1),
    NULL,
    TEXT("GCtestWindow")
  };
  MSG msg;

  UNUSED_ARG(arg);
  if (!RegisterClass(&win_class)) {
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("RegisterClass failed\n");
    FAIL;
  }

  win_handle = CreateWindowEx(
    0,
    TEXT("GCtestWindow"),
    TEXT("GCtest"),
    0,
    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
    NULL,
    NULL,
    GetModuleHandle(NULL),
    NULL);

  if (NULL == win_handle) {
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("CreateWindow failed\n");
    FAIL;
  }

  SetEvent(win_created_h);

  ShowWindow(win_handle, SW_SHOW);
  UpdateWindow(win_handle);

  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return 0;
}
#endif

#if !defined(NO_WINMAIN_ENTRY)
  int APIENTRY WinMain(HINSTANCE instance, HINSTANCE prev, WINMAIN_LPTSTR cmd,
                       int n)
#else
  int main(void)
#endif
{
# if NTHREADS > 0
   HANDLE h[NTHREADS];
   int i;
# endif
# ifdef MSWINCE
    HANDLE win_thr_h;
# endif
  DWORD thread_id;

# if !defined(NO_WINMAIN_ENTRY)
    UNUSED_ARG(instance);
    UNUSED_ARG(prev);
    UNUSED_ARG(cmd);
    UNUSED_ARG(n);
#   if defined(CPPCHECK)
      MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((MANAGED_STACK_ADDRESS_BOEHM_GC_word)(MANAGED_STACK_ADDRESS_BOEHM_GC_funcptr_uint)&WinMain);
#   endif
# endif
# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DLL) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_THREADS_DISCOVERY) \
        && !defined(MSWINCE) && !defined(THREAD_LOCAL_ALLOC)
    MANAGED_STACK_ADDRESS_BOEHM_GC_use_threads_discovery();
                /* Test with implicit thread registration if possible. */
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Using DllMain to track threads\n");
# endif
  MANAGED_STACK_ADDRESS_BOEHM_GC_COND_INIT();
  enable_incremental_mode();
  InitializeCriticalSection(&incr_cs);
  MANAGED_STACK_ADDRESS_BOEHM_GC_set_warn_proc(warn_proc);
# ifdef MSWINCE
    win_created_h = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (win_created_h == (HANDLE)NULL) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Event creation failed, errcode= %d\n", (int)GetLastError());
      FAIL;
    }
    win_thr_h = CreateThread(NULL, 0, thr_window, 0, 0, &thread_id);
    if (win_thr_h == (HANDLE)NULL) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Thread creation failed, errcode= %d\n", (int)GetLastError());
      FAIL;
    }
    if (WaitForSingleObject(win_created_h, INFINITE) != WAIT_OBJECT_0) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("WaitForSingleObject failed 1\n");
      FAIL;
    }
    CloseHandle(win_created_h);
# endif
  set_print_procs();
# if NTHREADS > 0
    for (i = 0; i < NTHREADS; i++) {
      h[i] = CreateThread(NULL, 0, thr_run_one_test, 0, 0, &thread_id);
      if (h[i] == (HANDLE)NULL) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Thread creation failed, errcode= %d\n",
                  (int)GetLastError());
        FAIL;
      }
    }
# else
    MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((MANAGED_STACK_ADDRESS_BOEHM_GC_word)(MANAGED_STACK_ADDRESS_BOEHM_GC_funcptr_uint)&thr_run_one_test);
# endif
  run_one_test();
# if NTHREADS > 0
    for (i = 0; i < NTHREADS; i++) {
      if (WaitForSingleObject(h[i], INFINITE) != WAIT_OBJECT_0) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Thread wait failed, errcode= %d\n", (int)GetLastError());
        FAIL;
      }
    }
# endif /* NTHREADS > 0 */
# ifdef MSWINCE
    PostMessage(win_handle, WM_CLOSE, 0, 0);
    if (WaitForSingleObject(win_thr_h, INFINITE) != WAIT_OBJECT_0) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("WaitForSingleObject failed 2\n");
      FAIL;
    }
# endif
  run_single_threaded_test();
  check_heap_stats();
  (void)MANAGED_STACK_ADDRESS_BOEHM_GC_unregister_my_thread(); /* just to check it works (for main) */
  return 0;
}

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS */


#ifdef PCR
int test(void)
{
    PCR_Th_T * th1;
    PCR_Th_T * th2;
    int code;

#   if defined(CPPCHECK)
      MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((MANAGED_STACK_ADDRESS_BOEHM_GC_word)(MANAGED_STACK_ADDRESS_BOEHM_GC_funcptr_uint)&PCR_MANAGED_STACK_ADDRESS_BOEHM_GC_Run);
      MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((MANAGED_STACK_ADDRESS_BOEHM_GC_word)(MANAGED_STACK_ADDRESS_BOEHM_GC_funcptr_uint)&PCR_MANAGED_STACK_ADDRESS_BOEHM_GC_Setup);
      MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((MANAGED_STACK_ADDRESS_BOEHM_GC_word)(MANAGED_STACK_ADDRESS_BOEHM_GC_funcptr_uint)&test);
#   endif
    n_tests = 0;
    /* MANAGED_STACK_ADDRESS_BOEHM_GC_enable_incremental(); */
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_warn_proc(warn_proc);
    set_print_procs();
    th1 = PCR_Th_Fork(run_one_test, 0);
    th2 = PCR_Th_Fork(run_one_test, 0);
    run_one_test();
    if (PCR_Th_T_Join(th1, &code, NIL, PCR_allSigsBlocked, PCR_waitForever)
        != PCR_ERes_okay || code != 0) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Thread 1 failed\n");
    }
    if (PCR_Th_T_Join(th2, &code, NIL, PCR_allSigsBlocked, PCR_waitForever)
        != PCR_ERes_okay || code != 0) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Thread 2 failed\n");
    }
    run_single_threaded_test();
    check_heap_stats();
    return 0;
}
#endif

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS)
# include <errno.h> /* for EAGAIN */

static void * thr_run_one_test(void *arg)
{
    UNUSED_ARG(arg);
    run_one_test();
    return 0;
}

static void MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK describe_norm_type(void *p, char *out_buf)
{
  UNUSED_ARG(p);
  BCOPY("NORMAL", out_buf, sizeof("NORMAL"));
}

static int MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK has_static_roots(const char *dlpi_name,
                                        void *section_start,
                                        size_t section_size)
{
  UNUSED_ARG(dlpi_name);
  UNUSED_ARG(section_start);
  UNUSED_ARG(section_size);
  return 1;
}

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_DEBUG
# define MANAGED_STACK_ADDRESS_BOEHM_GC_free MANAGED_STACK_ADDRESS_BOEHM_GC_debug_free
#endif

int main(void)
{
#   if NTHREADS > 0
      pthread_t th[NTHREADS];
      int i, nthreads;
#   endif
    pthread_attr_t attr;
    int code;
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_IRIX_THREADS
        /* Force a larger stack to be preallocated      */
        /* Since the initial can't always grow later.   */
        *((volatile char *)&code - 1024*1024) = 0;      /* Require 1 MB */
#   endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_IRIX_THREADS */
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_HPUX_THREADS)
        /* Default stack size is too small, especially with the 64 bit ABI */
        /* Increase it.                                                    */
        if (pthread_default_stacksize_np(1024*1024, 0) != 0) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("pthread_default_stacksize_np failed\n");
        }
#   endif       /* MANAGED_STACK_ADDRESS_BOEHM_GC_HPUX_THREADS */
#   ifdef PTW32_STATIC_LIB
        pthread_win32_process_attach_np();
        pthread_win32_thread_attach_np();
#   endif
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_THREADS_DISCOVERY) \
        && !defined(DARWIN_DONT_PARSE_STACK) && !defined(THREAD_LOCAL_ALLOC)
      /* Test with the Darwin implicit thread registration. */
      MANAGED_STACK_ADDRESS_BOEHM_GC_use_threads_discovery();
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Using Darwin task-threads-based world stop and push\n");
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_markers_count(0);
#   ifdef TEST_REUSE_SIG_SUSPEND
      MANAGED_STACK_ADDRESS_BOEHM_GC_set_suspend_signal(MANAGED_STACK_ADDRESS_BOEHM_GC_get_thr_restart_signal());
#   else
      MANAGED_STACK_ADDRESS_BOEHM_GC_set_suspend_signal(MANAGED_STACK_ADDRESS_BOEHM_GC_get_suspend_signal());
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_COND_INIT();

    if ((code = pthread_attr_init(&attr)) != 0) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("pthread_attr_init failed, errno= %d\n", code);
      FAIL;
    }
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_IRIX_THREADS) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_FREEBSD_THREADS) \
        || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_AIX_THREADS) \
        || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_OPENBSD_THREADS)
        if ((code = pthread_attr_setstacksize(&attr, 1000 * 1024)) != 0) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("pthread_attr_setstacksize failed, errno= %d\n", code);
          FAIL;
        }
#   endif
    n_tests = 0;
    enable_incremental_mode();
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_min_bytes_allocd(1);
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_get_min_bytes_allocd() != 1) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_get_min_bytes_allocd() wrong result\n");
        FAIL;
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_rate(10);
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_max_prior_attempts(MANAGED_STACK_ADDRESS_BOEHM_GC_get_max_prior_attempts());
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_get_rate() != 10) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_get_rate() wrong result\n");
        FAIL;
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_warn_proc(warn_proc);
#   ifndef VERY_SMALL_CONFIG
      if ((code = pthread_key_create(&fl_key, 0)) != 0) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Key creation failed, errno= %d\n", code);
        FAIL;
      }
#   endif
    set_print_procs();

    /* Minimal testing of some API functions.   */
    MANAGED_STACK_ADDRESS_BOEHM_GC_exclude_static_roots((/* no volatile */ void *)(MANAGED_STACK_ADDRESS_BOEHM_GC_word)&atomic_count,
                (void *)((MANAGED_STACK_ADDRESS_BOEHM_GC_word)&atomic_count + sizeof(atomic_count)));
    MANAGED_STACK_ADDRESS_BOEHM_GC_register_has_static_roots_callback(has_static_roots);
    MANAGED_STACK_ADDRESS_BOEHM_GC_register_describe_type_fn(MANAGED_STACK_ADDRESS_BOEHM_GC_I_NORMAL, describe_norm_type);
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT
      (void)MANAGED_STACK_ADDRESS_BOEHM_GC_new_proc(fake_gcj_mark_proc);
#   endif

#   if NTHREADS > 0
      for (i = 0; i < NTHREADS; ++i) {
        if ((code = pthread_create(th+i, &attr, thr_run_one_test, 0)) != 0) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Thread #%d creation failed, errno= %d\n", i, code);
          if (i > 0 && EAGAIN == code)
            break; /* Resource temporarily unavailable */
          FAIL;
        }
      }
      nthreads = i;
      for (; i <= NTHREADS; i++)
        run_one_test();
      for (i = 0; i < nthreads; ++i) {
        if ((code = pthread_join(th[i], 0)) != 0) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Thread #%d join failed, errno= %d\n", i, code);
          FAIL;
        }
      }
#   else
      (void)thr_run_one_test(NULL);
#   endif
    run_single_threaded_test();
#   ifdef TRACE_BUF
      MANAGED_STACK_ADDRESS_BOEHM_GC_print_trace(0);
#   endif
    check_heap_stats();
    (void)fflush(stdout);
    (void)pthread_attr_destroy(&attr);

    /* Dummy checking of various getters and setters. */
    (void)MANAGED_STACK_ADDRESS_BOEHM_GC_get_bytes_since_gc();
    (void)MANAGED_STACK_ADDRESS_BOEHM_GC_get_free_bytes();
    (void)MANAGED_STACK_ADDRESS_BOEHM_GC_get_hblk_size();
    (void)MANAGED_STACK_ADDRESS_BOEHM_GC_get_is_valid_displacement_print_proc();
    (void)MANAGED_STACK_ADDRESS_BOEHM_GC_get_is_visible_print_proc();
    (void)MANAGED_STACK_ADDRESS_BOEHM_GC_get_pages_executable();
    (void)MANAGED_STACK_ADDRESS_BOEHM_GC_get_warn_proc();
    (void)MANAGED_STACK_ADDRESS_BOEHM_GC_is_disabled();
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_allocd_bytes_per_finalizer(MANAGED_STACK_ADDRESS_BOEHM_GC_get_allocd_bytes_per_finalizer());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_disable_automatic_collection(MANAGED_STACK_ADDRESS_BOEHM_GC_get_disable_automatic_collection());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_dont_expand(MANAGED_STACK_ADDRESS_BOEHM_GC_get_dont_expand());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_dont_precollect(MANAGED_STACK_ADDRESS_BOEHM_GC_get_dont_precollect());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_finalize_on_demand(MANAGED_STACK_ADDRESS_BOEHM_GC_get_finalize_on_demand());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_finalizer_notifier(MANAGED_STACK_ADDRESS_BOEHM_GC_get_finalizer_notifier());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_force_unmap_on_gcollect(MANAGED_STACK_ADDRESS_BOEHM_GC_get_force_unmap_on_gcollect());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_free_space_divisor(MANAGED_STACK_ADDRESS_BOEHM_GC_get_free_space_divisor());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_full_freq(MANAGED_STACK_ADDRESS_BOEHM_GC_get_full_freq());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_java_finalization(MANAGED_STACK_ADDRESS_BOEHM_GC_get_java_finalization());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_max_retries(MANAGED_STACK_ADDRESS_BOEHM_GC_get_max_retries());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_no_dls(MANAGED_STACK_ADDRESS_BOEHM_GC_get_no_dls());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_non_gc_bytes(MANAGED_STACK_ADDRESS_BOEHM_GC_get_non_gc_bytes());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_on_collection_event(MANAGED_STACK_ADDRESS_BOEHM_GC_get_on_collection_event());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_on_heap_resize(MANAGED_STACK_ADDRESS_BOEHM_GC_get_on_heap_resize());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_on_mark_stack_empty(MANAGED_STACK_ADDRESS_BOEHM_GC_get_on_mark_stack_empty());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_on_thread_event(MANAGED_STACK_ADDRESS_BOEHM_GC_get_on_thread_event());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_oom_fn(MANAGED_STACK_ADDRESS_BOEHM_GC_get_oom_fn());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_push_other_roots(MANAGED_STACK_ADDRESS_BOEHM_GC_get_push_other_roots());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_same_obj_print_proc(MANAGED_STACK_ADDRESS_BOEHM_GC_get_same_obj_print_proc());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_sp_corrector(MANAGED_STACK_ADDRESS_BOEHM_GC_get_sp_corrector());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_start_callback(MANAGED_STACK_ADDRESS_BOEHM_GC_get_start_callback());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_stop_func(MANAGED_STACK_ADDRESS_BOEHM_GC_get_stop_func());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_thr_restart_signal(MANAGED_STACK_ADDRESS_BOEHM_GC_get_thr_restart_signal());
    MANAGED_STACK_ADDRESS_BOEHM_GC_set_time_limit(MANAGED_STACK_ADDRESS_BOEHM_GC_get_time_limit());
#   if !defined(PCR) && !defined(SMALL_CONFIG)
      MANAGED_STACK_ADDRESS_BOEHM_GC_set_abort_func(MANAGED_STACK_ADDRESS_BOEHM_GC_get_abort_func());
#   endif
#   ifndef NO_CLOCK
      MANAGED_STACK_ADDRESS_BOEHM_GC_set_time_limit_tv(MANAGED_STACK_ADDRESS_BOEHM_GC_get_time_limit_tv());
#   endif
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
      MANAGED_STACK_ADDRESS_BOEHM_GC_set_await_finalize_proc(MANAGED_STACK_ADDRESS_BOEHM_GC_get_await_finalize_proc());
      MANAGED_STACK_ADDRESS_BOEHM_GC_set_interrupt_finalizers(MANAGED_STACK_ADDRESS_BOEHM_GC_get_interrupt_finalizers());
#     ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_TOGGLE_REFS_NOT_NEEDED
        MANAGED_STACK_ADDRESS_BOEHM_GC_set_toggleref_func(MANAGED_STACK_ADDRESS_BOEHM_GC_get_toggleref_func());
#     endif
#   endif
#   if defined(CPPCHECK)
      UNTESTED(MANAGED_STACK_ADDRESS_BOEHM_GC_register_altstack);
#   endif /* CPPCHECK */

#   if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_DLOPEN) && !defined(DARWIN) \
       && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS)
      {
        void *h = MANAGED_STACK_ADDRESS_BOEHM_GC_dlopen("libc.so", 0 /* some value (maybe invalid) */);
        if (h != NULL) dlclose(h);
      }
#   endif
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_SIGMASK
      {
        sigset_t blocked;

        if (MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_sigmask(SIG_BLOCK, NULL, &blocked) != 0
            || MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_sigmask(SIG_BLOCK, &blocked, NULL) != 0) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_printf("pthread_sigmask failed\n");
          FAIL;
        }
      }
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_stop_world_external();
    MANAGED_STACK_ADDRESS_BOEHM_GC_start_world_external();
#   if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION) && !defined(JAVA_FINALIZATION_NOT_NEEDED)
      MANAGED_STACK_ADDRESS_BOEHM_GC_finalize_all();
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_clear_roots();

#   ifdef PTW32_STATIC_LIB
        pthread_win32_thread_detach_np();
        pthread_win32_process_detach_np();
#   else
        (void)MANAGED_STACK_ADDRESS_BOEHM_GC_unregister_my_thread();
#   endif
    return 0;
}
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS */
