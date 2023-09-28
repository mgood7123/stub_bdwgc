/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996-1999 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 1999-2004 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2008-2022 Ivan Maidanski
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

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_PRIVATE_H
#define MANAGED_STACK_ADDRESS_BOEHM_GC_PRIVATE_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_BUILD) && !defined(NOT_GCBUILD)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_BUILD
#endif

#if (defined(__linux__) || defined(__GLIBC__) || defined(__GNU__) \
     || defined(__CYGWIN__) || defined(HAVE_DLADDR) \
     || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_HAVE_PTHREAD_SIGMASK) \
     || defined(HAVE_PTHREAD_SETNAME_NP_WITHOUT_TID) \
     || defined(HAVE_PTHREAD_SETNAME_NP_WITH_TID_AND_ARG) \
     || defined(HAVE_PTHREAD_SETNAME_NP_WITH_TID)) && !defined(_GNU_SOURCE)
  /* Can't test LINUX, since this must be defined before other includes. */
# define _GNU_SOURCE 1
#endif

#if defined(__INTERIX) && !defined(_ALL_SOURCE)
# define _ALL_SOURCE 1
#endif

#if (defined(DGUX) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_THREADS) || defined(DGUX386_THREADS) \
     || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DGUX386_THREADS)) && !defined(_USING_POSIX4A_DRAFT10)
# define _USING_POSIX4A_DRAFT10 1
#endif

#if defined(__MINGW32__) && !defined(__MINGW_EXCPT_DEFINE_PSDK) \
    && defined(__i386__) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN) /* defined in gc.c */
  /* See the description in mark.c.     */
# define __MINGW_EXCPT_DEFINE_PSDK 1
#endif

# if defined(NO_DEBUGGING) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) && !defined(NDEBUG)
    /* To turn off assertion checking (in atomic_ops.h). */
#   define NDEBUG 1
# endif

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_H
# include "gc/gc.h"
#endif

#include <stdlib.h>
#if !defined(sony_news)
# include <stddef.h>
#endif

#ifdef DGUX
# include <sys/time.h>
# include <sys/resource.h>
#endif /* DGUX */

#ifdef BSD_TIME
# include <sys/time.h>
# include <sys/resource.h>
#endif /* BSD_TIME */

#ifdef PARALLEL_MARK
# define AO_REQUIRE_CAS
# if !defined(__GNUC__) && !defined(AO_ASSUME_WINDOWS98)
#   define AO_ASSUME_WINDOWS98
# endif
#endif

#include "gc/gc_tiny_fl.h"
#include "gc/gc_mark.h"

typedef MANAGED_STACK_ADDRESS_BOEHM_GC_word word;
typedef MANAGED_STACK_ADDRESS_BOEHM_GC_signed_word signed_word;

typedef int MANAGED_STACK_ADDRESS_BOEHM_GC_bool;
#define TRUE 1
#define FALSE 0

#ifndef PTR_T_DEFINED
  typedef char * ptr_t; /* A generic pointer to which we can add        */
                        /* byte displacements and which can be used     */
                        /* for address comparisons.                     */
# define PTR_T_DEFINED
#endif

#ifndef SIZE_MAX
# include <limits.h>
#endif
#if defined(SIZE_MAX) && !defined(CPPCHECK)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_MAX ((size_t)SIZE_MAX)
            /* Extra cast to workaround some buggy SIZE_MAX definitions. */
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_MAX (~(size_t)0)
#endif

#if MANAGED_STACK_ADDRESS_BOEHM_GC_GNUC_PREREQ(3, 0) && !defined(LINT2)
# define EXPECT(expr, outcome) __builtin_expect(expr,outcome)
  /* Equivalent to (expr), but predict that usually (expr)==outcome. */
#else
# define EXPECT(expr, outcome) (expr)
#endif /* __GNUC__ */

/* Saturated addition of size_t values.  Used to avoid value wrap       */
/* around on overflow.  The arguments should have no side effects.      */
#define SIZET_SAT_ADD(a, b) \
            (EXPECT((a) < MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_MAX - (b), TRUE) ? (a) + (b) : MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_MAX)

#include "gcconfig.h"

#if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ATOMIC_UNCOLLECTABLE) && defined(ATOMIC_UNCOLLECTABLE)
  /* For compatibility with old-style naming. */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ATOMIC_UNCOLLECTABLE
#endif

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_INNER
  /* This tagging macro must be used at the start of every variable     */
  /* definition which is declared with MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN.  Should be also used  */
  /* for the GC-scope function definitions and prototypes.  Must not be */
  /* used in gcconfig.h.  Shouldn't be used for the debugging-only      */
  /* functions.  Currently, not used for the functions declared in or   */
  /* called from the "dated" source files (located in "extra" folder).  */
# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DLL) && defined(__GNUC__) && !defined(ANY_MSWIN)
#   if MANAGED_STACK_ADDRESS_BOEHM_GC_GNUC_PREREQ(4, 0) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_VISIBILITY)
      /* See the corresponding MANAGED_STACK_ADDRESS_BOEHM_GC_API definition. */
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_INNER __attribute__((__visibility__("hidden")))
#   else
      /* The attribute is unsupported. */
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_INNER /* empty */
#   endif
# else
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_INNER /* empty */
# endif

# define MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN extern MANAGED_STACK_ADDRESS_BOEHM_GC_INNER
  /* Used only for the GC-scope variables (prefixed with "MANAGED_STACK_ADDRESS_BOEHM_GC_")         */
  /* declared in the header files.  Must not be used for thread-local   */
  /* variables.  Must not be used in gcconfig.h.  Shouldn't be used for */
  /* the debugging-only or profiling-only variables.  Currently, not    */
  /* used for the variables accessed from the "dated" source files      */
  /* (specific.c/h, and in the "extra" folder).                         */
  /* The corresponding variable definition must start with MANAGED_STACK_ADDRESS_BOEHM_GC_INNER.    */
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_INNER */

#ifdef __cplusplus
  /* Register storage specifier is deprecated in C++11. */
# define REGISTER /* empty */
#else
  /* Used only for several local variables in the performance-critical  */
  /* functions.  Should not be used for new code.                       */
# define REGISTER register
#endif

#if defined(CPPCHECK)
# define MACRO_BLKSTMT_BEGIN {
# define MACRO_BLKSTMT_END   }
# define LOCAL_VAR_INIT_OK =0 /* to avoid "uninit var" false positive */
#else
# define MACRO_BLKSTMT_BEGIN do {
# define MACRO_BLKSTMT_END   } while (0)
# define LOCAL_VAR_INIT_OK /* empty */
#endif

#if defined(M68K) && defined(__GNUC__)
  /* By default, __alignof__(word) is 2 on m68k.  Use this attribute to */
  /* have proper word alignment (i.e. 4-byte on a 32-bit arch).         */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_WORD_ALIGNED __attribute__((__aligned__(sizeof(word))))
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_WORD_ALIGNED /* empty */
#endif

  typedef MANAGED_STACK_ADDRESS_BOEHM_GC_word MANAGED_STACK_ADDRESS_BOEHM_GC_funcptr_uint;
# define FUNCPTR_IS_WORD

typedef unsigned int unsigned32;

#include "gc_hdrs.h"

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_ADDR
# ifndef ADDRESS_SANITIZER
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_ADDR /* empty */
# elif MANAGED_STACK_ADDRESS_BOEHM_GC_CLANG_PREREQ(3, 8)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_ADDR __attribute__((no_sanitize("address")))
# else
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_ADDR __attribute__((no_sanitize_address))
# endif
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_ADDR */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_MEMORY
# ifndef MEMORY_SANITIZER
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_MEMORY /* empty */
# elif MANAGED_STACK_ADDRESS_BOEHM_GC_CLANG_PREREQ(3, 8)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_MEMORY __attribute__((no_sanitize("memory")))
# else
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_MEMORY __attribute__((no_sanitize_memory))
# endif
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_MEMORY */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD
# ifndef THREAD_SANITIZER
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD /* empty */
# elif MANAGED_STACK_ADDRESS_BOEHM_GC_CLANG_PREREQ(3, 8)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD __attribute__((no_sanitize("thread")))
# else
    /* It seems that no_sanitize_thread attribute has no effect if the  */
    /* function is inlined (as of gcc 11.1.0, at least).                */
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD \
                MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NOINLINE __attribute__((no_sanitize_thread))
# endif
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD */

#ifndef UNUSED_ARG
# define UNUSED_ARG(arg) ((void)(arg))
#endif

#ifdef HAVE_CONFIG_H
  /* The "inline" keyword is determined by Autoconf AC_C_INLINE.    */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE static inline
#elif defined(_MSC_VER) || defined(__INTEL_COMPILER) || defined(__DMC__) \
        || (MANAGED_STACK_ADDRESS_BOEHM_GC_GNUC_PREREQ(3, 0) && defined(__STRICT_ANSI__)) \
        || defined(__BORLANDC__) || defined(__WATCOMC__)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE static __inline
#elif MANAGED_STACK_ADDRESS_BOEHM_GC_GNUC_PREREQ(3, 0) || defined(__sun)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE static inline
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE static
#endif

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NOINLINE
# if MANAGED_STACK_ADDRESS_BOEHM_GC_GNUC_PREREQ(4, 0)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NOINLINE __attribute__((__noinline__))
# elif _MSC_VER >= 1400
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NOINLINE __declspec(noinline)
# else
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NOINLINE /* empty */
# endif
#endif

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_API_OSCALL
  /* This is used to identify GC routines called by name from OS.       */
# if defined(__GNUC__)
#   if MANAGED_STACK_ADDRESS_BOEHM_GC_GNUC_PREREQ(4, 0) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_VISIBILITY)
      /* Same as MANAGED_STACK_ADDRESS_BOEHM_GC_API if MANAGED_STACK_ADDRESS_BOEHM_GC_DLL.      */
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_API_OSCALL extern __attribute__((__visibility__("default")))
#   else
      /* The attribute is unsupported.  */
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_API_OSCALL extern
#   endif
# else
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_API_OSCALL MANAGED_STACK_ADDRESS_BOEHM_GC_API
# endif
#endif

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_API_PRIV
# define MANAGED_STACK_ADDRESS_BOEHM_GC_API_PRIV MANAGED_STACK_ADDRESS_BOEHM_GC_API
#endif

#if defined(THREADS) && !defined(NN_PLATFORM_CTR)
# include "gc_atomic_ops.h"
# ifndef AO_HAVE_compiler_barrier
#   define AO_HAVE_compiler_barrier 1
# endif
#endif

#ifdef ANY_MSWIN
# ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN 1
# endif
# define NOSERVICE
# include <windows.h>
# include <winbase.h>
#endif /* ANY_MSWIN */

#include "gc_locks.h"

#define MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX (~(word)0)

#ifdef STACK_GROWS_UP
#   define COOLER_THAN <
#   define HOTTER_THAN >
#   define MAKE_COOLER(x,y) if ((word)((x) - (y)) < (word)(x)) {(x) -= (y);} \
                            else (x) = 0
#   define MAKE_HOTTER(x,y) (void)((x) += (y))
#else
#   define COOLER_THAN >
#   define HOTTER_THAN <
#   define MAKE_COOLER(x,y) if ((word)((x) + (y)) > (word)(x)) {(x) += (y);} \
                            else (x) = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX
#   define MAKE_HOTTER(x,y) (void)((x) -= (y))
#endif

#if defined(AMIGA) && defined(__SASC)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_FAR __far
#else
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_FAR
#endif

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(expr) \
        do { \
          if (EXPECT(!(expr), FALSE)) { \
            MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf("Assertion failure: %s:%d\n", __FILE__, __LINE__); \
            ABORT("assertion failure"); \
          } \
        } while (0)
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(expr)
#endif

#include "gc/gc_inline.h"

/*********************************/
/*                               */
/* Definitions for conservative  */
/* collector                     */
/*                               */
/*********************************/

/*********************************/
/*                               */
/* Easily changeable parameters  */
/*                               */
/*********************************/

/* #define ALL_INTERIOR_POINTERS */
                    /* Forces all pointers into the interior of an      */
                    /* object to be considered valid.  Also causes the  */
                    /* sizes of all objects to be inflated by at least  */
                    /* one byte.  This should suffice to guarantee      */
                    /* that in the presence of a compiler that does     */
                    /* not perform garbage-collector-unsafe             */
                    /* optimizations, all portable, strictly ANSI       */
                    /* conforming C programs should be safely usable    */
                    /* with malloc replaced by MANAGED_STACK_ADDRESS_BOEHM_GC_malloc and free       */
                    /* calls removed.  There are several disadvantages: */
                    /* 1. There are probably no interesting, portable,  */
                    /*    strictly ANSI conforming C programs.          */
                    /* 2. This option makes it hard for the collector   */
                    /*    to allocate space that is not "pointed to"    */
                    /*    by integers, etc.  Under SunOS 4.X with a     */
                    /*    statically linked libc, we empirically        */
                    /*    observed that it would be difficult to        */
                    /*    allocate individual objects > 100 KB.         */
                    /*    Even if only smaller objects are allocated,   */
                    /*    more swap space is likely to be needed.       */
                    /*    Fortunately, much of this will never be       */
                    /*    touched.                                      */
                    /* If you can easily avoid using this option, do.   */
                    /* If not, try to keep individual objects small.    */
                    /* This is now really controlled at startup,        */
                    /* through MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers.                */

EXTERN_C_BEGIN

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
# define MANAGED_STACK_ADDRESS_BOEHM_GC_INVOKE_FINALIZERS() MANAGED_STACK_ADDRESS_BOEHM_GC_notify_or_invoke_finalizers()
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_notify_or_invoke_finalizers(void);
                        /* If MANAGED_STACK_ADDRESS_BOEHM_GC_finalize_on_demand is not set, invoke  */
                        /* eligible finalizers. Otherwise:              */
                        /* Call *MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_notifier if there are     */
                        /* finalizers to be run, and we haven't called  */
                        /* this procedure yet this GC cycle.            */

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_finalize(void);
                        /* Perform all indicated finalization actions   */
                        /* on unmarked objects.                         */
                        /* Unreachable finalizable objects are enqueued */
                        /* for processing by MANAGED_STACK_ADDRESS_BOEHM_GC_invoke_finalizers.      */
                        /* Invoked with lock.                           */

# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_TOGGLE_REFS_NOT_NEEDED
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_process_togglerefs(void);
                        /* Process the toggle-refs before GC starts.    */
# endif
# ifndef SMALL_CONFIG
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_print_finalization_stats(void);
# endif
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_INVOKE_FINALIZERS() (void)0
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION */

#if !defined(DONT_ADD_BYTE_AT_END)
# ifdef LINT2
    /* Explicitly instruct the code analysis tool that                  */
    /* MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers is assumed to have only 0 or 1 value.   */
#   define EXTRA_BYTES ((size_t)(MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers? 1 : 0))
# else
#   define EXTRA_BYTES (size_t)MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers
# endif
# define MAX_EXTRA_BYTES 1
#else
# define EXTRA_BYTES 0
# define MAX_EXTRA_BYTES 0
#endif

# ifdef LARGE_CONFIG
#   define MINHINCR 64
#   define MAXHINCR 4096
# else
#   define MINHINCR 16   /* Minimum heap increment, in blocks of HBLKSIZE.  */
                         /* Note: must be multiple of largest page size.    */
#   define MAXHINCR 2048 /* Maximum heap increment, in blocks.  */
# endif /* !LARGE_CONFIG */

# define BL_LIMIT MANAGED_STACK_ADDRESS_BOEHM_GC_black_list_spacing
                           /* If we need a block of N bytes, and we have */
                           /* a block of N + BL_LIMIT bytes available,   */
                           /* and N > BL_LIMIT,                          */
                           /* but all possible positions in it are       */
                           /* blacklisted, we just use it anyway (and    */
                           /* print a warning, if warnings are enabled). */
                           /* This risks subsequently leaking the block  */
                           /* due to a false reference.  But not using   */
                           /* the block risks unreasonable immediate     */
                           /* heap growth.                               */

/*********************************/
/*                               */
/* Stack saving for debugging    */
/*                               */
/*********************************/

#ifdef NEED_CALLINFO
    struct callinfo {
        word ci_pc;     /* Caller, not callee, pc       */
#       if NARGS > 0
            word ci_arg[NARGS]; /* bit-wise complement to avoid retention */
#       endif
#       if (NFRAMES * (NARGS + 1)) % 2 == 1
            /* Likely alignment problem. */
            word ci_dummy;
#       endif
    };
#endif

#ifdef SAVE_CALL_CHAIN
  /* Fill in the pc and argument information for up to NFRAMES of my    */
  /* callers.  Ignore my frame and my callers frame.                    */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_save_callers(struct callinfo info[NFRAMES]);
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_print_callers(struct callinfo info[NFRAMES]);
#endif

EXTERN_C_END

/*********************************/
/*                               */
/* OS interface routines         */
/*                               */
/*********************************/

#ifndef NO_CLOCK
#ifdef BSD_TIME
# undef CLOCK_TYPE
# undef GET_TIME
# undef MS_TIME_DIFF
# define CLOCK_TYPE struct timeval
# define CLOCK_TYPE_INITIALIZER { 0, 0 }
# define GET_TIME(x) \
                do { \
                  struct rusage rusage; \
                  getrusage(RUSAGE_SELF, &rusage); \
                  x = rusage.ru_utime; \
                } while (0)
# define MS_TIME_DIFF(a,b) ((unsigned long)((long)(a.tv_sec-b.tv_sec) * 1000 \
                + (long)(a.tv_usec - b.tv_usec) / 1000 \
                - (a.tv_usec < b.tv_usec \
                   && (long)(a.tv_usec - b.tv_usec) % 1000 != 0 ? 1 : 0)))
                            /* "a" time is expected to be not earlier than  */
                            /* "b" one; the result has unsigned long type.  */
# define NS_FRAC_TIME_DIFF(a, b) ((unsigned long) \
                ((a.tv_usec < b.tv_usec \
                  && (long)(a.tv_usec - b.tv_usec) % 1000 != 0 ? 1000L : 0) \
                 + (long)(a.tv_usec - b.tv_usec) % 1000) * 1000)
                        /* The total time difference could be computed as   */
                        /* MS_TIME_DIFF(a,b)*1000000+NS_FRAC_TIME_DIFF(a,b).*/

