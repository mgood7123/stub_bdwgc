/*
 * Copyright (c) 2000-2005 by Hewlett-Packard Company.  All rights reserved.
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

/* Included indirectly from a thread-library-specific file.     */
/* This is the interface for thread-local allocation, whose     */
/* implementation is mostly thread-library-independent.         */
/* Here we describe only the interface that needs to be known   */
/* and invoked from the thread support layer; the actual        */
/* implementation also exports MANAGED_STACK_ADDRESS_BOEHM_GC_malloc and friends, which     */
/* are declared in gc.h.                                        */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_THREAD_LOCAL_ALLOC_H
#define MANAGED_STACK_ADDRESS_BOEHM_GC_THREAD_LOCAL_ALLOC_H

#include "gc_priv.h"

#ifdef THREAD_LOCAL_ALLOC

#if defined(USE_HPUX_TLS)
# error USE_HPUX_TLS macro was replaced by USE_COMPILER_TLS
#endif

#include <stdlib.h>

EXTERN_C_BEGIN

#if !defined(USE_PTHREAD_SPECIFIC) && !defined(USE_WIN32_SPECIFIC) \
    && !defined(USE_WIN32_COMPILER_TLS) && !defined(USE_COMPILER_TLS) \
    && !defined(USE_CUSTOM_SPECIFIC)
# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS)
#   if defined(CYGWIN32) && MANAGED_STACK_ADDRESS_BOEHM_GC_GNUC_PREREQ(4, 0)
#     if defined(__clang__)
        /* As of Cygwin clang3.5.2, thread-local storage is unsupported.    */
#       define USE_PTHREAD_SPECIFIC
#     else
#       define USE_COMPILER_TLS
#     endif
#   elif defined(__GNUC__) || defined(MSWINCE)
#     define USE_WIN32_SPECIFIC
#   else
#     define USE_WIN32_COMPILER_TLS
#   endif /* !GNU */

# elif defined(HOST_ANDROID)
#   if defined(ARM32) && (MANAGED_STACK_ADDRESS_BOEHM_GC_GNUC_PREREQ(4, 6) \
                          || MANAGED_STACK_ADDRESS_BOEHM_GC_CLANG_PREREQ_FULL(3, 8, 256229))
#     define USE_COMPILER_TLS
#   elif !defined(__clang__) && !defined(ARM32)
      /* TODO: Support clang/arm64 */
#     define USE_COMPILER_TLS
#   else
#     define USE_PTHREAD_SPECIFIC
#   endif

# elif defined(LINUX) && MANAGED_STACK_ADDRESS_BOEHM_GC_GNUC_PREREQ(3, 3) /* && !HOST_ANDROID */
#   if defined(ARM32) || defined(AVR32)
      /* TODO: support Linux/arm */
#     define USE_PTHREAD_SPECIFIC
#   elif defined(AARCH64) && defined(__clang__) && !MANAGED_STACK_ADDRESS_BOEHM_GC_CLANG_PREREQ(8, 0)
      /* To avoid "R_AARCH64_ABS64 used with TLS symbol" linker warnings. */
#     define USE_PTHREAD_SPECIFIC
#   else
#     define USE_COMPILER_TLS
#   endif

# elif (defined(FREEBSD) \
        || (defined(NETBSD) && __NetBSD_Version__ >= 600000000 /* 6.0 */)) \
       && (MANAGED_STACK_ADDRESS_BOEHM_GC_GNUC_PREREQ(4, 4) || MANAGED_STACK_ADDRESS_BOEHM_GC_CLANG_PREREQ(3, 9))
#   define USE_COMPILER_TLS

# elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_HPUX_THREADS)
#   ifdef __GNUC__
#     define USE_PTHREAD_SPECIFIC
        /* Empirically, as of gcc 3.3, USE_COMPILER_TLS doesn't work.   */
#   else
#     define USE_COMPILER_TLS
#   endif

# elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_IRIX_THREADS) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_OPENBSD_THREADS) \
       || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_SOLARIS_THREADS) \
       || defined(NN_PLATFORM_CTR) || defined(NN_BUILD_TARGET_PLATFORM_NX)
#   define USE_CUSTOM_SPECIFIC  /* Use our own. */

# else
#   define USE_PTHREAD_SPECIFIC
# endif
#endif /* !USE_x_SPECIFIC */

#ifndef THREAD_FREELISTS_KINDS
# ifdef ENABLE_DISCLAIM
#   define THREAD_FREELISTS_KINDS (NORMAL+2)
# else
#   define THREAD_FREELISTS_KINDS (NORMAL+1)
# endif
#endif /* !THREAD_FREELISTS_KINDS */

/* The first MANAGED_STACK_ADDRESS_BOEHM_GC_TINY_FREELISTS free lists correspond to the first   */
/* MANAGED_STACK_ADDRESS_BOEHM_GC_TINY_FREELISTS multiples of MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES, i.e. we keep    */
/* separate free lists for each multiple of MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES up to  */
/* (MANAGED_STACK_ADDRESS_BOEHM_GC_TINY_FREELISTS-1) * MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES.  After that they may   */
/* be spread out further.                                           */