#elif defined(MSWIN32) || defined(MSWINCE) || defined(WINXP_USE_PERF_COUNTER)
# if defined(MSWINRT_FLAVOR) || defined(WINXP_USE_PERF_COUNTER)
#   define CLOCK_TYPE ULONGLONG
#   define GET_TIME(x) \
                do { \
                  LARGE_INTEGER freq, tc; \
                  if (!QueryPerformanceFrequency(&freq)) \
                    ABORT("QueryPerformanceFrequency requires WinXP+"); \
                  /* Note: two standalone if statements are needed to   */ \
                  /* avoid MS VC false warning about potentially        */ \
                  /* uninitialized tc variable.                         */ \
                  if (!QueryPerformanceCounter(&tc)) \
                    ABORT("QueryPerformanceCounter failed"); \
                  x = (CLOCK_TYPE)((double)tc.QuadPart/freq.QuadPart * 1e9); \
                } while (0)
                /* TODO: Call QueryPerformanceFrequency once at GC init. */
#   define MS_TIME_DIFF(a, b) ((unsigned long)(((a) - (b)) / 1000000UL))
#   define NS_FRAC_TIME_DIFF(a, b) ((unsigned long)(((a) - (b)) % 1000000UL))
# else
#   define CLOCK_TYPE DWORD
#   define GET_TIME(x) (void)(x = GetTickCount())
#   define MS_TIME_DIFF(a, b) ((unsigned long)((a) - (b)))
#   define NS_FRAC_TIME_DIFF(a, b) 0UL
# endif /* !WINXP_USE_PERF_COUNTER */

#elif defined(NN_PLATFORM_CTR)
# define CLOCK_TYPE long long
  EXTERN_C_BEGIN
  CLOCK_TYPE n3ds_get_system_tick(void);
  CLOCK_TYPE n3ds_convert_tick_to_ms(CLOCK_TYPE tick);
  EXTERN_C_END
# define GET_TIME(x) (void)(x = n3ds_get_system_tick())
# define MS_TIME_DIFF(a,b) ((unsigned long)n3ds_convert_tick_to_ms((a)-(b)))
# define NS_FRAC_TIME_DIFF(a, b) 0UL /* TODO: implement it */

#elif defined(HAVE_CLOCK_GETTIME)
# include <time.h>
# define CLOCK_TYPE struct timespec
# define CLOCK_TYPE_INITIALIZER { 0, 0 }
# if defined(_POSIX_MONOTONIC_CLOCK) && !defined(NINTENDO_SWITCH)
#   define GET_TIME(x) \
                do { \
                  if (clock_gettime(CLOCK_MONOTONIC, &x) == -1) \
                    ABORT("clock_gettime failed"); \
                } while (0)
# else
#   define GET_TIME(x) \
                do { \
                  if (clock_gettime(CLOCK_REALTIME, &x) == -1) \
                    ABORT("clock_gettime failed"); \
                } while (0)
# endif
# define MS_TIME_DIFF(a, b) \
    /* a.tv_nsec - b.tv_nsec is in range -1e9 to 1e9 exclusively */ \
    ((unsigned long)((a).tv_nsec + (1000000L*1000 - (b).tv_nsec)) / 1000000UL \
     + ((unsigned long)((a).tv_sec - (b).tv_sec) * 1000UL) - 1000UL)
# define NS_FRAC_TIME_DIFF(a, b) \
    ((unsigned long)((a).tv_nsec + (1000000L*1000 - (b).tv_nsec)) % 1000000UL)

#else /* !BSD_TIME && !LINUX && !NN_PLATFORM_CTR && !MSWIN32 */
# include <time.h>
# if defined(FREEBSD) && !defined(CLOCKS_PER_SEC)
#   include <machine/limits.h>
#   define CLOCKS_PER_SEC CLK_TCK
# endif
# if !defined(CLOCKS_PER_SEC)
#   define CLOCKS_PER_SEC 1000000
    /* This is technically a bug in the implementation.                 */
    /* ANSI requires that CLOCKS_PER_SEC be defined.  But at least      */
    /* under SunOS 4.1.1, it isn't.  Also note that the combination of  */
    /* ANSI C and POSIX is incredibly gross here.  The type clock_t     */
    /* is used by both clock() and times().  But on some machines       */
    /* these use different notions of a clock tick, CLOCKS_PER_SEC      */
    /* seems to apply only to clock.  Hence we use it here.  On many    */
    /* machines, including SunOS, clock actually uses units of          */
    /* microseconds (which are not really clock ticks).                 */
# endif
# define CLOCK_TYPE clock_t
# define GET_TIME(x) (void)(x = clock())
# define MS_TIME_DIFF(a,b) (CLOCKS_PER_SEC % 1000 == 0 ? \
        (unsigned long)((a) - (b)) / (unsigned long)(CLOCKS_PER_SEC / 1000) \
        : ((unsigned long)((a) - (b)) * 1000) / (unsigned long)CLOCKS_PER_SEC)
  /* Avoid using double type since some targets (like ARM) might        */
  /* require -lm option for double-to-long conversion.                  */
# define NS_FRAC_TIME_DIFF(a, b) (CLOCKS_PER_SEC <= 1000 ? 0UL \
    : (unsigned long)(CLOCKS_PER_SEC <= (clock_t)1000000UL \
        ? (((a) - (b)) * ((clock_t)1000000UL / CLOCKS_PER_SEC) % 1000) * 1000 \
        : (CLOCKS_PER_SEC <= (clock_t)1000000UL * 1000 \
            ? ((a) - (b)) * ((clock_t)1000000UL * 1000 / CLOCKS_PER_SEC) \
            : (((a) - (b)) * (clock_t)1000000UL * 1000) / CLOCKS_PER_SEC) \
          % (clock_t)1000000UL))
#endif /* !BSD_TIME && !MSWIN32 */
# ifndef CLOCK_TYPE_INITIALIZER
    /* This is used to initialize CLOCK_TYPE variables (to some value)  */
    /* to avoid "variable might be uninitialized" compiler warnings.    */
#   define CLOCK_TYPE_INITIALIZER 0
# endif
#endif /* !NO_CLOCK */

/* We use bzero and bcopy internally.  They may not be available.       */
# if defined(SPARC) && defined(SUNOS4) \
     || (defined(M68K) && defined(NEXT)) || defined(VAX)
#   define BCOPY_EXISTS
# elif defined(AMIGA) || defined(DARWIN)
#   include <string.h>
#   define BCOPY_EXISTS
# elif defined(MACOS) && defined(POWERPC)
#   include <MacMemory.h>
#   define bcopy(x,y,n) BlockMoveData(x, y, n)
#   define bzero(x,n) BlockZero(x, n)
#   define BCOPY_EXISTS
# endif

# if !defined(BCOPY_EXISTS) || defined(CPPCHECK)
#   include <string.h>
#   define BCOPY(x,y,n) memcpy(y, x, (size_t)(n))
#   define BZERO(x,n)  memset(x, 0, (size_t)(n))
# else
#   define BCOPY(x,y,n) bcopy((void *)(x),(void *)(y),(size_t)(n))
#   define BZERO(x,n) bzero((void *)(x),(size_t)(n))
# endif

#ifdef PCR
# include "th/PCR_ThCtl.h"
#endif

EXTERN_C_BEGIN

#if defined(CPPCHECK) && defined(ANY_MSWIN)
# undef TEXT
# ifdef UNICODE
#   define TEXT(s) L##s
# else
#   define TEXT(s) s
# endif
#endif /* CPPCHECK && ANY_MSWIN */

/*
 * Stop and restart mutator threads.
 */
# ifdef PCR
#     define STOP_WORLD() \
        PCR_ThCtl_SetExclusiveMode(PCR_ThCtl_ExclusiveMode_stopNormal, \
                                   PCR_allSigsBlocked, \
                                   PCR_waitForever)
#     define START_WORLD() \
        PCR_ThCtl_SetExclusiveMode(PCR_ThCtl_ExclusiveMode_null, \
                                   PCR_allSigsBlocked, \
                                   PCR_waitForever)
# else
#   if defined(NN_PLATFORM_CTR) || defined(NINTENDO_SWITCH) \
       || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS)
      MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_stop_world(void);
      MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_start_world(void);
#     define STOP_WORLD() MANAGED_STACK_ADDRESS_BOEHM_GC_stop_world()
#     define START_WORLD() MANAGED_STACK_ADDRESS_BOEHM_GC_start_world()
#   else
        /* Just do a sanity check: we are not inside MANAGED_STACK_ADDRESS_BOEHM_GC_do_blocking().  */
#     define STOP_WORLD() MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_sp == NULL)
#     define START_WORLD()
#   endif
# endif

/* Abandon ship */
# if defined(SMALL_CONFIG) || defined(PCR)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_on_abort(msg) (void)0 /* be silent on abort */
# else
    MANAGED_STACK_ADDRESS_BOEHM_GC_API_PRIV MANAGED_STACK_ADDRESS_BOEHM_GC_abort_func MANAGED_STACK_ADDRESS_BOEHM_GC_on_abort;
# endif
# if defined(CPPCHECK)
#   define ABORT(msg) { MANAGED_STACK_ADDRESS_BOEHM_GC_on_abort(msg); abort(); }
# elif defined(PCR)
#   define ABORT(s) PCR_Base_Panic(s)
# else
#   if defined(MSWIN_XBOX1) && !defined(DebugBreak)
#     define DebugBreak() __debugbreak()
#   elif defined(MSWINCE) && !defined(DebugBreak) \
       && (!defined(UNDER_CE) || (defined(__MINGW32CE__) && !defined(ARM32)))
      /* This simplifies linking for WinCE (and, probably, doesn't      */
      /* hurt debugging much); use -DDebugBreak=DebugBreak to override  */
      /* this behavior if really needed.  This is also a workaround for */
      /* x86mingw32ce toolchain (if it is still declaring DebugBreak()  */
      /* instead of defining it as a macro).                            */
#     define DebugBreak() _exit(-1) /* there is no abort() in WinCE */
#   endif
#   if defined(MSWIN32) && (defined(NO_DEBUGGING) || defined(LINT2))
      /* A more user-friendly abort after showing fatal message.        */
#     define ABORT(msg) (MANAGED_STACK_ADDRESS_BOEHM_GC_on_abort(msg), _exit(-1))
                /* Exit on error without running "at-exit" callbacks.   */
#   elif defined(MSWINCE) && defined(NO_DEBUGGING)
#     define ABORT(msg) (MANAGED_STACK_ADDRESS_BOEHM_GC_on_abort(msg), ExitProcess(-1))
#   elif defined(MSWIN32) || defined(MSWINCE)
#     if defined(_CrtDbgBreak) && defined(_DEBUG) && defined(_MSC_VER)
#       define ABORT(msg) { MANAGED_STACK_ADDRESS_BOEHM_GC_on_abort(msg); \
                            _CrtDbgBreak() /* __debugbreak() */; }
#     else
#       define ABORT(msg) { MANAGED_STACK_ADDRESS_BOEHM_GC_on_abort(msg); DebugBreak(); }
                /* Note that: on a WinCE box, this could be silently    */
                /* ignored (i.e., the program is not aborted);          */
                /* DebugBreak is a statement in some toolchains.        */
#     endif
#   else
#     define ABORT(msg) (MANAGED_STACK_ADDRESS_BOEHM_GC_on_abort(msg), abort())
#   endif /* !MSWIN32 */
# endif /* !PCR */

/* For abort message with 1-3 arguments.  C_msg and C_fmt should be     */
/* literals.  C_msg should not contain format specifiers.  Arguments    */
/* should match their format specifiers.                                */
#define ABORT_ARG1(C_msg, C_fmt, arg1) \
                MACRO_BLKSTMT_BEGIN \
                  MANAGED_STACK_ADDRESS_BOEHM_GC_ERRINFO_PRINTF(C_msg /* + */ C_fmt "\n", arg1); \
                  ABORT(C_msg); \
                MACRO_BLKSTMT_END
#define ABORT_ARG2(C_msg, C_fmt, arg1, arg2) \
                MACRO_BLKSTMT_BEGIN \
                  MANAGED_STACK_ADDRESS_BOEHM_GC_ERRINFO_PRINTF(C_msg /* + */ C_fmt "\n", arg1, arg2); \
                  ABORT(C_msg); \
                MACRO_BLKSTMT_END
#define ABORT_ARG3(C_msg, C_fmt, arg1, arg2, arg3) \
                MACRO_BLKSTMT_BEGIN \
                  MANAGED_STACK_ADDRESS_BOEHM_GC_ERRINFO_PRINTF(C_msg /* + */ C_fmt "\n", \
                                    arg1, arg2, arg3); \
                  ABORT(C_msg); \
                MACRO_BLKSTMT_END

/* Same as ABORT but does not have 'no-return' attribute.       */
/* ABORT on a dummy condition (which is always true).           */
#define ABORT_RET(msg) \
    if ((MANAGED_STACK_ADDRESS_BOEHM_GC_funcptr_uint)MANAGED_STACK_ADDRESS_BOEHM_GC_current_warn_proc == ~(MANAGED_STACK_ADDRESS_BOEHM_GC_funcptr_uint)0) {} \
    else ABORT(msg)

/* Exit abnormally, but without making a mess (e.g. out of memory) */
# ifdef PCR
#   define EXIT() PCR_Base_Exit(1,PCR_waitForever)
# else
#   define EXIT() (MANAGED_STACK_ADDRESS_BOEHM_GC_on_abort(NULL), exit(1 /* EXIT_FAILURE */))
# endif

/* Print warning message, e.g. almost out of memory.    */
/* The argument (if any) format specifier should be:    */
/* "%s", "%p", "%"WARN_PRIdPTR or "%"WARN_PRIuPTR.      */
#define WARN(msg, arg) \
    (*MANAGED_STACK_ADDRESS_BOEHM_GC_current_warn_proc)((/* no const */ char *) \
                                (word)("GC Warning: " msg), \
                            (word)(arg))
MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN MANAGED_STACK_ADDRESS_BOEHM_GC_warn_proc MANAGED_STACK_ADDRESS_BOEHM_GC_current_warn_proc;

/* Print format type macro for decimal signed_word value passed WARN(). */
/* This could be redefined for Win64 or LLP64, but typically should     */
/* not be done as the WARN format string is, possibly, processed on the */
/* client side, so non-standard print type modifiers (like MS "I64d")   */
/* should be avoided here if possible.                                  */
#ifndef WARN_PRIdPTR
  /* Assume sizeof(void *) == sizeof(long) or a little-endian machine.  */
# define WARN_PRIdPTR "ld"
# define WARN_PRIuPTR "lu"
#endif

/* A tagging macro (for a code static analyzer) to indicate that the    */
/* string obtained from an untrusted source (e.g., argv[], getenv) is   */
/* safe to use in a vulnerable operation (e.g., open, exec).            */
#define TRUSTED_STRING(s) (char*)COVERT_DATAFLOW(s)

/* Get environment entry */
#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_READ_ENV_FILE
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER char * MANAGED_STACK_ADDRESS_BOEHM_GC_envfile_getenv(const char *name);
# define GETENV(name) MANAGED_STACK_ADDRESS_BOEHM_GC_envfile_getenv(name)
#elif defined(NO_GETENV) && !defined(CPPCHECK)
# define GETENV(name) NULL
#elif defined(EMPTY_GETENV_RESULTS)
  /* Workaround for a reputed Wine bug.   */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE char * fixed_getenv(const char *name)
  {
    char *value = getenv(name);
    return value != NULL && *value != '\0' ? value : NULL;
  }
# define GETENV(name) fixed_getenv(name)
#else
# define GETENV(name) getenv(name)
#endif

EXTERN_C_END

#if defined(DARWIN)
# include <mach/thread_status.h>
# ifndef MAC_OS_X_VERSION_MAX_ALLOWED
#   include <AvailabilityMacros.h>
                /* Include this header just to import the above macro.  */
# endif
# if defined(POWERPC)
#   if CPP_WORDSZ == 32
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_THREAD_STATE_T          ppc_thread_state_t
#   else
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_THREAD_STATE_T          ppc_thread_state64_t
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_THREAD_STATE       PPC_THREAD_STATE64
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_THREAD_STATE_COUNT PPC_THREAD_STATE64_COUNT
#   endif
# elif defined(I386) || defined(X86_64)
#   if CPP_WORDSZ == 32
#     if defined(i386_THREAD_STATE_COUNT) && !defined(x86_THREAD_STATE32_COUNT)
        /* Use old naming convention for 32-bit x86.    */
#       define MANAGED_STACK_ADDRESS_BOEHM_GC_THREAD_STATE_T                i386_thread_state_t
#       define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_THREAD_STATE             i386_THREAD_STATE
#       define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_THREAD_STATE_COUNT       i386_THREAD_STATE_COUNT
#     else
#       define MANAGED_STACK_ADDRESS_BOEHM_GC_THREAD_STATE_T                x86_thread_state32_t
#       define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_THREAD_STATE             x86_THREAD_STATE32
#       define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_THREAD_STATE_COUNT       x86_THREAD_STATE32_COUNT
#     endif
#   else
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_THREAD_STATE_T          x86_thread_state64_t
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_THREAD_STATE       x86_THREAD_STATE64
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_THREAD_STATE_COUNT x86_THREAD_STATE64_COUNT
#   endif
# elif defined(ARM32) && defined(ARM_UNIFIED_THREAD_STATE) \
       && !defined(CPPCHECK)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_THREAD_STATE_T            arm_unified_thread_state_t
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_THREAD_STATE         ARM_UNIFIED_THREAD_STATE
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_THREAD_STATE_COUNT   ARM_UNIFIED_THREAD_STATE_COUNT
# elif defined(ARM32)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_THREAD_STATE_T            arm_thread_state_t
#   ifdef ARM_MACHINE_THREAD_STATE_COUNT
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_THREAD_STATE       ARM_MACHINE_THREAD_STATE
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_THREAD_STATE_COUNT ARM_MACHINE_THREAD_STATE_COUNT
#   endif
# elif defined(AARCH64)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_THREAD_STATE_T            arm_thread_state64_t
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_THREAD_STATE         ARM_THREAD_STATE64
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_THREAD_STATE_COUNT   ARM_THREAD_STATE64_COUNT
# elif !defined(CPPCHECK)
#   error define MANAGED_STACK_ADDRESS_BOEHM_GC_THREAD_STATE_T
# endif
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_THREAD_STATE
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_THREAD_STATE         MACHINE_THREAD_STATE
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_THREAD_STATE_COUNT   MACHINE_THREAD_STATE_COUNT
# endif

# if CPP_WORDSZ == 32
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_HEADER   mach_header
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_SECTION  section
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_GETSECTBYNAME getsectbynamefromheader
# else
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_HEADER   mach_header_64
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_MACH_SECTION  section_64
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_GETSECTBYNAME getsectbynamefromheader_64
# endif

  /* Try to work out the right way to access thread state structure     */
  /* members.  The structure has changed its definition in different    */
  /* Darwin versions.  This now defaults to the (older) names           */
  /* without __, thus hopefully, not breaking any existing              */
  /* Makefile.direct builds.                                            */
# if __DARWIN_UNIX03
#   define THREAD_FLD_NAME(x) __ ## x
# else
#   define THREAD_FLD_NAME(x) x
# endif
# if defined(ARM32) && defined(ARM_UNIFIED_THREAD_STATE)
#   define THREAD_FLD(x) ts_32.THREAD_FLD_NAME(x)
# else
#   define THREAD_FLD(x) THREAD_FLD_NAME(x)
# endif
#endif /* DARWIN */

#ifndef WASI
# include <setjmp.h>
#endif

#include <stdio.h>

#if __STDC_VERSION__ >= 201112L
# include <assert.h> /* for static_assert */
#endif

EXTERN_C_BEGIN

/*********************************/
/*                               */
/* Word-size-dependent defines   */
/*                               */
/*********************************/

/* log[2] of CPP_WORDSZ.        */
#if CPP_WORDSZ == 32
# define LOGWL 5
#elif CPP_WORDSZ == 64
# define LOGWL 6
#endif

#define WORDS_TO_BYTES(x) ((x) << (LOGWL-3))
#define BYTES_TO_WORDS(x) ((x) >> (LOGWL-3))
#define modWORDSZ(n) ((n) & (CPP_WORDSZ-1)) /* n mod size of word */
#define divWORDSZ(n) ((n) >> LOGWL) /* divide n by size of word */

#define SIGNB ((word)1 << (CPP_WORDSZ-1))
#define BYTES_PER_WORD ((word)sizeof(word))

#if CPP_WORDSZ / 8 != ALIGNMENT
# define UNALIGNED_PTRS
#endif

#if MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES == 4
# define BYTES_TO_GRANULES(n) ((n)>>2)
# define GRANULES_TO_BYTES(n) ((n)<<2)
# define GRANULES_TO_WORDS(n) BYTES_TO_WORDS(GRANULES_TO_BYTES(n))
#elif MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES == 8
# define BYTES_TO_GRANULES(n) ((n)>>3)
# define GRANULES_TO_BYTES(n) ((n)<<3)
# if CPP_WORDSZ == 64
#   define GRANULES_TO_WORDS(n) (n)
# elif CPP_WORDSZ == 32
#   define GRANULES_TO_WORDS(n) ((n)<<1)
# else
#   define GRANULES_TO_WORDS(n) BYTES_TO_WORDS(GRANULES_TO_BYTES(n))
# endif
#elif MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES == 16
# define BYTES_TO_GRANULES(n) ((n)>>4)
# define GRANULES_TO_BYTES(n) ((n)<<4)
# if CPP_WORDSZ == 64
#   define GRANULES_TO_WORDS(n) ((n)<<1)
# elif CPP_WORDSZ == 32
#   define GRANULES_TO_WORDS(n) ((n)<<2)
# else
#   define GRANULES_TO_WORDS(n) BYTES_TO_WORDS(GRANULES_TO_BYTES(n))
# endif
#else
# error Bad MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES value
#endif

/*********************/
/*                   */
/*  Size Parameters  */
/*                   */
/*********************/

/* Heap block size, bytes. Should be power of 2.                */
/* Incremental GC with MPROTECT_VDB currently requires the      */
/* page size to be a multiple of HBLKSIZE.  Since most modern   */
/* architectures support variable page sizes down to 4 KB, and  */
/* x86 is generally 4 KB, we now default to 4 KB, except for    */
/*   Alpha: Seems to be used with 8 KB pages.                   */
/*   SMALL_CONFIG: Want less block-level fragmentation.         */
#ifndef HBLKSIZE
# if defined(SMALL_CONFIG) && !defined(LARGE_CONFIG)
#   define CPP_LOG_HBLKSIZE 10
# elif defined(ALPHA)
#   define CPP_LOG_HBLKSIZE 13
# else
#   define CPP_LOG_HBLKSIZE 12
# endif
#else
# if HBLKSIZE == 512
#   define CPP_LOG_HBLKSIZE 9
# elif HBLKSIZE == 1024
#   define CPP_LOG_HBLKSIZE 10
# elif HBLKSIZE == 2048
#   define CPP_LOG_HBLKSIZE 11
# elif HBLKSIZE == 4096
#   define CPP_LOG_HBLKSIZE 12
# elif HBLKSIZE == 8192
#   define CPP_LOG_HBLKSIZE 13
# elif HBLKSIZE == 16384
#   define CPP_LOG_HBLKSIZE 14
# elif HBLKSIZE == 32768
#   define CPP_LOG_HBLKSIZE 15
# elif HBLKSIZE == 65536
#   define CPP_LOG_HBLKSIZE 16
# elif !defined(CPPCHECK)
#   error Bad HBLKSIZE value
# endif
# undef HBLKSIZE
#endif

# define CPP_HBLKSIZE (1 << CPP_LOG_HBLKSIZE)
# define LOG_HBLKSIZE ((size_t)CPP_LOG_HBLKSIZE)
# define HBLKSIZE ((size_t)CPP_HBLKSIZE)

#define MANAGED_STACK_ADDRESS_BOEHM_GC_SQRT_SIZE_MAX ((((size_t)1) << (CPP_WORDSZ / 2)) - 1)

/*  Max size objects supported by freelist (larger objects are  */
/*  allocated directly with allchblk(), by rounding to the next */
/*  multiple of HBLKSIZE).                                      */
#define CPP_MAXOBJBYTES (CPP_HBLKSIZE/2)
#define MAXOBJBYTES ((size_t)CPP_MAXOBJBYTES)
#define CPP_MAXOBJWORDS BYTES_TO_WORDS(CPP_MAXOBJBYTES)
#define MAXOBJWORDS ((size_t)CPP_MAXOBJWORDS)
#define CPP_MAXOBJGRANULES BYTES_TO_GRANULES(CPP_MAXOBJBYTES)
#define MAXOBJGRANULES ((size_t)CPP_MAXOBJGRANULES)

# define divHBLKSZ(n) ((n) >> LOG_HBLKSIZE)

# define HBLK_PTR_DIFF(p,q) divHBLKSZ((ptr_t)p - (ptr_t)q)
        /* Equivalent to subtracting 2 hblk pointers.   */
        /* We do it this way because a compiler should  */
        /* find it hard to use an integer division      */
        /* instead of a shift.  The bundled SunOS 4.1   */
        /* o.w. sometimes pessimizes the subtraction to */
        /* involve a call to .div.                      */

# define modHBLKSZ(n) ((n) & (HBLKSIZE-1))

# define HBLKPTR(objptr) ((struct hblk *)(((word)(objptr)) \
                                          & ~(word)(HBLKSIZE-1)))
# define HBLKDISPL(objptr) modHBLKSZ((size_t)(objptr))

/* Round up allocation size (in bytes) to a multiple of a granule.      */
#define ROUNDUP_GRANULE_SIZE(lb) /* lb should have no side-effect */ \
        (SIZET_SAT_ADD(lb, MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES-1) \
         & ~(size_t)(MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES-1))

/* Round up byte allocation request (after adding EXTRA_BYTES) to   */
/* a multiple of a granule, then convert it to granules.            */
#define ALLOC_REQUEST_GRANS(lb) /* lb should have no side-effect */ \
        BYTES_TO_GRANULES(SIZET_SAT_ADD(lb, MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES-1 + EXTRA_BYTES))

#if MAX_EXTRA_BYTES == 0
# define ADD_EXTRA_BYTES(lb) (lb)
# define SMALL_OBJ(bytes) EXPECT((bytes) <= MAXOBJBYTES, TRUE)
#else
# define ADD_EXTRA_BYTES(lb) /* lb should have no side-effect */ \
            SIZET_SAT_ADD(lb, EXTRA_BYTES)
# define SMALL_OBJ(bytes) /* bytes argument should have no side-effect */ \
            (EXPECT((bytes) <= MAXOBJBYTES - MAX_EXTRA_BYTES, TRUE) \
             || (bytes) <= MAXOBJBYTES - EXTRA_BYTES)
        /* This really just tests bytes <= MAXOBJBYTES - EXTRA_BYTES.   */
        /* But we try to avoid looking up EXTRA_BYTES.                  */
#endif

/* Hash table representation of sets of pages.  Implements a map from   */
/* aligned HBLKSIZE chunks of the address space to one bit each.        */
/* This assumes it is OK to spuriously set bits, e.g. because multiple  */
/* addresses are represented by a single location.  Used by             */
/* black-listing code, and perhaps by dirty bit maintenance code.       */
#ifndef LOG_PHT_ENTRIES
# ifdef LARGE_CONFIG
#   if CPP_WORDSZ == 32
#     define LOG_PHT_ENTRIES 20 /* Collisions likely at 1M blocks,      */
                                /* which is >= 4 GB.  Each table takes  */
                                /* 128 KB, some of which may never be   */
                                /* touched.                             */
#   else
#     define LOG_PHT_ENTRIES 21 /* Collisions likely at 2M blocks,      */
                                /* which is >= 8 GB.  Each table takes  */
                                /* 256 KB, some of which may never be   */
                                /* touched.                             */
#   endif
# elif !defined(SMALL_CONFIG)
#   define LOG_PHT_ENTRIES  18   /* Collisions are likely if heap grows */
                                 /* to more than 256K hblks >= 1 GB.    */
                                 /* Each hash table occupies 32 KB.     */
                                 /* Even for somewhat smaller heaps,    */
                                 /* say half that, collisions may be an */
                                 /* issue because we blacklist          */
                                 /* addresses outside the heap.         */
# else
#   define LOG_PHT_ENTRIES  15   /* Collisions are likely if heap grows */
                                 /* to more than 32K hblks (128 MB).    */
                                 /* Each hash table occupies 4 KB.      */
# endif
#endif /* !LOG_PHT_ENTRIES */

# define PHT_ENTRIES ((word)1 << LOG_PHT_ENTRIES)
# define PHT_SIZE (LOG_PHT_ENTRIES > LOGWL ? PHT_ENTRIES >> LOGWL : 1)
typedef word page_hash_table[PHT_SIZE];

# define PHT_HASH(addr) ((((word)(addr)) >> LOG_HBLKSIZE) & (PHT_ENTRIES - 1))

# define get_pht_entry_from_index(bl, index) \
                (((bl)[divWORDSZ(index)] >> modWORDSZ(index)) & 1)
# define set_pht_entry_from_index(bl, index) \
                (void)((bl)[divWORDSZ(index)] |= (word)1 << modWORDSZ(index))

#if defined(THREADS) && defined(AO_HAVE_or)
  /* And, one more version for MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_black_list_normal/stack        */
  /* (invoked indirectly by MANAGED_STACK_ADDRESS_BOEHM_GC_do_local_mark) and                       */
  /* async_set_pht_entry_from_index (invoked by MANAGED_STACK_ADDRESS_BOEHM_GC_dirty or the write   */
  /* fault handler).                                                    */
# define set_pht_entry_from_index_concurrent(bl, index) \
                AO_or((volatile AO_t *)&(bl)[divWORDSZ(index)], \
                      (AO_t)((word)1 << modWORDSZ(index)))
#else
# define set_pht_entry_from_index_concurrent(bl, index) \
                set_pht_entry_from_index(bl, index)
#endif


/********************************************/
/*                                          */
/*    H e a p   B l o c k s                 */
/*                                          */
/********************************************/

#define MARK_BITS_PER_HBLK (HBLKSIZE/MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES)
                /* The upper bound.  We allocate 1 bit per allocation   */
                /* granule.  If MARK_BIT_PER_OBJ is not defined, we use */
                /* every n-th bit, where n is the number of allocation  */
                /* granules per object.  Otherwise, we only use the     */
                /* initial group of mark bits, and it is safe to        */
                /* allocate smaller header for large objects.           */

union word_ptr_ao_u {
  word w;
  signed_word sw;
  void *vp;
# ifdef PARALLEL_MARK
    volatile AO_t ao;
# endif
};

/* We maintain layout maps for heap blocks containing objects of a given */
/* size.  Each entry in this map describes a byte offset and has the     */
/* following type.                                                       */
struct hblkhdr {
    struct hblk * hb_next;      /* Link field for hblk free list         */
                                /* and for lists of chunks waiting to be */
                                /* reclaimed.                            */
    struct hblk * hb_prev;      /* Backwards link for free list.        */
    struct hblk * hb_block;     /* The corresponding block.             */
    unsigned char hb_obj_kind;
                         /* Kind of objects in the block.  Each kind    */
                         /* identifies a mark procedure and a set of    */
                         /* list headers.  Sometimes called regions.    */
    unsigned char hb_flags;
#       define IGNORE_OFF_PAGE  1       /* Ignore pointers that do not  */
                                        /* point to the first hblk of   */
                                        /* this object.                 */
#       define WAS_UNMAPPED 2   /* This is a free block, which has      */
                                /* been unmapped from the address       */
                                /* space.                               */
                                /* MANAGED_STACK_ADDRESS_BOEHM_GC_remap must be invoked on it       */
                                /* before it can be reallocated.        */
                                /* Only set with USE_MUNMAP.            */
#       define FREE_BLK 4       /* Block is free, i.e. not in use.      */
#       ifdef ENABLE_DISCLAIM
#         define HAS_DISCLAIM 8
                                /* This kind has a callback on reclaim. */
#         define MARK_UNCONDITIONALLY 0x10
                                /* Mark from all objects, marked or     */
                                /* not.  Used to mark objects needed by */
                                /* reclaim notifier.                    */
#       endif
#       ifndef MARK_BIT_PER_OBJ
#         define LARGE_BLOCK 0x20
#       endif
    unsigned short hb_last_reclaimed;
                                /* Value of MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no when block was     */
                                /* last allocated or swept. May wrap.   */
                                /* For a free block, this is maintained */
                                /* only for USE_MUNMAP, and indicates   */
                                /* when the header was allocated, or    */
                                /* when the size of the block last      */
                                /* changed.                             */
#   ifdef MARK_BIT_PER_OBJ
      unsigned32 hb_inv_sz;     /* A good upper bound for 2**32/hb_sz.  */
                                /* For large objects, we use            */
                                /* LARGE_INV_SZ.                        */
#     define LARGE_INV_SZ ((unsigned32)1 << 16)
#   endif
    word hb_sz; /* If in use, size in bytes, of objects in the block.   */
                /* if free, the size in bytes of the whole block.       */
                /* We assume that this is convertible to signed_word    */
                /* without generating a negative result.  We avoid      */
                /* generating free blocks larger than that.             */
    word hb_descr;              /* object descriptor for marking.  See  */
                                /* gc_mark.h.                           */
#   ifndef MARK_BIT_PER_OBJ
      unsigned short * hb_map;  /* Essentially a table of remainders    */
                                /* mod BYTES_TO_GRANULES(hb_sz), except */
                                /* for large blocks.  See MANAGED_STACK_ADDRESS_BOEHM_GC_obj_map.   */
#   endif
#   ifdef PARALLEL_MARK
      volatile AO_t hb_n_marks; /* Number of set mark bits, excluding   */
                                /* the one always set at the end.       */
                                /* Currently it is concurrently         */
                                /* updated and hence only approximate.  */
                                /* But a zero value does guarantee that */
                                /* the block contains no marked         */
                                /* objects.                             */
                                /* Ensuring this property means that we */
                                /* never decrement it to zero during a  */
                                /* collection, and hence the count may  */
                                /* be one too high.  Due to concurrent  */
                                /* updates, an arbitrary number of      */
                                /* increments, but not all of them (!)  */
                                /* may be lost, hence it may in theory  */
                                /* be much too low.                     */
                                /* The count may also be too high if    */
                                /* multiple mark threads mark the       */
                                /* same object due to a race.           */
#   else
      size_t hb_n_marks;        /* Without parallel marking, the count  */
                                /* is accurate.                         */
#   endif
#   ifdef USE_MARK_BYTES
#     define MARK_BITS_SZ (MARK_BITS_PER_HBLK + 1)
        /* Unlike the other case, this is in units of bytes.            */
        /* Since we force double-word alignment, we need at most one    */
        /* mark bit per 2 words.  But we do allocate and set one        */
        /* extra mark bit to avoid an explicit check for the            */
        /* partial object at the end of each block.                     */
      union {
        char _hb_marks[MARK_BITS_SZ];
                            /* The i'th byte is 1 if the object         */
                            /* starting at granule i or object i is     */
                            /* marked, 0 otherwise.                     */
                            /* The mark bit for the "one past the end"  */
                            /* object is always set to avoid a special  */
                            /* case test in the marker.                 */
        word dummy;     /* Force word alignment of mark bytes. */
      } _mark_byte_union;
#     define hb_marks _mark_byte_union._hb_marks
#   else
#     define MARK_BITS_SZ (MARK_BITS_PER_HBLK/CPP_WORDSZ + 1)
      word hb_marks[MARK_BITS_SZ];
#   endif /* !USE_MARK_BYTES */
};

# define ANY_INDEX 23   /* "Random" mark bit index for assertions */

/*  heap block body */

# define HBLK_WORDS (HBLKSIZE/sizeof(word))
# define HBLK_GRANULES (HBLKSIZE/MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES)

/* The number of objects in a block dedicated to a certain size.        */
/* may erroneously yield zero (instead of one) for large objects.       */
# define HBLK_OBJS(sz_in_bytes) (HBLKSIZE/(sz_in_bytes))

struct hblk {
    char hb_body[HBLKSIZE];
};

# define HBLK_IS_FREE(hdr) (((hdr) -> hb_flags & FREE_BLK) != 0)

# define OBJ_SZ_TO_BLOCKS(lb) divHBLKSZ((lb) + HBLKSIZE-1)
# define OBJ_SZ_TO_BLOCKS_CHECKED(lb) /* lb should have no side-effect */ \
                                divHBLKSZ(SIZET_SAT_ADD(lb, HBLKSIZE-1))
    /* Size of block (in units of HBLKSIZE) needed to hold objects of   */
    /* given lb (in bytes).  The checked variant prevents wrap around.  */

/* Object free list link */
# define obj_link(p) (*(void **)(p))

# define LOG_MAX_MARK_PROCS 6
# define MAX_MARK_PROCS (1 << LOG_MAX_MARK_PROCS)

/* Root sets.  Logically private to mark_rts.c.  But we don't want the  */
/* tables scanned, so we put them here.                                 */
/* MAX_ROOT_SETS is the maximum number of ranges that can be    */
/* registered as static roots.                                  */
# ifdef LARGE_CONFIG
#   define MAX_ROOT_SETS 8192
# elif !defined(SMALL_CONFIG)
#   define MAX_ROOT_SETS 2048
# else
#   define MAX_ROOT_SETS 512
# endif

# define MAX_EXCLUSIONS (MAX_ROOT_SETS/4)
/* Maximum number of segments that can be excluded from root sets.      */