/* One of these should be declared as the tlfs field in the     */
/* structure pointed to by a MANAGED_STACK_ADDRESS_BOEHM_GC_thread.                         */
typedef struct thread_local_freelists {
  void * _freelists[THREAD_FREELISTS_KINDS][MANAGED_STACK_ADDRESS_BOEHM_GC_TINY_FREELISTS];
# define ptrfree_freelists _freelists[PTRFREE]
# define normal_freelists _freelists[NORMAL]
        /* Note: Preserve *_freelists names for some clients.   */
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT
    void * gcj_freelists[MANAGED_STACK_ADDRESS_BOEHM_GC_TINY_FREELISTS];
#   define ERROR_FL ((void *)MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX)
        /* Value used for gcj_freelists[-1]; allocation is      */
        /* erroneous.                                           */
# endif
  /* Free lists contain either a pointer or a small count       */
  /* reflecting the number of granules allocated at that        */
  /* size.                                                      */
  /* 0 ==> thread-local allocation in use, free list            */
  /*       empty.                                               */
  /* > 0, <= DIRECT_GRANULES ==> Using global allocation,       */
  /*       too few objects of this size have been               */
  /*       allocated by this thread.                            */
  /* >= HBLKSIZE  => pointer to nonempty free list.             */
  /* > DIRECT_GRANULES, < HBLKSIZE ==> transition to            */
  /*    local alloc, equivalent to 0.                           */
# define DIRECT_GRANULES (HBLKSIZE/MANAGED_STACK_ADDRESS_BOEHM_GC_GRANULE_BYTES)
        /* Don't use local free lists for up to this much       */
        /* allocation.                                          */
} *MANAGED_STACK_ADDRESS_BOEHM_GC_tlfs;

#if defined(USE_PTHREAD_SPECIFIC)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_getspecific pthread_getspecific
# define MANAGED_STACK_ADDRESS_BOEHM_GC_setspecific pthread_setspecific
# define MANAGED_STACK_ADDRESS_BOEHM_GC_key_create pthread_key_create
# define MANAGED_STACK_ADDRESS_BOEHM_GC_remove_specific(key) (void)pthread_setspecific(key, NULL)
                        /* Explicitly delete the value to stop the TLS  */
                        /* destructor from being called repeatedly.     */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_remove_specific_after_fork(key, t) (void)0
                                        /* Should not need any action.  */
  typedef pthread_key_t MANAGED_STACK_ADDRESS_BOEHM_GC_key_t;
#elif defined(USE_COMPILER_TLS) || defined(USE_WIN32_COMPILER_TLS)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_getspecific(x) (x)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_setspecific(key, v) ((key) = (v), 0)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_key_create(key, d) 0
# define MANAGED_STACK_ADDRESS_BOEHM_GC_remove_specific(key) (void)MANAGED_STACK_ADDRESS_BOEHM_GC_setspecific(key, NULL)
                        /* Just to clear the pointer to tlfs. */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_remove_specific_after_fork(key, t) (void)0
  typedef void * MANAGED_STACK_ADDRESS_BOEHM_GC_key_t;
#elif defined(USE_WIN32_SPECIFIC)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_getspecific TlsGetValue
# define MANAGED_STACK_ADDRESS_BOEHM_GC_setspecific(key, v) !TlsSetValue(key, v)
        /* We assume 0 == success, msft does the opposite.      */
# ifndef TLS_OUT_OF_INDEXES
        /* this is currently missing in WinCE   */
#   define TLS_OUT_OF_INDEXES (DWORD)0xFFFFFFFF
# endif
# define MANAGED_STACK_ADDRESS_BOEHM_GC_key_create(key, d) \
        ((d) != 0 || (*(key) = TlsAlloc()) == TLS_OUT_OF_INDEXES ? -1 : 0)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_remove_specific(key) (void)MANAGED_STACK_ADDRESS_BOEHM_GC_setspecific(key, NULL)
        /* Need TlsFree on process exit/detach?   */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_remove_specific_after_fork(key, t) (void)0
  typedef DWORD MANAGED_STACK_ADDRESS_BOEHM_GC_key_t;
#elif defined(USE_CUSTOM_SPECIFIC)
  EXTERN_C_END
# include "specific.h"
  EXTERN_C_BEGIN
#else
# error implement me
#endif

/* Each thread structure must be initialized.   */
/* This call must be made from the new thread.  */
/* Caller holds allocation lock.                */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_init_thread_local(MANAGED_STACK_ADDRESS_BOEHM_GC_tlfs p);

/* Called when a thread is unregistered, or exits.      */
/* We hold the allocator lock.                          */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_destroy_thread_local(MANAGED_STACK_ADDRESS_BOEHM_GC_tlfs p);

/* The thread support layer must arrange to mark thread-local   */
/* free lists explicitly, since the link field is often         */
/* invisible to the marker.  It knows how to find all threads;  */
/* we take care of an individual thread freelist structure.     */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_mark_thread_local_fls_for(MANAGED_STACK_ADDRESS_BOEHM_GC_tlfs p);

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS
  MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_is_thread_tsd_valid(void *tsd);
  void MANAGED_STACK_ADDRESS_BOEHM_GC_check_tls_for(MANAGED_STACK_ADDRESS_BOEHM_GC_tlfs p);
# if defined(USE_CUSTOM_SPECIFIC)
    void MANAGED_STACK_ADDRESS_BOEHM_GC_check_tsd_marks(tsd *key);
# endif
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_TLS_FAST
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_TLS_FAST /* empty */
#endif

extern
#if defined(USE_COMPILER_TLS)
  __thread MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_TLS_FAST
#elif defined(USE_WIN32_COMPILER_TLS)
  __declspec(thread) MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_TLS_FAST
#endif
  MANAGED_STACK_ADDRESS_BOEHM_GC_key_t MANAGED_STACK_ADDRESS_BOEHM_GC_thread_key;
/* This is set up by the thread_local_alloc implementation.  No need    */
/* for cleanup on thread exit.  But the thread support layer makes sure */
/* that MANAGED_STACK_ADDRESS_BOEHM_GC_thread_key is traced, if necessary.                          */

EXTERN_C_END

#endif /* THREAD_LOCAL_ALLOC */

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_THREAD_LOCAL_ALLOC_H */