/*
 * Data structure for excluded static roots.
 */
struct exclusion {
    ptr_t e_start;
    ptr_t e_end;
};

/* Data structure for list of root sets.                                */
/* We keep a hash table, so that we can filter out duplicate additions. */
/* Under Win32, we need to do a better job of filtering overlaps, so    */
/* we resort to sequential search, and pay the price.                   */
struct roots {
        ptr_t r_start;/* multiple of word size */
        ptr_t r_end;  /* multiple of word size and greater than r_start */
#       ifndef ANY_MSWIN
          struct roots * r_next;
#       endif
        MANAGED_STACK_ADDRESS_BOEHM_GC_bool r_tmp;
                /* Delete before registering new dynamic libraries */
};

#ifndef ANY_MSWIN
    /* Size of hash table index to roots.       */
#   define LOG_RT_SIZE 6
#   define RT_SIZE (1 << LOG_RT_SIZE) /* Power of 2, may be != MAX_ROOT_SETS */
#endif /* !ANY_MSWIN */

#if (!defined(MAX_HEAP_SECTS) || defined(CPPCHECK)) \
    && (defined(ANY_MSWIN) || defined(USE_PROC_FOR_LIBRARIES))
# ifdef LARGE_CONFIG
#   if CPP_WORDSZ > 32
#     define MAX_HEAP_SECTS 81920
#   else
#     define MAX_HEAP_SECTS 7680
#   endif
# elif defined(SMALL_CONFIG) && !defined(USE_PROC_FOR_LIBRARIES)
#   if defined(PARALLEL_MARK) && (defined(MSWIN32) || defined(CYGWIN32))
#     define MAX_HEAP_SECTS 384
#   else
#     define MAX_HEAP_SECTS 128         /* Roughly 256 MB (128*2048*1024) */
#   endif
# elif CPP_WORDSZ > 32
#   define MAX_HEAP_SECTS 1024          /* Roughly 8 GB */
# else
#   define MAX_HEAP_SECTS 512           /* Roughly 4 GB */
# endif
#endif /* !MAX_HEAP_SECTS */

typedef struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry {
    ptr_t mse_start;    /* First word of object, word aligned.  */
    union word_ptr_ao_u mse_descr;
                        /* Descriptor; low order two bits are tags,     */
                        /* as described in gc_mark.h.                   */
} mse;

typedef int mark_state_t;   /* Current state of marking.                */
                            /* Used to remember where we are during     */
                            /* concurrent marking.                      */

struct disappearing_link;
struct finalizable_object;

struct dl_hashtbl_s {
    struct disappearing_link **head;
    word entries;
    unsigned log_size;
};

struct fnlz_roots_s {
  struct finalizable_object **fo_head;
  /* List of objects that should be finalized now: */
  struct finalizable_object *finalize_now;
};

union toggle_ref_u {
  /* The least significant bit is used to distinguish between choices.  */
  void *strong_ref;
  MANAGED_STACK_ADDRESS_BOEHM_GC_hidden_pointer weak_ref;
};

/* Extended descriptors.  MANAGED_STACK_ADDRESS_BOEHM_GC_typed_mark_proc understands these. */
/* These are used for simple objects that are larger than what  */
/* can be described by a BITMAP_BITS sized bitmap.              */
typedef struct {
    word ed_bitmap; /* the least significant bit corresponds to first word. */
    MANAGED_STACK_ADDRESS_BOEHM_GC_bool ed_continued;       /* next entry is continuation.  */
} typed_ext_descr_t;

struct HeapSect {
    ptr_t hs_start;
    size_t hs_bytes;
};

/* Lists of all heap blocks and free lists      */
/* as well as other random data structures      */
/* that should not be scanned by the            */
/* collector.                                   */
/* These are grouped together in a struct       */
/* so that they can be easily skipped by the    */
/* MANAGED_STACK_ADDRESS_BOEHM_GC_mark routine.                             */
/* The ordering is weird to make MANAGED_STACK_ADDRESS_BOEHM_GC_malloc      */
/* faster by keeping the important fields       */
/* sufficiently close together that a           */
/* single load of a base register will do.      */
/* Scalars that could easily appear to          */
/* be pointers are also put here.               */
/* The main fields should precede any           */
/* conditionally included fields, so that       */
/* gc_inline.h will work even if a different    */
/* set of macros is defined when the client is  */
/* compiled.                                    */

struct _MANAGED_STACK_ADDRESS_BOEHM_GC_arrays {
  word _heapsize;       /* Heap size in bytes (value never goes down).  */
  word _requested_heapsize;     /* Heap size due to explicit expansion. */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize_on_gc_disable MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._heapsize_on_gc_disable
  word _heapsize_on_gc_disable;
  ptr_t _last_heap_addr;
  word _large_free_bytes;
        /* Total bytes contained in blocks on large object free */
        /* list.                                                */
  word _large_allocd_bytes;
        /* Total number of bytes in allocated large objects blocks.     */
        /* For the purposes of this counter and the next one only, a    */
        /* large object is one that occupies a block of at least        */
        /* 2*HBLKSIZE.                                                  */
  word _max_large_allocd_bytes;
        /* Maximum number of bytes that were ever allocated in          */
        /* large object blocks.  This is used to help decide when it    */
        /* is safe to split up a large block.                           */
  word _bytes_allocd_before_gc;
                /* Number of bytes allocated before this        */
                /* collection cycle.                            */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_our_mem_bytes MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._our_mem_bytes
  word _our_mem_bytes;
# ifndef SEPARATE_GLOBALS
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._bytes_allocd
    word _bytes_allocd;
        /* Number of bytes allocated during this collection cycle.      */
# endif
  word _bytes_dropped;
        /* Number of black-listed bytes dropped during GC cycle */
        /* as a result of repeated scanning during allocation   */
        /* attempts.  These are treated largely as allocated,   */
        /* even though they are not useful to the client.       */
  word _bytes_finalized;
        /* Approximate number of bytes in objects (and headers) */
        /* that became ready for finalization in the last       */
        /* collection.                                          */
  word _bytes_freed;
        /* Number of explicitly deallocated bytes of memory     */
        /* since last collection.                               */
  word _finalizer_bytes_freed;
        /* Bytes of memory explicitly deallocated while         */
        /* finalizers were running.  Used to approximate memory */
        /* explicitly deallocated by finalizers.                */
  bottom_index *_all_bottom_indices;
        /* Pointer to the first (lowest address) bottom_index;  */
        /* assumes the lock is held.                            */
  bottom_index *_all_bottom_indices_end;
        /* Pointer to the last (highest address) bottom_index;  */
        /* assumes the lock is held.                            */
  ptr_t _scratch_free_ptr;
  hdr *_hdr_free_list;
  ptr_t _scratch_end_ptr;
        /* MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_end_ptr is end point of the current scratch area. */
# if defined(IRIX5) || (defined(USE_PROC_FOR_LIBRARIES) && !defined(LINUX))
#   define USE_SCRATCH_LAST_END_PTR
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_last_end_ptr MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._scratch_last_end_ptr
    ptr_t _scratch_last_end_ptr;
        /* MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_last_end_ptr is the end point of the last */
        /* obtained scratch area.                               */
        /* Used by MANAGED_STACK_ADDRESS_BOEHM_GC_register_dynamic_libraries().             */
# endif
# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) || defined(MAKE_BACK_GRAPH) \
     || defined(INCLUDE_LINUX_THREAD_DESCR) \
     || (defined(KEEP_BACK_PTRS) && ALIGNMENT == 1)
#   define SET_REAL_HEAP_BOUNDS
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_least_real_heap_addr MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._least_real_heap_addr
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_real_heap_addr MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._greatest_real_heap_addr
    word _least_real_heap_addr;
    word _greatest_real_heap_addr;
        /* Similar to MANAGED_STACK_ADDRESS_BOEHM_GC_least/greatest_plausible_heap_addr but */
        /* do not include future (potential) heap expansion.    */
        /* Both variables are zero initially.                   */
# endif
  mse *_mark_stack;
        /* Limits of stack for MANAGED_STACK_ADDRESS_BOEHM_GC_mark routine.  All ranges     */
        /* between MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack (incl.) and MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top  */
        /* (incl.) still need to be marked from.                */
  mse *_mark_stack_limit;
# ifdef PARALLEL_MARK
    mse *volatile _mark_stack_top;
        /* Updated only with mark lock held, but read asynchronously.   */
        /* TODO: Use union to avoid casts to AO_t */
# else
    mse *_mark_stack_top;
# endif
  word _composite_in_use; /* Number of bytes in the accessible  */
                          /* composite objects.                 */
  word _atomic_in_use;    /* Number of bytes in the accessible  */
                          /* atomic objects.                    */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_last_heap_growth_gc_no MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._last_heap_growth_gc_no
  word _last_heap_growth_gc_no;
                /* GC number of latest successful MANAGED_STACK_ADDRESS_BOEHM_GC_expand_hp_inner call */
# ifdef USE_MUNMAP
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_unmapped_bytes MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._unmapped_bytes
    word _unmapped_bytes;
#   ifdef COUNT_UNMAPPED_REGIONS
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_num_unmapped_regions MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._num_unmapped_regions
      signed_word _num_unmapped_regions;
#   endif
# else
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_unmapped_bytes 0
# endif
  bottom_index * _all_nils;
# define MANAGED_STACK_ADDRESS_BOEHM_GC_scan_ptr MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._scan_ptr
  struct hblk * _scan_ptr;
# ifdef PARALLEL_MARK
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_main_local_mark_stack MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._main_local_mark_stack
    mse *_main_local_mark_stack;
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_first_nonempty MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._first_nonempty
    volatile AO_t _first_nonempty;
                        /* Lowest entry on mark stack that may be       */
                        /* nonempty. Updated only by initiating thread. */
# endif
# define MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_size MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._mark_stack_size
  size_t _mark_stack_size;
# define MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._mark_state
  mark_state_t _mark_state; /* Initialized to MS_NONE (0). */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_too_small MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._mark_stack_too_small
  MANAGED_STACK_ADDRESS_BOEHM_GC_bool _mark_stack_too_small;
                        /* We need a larger mark stack.  May be set by  */
                        /* client supplied mark routines.               */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_objects_are_marked MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._objects_are_marked
  MANAGED_STACK_ADDRESS_BOEHM_GC_bool _objects_are_marked;
                /* Are there collectible marked objects in the heap?    */
# ifdef ENABLE_TRACE
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_trace_addr MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._trace_addr
    ptr_t _trace_addr;
# endif
# define MANAGED_STACK_ADDRESS_BOEHM_GC_capacity_heap_sects MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._capacity_heap_sects
  size_t _capacity_heap_sects;
# define MANAGED_STACK_ADDRESS_BOEHM_GC_n_heap_sects MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._n_heap_sects
  word _n_heap_sects;   /* Number of separately added heap sections.    */
# ifdef ANY_MSWIN
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_n_heap_bases MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._n_heap_bases
    word _n_heap_bases; /* See MANAGED_STACK_ADDRESS_BOEHM_GC_heap_bases.   */
# endif
# ifdef USE_PROC_FOR_LIBRARIES
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_n_memory MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._n_memory
    word _n_memory;     /* Number of GET_MEM allocated memory sections. */
# endif
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_gcjobjfreelist MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._gcjobjfreelist
    ptr_t *_gcjobjfreelist;
# endif
# define MANAGED_STACK_ADDRESS_BOEHM_GC_fo_entries MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._fo_entries
  word _fo_entries;
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_dl_hashtbl MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._dl_hashtbl
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._fnlz_roots
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_log_fo_table_size MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._log_fo_table_size
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_LONG_REFS_NOT_NEEDED
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_ll_hashtbl MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._ll_hashtbl
      struct dl_hashtbl_s _ll_hashtbl;
#   endif
    struct dl_hashtbl_s _dl_hashtbl;
    struct fnlz_roots_s _fnlz_roots;
    unsigned _log_fo_table_size;
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_TOGGLE_REFS_NOT_NEEDED
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_arr MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._toggleref_arr
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_size MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._toggleref_array_size
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_capacity MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._toggleref_array_capacity
      union toggle_ref_u *_toggleref_arr;
      size_t _toggleref_array_size;
      size_t _toggleref_array_capacity;
#   endif
# endif
# ifdef TRACE_BUF
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_trace_buf_ptr MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._trace_buf_ptr
    int _trace_buf_ptr;
# endif
# ifdef ENABLE_DISCLAIM
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_finalized_kind MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._finalized_kind
    unsigned _finalized_kind;
# endif
# define n_root_sets MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._n_root_sets
# define MANAGED_STACK_ADDRESS_BOEHM_GC_excl_table_entries MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._excl_table_entries
  int _n_root_sets;     /* MANAGED_STACK_ADDRESS_BOEHM_GC_static_roots[0..n_root_sets) contains the */
                        /* valid root sets.                             */
  size_t _excl_table_entries;   /* Number of entries in use.    */
# ifdef THREADS
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_roots_were_cleared MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._roots_were_cleared
    MANAGED_STACK_ADDRESS_BOEHM_GC_bool _roots_were_cleared;
# endif
# define MANAGED_STACK_ADDRESS_BOEHM_GC_explicit_typing_initialized MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._explicit_typing_initialized
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ed_size MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._ed_size
# define MANAGED_STACK_ADDRESS_BOEHM_GC_avail_descr MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._avail_descr
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ext_descriptors MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._ext_descriptors
# ifdef AO_HAVE_load_acquire
    volatile AO_t _explicit_typing_initialized;
# else
    MANAGED_STACK_ADDRESS_BOEHM_GC_bool _explicit_typing_initialized;
# endif
  size_t _ed_size;      /* Current size of above arrays.        */
  size_t _avail_descr;  /* Next available slot.                 */
  typed_ext_descr_t *_ext_descriptors;  /* Points to array of extended  */
                                        /* descriptors.                 */
  MANAGED_STACK_ADDRESS_BOEHM_GC_mark_proc _mark_procs[MAX_MARK_PROCS];
        /* Table of user-defined mark procedures.  There is     */
        /* a small number of these, which can be referenced     */
        /* by DS_PROC mark descriptors.  See gc_mark.h.         */
  char _modws_valid_offsets[sizeof(word)];
                                /* MANAGED_STACK_ADDRESS_BOEHM_GC_valid_offsets[i] ==>                */
                                /* MANAGED_STACK_ADDRESS_BOEHM_GC_modws_valid_offsets[i%sizeof(word)] */
# ifndef ANY_MSWIN
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_root_index MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._root_index
    struct roots * _root_index[RT_SIZE];
# endif
# ifdef SAVE_CALL_CHAIN
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_last_stack MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._last_stack
    struct callinfo _last_stack[NFRAMES];
                /* Stack at last garbage collection.  Useful for        */
                /* debugging mysterious object disappearances.  In the  */
                /* multi-threaded case, we currently only save the      */
                /* calling stack.                                       */
# endif
# ifndef SEPARATE_GLOBALS
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_objfreelist MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._objfreelist
    void *_objfreelist[MAXOBJGRANULES+1];
                          /* free list for objects */
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_aobjfreelist MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._aobjfreelist
    void *_aobjfreelist[MAXOBJGRANULES+1];
                          /* free list for atomic objects       */
# endif
  void *_uobjfreelist[MAXOBJGRANULES+1];
                          /* Uncollectible but traced objects.  */
                          /* Objects on this and _auobjfreelist */
                          /* are always marked, except during   */
                          /* garbage collections.               */
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ATOMIC_UNCOLLECTABLE
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_auobjfreelist MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._auobjfreelist
    void *_auobjfreelist[MAXOBJGRANULES+1];
                /* Atomic uncollectible but traced objects.     */
# endif
  size_t _size_map[MAXOBJBYTES+1];
        /* Number of granules to allocate when asked for a certain      */
        /* number of bytes (plus EXTRA_BYTES).  Should be accessed with */
        /* the allocation lock held.                                    */
# ifndef MARK_BIT_PER_OBJ
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_obj_map MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._obj_map
    unsigned short * _obj_map[MAXOBJGRANULES + 1];
                       /* If not NULL, then a pointer to a map of valid */
                       /* object addresses.                             */
                       /* MANAGED_STACK_ADDRESS_BOEHM_GC_obj_map[sz_in_granules][i] is              */
                       /* i % sz_in_granules.                           */
                       /* This is now used purely to replace a          */
                       /* division in the marker by a table lookup.     */
                       /* _obj_map[0] is used for large objects and     */
                       /* contains all nonzero entries.  This gets us   */
                       /* out of the marker fast path without an extra  */
                       /* test.                                         */
#   define OBJ_MAP_LEN  BYTES_TO_GRANULES(HBLKSIZE)
# endif
# define VALID_OFFSET_SZ HBLKSIZE
  char _valid_offsets[VALID_OFFSET_SZ];
                                /* MANAGED_STACK_ADDRESS_BOEHM_GC_valid_offsets[i] == TRUE ==> i    */
                                /* is registered as a displacement.     */
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_grungy_pages MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._grungy_pages
    page_hash_table _grungy_pages; /* Pages that were dirty at last     */
                                   /* MANAGED_STACK_ADDRESS_BOEHM_GC_read_dirty.                    */
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_dirty_pages MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._dirty_pages
    volatile page_hash_table _dirty_pages;
                        /* Pages dirtied since last MANAGED_STACK_ADDRESS_BOEHM_GC_read_dirty. */
# endif
# if (defined(CHECKSUMS) && (defined(GWW_VDB) || defined(SOFT_VDB))) \
     || defined(PROC_VDB)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_written_pages MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._written_pages
    page_hash_table _written_pages;     /* Pages ever dirtied   */
# endif
# define MANAGED_STACK_ADDRESS_BOEHM_GC_heap_sects MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._heap_sects
  struct HeapSect *_heap_sects;         /* Heap segments potentially    */
                                        /* client objects.              */
# if defined(USE_PROC_FOR_LIBRARIES)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_our_memory MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._our_memory
    struct HeapSect _our_memory[MAX_HEAP_SECTS];
                                        /* All GET_MEM allocated        */
                                        /* memory.  Includes block      */
                                        /* headers and the like.        */
# endif
# ifdef ANY_MSWIN
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_heap_bases MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._heap_bases
    ptr_t _heap_bases[MAX_HEAP_SECTS];
                /* Start address of memory regions obtained from kernel. */
# endif
# ifdef MSWINCE
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_heap_lengths MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._heap_lengths
    word _heap_lengths[MAX_HEAP_SECTS];
                /* Committed lengths of memory regions obtained from kernel. */
# endif
  struct roots _static_roots[MAX_ROOT_SETS];
  struct exclusion _excl_table[MAX_EXCLUSIONS];
  /* Block header index; see gc_headers.h */
  bottom_index * _top_index[TOP_SZ];
};

MANAGED_STACK_ADDRESS_BOEHM_GC_API_PRIV MANAGED_STACK_ADDRESS_BOEHM_GC_FAR struct _MANAGED_STACK_ADDRESS_BOEHM_GC_arrays MANAGED_STACK_ADDRESS_BOEHM_GC_arrays;

#define MANAGED_STACK_ADDRESS_BOEHM_GC_all_nils MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._all_nils
#define MANAGED_STACK_ADDRESS_BOEHM_GC_atomic_in_use MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._atomic_in_use
#define MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd_before_gc MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._bytes_allocd_before_gc
#define MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_dropped MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._bytes_dropped
#define MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_finalized MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._bytes_finalized
#define MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_freed MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._bytes_freed
#define MANAGED_STACK_ADDRESS_BOEHM_GC_composite_in_use MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._composite_in_use
#define MANAGED_STACK_ADDRESS_BOEHM_GC_excl_table MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._excl_table
#define MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_bytes_freed MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._finalizer_bytes_freed
#define MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._heapsize
#define MANAGED_STACK_ADDRESS_BOEHM_GC_large_allocd_bytes MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._large_allocd_bytes
#define MANAGED_STACK_ADDRESS_BOEHM_GC_large_free_bytes MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._large_free_bytes
#define MANAGED_STACK_ADDRESS_BOEHM_GC_last_heap_addr MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._last_heap_addr
#define MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._mark_stack
#define MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_limit MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._mark_stack_limit
#define MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._mark_stack_top
#define MANAGED_STACK_ADDRESS_BOEHM_GC_mark_procs MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._mark_procs
#define MANAGED_STACK_ADDRESS_BOEHM_GC_max_large_allocd_bytes MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._max_large_allocd_bytes
#define MANAGED_STACK_ADDRESS_BOEHM_GC_modws_valid_offsets MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._modws_valid_offsets
#define MANAGED_STACK_ADDRESS_BOEHM_GC_requested_heapsize MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._requested_heapsize
#define MANAGED_STACK_ADDRESS_BOEHM_GC_all_bottom_indices MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._all_bottom_indices
#define MANAGED_STACK_ADDRESS_BOEHM_GC_all_bottom_indices_end MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._all_bottom_indices_end
#define MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_free_ptr MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._scratch_free_ptr
#define MANAGED_STACK_ADDRESS_BOEHM_GC_hdr_free_list MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._hdr_free_list
#define MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_end_ptr MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._scratch_end_ptr
#define MANAGED_STACK_ADDRESS_BOEHM_GC_size_map MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._size_map
#define MANAGED_STACK_ADDRESS_BOEHM_GC_static_roots MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._static_roots
#define MANAGED_STACK_ADDRESS_BOEHM_GC_top_index MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._top_index
#define MANAGED_STACK_ADDRESS_BOEHM_GC_uobjfreelist MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._uobjfreelist
#define MANAGED_STACK_ADDRESS_BOEHM_GC_valid_offsets MANAGED_STACK_ADDRESS_BOEHM_GC_arrays._valid_offsets

#define beginMANAGED_STACK_ADDRESS_BOEHM_GC_arrays ((ptr_t)(&MANAGED_STACK_ADDRESS_BOEHM_GC_arrays))
#define endMANAGED_STACK_ADDRESS_BOEHM_GC_arrays ((ptr_t)(&MANAGED_STACK_ADDRESS_BOEHM_GC_arrays) + sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_arrays))

/* Object kinds: */
#ifndef MAXOBJKINDS
# define MAXOBJKINDS 16
#endif
MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN struct obj_kind {
  void **ok_freelist;   /* Array of free list headers for this kind of  */
                        /* object.  Point either to MANAGED_STACK_ADDRESS_BOEHM_GC_arrays or to     */
                        /* storage allocated with MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_alloc.     */
  struct hblk **ok_reclaim_list;
                        /* List headers for lists of blocks waiting to  */
                        /* be swept.  Indexed by object size in         */
                        /* granules.                                    */
  word ok_descriptor;   /* Descriptor template for objects in this      */
                        /* block.                                       */
  MANAGED_STACK_ADDRESS_BOEHM_GC_bool ok_relocate_descr;
                        /* Add object size in bytes to descriptor       */
                        /* template to obtain descriptor.  Otherwise    */
                        /* template is used as is.                      */
  MANAGED_STACK_ADDRESS_BOEHM_GC_bool ok_init;
                /* Clear objects before putting them on the free list.  */
# ifdef ENABLE_DISCLAIM
    MANAGED_STACK_ADDRESS_BOEHM_GC_bool ok_mark_unconditionally;
                        /* Mark from all, including unmarked, objects   */
                        /* in block.  Used to protect objects reachable */
                        /* from reclaim notifiers.                      */
    int (MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK *ok_disclaim_proc)(void * /*obj*/);
                        /* The disclaim procedure is called before obj  */
                        /* is reclaimed, but must also tolerate being   */
                        /* called with object from freelist.  Non-zero  */
                        /* exit prevents object from being reclaimed.   */
#   define OK_DISCLAIM_INITZ /* comma */, FALSE, 0
# else
#   define OK_DISCLAIM_INITZ /* empty */
# endif /* !ENABLE_DISCLAIM */
} MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[MAXOBJKINDS];

#define beginMANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds ((ptr_t)(&MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[0]))
#define endMANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds (beginMANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds + sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds))

/* Variables that used to be in MANAGED_STACK_ADDRESS_BOEHM_GC_arrays, but need to be accessed by   */
/* inline allocation code.  If they were in MANAGED_STACK_ADDRESS_BOEHM_GC_arrays, the inlined      */
/* allocation code would include MANAGED_STACK_ADDRESS_BOEHM_GC_arrays offsets (as it did), which   */
/* introduce maintenance problems.                                      */

#ifdef SEPARATE_GLOBALS
  extern word MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd;
        /* Number of bytes allocated during this collection cycle.      */
  extern ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_objfreelist[MAXOBJGRANULES+1];
                          /* free list for NORMAL objects */
# define beginMANAGED_STACK_ADDRESS_BOEHM_GC_objfreelist ((ptr_t)(&MANAGED_STACK_ADDRESS_BOEHM_GC_objfreelist[0]))
# define endMANAGED_STACK_ADDRESS_BOEHM_GC_objfreelist (beginMANAGED_STACK_ADDRESS_BOEHM_GC_objfreelist + sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_objfreelist))

  extern ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_aobjfreelist[MAXOBJGRANULES+1];
                          /* free list for atomic (PTRFREE) objects     */
# define beginMANAGED_STACK_ADDRESS_BOEHM_GC_aobjfreelist ((ptr_t)(&MANAGED_STACK_ADDRESS_BOEHM_GC_aobjfreelist[0]))
# define endMANAGED_STACK_ADDRESS_BOEHM_GC_aobjfreelist (beginMANAGED_STACK_ADDRESS_BOEHM_GC_aobjfreelist + sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_aobjfreelist))
#endif /* SEPARATE_GLOBALS */

/* Predefined kinds: */
#define PTRFREE MANAGED_STACK_ADDRESS_BOEHM_GC_I_PTRFREE
#define NORMAL  MANAGED_STACK_ADDRESS_BOEHM_GC_I_NORMAL
#define UNCOLLECTABLE 2
#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ATOMIC_UNCOLLECTABLE
# define AUNCOLLECTABLE 3
# define IS_UNCOLLECTABLE(k) (((k) & ~1) == UNCOLLECTABLE)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_N_KINDS_INITIAL_VALUE 4
#else
# define IS_UNCOLLECTABLE(k) ((k) == UNCOLLECTABLE)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_N_KINDS_INITIAL_VALUE 3
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_n_kinds;

MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN size_t MANAGED_STACK_ADDRESS_BOEHM_GC_page_size;
                /* May mean the allocation granularity size, not page size. */

#ifdef REAL_PAGESIZE_NEEDED
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN size_t MANAGED_STACK_ADDRESS_BOEHM_GC_real_page_size;
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_real_page_size MANAGED_STACK_ADDRESS_BOEHM_GC_page_size
#endif

/* Round up allocation size to a multiple of a page size.       */
/* MANAGED_STACK_ADDRESS_BOEHM_GC_setpagesize() is assumed to be already invoked.           */
#define ROUNDUP_PAGESIZE(lb) /* lb should have no side-effect */ \
            (SIZET_SAT_ADD(lb, MANAGED_STACK_ADDRESS_BOEHM_GC_page_size - 1) & ~(MANAGED_STACK_ADDRESS_BOEHM_GC_page_size - 1))

/* Same as above but used to make GET_MEM() argument safe.      */
#ifdef MMAP_SUPPORTED
# define ROUNDUP_PAGESIZE_IF_MMAP(lb) ROUNDUP_PAGESIZE(lb)
#else
# define ROUNDUP_PAGESIZE_IF_MMAP(lb) (lb)
#endif

#ifdef ANY_MSWIN
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN SYSTEM_INFO MANAGED_STACK_ADDRESS_BOEHM_GC_sysinfo;
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_is_heap_base(const void *p);
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN word MANAGED_STACK_ADDRESS_BOEHM_GC_black_list_spacing;
                        /* Average number of bytes between blacklisted  */
                        /* blocks. Approximate.                         */
                        /* Counts only blocks that are                  */
                        /* "stack-blacklisted", i.e. that are           */
                        /* problematic in the interior of an object.    */

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT
  extern struct hblk * MANAGED_STACK_ADDRESS_BOEHM_GC_hblkfreelist[];
  extern word MANAGED_STACK_ADDRESS_BOEHM_GC_free_bytes[];  /* Both remain visible to GNU GCJ.      */
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN word MANAGED_STACK_ADDRESS_BOEHM_GC_root_size; /* Total size of registered root sections. */

MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_debugging_started;
                                /* MANAGED_STACK_ADDRESS_BOEHM_GC_debug_malloc has been called.     */

/* This is used by MANAGED_STACK_ADDRESS_BOEHM_GC_do_blocking[_inner]().            */
struct blocking_data {
    MANAGED_STACK_ADDRESS_BOEHM_GC_fn_type fn;
    void * client_data; /* and result */
};

/* This is used by MANAGED_STACK_ADDRESS_BOEHM_GC_call_with_gc_active(), MANAGED_STACK_ADDRESS_BOEHM_GC_push_all_stack_sections(). */
struct MANAGED_STACK_ADDRESS_BOEHM_GC_traced_stack_sect_s {
  ptr_t saved_stack_ptr;
# ifdef IA64
    ptr_t saved_backing_store_ptr;
    ptr_t backing_store_end;
# endif
  struct MANAGED_STACK_ADDRESS_BOEHM_GC_traced_stack_sect_s *prev;
};

#ifdef THREADS
  /* Process all "traced stack sections" - scan entire stack except for */
  /* frames belonging to the user functions invoked by MANAGED_STACK_ADDRESS_BOEHM_GC_do_blocking.  */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_push_all_stack_sections(ptr_t lo, ptr_t hi,
                        struct MANAGED_STACK_ADDRESS_BOEHM_GC_traced_stack_sect_s *traced_stack_sect);
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN word MANAGED_STACK_ADDRESS_BOEHM_GC_total_stacksize; /* updated on every push_all_stacks */
#else
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_sp;
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN struct MANAGED_STACK_ADDRESS_BOEHM_GC_traced_stack_sect_s *MANAGED_STACK_ADDRESS_BOEHM_GC_traced_stack_sect;
                        /* Points to the "frame" data held in stack by  */
                        /* the innermost MANAGED_STACK_ADDRESS_BOEHM_GC_call_with_gc_active().      */
                        /* NULL if no such "frame" active.              */
#endif /* !THREADS */

#ifdef IA64
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_register_stackbottom;

  /* Similar to MANAGED_STACK_ADDRESS_BOEHM_GC_push_all_stack_sections() but for IA-64 registers store. */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_push_all_register_sections(ptr_t bs_lo, ptr_t bs_hi,
                  int eager, struct MANAGED_STACK_ADDRESS_BOEHM_GC_traced_stack_sect_s *traced_stack_sect);
#endif /* IA64 */

/*  Marks are in a reserved area in                          */
/*  each heap block.  Each word has one mark bit associated  */
/*  with it. Only those corresponding to the beginning of an */
/*  object are used.                                         */

/* Mark bit operations */

/*
 * Retrieve, set, clear the nth mark bit in a given heap block.
 *
 * (Recall that bit n corresponds to nth object or allocation granule
 * relative to the beginning of the block, including unused words)
 */

#ifdef USE_MARK_BYTES
# define mark_bit_from_hdr(hhdr,n) ((hhdr)->hb_marks[n])
# define set_mark_bit_from_hdr(hhdr,n) ((hhdr)->hb_marks[n] = 1)
# define clear_mark_bit_from_hdr(hhdr,n) ((hhdr)->hb_marks[n] = 0)
#else
/* Set mark bit correctly, even if mark bits may be concurrently        */
/* accessed.                                                            */
# if defined(PARALLEL_MARK) || (defined(THREAD_SANITIZER) && defined(THREADS))
    /* Workaround TSan false positive: there is no race between         */
    /* mark_bit_from_hdr and set_mark_bit_from_hdr when n is different  */
    /* (alternatively, USE_MARK_BYTES could be used).  If TSan is off,  */
    /* AO_or() is used only if we set USE_MARK_BITS explicitly.         */
#   define OR_WORD(addr, bits) AO_or((volatile AO_t *)(addr), (AO_t)(bits))
# else
#   define OR_WORD(addr, bits) (void)(*(addr) |= (bits))
# endif
# define mark_bit_from_hdr(hhdr,n) \
              (((hhdr)->hb_marks[divWORDSZ(n)] >> modWORDSZ(n)) & (word)1)
# define set_mark_bit_from_hdr(hhdr,n) \
              OR_WORD((hhdr)->hb_marks+divWORDSZ(n), (word)1 << modWORDSZ(n))
# define clear_mark_bit_from_hdr(hhdr,n) \
              ((hhdr)->hb_marks[divWORDSZ(n)] &= ~((word)1 << modWORDSZ(n)))
#endif /* !USE_MARK_BYTES */

#ifdef MARK_BIT_PER_OBJ
# define MARK_BIT_NO(offset, sz) (((word)(offset))/(sz))
        /* Get the mark bit index corresponding to the given byte       */
        /* offset and size (in bytes).                                  */
# define MARK_BIT_OFFSET(sz) 1
        /* Spacing between useful mark bits.                            */
# define IF_PER_OBJ(x) x
# define FINAL_MARK_BIT(sz) ((sz) > MAXOBJBYTES? 1 : HBLK_OBJS(sz))
        /* Position of final, always set, mark bit.                     */
#else
# define MARK_BIT_NO(offset, sz) BYTES_TO_GRANULES((word)(offset))
# define MARK_BIT_OFFSET(sz) BYTES_TO_GRANULES(sz)
# define IF_PER_OBJ(x)
# define FINAL_MARK_BIT(sz) \
                ((sz) > MAXOBJBYTES ? MARK_BITS_PER_HBLK \
                                : BYTES_TO_GRANULES((sz) * HBLK_OBJS(sz)))
#endif /* !MARK_BIT_PER_OBJ */

/* Important internal collector routines */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_approx_sp(void);

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_should_collect(void);

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER struct hblk * MANAGED_STACK_ADDRESS_BOEHM_GC_next_block(struct hblk *h, MANAGED_STACK_ADDRESS_BOEHM_GC_bool allow_free);
                        /* Get the next block whose address is at least */
                        /* h.  Returned block is managed by GC.  The    */
                        /* block must be in use unless allow_free is    */
                        /* true.  Return 0 if there is no such block.   */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER struct hblk * MANAGED_STACK_ADDRESS_BOEHM_GC_prev_block(struct hblk * h);
                        /* Get the last (highest address) block whose   */
                        /* address is at most h.  Returned block is     */
                        /* managed by GC, but may or may not be in use. */
                        /* Return 0 if there is no such block.          */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_mark_init(void);
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_clear_marks(void);
                        /* Clear mark bits for all heap objects.        */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_invalidate_mark_state(void);
                                /* Tell the marker that marked          */
                                /* objects may point to unmarked        */
                                /* ones, and roots may point to         */
                                /* unmarked objects.  Reset mark stack. */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_mark_some(ptr_t cold_gc_frame);
                        /* Perform about one pages worth of marking     */
                        /* work of whatever kind is needed.  Returns    */
                        /* quickly if no collection is in progress.     */
                        /* Return TRUE if mark phase finished.          */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_initiate_gc(void);
                                /* initiate collection.                 */
                                /* If the mark state is invalid, this   */
                                /* becomes full collection.  Otherwise  */
                                /* it's partial.                        */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_collection_in_progress(void);
                        /* Collection is in progress, or was abandoned. */

/* Push contents of the symbol residing in the static roots area        */
/* excluded from scanning by the the collector for a reason.            */
/* Note: it should be used only for symbols of relatively small size    */
/* (one or several words).                                              */
#define MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ALL_SYM(sym) MANAGED_STACK_ADDRESS_BOEHM_GC_push_all_eager(&(sym), &(sym) + 1)

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_push_all_stack(ptr_t b, ptr_t t);
                                    /* As MANAGED_STACK_ADDRESS_BOEHM_GC_push_all but consider      */
                                    /* interior pointers as valid.      */

#ifdef NO_VDB_FOR_STATIC_ROOTS
# define MANAGED_STACK_ADDRESS_BOEHM_GC_push_conditional_static(b, t, all) \
                ((void)(all), MANAGED_STACK_ADDRESS_BOEHM_GC_push_all(b, t))
#else
  /* Same as MANAGED_STACK_ADDRESS_BOEHM_GC_push_conditional (does either of MANAGED_STACK_ADDRESS_BOEHM_GC_push_all or         */
  /* MANAGED_STACK_ADDRESS_BOEHM_GC_push_selected depending on the third argument) but the caller   */
  /* guarantees the region belongs to the registered static roots.      */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_push_conditional_static(void *b, void *t, MANAGED_STACK_ADDRESS_BOEHM_GC_bool all);
#endif

#if defined(WRAP_MARK_SOME) && defined(PARALLEL_MARK)
  /* MANAGED_STACK_ADDRESS_BOEHM_GC_mark_local does not handle memory protection faults yet.  So,   */
  /* the static data regions are scanned immediately by MANAGED_STACK_ADDRESS_BOEHM_GC_push_roots.  */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_push_conditional_eager(void *bottom, void *top,
                                          MANAGED_STACK_ADDRESS_BOEHM_GC_bool all);
#endif

  /* In the threads case, we push part of the current thread stack      */
  /* with MANAGED_STACK_ADDRESS_BOEHM_GC_push_all_eager when we push the registers.  This gets the  */
  /* callee-save registers that may disappear.  The remainder of the    */
  /* stacks are scheduled for scanning in *MANAGED_STACK_ADDRESS_BOEHM_GC_push_other_roots, which   */
  /* is thread-package-specific.                                        */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_push_roots(MANAGED_STACK_ADDRESS_BOEHM_GC_bool all, ptr_t cold_gc_frame);
                                        /* Push all or dirty roots.     */

MANAGED_STACK_ADDRESS_BOEHM_GC_API_PRIV MANAGED_STACK_ADDRESS_BOEHM_GC_push_other_roots_proc MANAGED_STACK_ADDRESS_BOEHM_GC_push_other_roots;
                        /* Push system or application specific roots    */
                        /* onto the mark stack.  In some environments   */
                        /* (e.g. threads environments) this is          */
                        /* predefined to be non-zero.  A client         */
                        /* supplied replacement should also call the    */
                        /* original function.  Remains externally       */
                        /* visible as used by some well-known 3rd-party */
                        /* software (e.g., ECL) currently.              */

#ifdef THREADS
  void MANAGED_STACK_ADDRESS_BOEHM_GC_push_thread_structures(void);
#endif
MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN void (*MANAGED_STACK_ADDRESS_BOEHM_GC_push_typed_structures)(void);
                        /* A pointer such that we can avoid linking in  */
                        /* the typed allocation support if unused.      */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_with_callee_saves_pushed(void (*fn)(ptr_t, void *),
                                          volatile ptr_t arg);

#if defined(E2K) || defined(IA64) || defined(SPARC)
  /* Cause all stacked registers to be saved in memory.  Return a       */
  /* pointer to the top of the corresponding memory stack.              */
  ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_save_regs_in_stack(void);
#endif

#ifdef E2K
  /* Copy the full procedure stack to the provided buffer (with the     */
  /* given capacity).  Returns either the required buffer size if it    */
  /* is bigger than the provided buffer capacity, otherwise the amount  */
  /* of copied bytes.  May be called from a signal handler.             */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER size_t MANAGED_STACK_ADDRESS_BOEHM_GC_get_procedure_stack(ptr_t, size_t);

# if defined(CPPCHECK)
#   define PS_ALLOCA_BUF(sz) __builtin_alloca(sz)
# else
#   define PS_ALLOCA_BUF(sz) alloca(sz)
# endif

  /* Copy procedure (register) stack to a stack-allocated buffer.       */
  /* Usable from a signal handler.  The buffer is valid only within     */
  /* the current function.                                              */
# define GET_PROCEDURE_STACK_LOCAL(pbuf, psz)                       \
        do {                                                        \
          size_t capacity = 0;                                      \
                                                                    \
          for (*(pbuf) = NULL; ; capacity = *(psz)) {               \
            *(psz) = MANAGED_STACK_ADDRESS_BOEHM_GC_get_procedure_stack(*(pbuf), capacity);     \
            if (*(psz) <= capacity) break;                          \
            /* Allocate buffer on the stack; cannot return NULL. */ \
            *(pbuf) = PS_ALLOCA_BUF(*(psz));                        \
          }                                                         \
        } while (0)
#endif /* E2K */

#if defined(E2K) && defined(USE_PTR_HWTAG)
  /* Load value and get tag of the target memory.   */
# if defined(__ptr64__)
#   define LOAD_TAGGED_VALUE(v, tag, p)         \
        do {                                    \
          word val;                             \
          __asm__ __volatile__ (                \
            "ldd, sm %[adr], 0x0, %[val]\n\t"   \
            "gettagd %[val], %[tag]\n"          \
            : [val] "=r" (val),                 \
              [tag] "=r" (tag)                  \
            : [adr] "r" (p));                   \
          v = val;                              \
        } while (0)
# elif !defined(CPPCHECK)
#   error Unsupported -march for e2k target
# endif

# define LOAD_WORD_OR_CONTINUE(v, p) \
        { \
          int tag LOCAL_VAR_INIT_OK; \
          LOAD_TAGGED_VALUE(v, tag, p); \
          if (tag != 0) continue; \
        }
#else
# define LOAD_WORD_OR_CONTINUE(v, p) (void)(v = *(word *)(p))
#endif /* !E2K */

#if defined(AMIGA) || defined(MACOS) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS)
  void MANAGED_STACK_ADDRESS_BOEHM_GC_push_one(word p);
                              /* If p points to an object, mark it    */
                              /* and push contents on the mark stack  */
                              /* Pointer recognition test always      */
                              /* accepts interior pointers, i.e. this */
                              /* is appropriate for pointers found on */
                              /* stack.                               */
#endif

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS
  /* Same as MANAGED_STACK_ADDRESS_BOEHM_GC_push_one but for a sequence of registers.       */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_push_many_regs(const word *regs, unsigned count);
#endif

#if defined(PRINT_BLACK_LIST) || defined(KEEP_BACK_PTRS)
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_mark_and_push_stack(ptr_t p, ptr_t source);
                                /* Ditto, omits plausibility test       */
#else
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_mark_and_push_stack(ptr_t p);
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_clear_hdr_marks(hdr * hhdr);
                                    /* Clear the mark bits in a header */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_set_hdr_marks(hdr * hhdr);
                                    /* Set the mark bits in a header */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_set_fl_marks(ptr_t p);
                                    /* Set all mark bits associated with */
                                    /* a free list.                      */
#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) && defined(THREAD_LOCAL_ALLOC)
  void MANAGED_STACK_ADDRESS_BOEHM_GC_check_fl_marks(void **);
                                    /* Check that all mark bits         */
                                    /* associated with a free list are  */
                                    /* set.  Abort if not.              */
#endif

#ifndef AMIGA
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER
#endif
void MANAGED_STACK_ADDRESS_BOEHM_GC_add_roots_inner(ptr_t b, ptr_t e, MANAGED_STACK_ADDRESS_BOEHM_GC_bool tmp);

#ifdef USE_PROC_FOR_LIBRARIES
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_remove_roots_subregion(ptr_t b, ptr_t e);
#endif
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_exclude_static_roots_inner(void *start, void *finish);
#if defined(DYNAMIC_LOADING) || defined(ANY_MSWIN) || defined(PCR)
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_register_dynamic_libraries(void);
                /* Add dynamic library data sections to the root set. */
#endif
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_cond_register_dynamic_libraries(void);
                /* Remove and reregister dynamic libraries if we're     */
                /* configured to do that at each GC.                    */

/* Machine dependent startup routines */
ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_get_main_stack_base(void);     /* Cold end of stack.           */
#ifdef IA64
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_get_register_stack_base(void);
                                        /* Cold end of register stack.  */
#endif

void MANAGED_STACK_ADDRESS_BOEHM_GC_register_data_segments(void);

#ifdef THREADS
  /* Both are invoked from MANAGED_STACK_ADDRESS_BOEHM_GC_init only.        */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_thr_init(void);
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_init_parallel(void);
# ifndef DONT_USE_ATEXIT
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_is_main_thread(void);
# endif
#else
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_is_static_root(void *p);
                /* Is the address p in one of the registered static     */
                /* root sections?                                       */
# ifdef TRACE_BUF
    void MANAGED_STACK_ADDRESS_BOEHM_GC_add_trace_entry(char *kind, word arg1, word arg2);
# endif
#endif /* !THREADS */

/* Black listing: */
#ifdef PRINT_BLACK_LIST
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_black_list_normal(word p, ptr_t source);
                        /* Register bits as a possible future false     */
                        /* reference from the heap or static data       */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ADD_TO_BLACK_LIST_NORMAL(bits, source) \
                if (MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers) { \
                  MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_black_list_stack((word)(bits), (source)); \
                } else \
                  MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_black_list_normal((word)(bits), (source))
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_black_list_stack(word p, ptr_t source);
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ADD_TO_BLACK_LIST_STACK(bits, source) \
            MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_black_list_stack((word)(bits), (source))
#else
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_black_list_normal(word p);
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ADD_TO_BLACK_LIST_NORMAL(bits, source) \
                if (MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers) { \
                  MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_black_list_stack((word)(bits)); \
                } else \
                  MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_black_list_normal((word)(bits))
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_black_list_stack(word p);
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ADD_TO_BLACK_LIST_STACK(bits, source) \
            MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_black_list_stack((word)(bits))
#endif /* PRINT_BLACK_LIST */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_promote_black_lists(void);
                        /* Declare an end to a black listing phase.     */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_unpromote_black_lists(void);
                        /* Approximately undo the effect of the above.  */
                        /* This actually loses some information, but    */
                        /* only in a reasonably safe way.               */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_alloc(size_t bytes);
                                /* GC internal memory allocation for    */
                                /* small objects.  Deallocation is not  */
                                /* possible.  May return NULL.          */

#ifdef GWW_VDB
  /* MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_recycle_no_gww() not used.      */
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_recycle_no_gww MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_recycle_inner
#endif
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_recycle_inner(void *ptr, size_t bytes);
                                /* Reuse the memory region by the heap. */

/* Heap block layout maps: */
#ifndef MARK_BIT_PER_OBJ
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_add_map_entry(size_t sz);
                                /* Add a heap block map for objects of  */
                                /* size sz to obj_map.                  */
                                /* Return FALSE on failure.             */
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_register_displacement_inner(size_t offset);
                                /* Version of MANAGED_STACK_ADDRESS_BOEHM_GC_register_displacement  */
                                /* that assumes lock is already held.   */

/*  hblk allocation: */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_new_hblk(size_t size_in_granules, int kind);
                                /* Allocate a new heap block, and build */
                                /* a free list in it.                   */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_build_fl(struct hblk *h, size_t words, MANAGED_STACK_ADDRESS_BOEHM_GC_bool clear,
                           ptr_t list);
                                /* Build a free list for objects of     */
                                /* size sz in block h.  Append list to  */
                                /* end of the free lists.  Possibly     */
                                /* clear objects on the list.  Normally */
                                /* called by MANAGED_STACK_ADDRESS_BOEHM_GC_new_hblk, but also      */
                                /* called explicitly without GC lock.   */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER struct hblk * MANAGED_STACK_ADDRESS_BOEHM_GC_allochblk(size_t size_in_bytes, int kind,
                                    unsigned flags, size_t align_m1);
                                /* Allocate (and return pointer to)     */
                                /* a heap block for objects of the      */
                                /* given size and alignment (in bytes), */
                                /* searching over the appropriate free  */
                                /* block lists; inform the marker       */
                                /* that the found block is valid for    */
                                /* objects of the indicated size.       */
                                /* The client is responsible for        */
                                /* clearing the block, if necessary.    */
                                /* Note: we set obj_map field in the    */
                                /* header correctly; the caller is      */
                                /* responsible for building an object   */
                                /* freelist in the block.               */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_alloc_large(size_t lb, int k, unsigned flags,
                              size_t align_m1);
                        /* Allocate a large block of size lb bytes with */
                        /* the requested alignment (align_m1 plus one). */
                        /* The block is not cleared.  Assumes that      */
                        /* EXTRA_BYTES value is already added to lb.    */
                        /* The flags argument should be IGNORE_OFF_PAGE */
                        /* or 0.  Calls MANAGED_STACK_ADDRESS_BOEHM_GC_allochblk() to do the actual */
                        /* allocation, but also triggers GC and/or heap */
                        /* expansion as appropriate.  Updates value of  */
                        /* MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd; does also other accounting. */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_freehblk(struct hblk * p);
                                /* Deallocate a heap block and mark it  */
                                /* as invalid.                          */

/*  Miscellaneous GC routines.  */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_expand_hp_inner(word n);
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_start_reclaim(MANAGED_STACK_ADDRESS_BOEHM_GC_bool abort_if_found);
                                /* Restore unmarked objects to free     */
                                /* lists, or (if abort_if_found is      */
                                /* TRUE) report them.                   */
                                /* Sweeping of small object pages is    */
                                /* largely deferred.                    */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_continue_reclaim(word sz, int kind);
                                /* Sweep pages of the given size and    */
                                /* kind, as long as possible, and       */
                                /* as long as the corresponding free    */
                                /* list is empty.  sz is in granules.   */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_reclaim_all(MANAGED_STACK_ADDRESS_BOEHM_GC_stop_func stop_func, MANAGED_STACK_ADDRESS_BOEHM_GC_bool ignore_old);
                                /* Reclaim all blocks.  Abort (in a     */
                                /* consistent state) if stop_func()     */
                                /* returns TRUE.                        */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_reclaim_generic(struct hblk * hbp, hdr *hhdr, size_t sz,
                                  MANAGED_STACK_ADDRESS_BOEHM_GC_bool init, ptr_t list, word *pcount);
                                /* Rebuild free list in hbp with        */
                                /* header hhdr, with objects of size sz */
                                /* bytes.  Add list to the end of the   */
                                /* free list.  Add the number of        */
                                /* reclaimed bytes to *pcount.          */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_block_empty(hdr * hhdr);
                                /* Block completely unmarked?   */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER int MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK MANAGED_STACK_ADDRESS_BOEHM_GC_never_stop_func(void);
                                /* Always returns 0 (FALSE).            */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_try_to_collect_inner(MANAGED_STACK_ADDRESS_BOEHM_GC_stop_func stop_func);
                                /* Collect; caller must have acquired   */
                                /* lock.  Collection is aborted if      */
                                /* stop_func() returns TRUE.  Returns   */
                                /* TRUE if it completes successfully.   */
#define MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect_inner() \
                (void)MANAGED_STACK_ADDRESS_BOEHM_GC_try_to_collect_inner(MANAGED_STACK_ADDRESS_BOEHM_GC_never_stop_func)

#ifdef THREADS
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_in_thread_creation;
        /* We may currently be in thread creation or destruction.       */
        /* Only set to TRUE while allocation lock is held.              */
        /* When set, it is OK to run GC from unknown thread.            */
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized; /* MANAGED_STACK_ADDRESS_BOEHM_GC_init() has been run. */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_collect_a_little_inner(int n);
                                /* Do n units worth of garbage          */
                                /* collection work, if appropriate.     */
                                /* A unit is an amount appropriate for  */
                                /* HBLKSIZE bytes of allocation.        */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void * MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_aligned(size_t lb, int k, unsigned flags,
                                          size_t align_m1);

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void * MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_inner(size_t lb, int k, unsigned flags);
                                /* Allocate an object of the given      */
                                /* kind but assuming lock already held. */
                                /* Should not be used to directly       */
                                /* allocate objects requiring special   */
                                /* handling on allocation.  The flags   */
                                /* argument should be IGNORE_OFF_PAGE   */
                                /* or 0.  In the first case the client  */
                                /* guarantees that there will always be */
                                /* a pointer to the beginning (i.e.     */
                                /* within the first hblk) of the object */
                                /* while it is live.                    */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_collect_or_expand(word needed_blocks, unsigned flags,
                                      MANAGED_STACK_ADDRESS_BOEHM_GC_bool retry);

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_allocobj(size_t gran, int kind);
                                /* Make the indicated free list     */
                                /* nonempty, and return its head.   */
                                /* The size (gran) is in granules.  */

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ADD_CALLER
  /* MANAGED_STACK_ADDRESS_BOEHM_GC_DBG_EXTRAS is used by GC debug API functions (unlike MANAGED_STACK_ADDRESS_BOEHM_GC_EXTRAS  */
  /* used by GC debug API macros) thus MANAGED_STACK_ADDRESS_BOEHM_GC_RETURN_ADDR_PARENT (pointing  */
  /* to client caller) should be used if possible.                      */
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_HAVE_RETURN_ADDR_PARENT
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_DBG_EXTRAS MANAGED_STACK_ADDRESS_BOEHM_GC_RETURN_ADDR_PARENT, NULL, 0
# else
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_DBG_EXTRAS MANAGED_STACK_ADDRESS_BOEHM_GC_RETURN_ADDR, NULL, 0
# endif
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_DBG_EXTRAS "unknown", 0
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_ADD_CALLER */

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_COLLECT_AT_MALLOC
  extern size_t MANAGED_STACK_ADDRESS_BOEHM_GC_dbg_collect_at_malloc_min_lb;
                            /* variable visible outside for debugging   */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_DBG_COLLECT_AT_MALLOC(lb) \
                (void)((lb) >= MANAGED_STACK_ADDRESS_BOEHM_GC_dbg_collect_at_malloc_min_lb ? \
                            (MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect(), 0) : 0)
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_DBG_COLLECT_AT_MALLOC(lb) (void)0
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_COLLECT_AT_MALLOC */

/* Allocation routines that bypass the thread local cache.      */
#if defined(THREAD_LOCAL_ALLOC) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT)
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void *MANAGED_STACK_ADDRESS_BOEHM_GC_core_gcj_malloc(size_t lb, void *, unsigned flags);
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_init_headers(void);
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER struct hblkhdr * MANAGED_STACK_ADDRESS_BOEHM_GC_install_header(struct hblk *h);
                                /* Install a header for block h.        */
                                /* Return 0 on failure, or the header   */
                                /* otherwise.                           */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_install_counts(struct hblk * h, size_t sz);
                                /* Set up forwarding counts for block   */
                                /* h of size sz.                        */
                                /* Return FALSE on failure.             */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_remove_header(struct hblk * h);
                                /* Remove the header for block h.       */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_remove_counts(struct hblk * h, size_t sz);
                                /* Remove forwarding counts for h.      */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER hdr * MANAGED_STACK_ADDRESS_BOEHM_GC_find_header(ptr_t h);

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_os_get_mem(size_t bytes);
                        /* Get HBLKSIZE-aligned heap memory chunk from  */
                        /* the OS and add the chunk to MANAGED_STACK_ADDRESS_BOEHM_GC_our_memory.   */
                        /* Return NULL if out of memory.                */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_print_all_errors(void);
                        /* Print smashed and leaked objects, if any.    */
                        /* Clear the lists of such objects.             */

MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN void (*MANAGED_STACK_ADDRESS_BOEHM_GC_check_heap)(void);
                        /* Check that all objects in the heap with      */
                        /* debugging info are intact.                   */
                        /* Add any that are not to MANAGED_STACK_ADDRESS_BOEHM_GC_smashed list.     */
MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN void (*MANAGED_STACK_ADDRESS_BOEHM_GC_print_all_smashed)(void);
                        /* Print MANAGED_STACK_ADDRESS_BOEHM_GC_smashed if it's not empty.          */
                        /* Clear MANAGED_STACK_ADDRESS_BOEHM_GC_smashed list.                       */
MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN void (*MANAGED_STACK_ADDRESS_BOEHM_GC_print_heap_obj)(ptr_t p);
                        /* If possible print (using MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf)      */
                        /* a more detailed description (terminated with */
                        /* "\n") of the object referred to by p.        */

#if defined(LINUX) && defined(__ELF__) && !defined(SMALL_CONFIG)
  void MANAGED_STACK_ADDRESS_BOEHM_GC_print_address_map(void);
                        /* Print an address map of the process.         */
#endif

#ifndef SHORT_DBG_HDRS
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_findleak_delay_free;
                        /* Do not immediately deallocate object on      */
                        /* free() in the leak-finding mode, just mark   */
                        /* it as freed (and deallocate it after GC).    */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_check_leaked(ptr_t base); /* from dbg_mlc.c */
#endif

#ifdef AO_HAVE_store
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN volatile AO_t MANAGED_STACK_ADDRESS_BOEHM_GC_have_errors;
# define MANAGED_STACK_ADDRESS_BOEHM_GC_SET_HAVE_ERRORS() AO_store(&MANAGED_STACK_ADDRESS_BOEHM_GC_have_errors, (AO_t)TRUE)
# define get_have_errors() ((MANAGED_STACK_ADDRESS_BOEHM_GC_bool)AO_load(&MANAGED_STACK_ADDRESS_BOEHM_GC_have_errors))
                                /* The barriers are not needed.         */
#else
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_have_errors;
# define MANAGED_STACK_ADDRESS_BOEHM_GC_SET_HAVE_ERRORS() (void)(MANAGED_STACK_ADDRESS_BOEHM_GC_have_errors = TRUE)
# define get_have_errors() MANAGED_STACK_ADDRESS_BOEHM_GC_have_errors
#endif                          /* We saw a smashed or leaked object.   */
                                /* Call error printing routine          */
                                /* occasionally.  It is OK to read it   */
                                /* without acquiring the lock.          */
                                /* If set to true, it is never cleared. */

#define VERBOSE 2
#if !defined(NO_CLOCK) || !defined(SMALL_CONFIG)
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN int MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats;
                        /* Value 1 generates basic GC log;              */
                        /* VERBOSE generates additional messages.       */
#else /* SMALL_CONFIG */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats 0
  /* Will this remove the message character strings from the executable? */
  /* With a particular level of optimizations, it should...              */
#endif

#ifdef KEEP_BACK_PTRS
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN long MANAGED_STACK_ADDRESS_BOEHM_GC_backtraces;
#endif

/* A trivial (linear congruential) pseudo-random numbers generator, */
/* safe for the concurrent usage.                                   */
#define MANAGED_STACK_ADDRESS_BOEHM_GC_RAND_MAX ((int)(~0U >> 1))
#if defined(AO_HAVE_store) && defined(THREAD_SANITIZER)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_RAND_STATE_T volatile AO_t
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_RAND_NEXT(pseed) MANAGED_STACK_ADDRESS_BOEHM_GC_rand_next(pseed)
    MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE int MANAGED_STACK_ADDRESS_BOEHM_GC_rand_next(MANAGED_STACK_ADDRESS_BOEHM_GC_RAND_STATE_T *pseed)
    {
      AO_t next = (AO_t)((AO_load(pseed) * (unsigned32)1103515245UL + 12345)
                         & (unsigned32)((unsigned)MANAGED_STACK_ADDRESS_BOEHM_GC_RAND_MAX));
      AO_store(pseed, next);
      return (int)next;
    }
#else
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_RAND_STATE_T unsigned32
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_RAND_NEXT(pseed) /* overflow and race are OK */ \
        (int)(*(pseed) = (*(pseed) * (unsigned32)1103515245UL + 12345) \
                         & (unsigned32)((unsigned)MANAGED_STACK_ADDRESS_BOEHM_GC_RAND_MAX))
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_print_back_height;

#ifdef MAKE_BACK_GRAPH
  void MANAGED_STACK_ADDRESS_BOEHM_GC_print_back_graph_stats(void);
#endif

#ifdef THREADS
  /* Explicitly deallocate the object when we already hold lock.        */
  /* Only used for internally allocated objects.                        */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_free_inner(void * p);
#endif

/* Macros used for collector internal allocation.       */
/* These assume the collector lock is held.             */
#ifdef DBG_HDRS_ALL
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void * MANAGED_STACK_ADDRESS_BOEHM_GC_debug_generic_malloc_inner(size_t lb, int k,
                                                unsigned flags);
# define MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_MALLOC(lb, k) MANAGED_STACK_ADDRESS_BOEHM_GC_debug_generic_malloc_inner(lb, k, 0)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_MALLOC_IGNORE_OFF_PAGE(lb, k) \
               MANAGED_STACK_ADDRESS_BOEHM_GC_debug_generic_malloc_inner(lb, k, IGNORE_OFF_PAGE)
# ifdef THREADS
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_debug_free_inner(void * p);
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_FREE MANAGED_STACK_ADDRESS_BOEHM_GC_debug_free_inner
# else
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_FREE MANAGED_STACK_ADDRESS_BOEHM_GC_debug_free
# endif
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_MALLOC(lb, k) MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_inner(lb, k, 0)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_MALLOC_IGNORE_OFF_PAGE(lb, k) \
               MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_inner(lb, k, IGNORE_OFF_PAGE)
# ifdef THREADS
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_FREE MANAGED_STACK_ADDRESS_BOEHM_GC_free_inner
# else
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_FREE MANAGED_STACK_ADDRESS_BOEHM_GC_free
# endif
#endif /* !DBG_HDRS_ALL */

#ifdef USE_MUNMAP
  /* Memory unmapping: */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_unmap_old(unsigned threshold);
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_merge_unmapped(void);
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_unmap(ptr_t start, size_t bytes);
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_remap(ptr_t start, size_t bytes);
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_unmap_gap(ptr_t start1, size_t bytes1, ptr_t start2,
                             size_t bytes2);

# ifndef NOT_GCBUILD
    /* Compute end address for an unmap operation on the indicated block. */
    MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_unmap_end(ptr_t start, size_t bytes)
    {
      return (ptr_t)((word)(start + bytes) & ~(word)(MANAGED_STACK_ADDRESS_BOEHM_GC_page_size-1));
    }
# endif
#endif /* USE_MUNMAP */

#ifdef CAN_HANDLE_FORK
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN int MANAGED_STACK_ADDRESS_BOEHM_GC_handle_fork;
                /* Fork-handling mode:                                  */
                /* 0 means no fork handling requested (but client could */
                /* anyway call fork() provided it is surrounded with    */
                /* MANAGED_STACK_ADDRESS_BOEHM_GC_atfork_prepare/parent/child calls);               */
                /* -1 means GC tries to use pthread_at_fork if it is    */
                /* available (if it succeeds then MANAGED_STACK_ADDRESS_BOEHM_GC_handle_fork value  */
                /* is changed to 1), client should nonetheless surround */
                /* fork() with MANAGED_STACK_ADDRESS_BOEHM_GC_atfork_prepare/parent/child (for the  */
                /* case of pthread_at_fork failure or absence);         */
                /* 1 (or other values) means client fully relies on     */
                /* pthread_at_fork (so if it is missing or failed then  */
                /* abort occurs in MANAGED_STACK_ADDRESS_BOEHM_GC_init), MANAGED_STACK_ADDRESS_BOEHM_GC_atfork_prepare and the  */
                /* accompanying routines are no-op in such a case.      */
#endif

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL
# define MANAGED_STACK_ADDRESS_BOEHM_GC_incremental FALSE
# define MANAGED_STACK_ADDRESS_BOEHM_GC_auto_incremental FALSE
# define MANAGED_STACK_ADDRESS_BOEHM_GC_manual_vdb FALSE
# define MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(p) (void)(p)
# define REACHABLE_AFTER_DIRTY(p) (void)(p)

#else /* !MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL */
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_incremental;
                        /* Using incremental/generational collection.   */
                        /* Assumes dirty bits are being maintained.     */

  /* Virtual dirty bit implementation:            */
  /* Each implementation exports the following:   */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_read_dirty(MANAGED_STACK_ADDRESS_BOEHM_GC_bool output_unneeded);
                        /* Retrieve dirty bits.  Set output_unneeded to */
                        /* indicate that reading of the retrieved dirty */
                        /* bits is not planned till the next retrieval. */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_page_was_dirty(struct hblk *h);
                        /* Read retrieved dirty bits.   */

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_remove_protection(struct hblk *h, word nblocks,
                                   MANAGED_STACK_ADDRESS_BOEHM_GC_bool pointerfree);
                /* h is about to be written or allocated.  Ensure that  */
                /* it is not write protected by the virtual dirty bit   */
                /* implementation.  I.e., this is a call that:          */
                /* - hints that [h, h+nblocks) is about to be written;  */
                /* - guarantees that protection is removed;             */
                /* - may speed up some dirty bit implementations;       */
                /* - may be essential if we need to ensure that         */
                /* pointer-free system call buffers in the heap are     */
                /* not protected.                                       */

# if !defined(NO_VDB_FOR_STATIC_ROOTS) && !defined(PROC_VDB)
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_is_vdb_for_static_roots(void);
                /* Is VDB working for static roots?                     */
# endif

# ifdef CAN_HANDLE_FORK
#   if defined(PROC_VDB) || defined(SOFT_VDB)
      MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_dirty_update_child(void);
                /* Update pid-specific resources (like /proc file       */
                /* descriptors) needed by the dirty bits implementation */
                /* after fork in the child process.                     */
#   else
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_dirty_update_child() (void)0
#   endif
# endif /* CAN_HANDLE_FORK */

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_dirty_init(void);
                /* Returns true if dirty bits are maintained (otherwise */
                /* it is OK to be called again if the client invokes    */
                /* MANAGED_STACK_ADDRESS_BOEHM_GC_enable_incremental once more).                    */

  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_manual_vdb;
                /* The incremental collection is in the manual VDB      */
                /* mode.  Assumes MANAGED_STACK_ADDRESS_BOEHM_GC_incremental is true.  Should not   */
                /* be modified once MANAGED_STACK_ADDRESS_BOEHM_GC_incremental is set to true.      */

# define MANAGED_STACK_ADDRESS_BOEHM_GC_auto_incremental (MANAGED_STACK_ADDRESS_BOEHM_GC_incremental && !MANAGED_STACK_ADDRESS_BOEHM_GC_manual_vdb)

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_dirty_inner(const void *p); /* does not require locking */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(p) (MANAGED_STACK_ADDRESS_BOEHM_GC_manual_vdb ? MANAGED_STACK_ADDRESS_BOEHM_GC_dirty_inner(p) : (void)0)
# define REACHABLE_AFTER_DIRTY(p) MANAGED_STACK_ADDRESS_BOEHM_GC_reachable_here(p)
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL */

/* Same as MANAGED_STACK_ADDRESS_BOEHM_GC_base but excepts and returns a pointer to const object.   */
#define MANAGED_STACK_ADDRESS_BOEHM_GC_base_C(p) ((const void *)MANAGED_STACK_ADDRESS_BOEHM_GC_base((/* no const */ void *)(word)(p)))

/* Debugging print routines: */
void MANAGED_STACK_ADDRESS_BOEHM_GC_print_block_list(void);
void MANAGED_STACK_ADDRESS_BOEHM_GC_print_hblkfreelist(void);
void MANAGED_STACK_ADDRESS_BOEHM_GC_print_heap_sects(void);
void MANAGED_STACK_ADDRESS_BOEHM_GC_print_static_roots(void);

#ifdef KEEP_BACK_PTRS
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_store_back_pointer(ptr_t source, ptr_t dest);
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_marked_for_finalization(ptr_t dest);
# define MANAGED_STACK_ADDRESS_BOEHM_GC_STORE_BACK_PTR(source, dest) MANAGED_STACK_ADDRESS_BOEHM_GC_store_back_pointer(source, dest)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_MARKED_FOR_FINALIZATION(dest) MANAGED_STACK_ADDRESS_BOEHM_GC_marked_for_finalization(dest)
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_STORE_BACK_PTR(source, dest) (void)(source)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_MARKED_FOR_FINALIZATION(dest)
#endif /* !KEEP_BACK_PTRS */

/* Make arguments appear live to compiler */
void MANAGED_STACK_ADDRESS_BOEHM_GC_noop6(word, word, word, word, word, word);

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_FORMAT_PRINTF
# if MANAGED_STACK_ADDRESS_BOEHM_GC_GNUC_PREREQ(3, 0)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_FORMAT_PRINTF(spec_argnum, first_checked) \
        __attribute__((__format__(__printf__, spec_argnum, first_checked)))
# else
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_FORMAT_PRINTF(spec_argnum, first_checked)
# endif
#endif

/* Logging and diagnostic output:       */
/* MANAGED_STACK_ADDRESS_BOEHM_GC_printf is used typically on client explicit print requests.       */
/* For all MANAGED_STACK_ADDRESS_BOEHM_GC_X_printf routines, it is recommended to put "\n" at       */
/* 'format' string end (for output atomicity).                          */
MANAGED_STACK_ADDRESS_BOEHM_GC_API_PRIV void MANAGED_STACK_ADDRESS_BOEHM_GC_printf(const char * format, ...)
                        MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_FORMAT_PRINTF(1, 2);
                        /* A version of printf that doesn't allocate,   */
                        /* 1 KB total output length.                    */
                        /* (We use sprintf.  Hopefully that doesn't     */
                        /* allocate for long arguments.)                */
MANAGED_STACK_ADDRESS_BOEHM_GC_API_PRIV void MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf(const char * format, ...)
                        MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_FORMAT_PRINTF(1, 2);

/* Basic logging routine.  Typically, MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf is called directly  */
/* only inside various DEBUG_x blocks.                                  */
MANAGED_STACK_ADDRESS_BOEHM_GC_API_PRIV void MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf(const char * format, ...)
                        MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_FORMAT_PRINTF(1, 2);

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_ANDROID_LOG
# define MANAGED_STACK_ADDRESS_BOEHM_GC_PRINT_STATS_FLAG (MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats != 0)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_INFOLOG_PRINTF MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF
  /* MANAGED_STACK_ADDRESS_BOEHM_GC_verbose_log_printf is called only if MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats is VERBOSE. */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_verbose_log_printf MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf
#else
  extern MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_quiet;
# define MANAGED_STACK_ADDRESS_BOEHM_GC_PRINT_STATS_FLAG (!MANAGED_STACK_ADDRESS_BOEHM_GC_quiet)
  /* INFO/DBG loggers are enabled even if MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats is off. */
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_INFOLOG_PRINTF
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_INFOLOG_PRINTF if (MANAGED_STACK_ADDRESS_BOEHM_GC_quiet) {} else MANAGED_STACK_ADDRESS_BOEHM_GC_info_log_printf
# endif
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_info_log_printf(const char *format, ...)
                        MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_FORMAT_PRINTF(1, 2);
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_verbose_log_printf(const char *format, ...)
                        MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_FORMAT_PRINTF(1, 2);
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_ANDROID_LOG */

#if defined(SMALL_CONFIG) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ANDROID_LOG)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ERRINFO_PRINTF MANAGED_STACK_ADDRESS_BOEHM_GC_INFOLOG_PRINTF
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ERRINFO_PRINTF MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf
#endif

/* Convenient macros for MANAGED_STACK_ADDRESS_BOEHM_GC_[verbose_]log_printf invocation.    */
#define MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF \
                if (EXPECT(!MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats, TRUE)) {} else MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf
#define MANAGED_STACK_ADDRESS_BOEHM_GC_VERBOSE_LOG_PRINTF \
    if (EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats != VERBOSE, TRUE)) {} else MANAGED_STACK_ADDRESS_BOEHM_GC_verbose_log_printf
#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DBGLOG_PRINTF
# define MANAGED_STACK_ADDRESS_BOEHM_GC_DBGLOG_PRINTF if (!MANAGED_STACK_ADDRESS_BOEHM_GC_PRINT_STATS_FLAG) {} else MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf
#endif

void MANAGED_STACK_ADDRESS_BOEHM_GC_err_puts(const char *s);
                        /* Write s to stderr, don't buffer, don't add   */
                        /* newlines, don't ...                          */

/* Handy macro for logging size values (of word type) in KiB (rounding  */
/* to nearest value).                                                   */
#define TO_KiB_UL(v) ((unsigned long)(((v) + ((1 << 9) - 1)) >> 10))

MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_fail_count;
                        /* How many consecutive GC/expansion failures?  */
                        /* Reset by MANAGED_STACK_ADDRESS_BOEHM_GC_allochblk(); defined in alloc.c. */

MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN long MANAGED_STACK_ADDRESS_BOEHM_GC_large_alloc_warn_interval; /* defined in misc.c */

MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN signed_word MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_found;
                /* Number of reclaimed bytes after garbage collection;  */
                /* protected by GC lock; defined in reclaim.c.          */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_GET_HEAP_USAGE_NOT_NEEDED
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN word MANAGED_STACK_ADDRESS_BOEHM_GC_reclaimed_bytes_before_gc;
                /* Number of bytes reclaimed before this        */
                /* collection cycle; used for statistics only.  */
#endif

#ifdef USE_MUNMAP
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_unmap_threshold; /* defined in alloc.c */
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_force_unmap_on_gcollect; /* defined in misc.c */
#endif

#ifdef MSWIN32
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_no_win32_dlls; /* defined in os_dep.c */
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_wnt;     /* Is Windows NT derivative;    */
                                /* defined and set in os_dep.c. */
#endif

#ifdef THREADS
# if (defined(MSWIN32) && !defined(CONSOLE_LOG)) || defined(MSWINCE)
    MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN CRITICAL_SECTION MANAGED_STACK_ADDRESS_BOEHM_GC_write_cs; /* defined in misc.c */
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS
      MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_write_disabled;
                                /* defined in win32_threads.c;  */
                                /* protected by MANAGED_STACK_ADDRESS_BOEHM_GC_write_cs.    */

#   endif
# endif /* MSWIN32 || MSWINCE */
# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL) || defined(HAVE_LOCKFREE_AO_OR)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_acquire_dirty_lock() (void)0
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_release_dirty_lock() (void)0
# else
    /* Acquire the spin lock we use to update dirty bits.       */
    /* Threads should not get stopped holding it.  But we may   */
    /* acquire and release it during MANAGED_STACK_ADDRESS_BOEHM_GC_remove_protection call. */
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_acquire_dirty_lock() \
        do { /* empty */ \
        } while (AO_test_and_set_acquire(&MANAGED_STACK_ADDRESS_BOEHM_GC_fault_handler_lock) == AO_TS_SET)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_release_dirty_lock() AO_CLEAR(&MANAGED_STACK_ADDRESS_BOEHM_GC_fault_handler_lock)
    MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN volatile AO_TS_t MANAGED_STACK_ADDRESS_BOEHM_GC_fault_handler_lock;
                                        /* defined in os_dep.c */
# endif
# ifdef MSWINCE
    MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_dont_query_stack_min;
                                /* Defined and set in os_dep.c. */
# endif
#elif defined(IA64)
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_save_regs_ret_val; /* defined in mach_dep.c. */
                        /* Previously set to backing store pointer.     */
#endif /* !THREADS */

#ifdef THREAD_LOCAL_ALLOC
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_world_stopped; /* defined in alloc.c */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_mark_thread_local_free_lists(void);
#endif

#if defined(GLIBC_2_19_TSX_BUG) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS_PARAMARK)
  /* Parse string like <major>[.<minor>[<tail>]] and return major value. */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER int MANAGED_STACK_ADDRESS_BOEHM_GC_parse_version(int *pminor, const char *pverstr);
#endif

#if defined(MPROTECT_VDB) && defined(GWW_VDB)
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_gww_dirty_init(void);
                        /* Returns TRUE if GetWriteWatch is available.  */
                        /* May be called repeatedly.  May be called     */
                        /* with or without the GC lock held.            */
#endif

#if defined(CHECKSUMS) || defined(PROC_VDB)
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_page_was_ever_dirty(struct hblk * h);
                        /* Could the page contain valid heap pointers?  */
#endif

#ifdef CHECKSUMS
# if defined(MPROTECT_VDB) && !defined(DARWIN)
    void MANAGED_STACK_ADDRESS_BOEHM_GC_record_fault(struct hblk * h);
# endif
  void MANAGED_STACK_ADDRESS_BOEHM_GC_check_dirty(void);
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_default_print_heap_obj_proc(ptr_t p);

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_setpagesize(void);

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_initialize_offsets(void);      /* defined in obj_map.c */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_bl_init(void);
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_bl_init_no_interiors(void);    /* defined in blacklst.c */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_start_debugging_inner(void);   /* defined in dbg_mlc.c. */
                        /* Should not be called if MANAGED_STACK_ADDRESS_BOEHM_GC_debugging_started. */

/* Store debugging info into p.  Return displaced pointer.      */
/* Assumes we hold the allocation lock.                         */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void *MANAGED_STACK_ADDRESS_BOEHM_GC_store_debug_info_inner(void *p, word sz, const char *str,
                                         int linenum);

#ifdef REDIRECT_MALLOC
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_LINUX_THREADS
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_text_mapping(char *nm, ptr_t *startp, ptr_t *endp);
                                                /* from os_dep.c */
# endif
#elif defined(USE_WINALLOC)
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_add_current_malloc_heap(void);
#endif /* !REDIRECT_MALLOC */

#ifdef MAKE_BACK_GRAPH
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_build_back_graph(void);
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_traverse_back_graph(void);
#endif

#ifdef MSWIN32
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_init_win32(void);
#endif

#ifndef ANY_MSWIN
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void * MANAGED_STACK_ADDRESS_BOEHM_GC_roots_present(ptr_t);
        /* The type is a lie, since the real type doesn't make sense here, */
        /* and we only test for NULL.                                      */
#endif

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_get_next_stack(char *start, char * limit, char **lo,
                                  char **hi);
# if defined(MPROTECT_VDB) && !defined(CYGWIN32)
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_set_write_fault_handler(void);
# endif
# if defined(WRAP_MARK_SOME) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS)
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_started_thread_while_stopped(void);
        /* Did we invalidate mark phase with an unexpected thread start? */
# endif
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS */

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS) && defined(MPROTECT_VDB)
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_mprotect_stop(void);
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_mprotect_resume(void);
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_THREADS_DISCOVERY
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_darwin_register_self_mach_handler(void);
# endif
#endif

#ifdef THREADS
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_reset_finalizer_nested(void);
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER unsigned char *MANAGED_STACK_ADDRESS_BOEHM_GC_check_finalizer_nested(void);
# endif
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_do_blocking_inner(ptr_t data, void * context);
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_push_all_stacks(void);
# ifdef USE_PROC_FOR_LIBRARIES
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_segment_is_thread_stack(ptr_t lo, ptr_t hi);
# endif
# if (defined(HAVE_PTHREAD_ATTR_GET_NP) || defined(HAVE_PTHREAD_GETATTR_NP)) \
     && defined(IA64)
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_stack_base_below(ptr_t bound);
# endif
#endif /* THREADS */

#ifdef DYNAMIC_LOADING
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_register_main_static_data(void);
# ifdef DARWIN
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_init_dyld(void);
# endif
#endif /* DYNAMIC_LOADING */

#ifdef SEARCH_FOR_DATA_START
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_init_linux_data_start(void);
  void * MANAGED_STACK_ADDRESS_BOEHM_GC_find_limit(void *, int);
#endif

#ifdef UNIX_LIKE
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_set_and_save_fault_handler(void (*handler)(int));
#endif

#ifdef NEED_PROC_MAPS
# if defined(DYNAMIC_LOADING) && defined(USE_PROC_FOR_LIBRARIES)
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER const char *MANAGED_STACK_ADDRESS_BOEHM_GC_parse_map_entry(const char *maps_ptr,
                                            ptr_t *start, ptr_t *end,
                                            const char **prot,
                                            unsigned *maj_dev,
                                            const char **mapping_name);
# endif
# if defined(IA64) || defined(INCLUDE_LINUX_THREAD_DESCR)
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_enclosing_mapping(ptr_t addr,
                                          ptr_t *startp, ptr_t *endp);
# endif
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER const char *MANAGED_STACK_ADDRESS_BOEHM_GC_get_maps(void);
#endif /* NEED_PROC_MAPS */

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER word MANAGED_STACK_ADDRESS_BOEHM_GC_compute_large_free_bytes(void);
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER word MANAGED_STACK_ADDRESS_BOEHM_GC_compute_root_size(void);
#endif

/* Check a compile time assertion at compile time.      */
#if defined(_MSC_VER) && (_MSC_VER >= 1700)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_STATIC_ASSERT(expr) \
                static_assert(expr, "static assertion failed: " #expr)
#elif defined(static_assert) && !defined(CPPCHECK) \
      && (__STDC_VERSION__ >= 201112L)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_STATIC_ASSERT(expr) static_assert(expr, #expr)
#elif defined(mips) && !defined(__GNUC__) && !defined(CPPCHECK)
/* DOB: MIPSPro C gets an internal error taking the sizeof an array type.
   This code works correctly (ugliness is to avoid "unused var" warnings) */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_STATIC_ASSERT(expr) \
    do { if (0) { char j[(expr)? 1 : -1]; j[0]='\0'; j[0]=j[0]; } } while(0)
#else
  /* The error message for failure is a bit baroque, but ...    */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_STATIC_ASSERT(expr) (void)sizeof(char[(expr)? 1 : -1])
#endif

/* Runtime check for an argument declared as non-null is actually not null. */
#if MANAGED_STACK_ADDRESS_BOEHM_GC_GNUC_PREREQ(4, 0)
  /* Workaround tautological-pointer-compare Clang warning.     */
# define NONNULL_ARG_NOT_NULL(arg) (*(volatile void **)(word)(&(arg)) != NULL)
#else
# define NONNULL_ARG_NOT_NULL(arg) (NULL != (arg))
#endif

#define COND_DUMP_CHECKS \
          do { \
            MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK()); \
            MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_compute_large_free_bytes() == MANAGED_STACK_ADDRESS_BOEHM_GC_large_free_bytes); \
            MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_compute_root_size() == MANAGED_STACK_ADDRESS_BOEHM_GC_root_size); \
          } while (0)

#ifndef NO_DEBUGGING
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_dump_regularly;
                                /* Generate regular debugging dumps.    */
# define COND_DUMP if (EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_dump_regularly, FALSE)) { \
                        MANAGED_STACK_ADDRESS_BOEHM_GC_dump_named(NULL); \
                   } else COND_DUMP_CHECKS
#else
# define COND_DUMP COND_DUMP_CHECKS
#endif

#if defined(PARALLEL_MARK)
  /* We need additional synchronization facilities from the thread      */
  /* support.  We believe these are less performance critical           */
  /* than the main garbage collector lock; standard pthreads-based      */
  /* implementations should be sufficient.                              */

# define MANAGED_STACK_ADDRESS_BOEHM_GC_markers_m1 MANAGED_STACK_ADDRESS_BOEHM_GC_parallel
                        /* Number of mark threads we would like to have */
                        /* excluding the initiating thread.             */

  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_parallel_mark_disabled;
                        /* A flag to temporarily avoid parallel marking.*/

  /* The mark lock and condition variable.  If the GC lock is also      */
  /* acquired, the GC lock must be acquired first.  The mark lock is    */
  /* used to both protect some variables used by the parallel           */
  /* marker, and to protect MANAGED_STACK_ADDRESS_BOEHM_GC_fl_builder_count, below.                 */
  /* MANAGED_STACK_ADDRESS_BOEHM_GC_notify_all_marker() is called when                              */
  /* the state of the parallel marker changes                           */
  /* in some significant way (see gc_mark.h for details).  The          */
  /* latter set of events includes incrementing MANAGED_STACK_ADDRESS_BOEHM_GC_mark_no.             */
  /* MANAGED_STACK_ADDRESS_BOEHM_GC_notify_all_builder() is called when MANAGED_STACK_ADDRESS_BOEHM_GC_fl_builder_count         */
  /* reaches 0.                                                         */

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_wait_for_markers_init(void);
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_acquire_mark_lock(void);
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_release_mark_lock(void);
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_notify_all_builder(void);
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_wait_for_reclaim(void);

  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN signed_word MANAGED_STACK_ADDRESS_BOEHM_GC_fl_builder_count; /* Protected by mark lock. */

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_notify_all_marker(void);
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_wait_marker(void);
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN word MANAGED_STACK_ADDRESS_BOEHM_GC_mark_no;            /* Protected by mark lock.      */

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_help_marker(word my_mark_no);
              /* Try to help out parallel marker for mark cycle         */
              /* my_mark_no.  Returns if the mark cycle finishes or     */
              /* was already done, or there was nothing to do for       */
              /* some other reason.                                     */

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_start_mark_threads_inner(void);
#endif /* PARALLEL_MARK */

#if defined(SIGNAL_BASED_STOP_WORLD) && !defined(SIG_SUSPEND)
  /* We define the thread suspension signal here, so that we can refer  */
  /* to it in the dirty bit implementation, if necessary.  Ideally we   */
  /* would allocate a (real-time?) signal using the standard mechanism. */
  /* unfortunately, there is no standard mechanism.  (There is one      */
  /* in Linux glibc, but it's not exported.)  Thus we continue to use   */
  /* the same hard-coded signals we've always used.                     */
# ifdef THREAD_SANITIZER
    /* Unfortunately, use of an asynchronous signal to suspend threads  */
    /* leads to the situation when the signal is not delivered (is      */
    /* stored to pending_signals in TSan runtime actually) while the    */
    /* destination thread is blocked in pthread_mutex_lock.  Thus, we   */
    /* use some synchronous one instead (which is again unlikely to be  */
    /* used by clients directly).                                       */
#   define SIG_SUSPEND SIGSYS
# elif (defined(MANAGED_STACK_ADDRESS_BOEHM_GC_LINUX_THREADS) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DGUX386_THREADS)) \
       && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_USESIGRT_SIGNALS)
#   if defined(SPARC) && !defined(SIGPWR)
      /* Linux/SPARC doesn't properly define SIGPWR in <signal.h>.      */
      /* It is aliased to SIGLOST in asm/signal.h, though.              */
#     define SIG_SUSPEND SIGLOST
#   else
      /* Linuxthreads itself uses SIGUSR1 and SIGUSR2.                  */
#     define SIG_SUSPEND SIGPWR
#   endif
# elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_FREEBSD_THREADS) && defined(__GLIBC__) \
       && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_USESIGRT_SIGNALS)
#   define SIG_SUSPEND (32+6)
# elif (defined(MANAGED_STACK_ADDRESS_BOEHM_GC_FREEBSD_THREADS) || defined(HURD) || defined(RTEMS)) \
       && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_USESIGRT_SIGNALS)
#   define SIG_SUSPEND SIGUSR1
        /* SIGTSTP and SIGCONT could be used alternatively on FreeBSD.  */
# elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_OPENBSD_THREADS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_USESIGRT_SIGNALS)
#     define SIG_SUSPEND SIGXFSZ
# elif defined(_SIGRTMIN) && !defined(CPPCHECK)
#   define SIG_SUSPEND _SIGRTMIN + 6
# else
#   define SIG_SUSPEND SIGRTMIN + 6
# endif
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS && !SIG_SUSPEND */

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_SEM_INIT_PSHARED)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_SEM_INIT_PSHARED 0
#endif

/* Some macros for setjmp that works across signal handlers     */
/* were possible, and a couple of routines to facilitate        */
/* catching accesses to bad addresses when that's               */
/* possible/needed.                                             */
#if (defined(UNIX_LIKE) || (defined(NEED_FIND_LIMIT) && defined(CYGWIN32))) \
    && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_SIGSETJMP)
# if defined(SUNOS5SIGS) && !defined(FREEBSD) && !defined(LINUX)
    EXTERN_C_END
#   include <sys/siginfo.h>
    EXTERN_C_BEGIN
# endif
  /* Define SETJMP and friends to be the version that restores  */
  /* the signal mask.                                           */
# define SETJMP(env) sigsetjmp(env, 1)
# define LONGJMP(env, val) siglongjmp(env, val)
# define JMP_BUF sigjmp_buf
#else
# ifdef ECOS
#   define SETJMP(env) hal_setjmp(env)
# else
#   define SETJMP(env) setjmp(env)
# endif
# define LONGJMP(env, val) longjmp(env, val)
# define JMP_BUF jmp_buf
#endif /* !UNIX_LIKE || MANAGED_STACK_ADDRESS_BOEHM_GC_NO_SIGSETJMP */

#if defined(DATASTART_USES_BSDGETDATASTART)
  EXTERN_C_END
# include <machine/trap.h>
  EXTERN_C_BEGIN
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_FreeBSDGetDataStart(size_t, ptr_t);
# define DATASTART_IS_FUNC
#endif /* DATASTART_USES_BSDGETDATASTART */

#if defined(NEED_FIND_LIMIT) \
     || (defined(WRAP_MARK_SOME) && defined(NO_SEH_AVAILABLE)) \
     || (defined(USE_PROC_FOR_LIBRARIES) && defined(THREADS))
  MANAGED_STACK_ADDRESS_BOEHM_GC_EXTERN JMP_BUF MANAGED_STACK_ADDRESS_BOEHM_GC_jmp_buf;

  /* Set up a handler for address faults which will longjmp to  */
  /* MANAGED_STACK_ADDRESS_BOEHM_GC_jmp_buf.                                                */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_setup_temporary_fault_handler(void);

  /* Undo the effect of MANAGED_STACK_ADDRESS_BOEHM_GC_setup_temporary_fault_handler.       */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_reset_fault_handler(void);
#endif /* NEED_FIND_LIMIT || USE_PROC_FOR_LIBRARIES || WRAP_MARK_SOME */

/* Some convenience macros for cancellation support. */
#ifdef CANCEL_SAFE
# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) \
     && (defined(USE_COMPILER_TLS) \
         || (defined(LINUX) && !defined(ARM32) && MANAGED_STACK_ADDRESS_BOEHM_GC_GNUC_PREREQ(3, 3) \
             || defined(HPUX) /* and probably others ... */))
    extern __thread unsigned char MANAGED_STACK_ADDRESS_BOEHM_GC_cancel_disable_count;
#   define NEED_CANCEL_DISABLE_COUNT
#   define INCR_CANCEL_DISABLE() ++MANAGED_STACK_ADDRESS_BOEHM_GC_cancel_disable_count
#   define DECR_CANCEL_DISABLE() --MANAGED_STACK_ADDRESS_BOEHM_GC_cancel_disable_count
#   define ASSERT_CANCEL_DISABLED() MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_cancel_disable_count > 0)
# else
#   define INCR_CANCEL_DISABLE()
#   define DECR_CANCEL_DISABLE()
#   define ASSERT_CANCEL_DISABLED() (void)0
# endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS */
# define DISABLE_CANCEL(state) \
        do { pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &state); \
          INCR_CANCEL_DISABLE(); } while (0)
# define RESTORE_CANCEL(state) \
        do { ASSERT_CANCEL_DISABLED(); \
          pthread_setcancelstate(state, NULL); \
          DECR_CANCEL_DISABLE(); } while (0)
#else
# define DISABLE_CANCEL(state) (void)0
# define RESTORE_CANCEL(state) (void)0
# define ASSERT_CANCEL_DISABLED() (void)0
#endif /* !CANCEL_SAFE */

/* Multiply 32-bit unsigned values (used by MANAGED_STACK_ADDRESS_BOEHM_GC_push_contents_hdr).  */
#ifdef NO_LONGLONG64
# define LONG_MULT(hprod, lprod, x, y) \
    do { \
      unsigned32 lx = (x) & 0xffffU; \
      unsigned32 ly = (y) & 0xffffU; \
      unsigned32 hx = (x) >> 16; \
      unsigned32 hy = (y) >> 16; \
      unsigned32 lxhy = lx * hy; \
      unsigned32 mid = hx * ly + lxhy; /* may overflow */ \
      unsigned32 lxly = lx * ly; \
      \
      lprod = (mid << 16) + lxly; /* may overflow */ \
      hprod = hx * hy + ((lprod) < lxly ? 1U : 0) \
              + (mid < lxhy ? (unsigned32)0x10000UL : 0) + (mid >> 16); \
    } while (0)
#elif defined(I386) && defined(__GNUC__) && !defined(NACL)
# define LONG_MULT(hprod, lprod, x, y) \
    __asm__ __volatile__ ("mull %2" \
                          : "=a" (lprod), "=d" (hprod) \
                          : "r" (y), "0" (x))
#else
# if defined(__int64) && !defined(__GNUC__) && !defined(CPPCHECK)
#   define ULONG_MULT_T unsigned __int64
# else
#   define ULONG_MULT_T unsigned long long
# endif
# define LONG_MULT(hprod, lprod, x, y) \
    do { \
        ULONG_MULT_T prod = (ULONG_MULT_T)(x) * (ULONG_MULT_T)(y); \
        \
        MANAGED_STACK_ADDRESS_BOEHM_GC_STATIC_ASSERT(sizeof(x) + sizeof(y) <= sizeof(prod)); \
        hprod = (unsigned32)(prod >> 32); \
        lprod = (unsigned32)prod; \
    } while (0)
#endif /* !I386 && !NO_LONGLONG64 */

EXTERN_C_END

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_PRIVATE_H */
