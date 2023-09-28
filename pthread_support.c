/*
 * Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 1998 by Fergus Henderson.  All rights reserved.
 * Copyright (c) 2000-2008 by Hewlett-Packard Company.  All rights reserved.
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

#include "private/pthread_support.h"

/*
 * Support code originally for LinuxThreads, the clone()-based kernel
 * thread package for Linux which is included in libc6.
 *
 * This code no doubt makes some assumptions beyond what is
 * guaranteed by the pthread standard, though it now does
 * very little of that.  It now also supports NPTL, and many
 * other Posix thread implementations.  We are trying to merge
 * all flavors of pthread support code into this file.
 */

#ifdef THREADS

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS
# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS) \
     || (defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) && defined(EMULATE_PTHREAD_SEMAPHORE))
#   include "private/darwin_semaphore.h"
# elif !defined(SN_TARGET_ORBIS) && !defined(SN_TARGET_PSP2)
#   include <semaphore.h>
# endif
# include <errno.h>
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS
# include <sched.h>
# include <time.h>
# if !defined(SN_TARGET_ORBIS) && !defined(SN_TARGET_PSP2)
#   if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_RTEMS_PTHREADS)
#     include <sys/mman.h>
#   endif
#   include <sys/time.h>
#   include <sys/stat.h>
#   include <fcntl.h>
# endif
# include <signal.h>
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS */

#ifdef E2K
# include <alloca.h>
#endif

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_FREEBSD_THREADS)
# include <sys/sysctl.h>
#endif

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NETBSD_THREADS) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_OPENBSD_THREADS)
# include <sys/param.h>
# include <sys/sysctl.h>
#endif

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DGUX386_THREADS)
# include <sys/dg_sys_info.h>
# include <sys/_int_psem.h>
  /* sem_t is an uint in DG/UX */
  typedef unsigned int sem_t;
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_DGUX386_THREADS */

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS) \
    && !defined(SN_TARGET_ORBIS) && !defined(SN_TARGET_PSP2)
  /* Undefine macros used to redirect pthread primitives.       */
# undef pthread_create
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_SIGMASK
#   undef pthread_sigmask
# endif
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_CANCEL
#   undef pthread_cancel
# endif
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_HAVE_PTHREAD_EXIT
#   undef pthread_exit
# endif
# undef pthread_join
# undef pthread_detach
# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_OSF1_THREADS) && defined(_PTHREAD_USE_MANGLED_NAMES_) \
     && !defined(_PTHREAD_USE_PTDNAM_)
    /* Restore the original mangled names on Tru64 UNIX.        */
#   define pthread_create __pthread_create
#   define pthread_join   __pthread_join
#   define pthread_detach __pthread_detach
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_CANCEL
#     define pthread_cancel __pthread_cancel
#   endif
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_HAVE_PTHREAD_EXIT
#     define pthread_exit __pthread_exit
#   endif
# endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_OSF1_THREADS */
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS */

#if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) \
    && !defined(SN_TARGET_ORBIS) && !defined(SN_TARGET_PSP2)
  /* TODO: Enable MANAGED_STACK_ADDRESS_BOEHM_GC_USE_DLOPEN_WRAP for Cygwin? */

# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_USE_LD_WRAP
#   define WRAP_FUNC(f) __wrap_##f
#   define REAL_FUNC(f) __real_##f
    int REAL_FUNC(pthread_create)(pthread_t *,
                                  MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_CREATE_CONST pthread_attr_t *,
                                  void * (*start_routine)(void *), void *);
    int REAL_FUNC(pthread_join)(pthread_t, void **);
    int REAL_FUNC(pthread_detach)(pthread_t);
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_SIGMASK
      int REAL_FUNC(pthread_sigmask)(int, const sigset_t *, sigset_t *);
#   endif
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_CANCEL
      int REAL_FUNC(pthread_cancel)(pthread_t);
#   endif
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_HAVE_PTHREAD_EXIT
      void REAL_FUNC(pthread_exit)(void *) MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_EXIT_ATTRIBUTE;
#   endif
# elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_USE_DLOPEN_WRAP)
#   include <dlfcn.h>
#   define WRAP_FUNC(f) f
#   define REAL_FUNC(f) MANAGED_STACK_ADDRESS_BOEHM_GC_real_##f
    /* We define both MANAGED_STACK_ADDRESS_BOEHM_GC_f and plain f to be the wrapped function.  */
    /* In that way plain calls work, as do calls from files that    */
    /* included gc.h, which redefined f to MANAGED_STACK_ADDRESS_BOEHM_GC_f.                    */
    /* FIXME: Needs work for DARWIN and True64 (OSF1) */
    typedef int (*MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_create_t)(pthread_t *,
                                MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_CREATE_CONST pthread_attr_t *,
                                void * (*)(void *), void *);
    static MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_create_t REAL_FUNC(pthread_create);
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_SIGMASK
      typedef int (*MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_sigmask_t)(int, const sigset_t *, sigset_t *);
      static MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_sigmask_t REAL_FUNC(pthread_sigmask);
#   endif
    typedef int (*MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_join_t)(pthread_t, void **);
    static MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_join_t REAL_FUNC(pthread_join);
    typedef int (*MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_detach_t)(pthread_t);
    static MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_detach_t REAL_FUNC(pthread_detach);
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_CANCEL
      typedef int (*MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_cancel_t)(pthread_t);
      static MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_cancel_t REAL_FUNC(pthread_cancel);
#   endif
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_HAVE_PTHREAD_EXIT
      typedef void (*MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_exit_t)(void *) MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_EXIT_ATTRIBUTE;
      static MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_exit_t REAL_FUNC(pthread_exit);
#   endif
# else
#   define WRAP_FUNC(f) MANAGED_STACK_ADDRESS_BOEHM_GC_##f
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_DGUX386_THREADS
#     define REAL_FUNC(f) __d10_##f
#   else
#     define REAL_FUNC(f) f
#   endif
# endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_USE_LD_WRAP && !MANAGED_STACK_ADDRESS_BOEHM_GC_USE_DLOPEN_WRAP */

# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_USE_LD_WRAP) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_USE_DLOPEN_WRAP)
    /* Define MANAGED_STACK_ADDRESS_BOEHM_GC_ functions as aliases for the plain ones, which will   */
    /* be intercepted.  This allows files which include gc.h, and hence */
    /* generate references to the MANAGED_STACK_ADDRESS_BOEHM_GC_ symbols, to see the right ones.   */
    MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_create(pthread_t *t,
                                 MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_CREATE_CONST pthread_attr_t *a,
                                 void * (*fn)(void *), void *arg)
    {
      return pthread_create(t, a, fn, arg);
    }

#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_SIGMASK
      MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_sigmask(int how, const sigset_t *mask,
                                    sigset_t *old)
      {
        return pthread_sigmask(how, mask, old);
      }
#   endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_SIGMASK */

    MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_join(pthread_t t, void **res)
    {
      return pthread_join(t, res);
    }

    MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_detach(pthread_t t)
    {
      return pthread_detach(t);
    }

#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_CANCEL
      MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_cancel(pthread_t t)
      {
        return pthread_cancel(t);
      }
#   endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_CANCEL */

#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_HAVE_PTHREAD_EXIT
      MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_EXIT_ATTRIBUTE void MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_exit(void *retval)
      {
        pthread_exit(retval);
      }
#   endif
# endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_USE_LD_WRAP || MANAGED_STACK_ADDRESS_BOEHM_GC_USE_DLOPEN_WRAP */

# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_USE_DLOPEN_WRAP
    STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_syms_initialized = FALSE;

    STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_init_real_syms(void)
    {
      void *dl_handle;

      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(!MANAGED_STACK_ADDRESS_BOEHM_GC_syms_initialized);
#     ifdef RTLD_NEXT
        dl_handle = RTLD_NEXT;
#     else
        dl_handle = dlopen("libpthread.so.0", RTLD_LAZY);
        if (NULL == dl_handle) {
          dl_handle = dlopen("libpthread.so", RTLD_LAZY); /* without ".0" */
          if (NULL == dl_handle) ABORT("Couldn't open libpthread");
        }
#     endif
      REAL_FUNC(pthread_create) = (MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_create_t)(word)
                                dlsym(dl_handle, "pthread_create");
#     ifdef RTLD_NEXT
        if (REAL_FUNC(pthread_create) == 0)
          ABORT("pthread_create not found"
                " (probably -lgc is specified after -lpthread)");
#     endif
#     ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_SIGMASK
        REAL_FUNC(pthread_sigmask) = (MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_sigmask_t)(word)
                                dlsym(dl_handle, "pthread_sigmask");
#     endif
      REAL_FUNC(pthread_join) = (MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_join_t)(word)
                                dlsym(dl_handle, "pthread_join");
      REAL_FUNC(pthread_detach) = (MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_detach_t)(word)
                                dlsym(dl_handle, "pthread_detach");
#     ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_CANCEL
        REAL_FUNC(pthread_cancel) = (MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_cancel_t)(word)
                                dlsym(dl_handle, "pthread_cancel");
#     endif
#     ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_HAVE_PTHREAD_EXIT
        REAL_FUNC(pthread_exit) = (MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_exit_t)(word)
                                dlsym(dl_handle, "pthread_exit");
#     endif
      MANAGED_STACK_ADDRESS_BOEHM_GC_syms_initialized = TRUE;
    }

#   define INIT_REAL_SYMS() if (EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_syms_initialized, TRUE)) {} \
                            else MANAGED_STACK_ADDRESS_BOEHM_GC_init_real_syms()
# else
#   define INIT_REAL_SYMS() (void)0
# endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_USE_DLOPEN_WRAP */

#else
# define WRAP_FUNC(f) MANAGED_STACK_ADDRESS_BOEHM_GC_##f
# define REAL_FUNC(f) f
# define INIT_REAL_SYMS() (void)0
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_ALWAYS_MULTITHREADED
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_need_to_lock = FALSE;
#endif

#ifdef THREAD_LOCAL_ALLOC
  /* We must explicitly mark ptrfree and gcj free lists, since the free */
  /* list links wouldn't otherwise be found.  We also set them in the   */
  /* normal free lists, since that involves touching less memory than   */
  /* if we scanned them normally.                                       */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_mark_thread_local_free_lists(void)
  {
    int i;
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread p;

    for (i = 0; i < THREAD_TABLE_SZ; ++i) {
      for (p = MANAGED_STACK_ADDRESS_BOEHM_GC_threads[i]; p != NULL; p = p -> tm.next) {
        if (!KNOWN_FINISHED(p))
          MANAGED_STACK_ADDRESS_BOEHM_GC_mark_thread_local_fls_for(&p->tlfs);
      }
    }
  }

# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS)
    /* Check that all thread-local free-lists are completely marked.    */
    /* Also check that thread-specific-data structures are marked.      */
    void MANAGED_STACK_ADDRESS_BOEHM_GC_check_tls(void)
    {
        int i;
        MANAGED_STACK_ADDRESS_BOEHM_GC_thread p;

        for (i = 0; i < THREAD_TABLE_SZ; ++i) {
          for (p = MANAGED_STACK_ADDRESS_BOEHM_GC_threads[i]; p != NULL; p = p -> tm.next) {
            if (!KNOWN_FINISHED(p))
              MANAGED_STACK_ADDRESS_BOEHM_GC_check_tls_for(&p->tlfs);
          }
        }
#       if defined(USE_CUSTOM_SPECIFIC)
          if (MANAGED_STACK_ADDRESS_BOEHM_GC_thread_key != 0)
            MANAGED_STACK_ADDRESS_BOEHM_GC_check_tsd_marks(MANAGED_STACK_ADDRESS_BOEHM_GC_thread_key);
#       endif
    }
# endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS */
#endif /* THREAD_LOCAL_ALLOC */

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS
  /* A macro for functions and variables which should be accessible     */
  /* from win32_threads.c but otherwise could be static.                */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_INNER_WIN32THREAD MANAGED_STACK_ADDRESS_BOEHM_GC_INNER
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_INNER_WIN32THREAD STATIC
#endif

#ifdef PARALLEL_MARK

# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) || defined(USE_PROC_FOR_LIBRARIES) \
     || (defined(IA64) && (defined(HAVE_PTHREAD_ATTR_GET_NP) \
                           || defined(HAVE_PTHREAD_GETATTR_NP)))
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER_WIN32THREAD ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_marker_sp[MAX_MARKERS - 1] = {0};
                                        /* The cold end of the stack    */
                                        /* for markers.                 */
# endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS || USE_PROC_FOR_LIBRARIES */

# if defined(IA64) && defined(USE_PROC_FOR_LIBRARIES)
    static ptr_t marker_bsp[MAX_MARKERS - 1] = {0};
# endif

# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_THREADS_DISCOVERY)
    static mach_port_t marker_mach_threads[MAX_MARKERS - 1] = {0};

    /* Used only by MANAGED_STACK_ADDRESS_BOEHM_GC_suspend_thread_list().   */
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_is_mach_marker(thread_act_t thread)
    {
      int i;
      for (i = 0; i < MANAGED_STACK_ADDRESS_BOEHM_GC_markers_m1; i++) {
        if (marker_mach_threads[i] == thread)
          return TRUE;
      }
      return FALSE;
    }
# endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS */

# ifdef HAVE_PTHREAD_SETNAME_NP_WITH_TID_AND_ARG /* NetBSD */
    static void set_marker_thread_name(unsigned id)
    {
      int err = pthread_setname_np(pthread_self(), "GC-marker-%zu",
                                   (void*)(size_t)id);
      if (EXPECT(err != 0, FALSE))
        WARN("pthread_setname_np failed, errno= %" WARN_PRIdPTR "\n",
             (signed_word)err);
    }

# elif defined(HAVE_PTHREAD_SETNAME_NP_WITH_TID) \
       || defined(HAVE_PTHREAD_SETNAME_NP_WITHOUT_TID) \
       || defined(HAVE_PTHREAD_SET_NAME_NP)
#   ifdef HAVE_PTHREAD_SET_NAME_NP
#     include <pthread_np.h>
#   endif
    static void set_marker_thread_name(unsigned id)
    {
      char name_buf[16]; /* pthread_setname_np may fail for longer names */
      int len = sizeof("GC-marker-") - 1;

      /* Compose the name manually as snprintf may be unavailable or    */
      /* "%u directive output may be truncated" warning may occur.      */
      BCOPY("GC-marker-", name_buf, len);
      if (id >= 10)
        name_buf[len++] = (char)('0' + (id / 10) % 10);
      name_buf[len] = (char)('0' + id % 10);
      name_buf[len + 1] = '\0';

#     ifdef HAVE_PTHREAD_SETNAME_NP_WITHOUT_TID /* iOS, OS X */
        (void)pthread_setname_np(name_buf);
#     elif defined(HAVE_PTHREAD_SET_NAME_NP) /* OpenBSD */
        pthread_set_name_np(pthread_self(), name_buf);
#     else /* Linux, Solaris, etc. */
        if (EXPECT(pthread_setname_np(pthread_self(), name_buf) != 0, FALSE))
          WARN("pthread_setname_np failed\n", 0);
#     endif
    }

# elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) && !defined(MSWINCE)
    /* A pointer to SetThreadDescription() which is available since     */
    /* Windows 10.  The function prototype is in processthreadsapi.h.   */
    static FARPROC setThreadDescription_fn;

    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_init_win32_thread_naming(HMODULE hK32)
    {
      if (hK32)
        setThreadDescription_fn = GetProcAddress(hK32, "SetThreadDescription");
    }

    static void set_marker_thread_name(unsigned id)
    {
      WCHAR name_buf[16];
      int len = sizeof(L"GC-marker-") / sizeof(WCHAR) - 1;
      HRESULT hr;

      if (!setThreadDescription_fn) return; /* missing SetThreadDescription */

      /* Compose the name manually as swprintf may be unavailable.      */
      BCOPY(L"GC-marker-", name_buf, len * sizeof(WCHAR));
      if (id >= 10)
        name_buf[len++] = (WCHAR)('0' + (id / 10) % 10);
      name_buf[len] = (WCHAR)('0' + id % 10);
      name_buf[len + 1] = 0;

      /* Invoke SetThreadDescription().  Cast the function pointer to word  */
      /* first to avoid "incompatible function types" compiler warning.     */
      hr = (*(HRESULT (WINAPI *)(HANDLE, const WCHAR *))
            (MANAGED_STACK_ADDRESS_BOEHM_GC_funcptr_uint)setThreadDescription_fn)(GetCurrentThread(),
                                                      name_buf);
      if (hr < 0)
        WARN("SetThreadDescription failed\n", 0);
    }
# else
#   define set_marker_thread_name(id) (void)(id)
# endif

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER_WIN32THREAD
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS_PARAMARK
    void *MANAGED_STACK_ADDRESS_BOEHM_GC_mark_thread(void *id)
# elif defined(MSWINCE)
    DWORD WINAPI MANAGED_STACK_ADDRESS_BOEHM_GC_mark_thread(LPVOID id)
# else
    unsigned __stdcall MANAGED_STACK_ADDRESS_BOEHM_GC_mark_thread(void *id)
# endif
  {
    word my_mark_no = 0;
    IF_CANCEL(int cancel_state;)

    if ((word)id == MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX) return 0; /* to prevent a compiler warning */
    DISABLE_CANCEL(cancel_state);
                         /* Mark threads are not cancellable; they      */
                         /* should be invisible to client.              */
    set_marker_thread_name((unsigned)(word)id);
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) || defined(USE_PROC_FOR_LIBRARIES) \
       || (defined(IA64) && (defined(HAVE_PTHREAD_ATTR_GET_NP) \
                             || defined(HAVE_PTHREAD_GETATTR_NP)))
      MANAGED_STACK_ADDRESS_BOEHM_GC_marker_sp[(word)id] = MANAGED_STACK_ADDRESS_BOEHM_GC_approx_sp();
#   endif
#   if defined(IA64) && defined(USE_PROC_FOR_LIBRARIES)
      marker_bsp[(word)id] = MANAGED_STACK_ADDRESS_BOEHM_GC_save_regs_in_stack();
#   endif
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_THREADS_DISCOVERY)
      marker_mach_threads[(word)id] = mach_thread_self();
#   endif
#   if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS_PARAMARK)
      MANAGED_STACK_ADDRESS_BOEHM_GC_marker_Id[(word)id] = thread_id_self();
#   endif

    /* Inform MANAGED_STACK_ADDRESS_BOEHM_GC_start_mark_threads about completion of marker data init. */
    MANAGED_STACK_ADDRESS_BOEHM_GC_acquire_mark_lock();
    if (0 == --MANAGED_STACK_ADDRESS_BOEHM_GC_fl_builder_count) /* count may have a negative value */
      MANAGED_STACK_ADDRESS_BOEHM_GC_notify_all_builder();

    /* MANAGED_STACK_ADDRESS_BOEHM_GC_mark_no is passed only to allow MANAGED_STACK_ADDRESS_BOEHM_GC_help_marker to terminate   */
    /* promptly.  This is important if it were called from the signal   */
    /* handler or from the GC lock acquisition code.  Under Linux, it's */
    /* not safe to call it from a signal handler, since it uses mutexes */
    /* and condition variables.  Since it is called only here, the      */
    /* argument is unnecessary.                                         */
    for (;; ++my_mark_no) {
      if (my_mark_no - MANAGED_STACK_ADDRESS_BOEHM_GC_mark_no > (word)2) {
        /* resynchronize if we get far off, e.g. because MANAGED_STACK_ADDRESS_BOEHM_GC_mark_no     */
        /* wrapped.                                                     */
        my_mark_no = MANAGED_STACK_ADDRESS_BOEHM_GC_mark_no;
      }
#     ifdef DEBUG_THREADS
        MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Starting helper for mark number %lu (thread %u)\n",
                      (unsigned long)my_mark_no, (unsigned)(word)id);
#     endif
      MANAGED_STACK_ADDRESS_BOEHM_GC_help_marker(my_mark_no);
    }
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER_WIN32THREAD int MANAGED_STACK_ADDRESS_BOEHM_GC_available_markers_m1 = 0;

#endif /* PARALLEL_MARK */

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS_PARAMARK

# ifdef GLIBC_2_1_MUTEX_HACK
    /* Ugly workaround for a linux threads bug in the final versions    */
    /* of glibc 2.1.  Pthread_mutex_trylock sets the mutex owner        */
    /* field even when it fails to acquire the mutex.  This causes      */
    /* pthread_cond_wait to die.  Should not be needed for glibc 2.2.   */
    /* According to the man page, we should use                         */
    /* PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP, but that isn't actually */
    /* defined.                                                         */
    static pthread_mutex_t mark_mutex =
        {0, 0, 0, PTHREAD_MUTEX_ERRORCHECK_NP, {0, 0}};
# else
    static pthread_mutex_t mark_mutex = PTHREAD_MUTEX_INITIALIZER;
# endif

# ifdef CAN_HANDLE_FORK
    static pthread_cond_t mark_cv;
                        /* initialized by MANAGED_STACK_ADDRESS_BOEHM_GC_start_mark_threads_inner   */
# else
    static pthread_cond_t mark_cv = PTHREAD_COND_INITIALIZER;
# endif

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_start_mark_threads_inner(void)
  {
    int i;
    pthread_attr_t attr;
#   ifndef NO_MARKER_SPECIAL_SIGMASK
      sigset_t set, oldset;
#   endif

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    ASSERT_CANCEL_DISABLED();
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_available_markers_m1 <= 0 || MANAGED_STACK_ADDRESS_BOEHM_GC_parallel) return;
                /* Skip if parallel markers disabled or already started. */
    MANAGED_STACK_ADDRESS_BOEHM_GC_wait_for_gc_completion(TRUE);

#   ifdef CAN_HANDLE_FORK
      /* Initialize mark_cv (for the first time), or cleanup its value  */
      /* after forking in the child process.  All the marker threads in */
      /* the parent process were blocked on this variable at fork, so   */
      /* pthread_cond_wait() malfunction (hang) is possible in the      */
      /* child process without such a cleanup.                          */
      /* TODO: This is not portable, it is better to shortly unblock    */
      /* all marker threads in the parent process at fork.              */
      {
        pthread_cond_t mark_cv_local = PTHREAD_COND_INITIALIZER;
        BCOPY(&mark_cv_local, &mark_cv, sizeof(mark_cv));
      }
#   endif

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_fl_builder_count == 0);
    INIT_REAL_SYMS(); /* for pthread_create */
    if (0 != pthread_attr_init(&attr)) ABORT("pthread_attr_init failed");
    if (0 != pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
        ABORT("pthread_attr_setdetachstate failed");

#   ifdef DEFAULT_STACK_MAYBE_SMALL
      /* Default stack size is usually too small: increase it.  */
      /* Otherwise marker threads or GC may run out of space.   */
      {
        size_t old_size;

        if (pthread_attr_getstacksize(&attr, &old_size) != 0)
          ABORT("pthread_attr_getstacksize failed");
        if (old_size < MIN_STACK_SIZE
            && old_size != 0 /* stack size is known */) {
          if (pthread_attr_setstacksize(&attr, MIN_STACK_SIZE) != 0)
            ABORT("pthread_attr_setstacksize failed");
        }
      }
#   endif /* DEFAULT_STACK_MAYBE_SMALL */

#   ifndef NO_MARKER_SPECIAL_SIGMASK
      /* Apply special signal mask to GC marker threads, and don't drop */
      /* user defined signals by GC marker threads.                     */
      if (sigfillset(&set) != 0)
        ABORT("sigfillset failed");

#     ifdef SIGNAL_BASED_STOP_WORLD
        /* These are used by GC to stop and restart the world.  */
        if (sigdelset(&set, MANAGED_STACK_ADDRESS_BOEHM_GC_get_suspend_signal()) != 0
            || sigdelset(&set, MANAGED_STACK_ADDRESS_BOEHM_GC_get_thr_restart_signal()) != 0)
          ABORT("sigdelset failed");
#     endif

      if (EXPECT(REAL_FUNC(pthread_sigmask)(SIG_BLOCK,
                                            &set, &oldset) < 0, FALSE)) {
        WARN("pthread_sigmask set failed, no markers started\n", 0);
        MANAGED_STACK_ADDRESS_BOEHM_GC_markers_m1 = 0;
        (void)pthread_attr_destroy(&attr);
        return;
      }
#   endif /* !NO_MARKER_SPECIAL_SIGMASK */

    /* To have proper MANAGED_STACK_ADDRESS_BOEHM_GC_parallel value in MANAGED_STACK_ADDRESS_BOEHM_GC_help_marker.      */
    MANAGED_STACK_ADDRESS_BOEHM_GC_markers_m1 = MANAGED_STACK_ADDRESS_BOEHM_GC_available_markers_m1;

    for (i = 0; i < MANAGED_STACK_ADDRESS_BOEHM_GC_available_markers_m1; ++i) {
      pthread_t new_thread;

#     ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS
        MANAGED_STACK_ADDRESS_BOEHM_GC_marker_last_stack_min[i] = ADDR_LIMIT;
#     endif
      if (EXPECT(REAL_FUNC(pthread_create)(&new_thread, &attr, MANAGED_STACK_ADDRESS_BOEHM_GC_mark_thread,
                                           (void *)(word)i) != 0, FALSE)) {
        WARN("Marker thread %" WARN_PRIdPTR " creation failed\n",
             (signed_word)i);
        /* Don't try to create other marker threads.    */
        MANAGED_STACK_ADDRESS_BOEHM_GC_markers_m1 = i;
        break;
      }
    }

#   ifndef NO_MARKER_SPECIAL_SIGMASK
      /* Restore previous signal mask.  */
      if (EXPECT(REAL_FUNC(pthread_sigmask)(SIG_SETMASK,
                                            &oldset, NULL) < 0, FALSE)) {
        WARN("pthread_sigmask restore failed\n", 0);
      }
#   endif

    (void)pthread_attr_destroy(&attr);
    MANAGED_STACK_ADDRESS_BOEHM_GC_wait_for_markers_init();
    MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF("Started %d mark helper threads\n", MANAGED_STACK_ADDRESS_BOEHM_GC_markers_m1);
  }

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS_PARAMARK */

/* A hash table to keep information about the registered threads.       */
/* Not used if MANAGED_STACK_ADDRESS_BOEHM_GC_win32_dll_threads is set.                             */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_thread MANAGED_STACK_ADDRESS_BOEHM_GC_threads[THREAD_TABLE_SZ] = {0};

/* It may not be safe to allocate when we register the first thread.    */
/* Note that next and status fields are unused, but there might be some */
/* other fields (crtn) to be pushed.                                    */
static struct MANAGED_STACK_ADDRESS_BOEHM_GC_StackContext_Rep first_crtn;
static struct MANAGED_STACK_ADDRESS_BOEHM_GC_Thread_Rep first_thread;

/* A place to retain a pointer to an allocated object while a thread    */
/* registration is ongoing.  Protected by the GC lock.                  */
static MANAGED_STACK_ADDRESS_BOEHM_GC_stack_context_t saved_crtn = NULL;

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_thr_initialized = FALSE;
#endif

void MANAGED_STACK_ADDRESS_BOEHM_GC_push_thread_structures(void)
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
# if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_THREADS_DISCOVERY) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS)
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_win32_dll_threads) {
      /* Unlike the other threads implementations, the thread table     */
      /* here contains no pointers to the collectible heap (note also   */
      /* that MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS is incompatible with DllMain-based thread     */
      /* registration).  Thus we have no private structures we need     */
      /* to preserve.                                                   */
    } else
# endif
  /* else */ {
    MANAGED_STACK_ADDRESS_BOEHM_GC_push_all(&MANAGED_STACK_ADDRESS_BOEHM_GC_threads, (ptr_t)(&MANAGED_STACK_ADDRESS_BOEHM_GC_threads) + sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_threads));
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NULL == first_thread.tm.next);
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NULL == first_thread.status);
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ALL_SYM(first_thread.crtn);
    MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ALL_SYM(saved_crtn);
  }
# if defined(THREAD_LOCAL_ALLOC) && defined(USE_CUSTOM_SPECIFIC)
    MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ALL_SYM(MANAGED_STACK_ADDRESS_BOEHM_GC_thread_key);
# endif
}

#if defined(MPROTECT_VDB) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS)
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_win32_unprotect_thread(MANAGED_STACK_ADDRESS_BOEHM_GC_thread t)
  {
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_win32_dll_threads && MANAGED_STACK_ADDRESS_BOEHM_GC_auto_incremental) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_stack_context_t crtn = t -> crtn;

      if (crtn != &first_crtn) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(SMALL_OBJ(MANAGED_STACK_ADDRESS_BOEHM_GC_size(crtn)));
        MANAGED_STACK_ADDRESS_BOEHM_GC_remove_protection(HBLKPTR(crtn), 1, FALSE);
      }
      if (t != &first_thread) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(SMALL_OBJ(MANAGED_STACK_ADDRESS_BOEHM_GC_size(t)));
        MANAGED_STACK_ADDRESS_BOEHM_GC_remove_protection(HBLKPTR(t), 1, FALSE);
      }
    }
  }
#endif /* MPROTECT_VDB && MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS */

#ifdef DEBUG_THREADS
  STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_count_threads(void)
  {
    int i;
    int count = 0;

#   if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_THREADS_DISCOVERY) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS)
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_win32_dll_threads) return -1; /* not implemented */
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    for (i = 0; i < THREAD_TABLE_SZ; ++i) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_thread p;

        for (p = MANAGED_STACK_ADDRESS_BOEHM_GC_threads[i]; p != NULL; p = p -> tm.next) {
            if (!KNOWN_FINISHED(p))
                ++count;
        }
    }
    return count;
  }
#endif /* DEBUG_THREADS */

/* Add a thread to MANAGED_STACK_ADDRESS_BOEHM_GC_threads.  We assume it wasn't already there.      */
/* The id field is set by the caller.                                   */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER_WIN32THREAD MANAGED_STACK_ADDRESS_BOEHM_GC_thread MANAGED_STACK_ADDRESS_BOEHM_GC_new_thread(thread_id_t self_id)
{
    int hv = THREAD_TABLE_INDEX(self_id);
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread result;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
#   ifdef DEBUG_THREADS
        MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Creating thread %p\n", (void *)(signed_word)self_id);
        for (result = MANAGED_STACK_ADDRESS_BOEHM_GC_threads[hv];
             result != NULL; result = result -> tm.next)
          if (!THREAD_ID_EQUAL(result -> id, self_id)) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Hash collision at MANAGED_STACK_ADDRESS_BOEHM_GC_threads[%d]\n", hv);
            break;
          }
#   endif
    if (EXPECT(NULL == first_thread.crtn, FALSE)) {
        result = &first_thread;
        first_thread.crtn = &first_crtn;
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_threads[hv]);
#       ifdef CPPCHECK
          MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((unsigned char)first_thread.flags_pad[0]);
#         if defined(THREAD_SANITIZER) && defined(SIGNAL_BASED_STOP_WORLD)
            MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((unsigned char)first_crtn.dummy[0]);
#         endif
#         ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
            MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((unsigned char)first_crtn.fnlz_pad[0]);
#         endif
#       endif
    } else {
        MANAGED_STACK_ADDRESS_BOEHM_GC_stack_context_t crtn;

        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(!MANAGED_STACK_ADDRESS_BOEHM_GC_win32_dll_threads);
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(!MANAGED_STACK_ADDRESS_BOEHM_GC_in_thread_creation);
        MANAGED_STACK_ADDRESS_BOEHM_GC_in_thread_creation = TRUE; /* OK to collect from unknown thread */
        crtn = (MANAGED_STACK_ADDRESS_BOEHM_GC_stack_context_t)MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_MALLOC(
                        sizeof(struct MANAGED_STACK_ADDRESS_BOEHM_GC_StackContext_Rep), NORMAL);

        /* The current stack is not scanned until the thread is         */
        /* registered, thus crtn pointer is to be retained in the       */
        /* global data roots for a while (and pushed explicitly if      */
        /* a collection occurs here).                                   */
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NULL == saved_crtn);
        saved_crtn = crtn;
        result = (MANAGED_STACK_ADDRESS_BOEHM_GC_thread)MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_MALLOC(sizeof(struct MANAGED_STACK_ADDRESS_BOEHM_GC_Thread_Rep),
                                               NORMAL);
        saved_crtn = NULL; /* no more collections till thread is registered */
        MANAGED_STACK_ADDRESS_BOEHM_GC_in_thread_creation = FALSE;
        if (NULL == crtn || NULL == result)
          ABORT("Failed to allocate memory for thread registering");
        result -> crtn = crtn;
    }
    /* The id field is not set here. */
#   ifdef USE_TKILL_ON_ANDROID
      result -> kernel_id = gettid();
#   endif
    result -> tm.next = MANAGED_STACK_ADDRESS_BOEHM_GC_threads[hv];
    MANAGED_STACK_ADDRESS_BOEHM_GC_threads[hv] = result;
#   ifdef NACL
      MANAGED_STACK_ADDRESS_BOEHM_GC_nacl_initialize_gc_thread(result);
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(0 == result -> flags);
    if (EXPECT(result != &first_thread, TRUE))
      MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(result);
    return result;
}

/* Delete a thread from MANAGED_STACK_ADDRESS_BOEHM_GC_threads.  We assume it is there.  (The code  */
/* intentionally traps if it was not.)  It is also safe to delete the   */
/* main thread.  If MANAGED_STACK_ADDRESS_BOEHM_GC_win32_dll_threads is set, it should be called    */
/* only from the thread being deleted.  If a thread has been joined,    */
/* but we have not yet been notified, then there may be more than one   */
/* thread in the table with the same thread id - this is OK because we  */
/* delete a specific one.                                               */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER_WIN32THREAD void MANAGED_STACK_ADDRESS_BOEHM_GC_delete_thread(MANAGED_STACK_ADDRESS_BOEHM_GC_thread t)
{
# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) && !defined(MSWINCE)
    CloseHandle(t -> handle);
# endif
# if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_THREADS_DISCOVERY) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS)
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_win32_dll_threads) {
      /* This is intended to be lock-free.  It is either called         */
      /* synchronously from the thread being deleted, or by the joining */
      /* thread.  In this branch asynchronous changes to (*t) are       */
      /* possible.  Note that it is not allowed to call MANAGED_STACK_ADDRESS_BOEHM_GC_printf (and  */
      /* the friends) here, see MANAGED_STACK_ADDRESS_BOEHM_GC_stop_world() in win32_threads.c for  */
      /* the information.                                               */
      t -> crtn -> stack_end = NULL;
      t -> id = 0;
      t -> flags = 0; /* !IS_SUSPENDED */
#     ifdef RETRY_GET_THREAD_CONTEXT
        t -> context_sp = NULL;
#     endif
      AO_store_release(&(t -> tm.in_use), FALSE);
    } else
# endif
  /* else */ {
    thread_id_t id = t -> id;
    int hv = THREAD_TABLE_INDEX(id);
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread p;
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread prev = NULL;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
#   if defined(DEBUG_THREADS) && !defined(MSWINCE) \
       && (!defined(MSWIN32) || defined(CONSOLE_LOG))
      MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Deleting thread %p, n_threads= %d\n",
                    (void *)(signed_word)id, MANAGED_STACK_ADDRESS_BOEHM_GC_count_threads());
#   endif
    for (p = MANAGED_STACK_ADDRESS_BOEHM_GC_threads[hv]; p != t; p = p -> tm.next) {
      prev = p;
    }
    if (NULL == prev) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_threads[hv] = p -> tm.next;
    } else {
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(prev != &first_thread);
        prev -> tm.next = p -> tm.next;
        MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(prev);
    }
    if (EXPECT(p != &first_thread, TRUE)) {
#     ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS
        mach_port_deallocate(mach_task_self(), p -> mach_thread);
#     endif
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(p -> crtn != &first_crtn);
      MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_FREE(p -> crtn);
      MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_FREE(p);
    }
  }
}

/* Return a MANAGED_STACK_ADDRESS_BOEHM_GC_thread corresponding to a given thread id, or    */
/* NULL if it is not there.                                     */
/* Caller holds allocation lock or otherwise inhibits updates.  */
/* If there is more than one thread with the given id we        */
/* return the most recent one.                                  */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_thread MANAGED_STACK_ADDRESS_BOEHM_GC_lookup_thread(thread_id_t id)
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_thread p;

# if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_THREADS_DISCOVERY) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS)
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_win32_dll_threads)
      return MANAGED_STACK_ADDRESS_BOEHM_GC_win32_dll_lookup_thread(id);
# endif
  for (p = MANAGED_STACK_ADDRESS_BOEHM_GC_threads[THREAD_TABLE_INDEX(id)];
       p != NULL; p = p -> tm.next) {
    if (EXPECT(THREAD_ID_EQUAL(p -> id, id), TRUE)) break;
  }
  return p;
}

/* Same as MANAGED_STACK_ADDRESS_BOEHM_GC_self_thread_inner() but acquires the GC lock.     */
STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_thread MANAGED_STACK_ADDRESS_BOEHM_GC_self_thread(void) {
  MANAGED_STACK_ADDRESS_BOEHM_GC_thread p;

  LOCK();
  p = MANAGED_STACK_ADDRESS_BOEHM_GC_self_thread_inner();
  UNLOCK();
  return p;
}

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
  /* Called by MANAGED_STACK_ADDRESS_BOEHM_GC_finalize() (in case of an allocation failure observed). */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_reset_finalizer_nested(void)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    MANAGED_STACK_ADDRESS_BOEHM_GC_self_thread_inner() -> crtn -> finalizer_nested = 0;
  }

  /* Checks and updates the thread-local level of finalizers recursion. */
  /* Returns NULL if MANAGED_STACK_ADDRESS_BOEHM_GC_invoke_finalizers() should not be called by the */
  /* collector (to minimize the risk of a deep finalizers recursion),   */
  /* otherwise returns a pointer to the thread-local finalizer_nested.  */
  /* Called by MANAGED_STACK_ADDRESS_BOEHM_GC_notify_or_invoke_finalizers() only.                   */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER unsigned char *MANAGED_STACK_ADDRESS_BOEHM_GC_check_finalizer_nested(void)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_stack_context_t crtn;
    unsigned nesting_level;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    crtn = MANAGED_STACK_ADDRESS_BOEHM_GC_self_thread_inner() -> crtn;
    nesting_level = crtn -> finalizer_nested;
    if (nesting_level) {
      /* We are inside another MANAGED_STACK_ADDRESS_BOEHM_GC_invoke_finalizers().          */
      /* Skip some implicitly-called MANAGED_STACK_ADDRESS_BOEHM_GC_invoke_finalizers()     */
      /* depending on the nesting (recursion) level.            */
      if (++(crtn -> finalizer_skipped) < (1U << nesting_level))
        return NULL;
      crtn -> finalizer_skipped = 0;
    }
    crtn -> finalizer_nested = (unsigned char)(nesting_level + 1);
    return &(crtn -> finalizer_nested);
  }
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION */

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) && defined(THREAD_LOCAL_ALLOC)
  /* This is called from thread-local MANAGED_STACK_ADDRESS_BOEHM_GC_malloc(). */
  MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_is_thread_tsd_valid(void *tsd)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread me = MANAGED_STACK_ADDRESS_BOEHM_GC_self_thread();

    return (word)tsd >= (word)(&me->tlfs)
            && (word)tsd < (word)(&me->tlfs) + sizeof(me->tlfs);
  }
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS && THREAD_LOCAL_ALLOC */

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_thread_is_registered(void)
{
  /* TODO: Use MANAGED_STACK_ADDRESS_BOEHM_GC_get_tlfs() instead. */
  MANAGED_STACK_ADDRESS_BOEHM_GC_thread me = MANAGED_STACK_ADDRESS_BOEHM_GC_self_thread();

  return me != NULL && !KNOWN_FINISHED(me);
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_register_altstack(void *normstack,
                MANAGED_STACK_ADDRESS_BOEHM_GC_word normstack_size, void *altstack, MANAGED_STACK_ADDRESS_BOEHM_GC_word altstack_size)
{
#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS
  /* TODO: Implement */
  UNUSED_ARG(normstack);
  UNUSED_ARG(normstack_size);
  UNUSED_ARG(altstack);
  UNUSED_ARG(altstack_size);
#else
  MANAGED_STACK_ADDRESS_BOEHM_GC_thread me;
  MANAGED_STACK_ADDRESS_BOEHM_GC_stack_context_t crtn;

  LOCK();
  me = MANAGED_STACK_ADDRESS_BOEHM_GC_self_thread_inner();
  if (EXPECT(NULL == me, FALSE)) {
    /* We are called before MANAGED_STACK_ADDRESS_BOEHM_GC_thr_init. */
    me = &first_thread;
  }
  crtn = me -> crtn;
  crtn -> normstack = (ptr_t)normstack;
  crtn -> normstack_size = normstack_size;
  crtn -> altstack = (ptr_t)altstack;
  crtn -> altstack_size = altstack_size;
  UNLOCK();
#endif
}

#ifdef USE_PROC_FOR_LIBRARIES
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_segment_is_thread_stack(ptr_t lo, ptr_t hi)
  {
    int i;
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread p;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
#   ifdef PARALLEL_MARK
      for (i = 0; i < MANAGED_STACK_ADDRESS_BOEHM_GC_markers_m1; ++i) {
        if ((word)MANAGED_STACK_ADDRESS_BOEHM_GC_marker_sp[i] > (word)lo
            && (word)MANAGED_STACK_ADDRESS_BOEHM_GC_marker_sp[i] < (word)hi)
          return TRUE;
#       ifdef IA64
          if ((word)marker_bsp[i] > (word)lo
              && (word)marker_bsp[i] < (word)hi)
            return TRUE;
#       endif
      }
#   endif
    for (i = 0; i < THREAD_TABLE_SZ; i++) {
      for (p = MANAGED_STACK_ADDRESS_BOEHM_GC_threads[i]; p != NULL; p = p -> tm.next) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_stack_context_t crtn = p -> crtn;

        if (crtn -> stack_end != NULL) {
#         ifdef STACK_GROWS_UP
            if ((word)crtn -> stack_end >= (word)lo
                && (word)crtn -> stack_end < (word)hi)
              return TRUE;
#         else
            if ((word)crtn -> stack_end > (word)lo
                && (word)crtn -> stack_end <= (word)hi)
              return TRUE;
#         endif
        }
      }
    }
    return FALSE;
  }
#endif /* USE_PROC_FOR_LIBRARIES */

#if (defined(HAVE_PTHREAD_ATTR_GET_NP) || defined(HAVE_PTHREAD_GETATTR_NP)) \
    && defined(IA64)
  /* Find the largest stack base smaller than bound.  May be used       */
  /* to find the boundary between a register stack and adjacent         */
  /* immediately preceding memory stack.                                */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_stack_base_below(ptr_t bound)
  {
    int i;
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread p;
    ptr_t result = 0;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
#   ifdef PARALLEL_MARK
      for (i = 0; i < MANAGED_STACK_ADDRESS_BOEHM_GC_markers_m1; ++i) {
        if ((word)MANAGED_STACK_ADDRESS_BOEHM_GC_marker_sp[i] > (word)result
            && (word)MANAGED_STACK_ADDRESS_BOEHM_GC_marker_sp[i] < (word)bound)
          result = MANAGED_STACK_ADDRESS_BOEHM_GC_marker_sp[i];
      }
#   endif
    for (i = 0; i < THREAD_TABLE_SZ; i++) {
      for (p = MANAGED_STACK_ADDRESS_BOEHM_GC_threads[i]; p != NULL; p = p -> tm.next) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_stack_context_t crtn = p -> crtn;

        if ((word)(crtn -> stack_end) > (word)result
            && (word)(crtn -> stack_end) < (word)bound) {
          result = crtn -> stack_end;
        }
      }
    }
    return result;
  }
#endif /* IA64 */

#ifndef STAT_READ
# define STAT_READ read
        /* If read is wrapped, this may need to be redefined to call    */
        /* the real one.                                                */
#endif

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_HPUX_THREADS
# define MANAGED_STACK_ADDRESS_BOEHM_GC_get_nprocs() pthread_num_processors_np()

#elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_OSF1_THREADS) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_AIX_THREADS) \
      || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_HAIKU_THREADS) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_SOLARIS_THREADS) \
      || defined(HURD) || defined(HOST_ANDROID) || defined(NACL)
  MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE int MANAGED_STACK_ADDRESS_BOEHM_GC_get_nprocs(void)
  {
    int nprocs = (int)sysconf(_SC_NPROCESSORS_ONLN);
    return nprocs > 0 ? nprocs : 1; /* ignore error silently */
  }

#elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_IRIX_THREADS)
  MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE int MANAGED_STACK_ADDRESS_BOEHM_GC_get_nprocs(void)
  {
    int nprocs = (int)sysconf(_SC_NPROC_ONLN);
    return nprocs > 0 ? nprocs : 1; /* ignore error silently */
  }

#elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_LINUX_THREADS) /* && !HOST_ANDROID && !NACL */
  /* Return the number of processors. */
  STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_get_nprocs(void)
  {
    /* Should be "return sysconf(_SC_NPROCESSORS_ONLN);" but that     */
    /* appears to be buggy in many cases.                             */
    /* We look for lines "cpu<n>" in /proc/stat.                      */
#   define PROC_STAT_BUF_SZ ((1 + MAX_MARKERS) * 100) /* should be enough */
    /* No need to read the entire /proc/stat to get maximum cpu<N> as   */
    /* - the requested lines are located at the beginning of the file;  */
    /* - the lines with cpu<N> where N > MAX_MARKERS are not needed.    */
    char stat_buf[PROC_STAT_BUF_SZ+1];
    int f;
    int result, i, len;

    f = open("/proc/stat", O_RDONLY);
    if (f < 0) {
      WARN("Could not open /proc/stat\n", 0);
      return 1; /* assume an uniprocessor */
    }
    len = STAT_READ(f, stat_buf, sizeof(stat_buf)-1);
    /* Unlikely that we need to retry because of an incomplete read here. */
    if (len < 0) {
      WARN("Failed to read /proc/stat, errno= %" WARN_PRIdPTR "\n",
           (signed_word)errno);
      close(f);
      return 1;
    }
    stat_buf[len] = '\0'; /* to avoid potential buffer overrun by atoi() */
    close(f);

    result = 1;
        /* Some old kernels only have a single "cpu nnnn ..."   */
        /* entry in /proc/stat.  We identify those as           */
        /* uniprocessors.                                       */

    for (i = 0; i < len - 4; ++i) {
      if (stat_buf[i] == '\n' && stat_buf[i+1] == 'c'
          && stat_buf[i+2] == 'p' && stat_buf[i+3] == 'u') {
        int cpu_no = atoi(&stat_buf[i + 4]);
        if (cpu_no >= result)
          result = cpu_no + 1;
      }
    }
    return result;
  }

#elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DGUX386_THREADS)
  /* Return the number of processors, or i <= 0 if it can't be determined. */
  STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_get_nprocs(void)
  {
    int numCpus;
    struct dg_sys_info_pm_info pm_sysinfo;
    int status = 0;

    status = dg_sys_info((long int *) &pm_sysinfo,
        DG_SYS_INFO_PM_INFO_TYPE, DG_SYS_INFO_PM_CURRENT_VERSION);
    if (status < 0) {
       /* set -1 for error */
       numCpus = -1;
    } else {
      /* Active CPUs */
      numCpus = pm_sysinfo.idle_vp_count;
    }
    return numCpus;
  }

#elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_FREEBSD_THREADS) \
      || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NETBSD_THREADS) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_OPENBSD_THREADS)
  STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_get_nprocs(void)
  {
    int mib[] = {CTL_HW,HW_NCPU};
    int res;
    size_t len = sizeof(res);

    sysctl(mib, sizeof(mib)/sizeof(int), &res, &len, NULL, 0);
    return res;
  }

#else
  /* E.g., MANAGED_STACK_ADDRESS_BOEHM_GC_RTEMS_PTHREADS */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_get_nprocs() 1 /* not implemented */
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_LINUX_THREADS && !MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS && ... */

#if defined(ARM32) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_LINUX_THREADS) && !defined(NACL)
  /* Some buggy Linux/arm kernels show only non-sleeping CPUs in        */
  /* /proc/stat (and /proc/cpuinfo), so another data system source is   */
  /* tried first.  Result <= 0 on error.                                */
  STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_get_nprocs_present(void)
  {
    char stat_buf[16];
    int f;
    int len;

    f = open("/sys/devices/system/cpu/present", O_RDONLY);
    if (f < 0)
      return -1; /* cannot open the file */

    len = STAT_READ(f, stat_buf, sizeof(stat_buf));
    close(f);

    /* Recognized file format: "0\n" or "0-<max_cpu_id>\n"      */
    /* The file might probably contain a comma-separated list   */
    /* but we do not need to handle it (just silently ignore).  */
    if (len < 2 || stat_buf[0] != '0' || stat_buf[len - 1] != '\n') {
      return 0; /* read error or unrecognized content */
    } else if (len == 2) {
      return 1; /* an uniprocessor */
    } else if (stat_buf[1] != '-') {
      return 0; /* unrecognized content */
    }

    stat_buf[len - 1] = '\0'; /* terminate the string */
    return atoi(&stat_buf[2]) + 1; /* skip "0-" and parse max_cpu_num */
  }
#endif /* ARM32 && MANAGED_STACK_ADDRESS_BOEHM_GC_LINUX_THREADS && !NACL */

#if defined(CAN_HANDLE_FORK) && defined(THREAD_SANITIZER)
# include "private/gc_pmark.h" /* for MS_NONE */

  /* Workaround for TSan which does not notice that the GC lock */
  /* is acquired in fork_prepare_proc().                        */
  MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD
  static MANAGED_STACK_ADDRESS_BOEHM_GC_bool collection_in_progress(void)
  {
    return MANAGED_STACK_ADDRESS_BOEHM_GC_mark_state != MS_NONE;
  }
#else
# define collection_in_progress() MANAGED_STACK_ADDRESS_BOEHM_GC_collection_in_progress()
#endif

/* We hold the GC lock.  Wait until an in-progress GC has finished.     */
/* Repeatedly releases the GC lock in order to wait.                    */
/* If wait_for_all is true, then we exit with the GC lock held and no   */
/* collection in progress; otherwise we just wait for the current GC    */
/* to finish.                                                           */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_wait_for_gc_completion(MANAGED_STACK_ADDRESS_BOEHM_GC_bool wait_for_all)
{
# if !defined(THREAD_SANITIZER) || !defined(CAN_CALL_ATFORK)
    /* MANAGED_STACK_ADDRESS_BOEHM_GC_lock_holder is accessed with the lock held, so there is no    */
    /* data race actually (unlike what is reported by TSan).            */
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
# endif
  ASSERT_CANCEL_DISABLED();
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL
    (void)wait_for_all;
# else
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_incremental && collection_in_progress()) {
        word old_gc_no = MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no;

        /* Make sure that no part of our stack is still on the mark     */
        /* stack, since it's about to be unmapped.                      */
#       ifdef LINT2
          /* Note: do not transform this if-do-while construction into  */
          /* a single while statement because it might cause some       */
          /* static code analyzers to report a false positive (FP)      */
          /* code defect about missing unlock after lock.               */
#       endif
        do {
            ENTER_GC();
            MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(!MANAGED_STACK_ADDRESS_BOEHM_GC_in_thread_creation);
            MANAGED_STACK_ADDRESS_BOEHM_GC_in_thread_creation = TRUE;
            MANAGED_STACK_ADDRESS_BOEHM_GC_collect_a_little_inner(1);
            MANAGED_STACK_ADDRESS_BOEHM_GC_in_thread_creation = FALSE;
            EXIT_GC();

            UNLOCK();
#           ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS
              Sleep(0);
#           else
              sched_yield();
#           endif
            LOCK();
        } while (MANAGED_STACK_ADDRESS_BOEHM_GC_incremental && collection_in_progress()
                 && (wait_for_all || old_gc_no == MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no));
    }
# endif
}

#ifdef CAN_HANDLE_FORK

  /* Procedures called before and after a fork.  The goal here is to    */
  /* make it safe to call MANAGED_STACK_ADDRESS_BOEHM_GC_malloc() in a forked child.  It is unclear */
  /* that is attainable, since the single UNIX spec seems to imply that */
  /* one should only call async-signal-safe functions, and we probably  */
  /* cannot quite guarantee that.  But we give it our best shot.  (That */
  /* same spec also implies that it is not safe to call the system      */
  /* malloc between fork and exec.  Thus we're doing no worse than it.) */

  IF_CANCEL(static int fork_cancel_state;) /* protected by allocation lock */

# ifdef PARALLEL_MARK
#   ifdef THREAD_SANITIZER
#     if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) && defined(CAN_CALL_ATFORK)
        STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_generic_lock(pthread_mutex_t *);
#     endif
      MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD
      static void wait_for_reclaim_atfork(void);
#   else
#     define wait_for_reclaim_atfork() MANAGED_STACK_ADDRESS_BOEHM_GC_wait_for_reclaim()
#   endif
# endif /* PARALLEL_MARK */

  /* Prevent TSan false positive about the race during items removal    */
  /* from MANAGED_STACK_ADDRESS_BOEHM_GC_threads.  (The race cannot happen since only one thread    */
  /* survives in the child.)                                            */
# ifdef CAN_CALL_ATFORK
    MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD
# endif
  static void store_to_threads_table(int hv, MANAGED_STACK_ADDRESS_BOEHM_GC_thread me)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_threads[hv] = me;
  }

  /* Remove all entries from the MANAGED_STACK_ADDRESS_BOEHM_GC_threads table, except the one for   */
  /* the current thread.  We need to do this in the child process after */
  /* a fork(), since only the current thread survives in the child.     */
  STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_remove_all_threads_but_me(void)
  {
    int hv;
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread me = NULL;
    pthread_t self = pthread_self(); /* same as in parent */
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS
#     define pthread_id id
#   endif

    for (hv = 0; hv < THREAD_TABLE_SZ; ++hv) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_thread p, next;

      for (p = MANAGED_STACK_ADDRESS_BOEHM_GC_threads[hv]; p != NULL; p = next) {
        next = p -> tm.next;
        if (THREAD_EQUAL(p -> pthread_id, self)
            && me == NULL) { /* ignore dead threads with the same id */
          me = p;
          p -> tm.next = NULL;
        } else {
#         ifdef THREAD_LOCAL_ALLOC
            if (!KNOWN_FINISHED(p)) {
              /* Cannot call MANAGED_STACK_ADDRESS_BOEHM_GC_destroy_thread_local here.  The free    */
              /* lists may be in an inconsistent state (as thread p may */
              /* be updating one of the lists by MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_many */
              /* or MANAGED_STACK_ADDRESS_BOEHM_GC_FAST_MALLOC_GRANS when fork is invoked).         */
              /* This should not be a problem because the lost elements */
              /* of the free lists will be collected during GC.         */
              MANAGED_STACK_ADDRESS_BOEHM_GC_remove_specific_after_fork(MANAGED_STACK_ADDRESS_BOEHM_GC_thread_key, p -> pthread_id);
            }
#         endif
          /* TODO: To avoid TSan hang (when updating MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_freed),   */
          /* we just skip explicit freeing of MANAGED_STACK_ADDRESS_BOEHM_GC_threads entries.       */
#         if !defined(THREAD_SANITIZER) || !defined(CAN_CALL_ATFORK)
            if (p != &first_thread) {
              /* TODO: Should call mach_port_deallocate? */
              MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(p -> crtn != &first_crtn);
              MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_FREE(p -> crtn);
              MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_FREE(p);
            }
#         endif
        }
      }
      store_to_threads_table(hv, NULL);
    }

#   ifdef LINT2
      if (NULL == me) ABORT("Current thread is not found after fork");
#   else
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(me != NULL);
#   endif
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS
      /* Update Win32 thread id and handle.     */
      me -> id = thread_id_self(); /* differs from that in parent */
#     ifndef MSWINCE
        if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                        GetCurrentProcess(), (HANDLE *)&(me -> handle),
                        0 /* dwDesiredAccess */, FALSE /* bInheritHandle */,
                        DUPLICATE_SAME_ACCESS))
          ABORT("DuplicateHandle failed");
#     endif
#   endif
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS
      /* Update thread Id after fork (it is OK to call  */
      /* MANAGED_STACK_ADDRESS_BOEHM_GC_destroy_thread_local and MANAGED_STACK_ADDRESS_BOEHM_GC_free_inner      */
      /* before update).                                */
      me -> mach_thread = mach_thread_self();
#   endif
#   ifdef USE_TKILL_ON_ANDROID
      me -> kernel_id = gettid();
#   endif

    /* Put "me" back to MANAGED_STACK_ADDRESS_BOEHM_GC_threads.     */
    store_to_threads_table(THREAD_TABLE_INDEX(me -> id), me);

#   if defined(THREAD_LOCAL_ALLOC) && !defined(USE_CUSTOM_SPECIFIC)
      /* Some TLS implementations (e.g., on Cygwin) might be not        */
      /* fork-friendly, so we re-assign thread-local pointer to 'tlfs'  */
      /* for safety instead of the assertion check (again, it is OK to  */
      /* call MANAGED_STACK_ADDRESS_BOEHM_GC_destroy_thread_local and MANAGED_STACK_ADDRESS_BOEHM_GC_free_inner before).        */
      {
        int res = MANAGED_STACK_ADDRESS_BOEHM_GC_setspecific(MANAGED_STACK_ADDRESS_BOEHM_GC_thread_key, &me->tlfs);

        if (COVERT_DATAFLOW(res) != 0)
          ABORT("MANAGED_STACK_ADDRESS_BOEHM_GC_setspecific failed (in child)");
      }
#   endif
#   undef pthread_id
  }

  /* Called before a fork().    */
# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) && defined(CAN_CALL_ATFORK)
    /* MANAGED_STACK_ADDRESS_BOEHM_GC_lock_holder is updated safely (no data race actually).        */
    MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD
# endif
  static void fork_prepare_proc(void)
  {
    /* Acquire all relevant locks, so that after releasing the locks    */
    /* the child will see a consistent state in which monitor           */
    /* invariants hold.  Unfortunately, we can't acquire libc locks     */
    /* we might need, and there seems to be no guarantee that libc      */
    /* must install a suitable fork handler.                            */
    /* Wait for an ongoing GC to finish, since we can't finish it in    */
    /* the (one remaining thread in) the child.                         */

      LOCK();
      DISABLE_CANCEL(fork_cancel_state);
                /* Following waits may include cancellation points. */
#     ifdef PARALLEL_MARK
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_parallel)
          wait_for_reclaim_atfork();
#     endif
      MANAGED_STACK_ADDRESS_BOEHM_GC_wait_for_gc_completion(TRUE);
#     ifdef PARALLEL_MARK
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_parallel) {
#         if defined(THREAD_SANITIZER) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) \
             && defined(CAN_CALL_ATFORK)
            /* Prevent TSan false positive about the data race  */
            /* when updating MANAGED_STACK_ADDRESS_BOEHM_GC_mark_lock_holder.               */
            MANAGED_STACK_ADDRESS_BOEHM_GC_generic_lock(&mark_mutex);
#         else
            MANAGED_STACK_ADDRESS_BOEHM_GC_acquire_mark_lock();
#         endif
        }
#     endif
      MANAGED_STACK_ADDRESS_BOEHM_GC_acquire_dirty_lock();
  }

  /* Called in parent after a fork() (even if the latter failed).       */
# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) && defined(CAN_CALL_ATFORK)
    MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD
# endif
  static void fork_parent_proc(void)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_release_dirty_lock();
#   ifdef PARALLEL_MARK
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_parallel) {
#       if defined(THREAD_SANITIZER) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) \
           && defined(CAN_CALL_ATFORK)
          /* To match that in fork_prepare_proc. */
          (void)pthread_mutex_unlock(&mark_mutex);
#       else
          MANAGED_STACK_ADDRESS_BOEHM_GC_release_mark_lock();
#       endif
      }
#   endif
    RESTORE_CANCEL(fork_cancel_state);
    UNLOCK();
  }

  /* Called in child after a fork().    */
# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) && defined(CAN_CALL_ATFORK)
    MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD
# endif
  static void fork_child_proc(void)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_release_dirty_lock();
#   ifdef PARALLEL_MARK
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_parallel) {
#       if defined(THREAD_SANITIZER) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) \
           && defined(CAN_CALL_ATFORK)
          (void)pthread_mutex_unlock(&mark_mutex);
#       else
          MANAGED_STACK_ADDRESS_BOEHM_GC_release_mark_lock();
#       endif
        /* Turn off parallel marking in the child, since we are probably  */
        /* just going to exec, and we would have to restart mark threads. */
        MANAGED_STACK_ADDRESS_BOEHM_GC_parallel = FALSE;
      }
#     ifdef THREAD_SANITIZER
        /* TSan does not support threads creation in the child process. */
        MANAGED_STACK_ADDRESS_BOEHM_GC_available_markers_m1 = 0;
#     endif
#   endif
    /* Clean up the thread table, so that just our thread is left.      */
    MANAGED_STACK_ADDRESS_BOEHM_GC_remove_all_threads_but_me();
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL
      MANAGED_STACK_ADDRESS_BOEHM_GC_dirty_update_child();
#   endif
    RESTORE_CANCEL(fork_cancel_state);
    UNLOCK();
    /* Even though after a fork the child only inherits the single      */
    /* thread that called the fork(), if another thread in the parent   */
    /* was attempting to lock the mutex while being held in             */
    /* fork_child_prepare(), the mutex will be left in an inconsistent  */
    /* state in the child after the UNLOCK.  This is the case, at       */
    /* least, in Mac OS X and leads to an unusable GC in the child      */
    /* which will block when attempting to perform any GC operation     */
    /* that acquires the allocation mutex.                              */
#   if defined(USE_PTHREAD_LOCKS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS)
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_DONT_HOLD_LOCK());
      /* Reinitialize the mutex.  It should be safe since we are        */
      /* running this in the child which only inherits a single thread. */
      /* mutex_destroy() may return EBUSY, which makes no sense, but    */
      /* that is the reason for the need of the reinitialization.       */
      /* Note: excluded for Cygwin as does not seem to be needed.       */
      (void)pthread_mutex_destroy(&MANAGED_STACK_ADDRESS_BOEHM_GC_allocate_ml);
      /* TODO: Probably some targets might need the default mutex       */
      /* attribute to be passed instead of NULL.                        */
      if (0 != pthread_mutex_init(&MANAGED_STACK_ADDRESS_BOEHM_GC_allocate_ml, NULL))
        ABORT("pthread_mutex_init failed (in child)");
#   endif
  }

  /* Routines for fork handling by client (no-op if pthread_atfork works). */
  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_atfork_prepare(void)
  {
    if (!EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized, TRUE)) MANAGED_STACK_ADDRESS_BOEHM_GC_init();
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS) && defined(MPROTECT_VDB)
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_auto_incremental) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(0 == MANAGED_STACK_ADDRESS_BOEHM_GC_handle_fork);
        ABORT("Unable to fork while mprotect_thread is running");
      }
#   endif
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_handle_fork <= 0)
      fork_prepare_proc();
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_atfork_parent(void)
  {
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_handle_fork <= 0)
      fork_parent_proc();
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_atfork_child(void)
  {
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_handle_fork <= 0)
      fork_child_proc();
  }

  /* Prepare for forks if requested.    */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER_WIN32THREAD void MANAGED_STACK_ADDRESS_BOEHM_GC_setup_atfork(void)
  {
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_handle_fork) {
#     ifdef CAN_CALL_ATFORK
        if (pthread_atfork(fork_prepare_proc, fork_parent_proc,
                           fork_child_proc) == 0) {
          /* Handlers successfully registered.  */
          MANAGED_STACK_ADDRESS_BOEHM_GC_handle_fork = 1;
        } else
#     endif
      /* else */ if (MANAGED_STACK_ADDRESS_BOEHM_GC_handle_fork != -1)
        ABORT("pthread_atfork failed");
    }
  }

#endif /* CAN_HANDLE_FORK */

#ifdef INCLUDE_LINUX_THREAD_DESCR
  __thread int MANAGED_STACK_ADDRESS_BOEHM_GC_dummy_thread_local;
#endif

#ifdef PARALLEL_MARK
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS
    static void setup_mark_lock(void);
# endif

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER_WIN32THREAD unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_required_markers_cnt = 0;
                        /* The default value (0) means the number of    */
                        /* markers should be selected automatically.    */

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_markers_count(unsigned markers)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_required_markers_cnt = markers < MAX_MARKERS ? markers : MAX_MARKERS;
  }
#endif /* PARALLEL_MARK */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_in_thread_creation = FALSE;
                                /* Protected by allocation lock. */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER_WIN32THREAD void MANAGED_STACK_ADDRESS_BOEHM_GC_record_stack_base(MANAGED_STACK_ADDRESS_BOEHM_GC_stack_context_t crtn,
                                               const struct MANAGED_STACK_ADDRESS_BOEHM_GC_stack_base *sb)
{
# if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS)
    crtn -> stack_ptr = (ptr_t)sb->mem_base;
# endif
  if ((crtn -> stack_end = (ptr_t)sb->mem_base) == NULL)
    ABORT("Bad stack base in MANAGED_STACK_ADDRESS_BOEHM_GC_register_my_thread");
# ifdef IA64
    crtn -> backing_store_end = (ptr_t)sb->reg_base;
# elif defined(I386) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS)
    crtn -> initial_stack_base = (ptr_t)sb->mem_base;
# endif
}

#if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_THREADS_DISCOVERY) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) \
    || !defined(DONT_USE_ATEXIT)
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER_WIN32THREAD thread_id_t MANAGED_STACK_ADDRESS_BOEHM_GC_main_thread_id;
#endif

#ifndef DONT_USE_ATEXIT
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_is_main_thread(void)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_thr_initialized);
    return THREAD_ID_EQUAL(MANAGED_STACK_ADDRESS_BOEHM_GC_main_thread_id, thread_id_self());
  }
#endif /* !DONT_USE_ATEXIT */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS

STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_thread MANAGED_STACK_ADDRESS_BOEHM_GC_register_my_thread_inner(const struct MANAGED_STACK_ADDRESS_BOEHM_GC_stack_base *sb,
                                             thread_id_t self_id)
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_thread me;

  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
  me = MANAGED_STACK_ADDRESS_BOEHM_GC_new_thread(self_id);
  me -> id = self_id;
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS
    me -> mach_thread = mach_thread_self();
# endif
  MANAGED_STACK_ADDRESS_BOEHM_GC_record_stack_base(me -> crtn, sb);
  return me;
}

  STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_nprocs = 1;
                        /* Number of processors.  We may not have       */
                        /* access to all of them, but this is as good   */
                        /* a guess as any ...                           */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_thr_init(void)
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(!MANAGED_STACK_ADDRESS_BOEHM_GC_thr_initialized);
  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((word)(&MANAGED_STACK_ADDRESS_BOEHM_GC_threads) % sizeof(word) == 0);
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS
    MANAGED_STACK_ADDRESS_BOEHM_GC_thr_initialized = TRUE;
# endif
# ifdef CAN_HANDLE_FORK
    MANAGED_STACK_ADDRESS_BOEHM_GC_setup_atfork();
# endif

# ifdef INCLUDE_LINUX_THREAD_DESCR
    /* Explicitly register the region including the address     */
    /* of a thread local variable.  This should include thread  */
    /* locals for the main thread, except for those allocated   */
    /* in response to dlopen calls.                             */
    {
      ptr_t thread_local_addr = (ptr_t)(&MANAGED_STACK_ADDRESS_BOEHM_GC_dummy_thread_local);
      ptr_t main_thread_start, main_thread_end;
      if (!MANAGED_STACK_ADDRESS_BOEHM_GC_enclosing_mapping(thread_local_addr, &main_thread_start,
                                &main_thread_end)) {
        ABORT("Failed to find mapping for main thread thread locals");
      } else {
        /* main_thread_start and main_thread_end are initialized.       */
        MANAGED_STACK_ADDRESS_BOEHM_GC_add_roots_inner(main_thread_start, main_thread_end, FALSE);
      }
    }
# endif

  /* Set MANAGED_STACK_ADDRESS_BOEHM_GC_nprocs and MANAGED_STACK_ADDRESS_BOEHM_GC_available_markers_m1. */
  {
    char * nprocs_string = GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_NPROCS");
    MANAGED_STACK_ADDRESS_BOEHM_GC_nprocs = -1;
    if (nprocs_string != NULL) MANAGED_STACK_ADDRESS_BOEHM_GC_nprocs = atoi(nprocs_string);
  }
  if (MANAGED_STACK_ADDRESS_BOEHM_GC_nprocs <= 0
#     if defined(ARM32) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_LINUX_THREADS) && !defined(NACL)
        && (MANAGED_STACK_ADDRESS_BOEHM_GC_nprocs = MANAGED_STACK_ADDRESS_BOEHM_GC_get_nprocs_present()) <= 1
                                /* Workaround for some Linux/arm kernels */
#     endif
      )
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_nprocs = MANAGED_STACK_ADDRESS_BOEHM_GC_get_nprocs();
  }
  if (MANAGED_STACK_ADDRESS_BOEHM_GC_nprocs <= 0) {
    WARN("MANAGED_STACK_ADDRESS_BOEHM_GC_get_nprocs() returned %" WARN_PRIdPTR "\n",
         (signed_word)MANAGED_STACK_ADDRESS_BOEHM_GC_nprocs);
    MANAGED_STACK_ADDRESS_BOEHM_GC_nprocs = 2; /* assume dual-core */
#   ifdef PARALLEL_MARK
      MANAGED_STACK_ADDRESS_BOEHM_GC_available_markers_m1 = 0; /* but use only one marker */
#   endif
  } else {
#   ifdef PARALLEL_MARK
      {
        char * markers_string = GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_MARKERS");
        int markers = MANAGED_STACK_ADDRESS_BOEHM_GC_required_markers_cnt;

        if (markers_string != NULL) {
          markers = atoi(markers_string);
          if (markers <= 0 || markers > MAX_MARKERS) {
            WARN("Too big or invalid number of mark threads: %" WARN_PRIdPTR
                 "; using maximum threads\n", (signed_word)markers);
            markers = MAX_MARKERS;
          }
        } else if (0 == markers) {
          /* Unless the client sets the desired number of       */
          /* parallel markers, it is determined based on the    */
          /* number of CPU cores.                               */
          markers = MANAGED_STACK_ADDRESS_BOEHM_GC_nprocs;
#         if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_MIN_MARKERS) && !defined(CPPCHECK)
            /* This is primarily for targets without getenv().  */
            if (markers < MANAGED_STACK_ADDRESS_BOEHM_GC_MIN_MARKERS)
              markers = MANAGED_STACK_ADDRESS_BOEHM_GC_MIN_MARKERS;
#         endif
          if (markers > MAX_MARKERS)
            markers = MAX_MARKERS; /* silently limit the value */
        }
        MANAGED_STACK_ADDRESS_BOEHM_GC_available_markers_m1 = markers - 1;
      }
#   endif
  }
  MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF("Number of processors: %d\n", MANAGED_STACK_ADDRESS_BOEHM_GC_nprocs);

# if defined(BASE_ATOMIC_OPS_EMULATED) && defined(SIGNAL_BASED_STOP_WORLD)
    /* Ensure the process is running on just one CPU core.      */
    /* This is needed because the AO primitives emulated with   */
    /* locks cannot be used inside signal handlers.             */
    {
      cpu_set_t mask;
      int cpu_set_cnt = 0;
      int cpu_lowest_set = 0;
#     ifdef RANDOM_ONE_CPU_CORE
        int cpu_highest_set = 0;
#     endif
      int i = MANAGED_STACK_ADDRESS_BOEHM_GC_nprocs > 1 ? MANAGED_STACK_ADDRESS_BOEHM_GC_nprocs : 2; /* check at least 2 cores */

      if (sched_getaffinity(0 /* current process */,
                            sizeof(mask), &mask) == -1)
        ABORT_ARG1("sched_getaffinity failed", ": errno= %d", errno);
      while (i-- > 0)
        if (CPU_ISSET(i, &mask)) {
#         ifdef RANDOM_ONE_CPU_CORE
            if (i + 1 != cpu_lowest_set) cpu_highest_set = i;
#         endif
          cpu_lowest_set = i;
          cpu_set_cnt++;
        }
      if (0 == cpu_set_cnt)
        ABORT("sched_getaffinity returned empty mask");
      if (cpu_set_cnt > 1) {
#       ifdef RANDOM_ONE_CPU_CORE
          if (cpu_lowest_set < cpu_highest_set) {
            /* Pseudo-randomly adjust the bit to set among valid ones.  */
            cpu_lowest_set += (unsigned)getpid() %
                                (cpu_highest_set - cpu_lowest_set + 1);
          }
#       endif
        CPU_ZERO(&mask);
        CPU_SET(cpu_lowest_set, &mask); /* select just one CPU */
        if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
          ABORT_ARG1("sched_setaffinity failed", ": errno= %d", errno);
        WARN("CPU affinity mask is set to %p\n", (word)1 << cpu_lowest_set);
      }
    }
# endif /* BASE_ATOMIC_OPS_EMULATED */

# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS
    MANAGED_STACK_ADDRESS_BOEHM_GC_stop_init();
# endif

# ifdef PARALLEL_MARK
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_available_markers_m1 <= 0) {
      /* Disable parallel marking.      */
      MANAGED_STACK_ADDRESS_BOEHM_GC_parallel = FALSE;
      MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF(
                "Single marker thread, turning off parallel marking\n");
    } else {
      setup_mark_lock();
    }
# endif

  /* Add the initial thread, so we can stop it. */
  {
    struct MANAGED_STACK_ADDRESS_BOEHM_GC_stack_base sb;
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread me;
    thread_id_t self_id = thread_id_self();

    sb.mem_base = MANAGED_STACK_ADDRESS_BOEHM_GC_stackbottom;
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(sb.mem_base != NULL);
#   ifdef IA64
      sb.reg_base = MANAGED_STACK_ADDRESS_BOEHM_GC_register_stackbottom;
#   elif defined(E2K)
      sb.reg_base = NULL;
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_self_thread_inner());
    me = MANAGED_STACK_ADDRESS_BOEHM_GC_register_my_thread_inner(&sb, self_id);
#   ifndef DONT_USE_ATEXIT
      MANAGED_STACK_ADDRESS_BOEHM_GC_main_thread_id = self_id;
#   endif
    me -> flags = DETACHED;
  }
}

#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS */

/* Perform all initializations, including those that may require        */
/* allocation, e.g. initialize thread local free lists if used.         */
/* Must be called before a thread is created.                           */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_init_parallel(void)
{
# ifdef THREAD_LOCAL_ALLOC
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread me;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized);
    LOCK();
    me = MANAGED_STACK_ADDRESS_BOEHM_GC_self_thread_inner();
    MANAGED_STACK_ADDRESS_BOEHM_GC_init_thread_local(&me->tlfs);
    UNLOCK();
# endif
# if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_THREADS_DISCOVERY) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS)
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_win32_dll_threads) {
      set_need_to_lock();
        /* Cannot intercept thread creation.  Hence we don't know if    */
        /* other threads exist.  However, client is not allowed to      */
        /* create other threads before collector initialization.        */
        /* Thus it's OK not to lock before this.                        */
    }
# endif
}

#if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_SIGMASK) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS)
  MANAGED_STACK_ADDRESS_BOEHM_GC_API int WRAP_FUNC(pthread_sigmask)(int how, const sigset_t *set,
                                        sigset_t *oset)
  {
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS
      /* pthreads-win32 does not support sigmask.       */
      /* So, nothing required here...                   */
#   else
      sigset_t fudged_set;

      INIT_REAL_SYMS();
      if (EXPECT(set != NULL, TRUE)
          && (how == SIG_BLOCK || how == SIG_SETMASK)) {
        int sig_suspend = MANAGED_STACK_ADDRESS_BOEHM_GC_get_suspend_signal();

        fudged_set = *set;
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(sig_suspend >= 0);
        if (sigdelset(&fudged_set, sig_suspend) != 0)
          ABORT("sigdelset failed");
        set = &fudged_set;
      }
#   endif
    return REAL_FUNC(pthread_sigmask)(how, set, oset);
  }
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_SIGMASK */

/* Wrapper for functions that are likely to block for an appreciable    */
/* length of time.                                                      */

#ifdef E2K
  /* Cannot be defined as a function because the stack-allocated buffer */
  /* (pointed to by bs_lo) should be preserved till completion of       */
  /* MANAGED_STACK_ADDRESS_BOEHM_GC_do_blocking_inner (or MANAGED_STACK_ADDRESS_BOEHM_GC_suspend_self_blocked).                 */
# define do_blocking_enter(pTopOfStackUnset, me)            \
        do {                                                \
          ptr_t bs_lo;                                      \
          size_t stack_size;                                \
          MANAGED_STACK_ADDRESS_BOEHM_GC_stack_context_t crtn = (me) -> crtn;           \
                                                            \
          *(pTopOfStackUnset) = FALSE;                      \
          crtn -> stack_ptr = MANAGED_STACK_ADDRESS_BOEHM_GC_approx_sp();               \
          MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NULL == crtn -> backing_store_end);     \
          GET_PROCEDURE_STACK_LOCAL(&bs_lo, &stack_size);   \
          crtn -> backing_store_end = bs_lo;                \
          crtn -> backing_store_ptr = bs_lo + stack_size;   \
          (me) -> flags |= DO_BLOCKING;                     \
        } while (0)

#else /* !E2K */
  static void do_blocking_enter(MANAGED_STACK_ADDRESS_BOEHM_GC_bool *pTopOfStackUnset, MANAGED_STACK_ADDRESS_BOEHM_GC_thread me)
  {
#   if defined(SPARC) || defined(IA64)
        ptr_t bs_hi = MANAGED_STACK_ADDRESS_BOEHM_GC_save_regs_in_stack();
        /* TODO: regs saving already done by MANAGED_STACK_ADDRESS_BOEHM_GC_with_callee_saves_pushed */
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_stack_context_t crtn = me -> crtn;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((me -> flags & DO_BLOCKING) == 0);
    *pTopOfStackUnset = FALSE;
#   ifdef SPARC
        crtn -> stack_ptr = bs_hi;
#   else
        crtn -> stack_ptr = MANAGED_STACK_ADDRESS_BOEHM_GC_approx_sp();
#   endif
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS) && !defined(DARWIN_DONT_PARSE_STACK)
        if (NULL == crtn -> topOfStack) {
            /* MANAGED_STACK_ADDRESS_BOEHM_GC_do_blocking_inner is not called recursively,  */
            /* so topOfStack should be computed now.            */
            *pTopOfStackUnset = TRUE;
            crtn -> topOfStack = MANAGED_STACK_ADDRESS_BOEHM_GC_FindTopOfStack(0);
        }
#   endif
#   ifdef IA64
        crtn -> backing_store_ptr = bs_hi;
#   endif
    me -> flags |= DO_BLOCKING;
    /* Save context here if we want to support precise stack marking.   */
  }
#endif /* !E2K */

static void do_blocking_leave(MANAGED_STACK_ADDRESS_BOEHM_GC_thread me, MANAGED_STACK_ADDRESS_BOEHM_GC_bool topOfStackUnset)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    me -> flags &= (unsigned char)~DO_BLOCKING;
#   ifdef E2K
      {
        MANAGED_STACK_ADDRESS_BOEHM_GC_stack_context_t crtn = me -> crtn;

        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(crtn -> backing_store_end != NULL);
        crtn -> backing_store_ptr = NULL;
        crtn -> backing_store_end = NULL;
      }
#   endif
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS) && !defined(DARWIN_DONT_PARSE_STACK)
        if (topOfStackUnset)
          me -> crtn -> topOfStack = NULL; /* make it unset again */
#   else
        (void)topOfStackUnset;
#   endif
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_do_blocking_inner(ptr_t data, void *context)
{
    struct blocking_data *d = (struct blocking_data *)data;
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread me;
    MANAGED_STACK_ADDRESS_BOEHM_GC_bool topOfStackUnset;

    UNUSED_ARG(context);
    LOCK();
    me = MANAGED_STACK_ADDRESS_BOEHM_GC_self_thread_inner();
    do_blocking_enter(&topOfStackUnset, me);
    UNLOCK();

    d -> client_data = (d -> fn)(d -> client_data);

    LOCK();   /* This will block if the world is stopped.       */
#   ifdef LINT2
      {
#        ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS
           MANAGED_STACK_ADDRESS_BOEHM_GC_thread saved_me = me;
#        endif

         /* The pointer to the GC thread descriptor should not be   */
         /* changed while the thread is registered but a static     */
         /* analysis tool might complain that this pointer value    */
         /* (obtained in the first locked section) is unreliable in */
         /* the second locked section.                              */
         me = MANAGED_STACK_ADDRESS_BOEHM_GC_self_thread_inner();
         MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(me == saved_me);
      }
#   endif
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ENABLE_SUSPEND_THREAD) && defined(SIGNAL_BASED_STOP_WORLD)
      /* Note: this code cannot be moved into do_blocking_leave()   */
      /* otherwise there could be a static analysis tool warning    */
      /* (false positive) about unlock without a matching lock.     */
      while (EXPECT((me -> ext_suspend_cnt & 1) != 0, FALSE)) {
        word suspend_cnt = (word)(me -> ext_suspend_cnt);
                        /* read suspend counter (number) before unlocking */

        UNLOCK();
        MANAGED_STACK_ADDRESS_BOEHM_GC_suspend_self_inner(me, suspend_cnt);
        LOCK();
      }
#   endif
    do_blocking_leave(me, topOfStackUnset);
    UNLOCK();
}

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ENABLE_SUSPEND_THREAD) && defined(SIGNAL_BASED_STOP_WORLD)
  /* Similar to MANAGED_STACK_ADDRESS_BOEHM_GC_do_blocking_inner() but assuming the GC lock is held */
  /* and fn is MANAGED_STACK_ADDRESS_BOEHM_GC_suspend_self_inner.                                   */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_suspend_self_blocked(ptr_t thread_me, void *context)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread me = (MANAGED_STACK_ADDRESS_BOEHM_GC_thread)thread_me;
    MANAGED_STACK_ADDRESS_BOEHM_GC_bool topOfStackUnset;

    UNUSED_ARG(context);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    do_blocking_enter(&topOfStackUnset, me);
    while ((me -> ext_suspend_cnt & 1) != 0) {
      word suspend_cnt = (word)(me -> ext_suspend_cnt);

      UNLOCK();
      MANAGED_STACK_ADDRESS_BOEHM_GC_suspend_self_inner(me, suspend_cnt);
      LOCK();
    }
    do_blocking_leave(me, topOfStackUnset);
  }
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_ENABLE_SUSPEND_THREAD */

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_stackbottom(void *gc_thread_handle,
                                       const struct MANAGED_STACK_ADDRESS_BOEHM_GC_stack_base *sb)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread t = (MANAGED_STACK_ADDRESS_BOEHM_GC_thread)gc_thread_handle;
    MANAGED_STACK_ADDRESS_BOEHM_GC_stack_context_t crtn;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(sb -> mem_base != NULL);
    if (!EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized, TRUE)) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NULL == t);
      /* Alter the stack bottom of the primordial thread.       */
      MANAGED_STACK_ADDRESS_BOEHM_GC_stackbottom = (char*)(sb -> mem_base);
#     ifdef IA64
        MANAGED_STACK_ADDRESS_BOEHM_GC_register_stackbottom = (ptr_t)(sb -> reg_base);
#     endif
      return;
    }

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    if (NULL == t) /* current thread? */
      t = MANAGED_STACK_ADDRESS_BOEHM_GC_self_thread_inner();
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(!KNOWN_FINISHED(t));
    crtn = t -> crtn;
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((t -> flags & DO_BLOCKING) == 0
              && NULL == crtn -> traced_stack_sect); /* for now */

    crtn -> stack_end = (ptr_t)(sb -> mem_base);
#   ifdef IA64
      crtn -> backing_store_end = (ptr_t)(sb -> reg_base);
#   endif
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS
      /* Reset the known minimum (hottest address in the stack). */
      crtn -> last_stack_min = ADDR_LIMIT;
#   endif
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_my_stackbottom(struct MANAGED_STACK_ADDRESS_BOEHM_GC_stack_base *sb)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread me;
    MANAGED_STACK_ADDRESS_BOEHM_GC_stack_context_t crtn;

    LOCK();
    me = MANAGED_STACK_ADDRESS_BOEHM_GC_self_thread_inner();
    /* The thread is assumed to be registered.  */
    crtn = me -> crtn;
    sb -> mem_base = crtn -> stack_end;
#   ifdef E2K
      sb -> reg_base = NULL;
#   elif defined(IA64)
      sb -> reg_base = crtn -> backing_store_end;
#   endif
    UNLOCK();
    return (void *)me; /* gc_thread_handle */
}

/* MANAGED_STACK_ADDRESS_BOEHM_GC_call_with_gc_active() has the opposite to MANAGED_STACK_ADDRESS_BOEHM_GC_do_blocking()        */
/* functionality.  It might be called from a user function invoked by   */
/* MANAGED_STACK_ADDRESS_BOEHM_GC_do_blocking() to temporarily back allow calling any GC function   */
/* and/or manipulating pointers to the garbage collected heap.          */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_call_with_gc_active(MANAGED_STACK_ADDRESS_BOEHM_GC_fn_type fn,
                                             void * client_data)
{
    struct MANAGED_STACK_ADDRESS_BOEHM_GC_traced_stack_sect_s stacksect;
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread me;
    MANAGED_STACK_ADDRESS_BOEHM_GC_stack_context_t crtn;
    ptr_t stack_end;
#   ifdef E2K
      ptr_t saved_bs_ptr, saved_bs_end;
#   endif

    LOCK();   /* This will block if the world is stopped.       */
    me = MANAGED_STACK_ADDRESS_BOEHM_GC_self_thread_inner();
    crtn = me -> crtn;

    /* Adjust our stack bottom value (this could happen unless  */
    /* MANAGED_STACK_ADDRESS_BOEHM_GC_get_stack_base() was used which returned MANAGED_STACK_ADDRESS_BOEHM_GC_SUCCESS). */
    stack_end = crtn -> stack_end; /* read of a volatile field */
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(stack_end != NULL);
    if ((word)stack_end HOTTER_THAN (word)(&stacksect)) {
      crtn -> stack_end = (ptr_t)(&stacksect);
#     if defined(I386) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS)
        crtn -> initial_stack_base = (ptr_t)(&stacksect);
#     endif
    }

    if ((me -> flags & DO_BLOCKING) == 0) {
      /* We are not inside MANAGED_STACK_ADDRESS_BOEHM_GC_do_blocking() - do nothing more.  */
      UNLOCK();
      client_data = fn(client_data);
      /* Prevent treating the above as a tail call.     */
      MANAGED_STACK_ADDRESS_BOEHM_GC_noop1(COVERT_DATAFLOW(&stacksect));
      return client_data; /* result */
    }

#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ENABLE_SUSPEND_THREAD) && defined(SIGNAL_BASED_STOP_WORLD)
      while (EXPECT((me -> ext_suspend_cnt & 1) != 0, FALSE)) {
        word suspend_cnt = (word)(me -> ext_suspend_cnt);
        UNLOCK();
        MANAGED_STACK_ADDRESS_BOEHM_GC_suspend_self_inner(me, suspend_cnt);
        LOCK();
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(me -> crtn == crtn);
      }
#   endif

    /* Setup new "stack section".       */
    stacksect.saved_stack_ptr = crtn -> stack_ptr;
#   ifdef IA64
      /* This is the same as in MANAGED_STACK_ADDRESS_BOEHM_GC_call_with_stack_base().      */
      stacksect.backing_store_end = MANAGED_STACK_ADDRESS_BOEHM_GC_save_regs_in_stack();
      /* Unnecessarily flushes register stack,          */
      /* but that probably doesn't hurt.                */
      stacksect.saved_backing_store_ptr = crtn -> backing_store_ptr;
#   elif defined(E2K)
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(crtn -> backing_store_end != NULL);
      saved_bs_end = crtn -> backing_store_end;
      saved_bs_ptr = crtn -> backing_store_ptr;
      crtn -> backing_store_ptr = NULL;
      crtn -> backing_store_end = NULL;
#   endif
    stacksect.prev = crtn -> traced_stack_sect;
    me -> flags &= (unsigned char)~DO_BLOCKING;
    crtn -> traced_stack_sect = &stacksect;

    UNLOCK();
    client_data = fn(client_data);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((me -> flags & DO_BLOCKING) == 0);

    /* Restore original "stack section".        */
#   ifdef E2K
      (void)MANAGED_STACK_ADDRESS_BOEHM_GC_save_regs_in_stack();
#   endif
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(me -> crtn == crtn);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(crtn -> traced_stack_sect == &stacksect);
#   ifdef CPPCHECK
      MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((word)(crtn -> traced_stack_sect));
#   endif
    crtn -> traced_stack_sect = stacksect.prev;
#   ifdef IA64
      crtn -> backing_store_ptr = stacksect.saved_backing_store_ptr;
#   elif defined(E2K)
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NULL == crtn -> backing_store_end);
      crtn -> backing_store_end = saved_bs_end;
      crtn -> backing_store_ptr = saved_bs_ptr;
#   endif
    me -> flags |= DO_BLOCKING;
    crtn -> stack_ptr = stacksect.saved_stack_ptr;
    UNLOCK();
    return client_data; /* result */
}

STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_unregister_my_thread_inner(MANAGED_STACK_ADDRESS_BOEHM_GC_thread me)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
#   ifdef DEBUG_THREADS
      MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Unregistering thread %p, gc_thread= %p, n_threads= %d\n",
                    (void *)(signed_word)(me -> id), (void *)me,
                    MANAGED_STACK_ADDRESS_BOEHM_GC_count_threads());
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(!KNOWN_FINISHED(me));
#   if defined(THREAD_LOCAL_ALLOC)
      MANAGED_STACK_ADDRESS_BOEHM_GC_destroy_thread_local(&me->tlfs);
#   endif
#   ifdef NACL
      MANAGED_STACK_ADDRESS_BOEHM_GC_nacl_shutdown_gc_thread();
#   endif
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS
#     if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_HAVE_PTHREAD_EXIT) || !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_CANCEL)
        /* Handle DISABLED_GC flag which is set by the  */
        /* intercepted pthread_cancel or pthread_exit.  */
        if ((me -> flags & DISABLED_GC) != 0) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc--;
        }
#     endif
      if ((me -> flags & DETACHED) == 0) {
          me -> flags |= FINISHED;
      } else
#   endif
    /* else */ {
      MANAGED_STACK_ADDRESS_BOEHM_GC_delete_thread(me);
    }
#   if defined(THREAD_LOCAL_ALLOC)
      /* It is required to call remove_specific defined in specific.c. */
      MANAGED_STACK_ADDRESS_BOEHM_GC_remove_specific(MANAGED_STACK_ADDRESS_BOEHM_GC_thread_key);
#   endif
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_unregister_my_thread(void)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread me;
    IF_CANCEL(int cancel_state;)

    /* Client should not unregister the thread explicitly if it */
    /* is registered by DllMain, except for the main thread.    */
#   if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_THREADS_DISCOVERY) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS)
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(!MANAGED_STACK_ADDRESS_BOEHM_GC_win32_dll_threads
                || THREAD_ID_EQUAL(MANAGED_STACK_ADDRESS_BOEHM_GC_main_thread_id, thread_id_self()));
#   endif

    LOCK();
    DISABLE_CANCEL(cancel_state);
    /* Wait for any GC that may be marking from our stack to    */
    /* complete before we remove this thread.                   */
    MANAGED_STACK_ADDRESS_BOEHM_GC_wait_for_gc_completion(FALSE);
    me = MANAGED_STACK_ADDRESS_BOEHM_GC_self_thread_inner();
#   ifdef DEBUG_THREADS
        MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf(
                "Called MANAGED_STACK_ADDRESS_BOEHM_GC_unregister_my_thread on %p, gc_thread= %p\n",
                (void *)(signed_word)thread_id_self(), (void *)me);
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(THREAD_ID_EQUAL(me -> id, thread_id_self()));
    MANAGED_STACK_ADDRESS_BOEHM_GC_unregister_my_thread_inner(me);
    RESTORE_CANCEL(cancel_state);
    UNLOCK();
    return MANAGED_STACK_ADDRESS_BOEHM_GC_SUCCESS;
}

#if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_CANCEL) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS)
  /* We should deal with the fact that apparently on Solaris and,       */
  /* probably, on some Linux we can't collect while a thread is         */
  /* exiting, since signals aren't handled properly.  This currently    */
  /* gives rise to deadlocks.  The only workaround seen is to intercept */
  /* pthread_cancel() and pthread_exit(), and disable the collections   */
  /* until the thread exit handler is called.  That's ugly, because we  */
  /* risk growing the heap unnecessarily. But it seems that we don't    */
  /* really have an option in that the process is not in a fully        */
  /* functional state while a thread is exiting.                        */
  MANAGED_STACK_ADDRESS_BOEHM_GC_API int WRAP_FUNC(pthread_cancel)(pthread_t thread)
  {
#   ifdef CANCEL_SAFE
      MANAGED_STACK_ADDRESS_BOEHM_GC_thread t;
#   endif

    INIT_REAL_SYMS();
#   ifdef CANCEL_SAFE
      LOCK();
      t = MANAGED_STACK_ADDRESS_BOEHM_GC_lookup_by_pthread(thread);
      /* We test DISABLED_GC because pthread_exit could be called at    */
      /* the same time.  (If t is NULL then pthread_cancel should       */
      /* return ESRCH.)                                                 */
      if (t != NULL && (t -> flags & DISABLED_GC) == 0) {
        t -> flags |= DISABLED_GC;
        MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc++;
      }
      UNLOCK();
#   endif
    return REAL_FUNC(pthread_cancel)(thread);
  }
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_CANCEL */

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_HAVE_PTHREAD_EXIT
  MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_EXIT_ATTRIBUTE void WRAP_FUNC(pthread_exit)(void *retval)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread me;

    INIT_REAL_SYMS();
    LOCK();
    me = MANAGED_STACK_ADDRESS_BOEHM_GC_self_thread_inner();
    /* We test DISABLED_GC because someone else could call    */
    /* pthread_cancel at the same time.                       */
    if (me != NULL && (me -> flags & DISABLED_GC) == 0) {
      me -> flags |= DISABLED_GC;
      MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc++;
    }
    UNLOCK();

    REAL_FUNC(pthread_exit)(retval);
  }
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_HAVE_PTHREAD_EXIT */

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_allow_register_threads(void)
{
  /* Check GC is initialized and the current thread is registered.  */
  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_self_thread() != NULL);

  INIT_REAL_SYMS(); /* to initialize symbols while single-threaded */
  MANAGED_STACK_ADDRESS_BOEHM_GC_start_mark_threads();
  set_need_to_lock();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_register_my_thread(const struct MANAGED_STACK_ADDRESS_BOEHM_GC_stack_base *sb)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread me;

    if (MANAGED_STACK_ADDRESS_BOEHM_GC_need_to_lock == FALSE)
        ABORT("Threads explicit registering is not previously enabled");

    /* We lock here, since we want to wait for an ongoing GC.   */
    LOCK();
    me = MANAGED_STACK_ADDRESS_BOEHM_GC_self_thread_inner();
    if (EXPECT(NULL == me, TRUE)) {
      me = MANAGED_STACK_ADDRESS_BOEHM_GC_register_my_thread_inner(sb, thread_id_self());
#     ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS
#       ifdef CPPCHECK
          MANAGED_STACK_ADDRESS_BOEHM_GC_noop1(me -> flags);
#       endif
        /* Treat as detached, since we do not need to worry about       */
        /* pointer results.                                             */
        me -> flags |= DETACHED;
#     else
        (void)me;
#     endif
    } else
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS
      /* else */ if (KNOWN_FINISHED(me)) {
        /* This code is executed when a thread is registered from the   */
        /* client thread key destructor.                                */
#       ifdef NACL
          MANAGED_STACK_ADDRESS_BOEHM_GC_nacl_initialize_gc_thread(me);
#       endif
#       ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS
          /* Reinitialize mach_thread to avoid thread_suspend fail      */
          /* with MACH_SEND_INVALID_DEST error.                         */
          me -> mach_thread = mach_thread_self();
#       endif
        MANAGED_STACK_ADDRESS_BOEHM_GC_record_stack_base(me -> crtn, sb);
        me -> flags &= (unsigned char)~FINISHED; /* but not DETACHED */
      } else
#   endif
    /* else */ {
        UNLOCK();
        return MANAGED_STACK_ADDRESS_BOEHM_GC_DUPLICATE;
    }

#   ifdef THREAD_LOCAL_ALLOC
      MANAGED_STACK_ADDRESS_BOEHM_GC_init_thread_local(&me->tlfs);
#   endif
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_EXPLICIT_SIGNALS_UNBLOCK
      /* Since this could be executed from a thread destructor, */
      /* our signals might already be blocked.                  */
      MANAGED_STACK_ADDRESS_BOEHM_GC_unblock_gc_signals();
#   endif
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ENABLE_SUSPEND_THREAD) && defined(SIGNAL_BASED_STOP_WORLD)
      if (EXPECT((me -> ext_suspend_cnt & 1) != 0, FALSE)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_with_callee_saves_pushed(MANAGED_STACK_ADDRESS_BOEHM_GC_suspend_self_blocked, (ptr_t)me);
      }
#   endif
    UNLOCK();
    return MANAGED_STACK_ADDRESS_BOEHM_GC_SUCCESS;
}

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS) \
    && !defined(SN_TARGET_ORBIS) && !defined(SN_TARGET_PSP2)

  /* Called at thread exit.  Never called for main thread.      */
  /* That is OK, since it results in at most a tiny one-time    */
  /* leak.  And linuxthreads implementation does not reclaim    */
  /* the primordial (main) thread resources or id anyway.       */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER_PTHRSTART void MANAGED_STACK_ADDRESS_BOEHM_GC_thread_exit_proc(void *arg)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread me = (MANAGED_STACK_ADDRESS_BOEHM_GC_thread)arg;
    IF_CANCEL(int cancel_state;)

#   ifdef DEBUG_THREADS
        MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Called MANAGED_STACK_ADDRESS_BOEHM_GC_thread_exit_proc on %p, gc_thread= %p\n",
                      (void *)(signed_word)(me -> id), (void *)me);
#   endif
    LOCK();
    DISABLE_CANCEL(cancel_state);
    MANAGED_STACK_ADDRESS_BOEHM_GC_wait_for_gc_completion(FALSE);
    MANAGED_STACK_ADDRESS_BOEHM_GC_unregister_my_thread_inner(me);
    RESTORE_CANCEL(cancel_state);
    UNLOCK();
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API int WRAP_FUNC(pthread_join)(pthread_t thread, void **retval)
  {
    int result;
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread t;

    INIT_REAL_SYMS();
#   ifdef DEBUG_THREADS
      MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("thread %p is joining thread %p\n",
                    (void *)MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_PTRVAL(pthread_self()),
                    (void *)MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_PTRVAL(thread));
#   endif

    /* After the join, thread id may have been recycled.                */
    LOCK();
    t = (MANAGED_STACK_ADDRESS_BOEHM_GC_thread)COVERT_DATAFLOW(MANAGED_STACK_ADDRESS_BOEHM_GC_lookup_by_pthread(thread));
      /* This is guaranteed to be the intended one, since the thread id */
      /* cannot have been recycled by pthreads.                         */
    UNLOCK();

    result = REAL_FUNC(pthread_join)(thread, retval);
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_FREEBSD_THREADS)
      /* On FreeBSD, the wrapped pthread_join() sometimes returns       */
      /* (what appears to be) a spurious EINTR which caused the test    */
      /* and real code to fail gratuitously.  Having looked at system   */
      /* pthread library source code, I see how such return code value  */
      /* may be generated.  In one path of the code, pthread_join just  */
      /* returns the errno setting of the thread being joined - this    */
      /* does not match the POSIX specification or the local man pages. */
      /* Thus, I have taken the liberty to catch this one spurious      */
      /* return value.                                                  */
      if (EXPECT(result == EINTR, FALSE)) result = 0;
#   endif

    if (EXPECT(0 == result, TRUE)) {
      LOCK();
      /* Here the pthread id may have been recycled.  Delete the thread */
      /* from MANAGED_STACK_ADDRESS_BOEHM_GC_threads (unless it has been registered again from the  */
      /* client thread key destructor).                                 */
      if (KNOWN_FINISHED(t)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_delete_thread(t);
      }
      UNLOCK();
    }

#   ifdef DEBUG_THREADS
      MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("thread %p join with thread %p %s\n",
                    (void *)MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_PTRVAL(pthread_self()),
                    (void *)MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_PTRVAL(thread),
                    result != 0 ? "failed" : "succeeded");
#   endif
    return result;
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API int WRAP_FUNC(pthread_detach)(pthread_t thread)
  {
    int result;
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread t;

    INIT_REAL_SYMS();
    LOCK();
    t = (MANAGED_STACK_ADDRESS_BOEHM_GC_thread)COVERT_DATAFLOW(MANAGED_STACK_ADDRESS_BOEHM_GC_lookup_by_pthread(thread));
    UNLOCK();
    result = REAL_FUNC(pthread_detach)(thread);
    if (EXPECT(0 == result, TRUE)) {
      LOCK();
      /* Here the pthread id may have been recycled.    */
      if (KNOWN_FINISHED(t)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_delete_thread(t);
      } else {
        t -> flags |= DETACHED;
      }
      UNLOCK();
    }
    return result;
  }

  struct start_info {
    void *(*start_routine)(void *);
    void *arg;
    sem_t registered;           /* 1 ==> in our thread table, but       */
                                /* parent hasn't yet noticed.           */
    unsigned char flags;
  };

  /* Called from MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_start_inner().  Defined in this file to     */
  /* minimize the number of include files in pthread_start.c (because   */
  /* sem_t and sem_post() are not used in that file directly).          */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER_PTHRSTART MANAGED_STACK_ADDRESS_BOEHM_GC_thread MANAGED_STACK_ADDRESS_BOEHM_GC_start_rtn_prepare_thread(
                                        void *(**pstart)(void *),
                                        void **pstart_arg,
                                        struct MANAGED_STACK_ADDRESS_BOEHM_GC_stack_base *sb, void *arg)
  {
    struct start_info *psi = (struct start_info *)arg;
    thread_id_t self_id = thread_id_self();
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread me;

#   ifdef DEBUG_THREADS
      MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Starting thread %p, sp= %p\n",
                    (void *)MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_PTRVAL(pthread_self()), (void *)&arg);
#   endif
    /* If a GC occurs before the thread is registered, that GC will     */
    /* ignore this thread.  That's fine, since it will block trying to  */
    /* acquire the allocation lock, and won't yet hold interesting      */
    /* pointers.                                                        */
    LOCK();
    /* We register the thread here instead of in the parent, so that    */
    /* we don't need to hold the allocation lock during pthread_create. */
    me = MANAGED_STACK_ADDRESS_BOEHM_GC_register_my_thread_inner(sb, self_id);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(me != &first_thread);
    me -> flags = psi -> flags;
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS
      MANAGED_STACK_ADDRESS_BOEHM_GC_win32_cache_self_pthread(self_id);
#   endif
#   ifdef THREAD_LOCAL_ALLOC
      MANAGED_STACK_ADDRESS_BOEHM_GC_init_thread_local(&me->tlfs);
#   endif
    UNLOCK();

    *pstart = psi -> start_routine;
    *pstart_arg = psi -> arg;
#   if defined(DEBUG_THREADS) && defined(FUNCPTR_IS_WORD)
      MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("start_routine= %p\n", (void *)(word)(*pstart));
#   endif
    sem_post(&(psi -> registered));     /* Last action on *psi; */
                                        /* OK to deallocate.    */
    return me;
  }

  STATIC void * MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_start(void * arg)
  {
#   ifdef INCLUDE_LINUX_THREAD_DESCR
      struct MANAGED_STACK_ADDRESS_BOEHM_GC_stack_base sb;

#     ifdef REDIRECT_MALLOC
        /* MANAGED_STACK_ADDRESS_BOEHM_GC_get_stack_base may call pthread_getattr_np, which can     */
        /* unfortunately call realloc, which may allocate from an       */
        /* unregistered thread.  This is unpleasant, since it might     */
        /* force heap growth (or, even, heap overflow).                 */
        MANAGED_STACK_ADDRESS_BOEHM_GC_disable();
#     endif
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_get_stack_base(&sb) != MANAGED_STACK_ADDRESS_BOEHM_GC_SUCCESS)
        ABORT("Failed to get thread stack base");
#     ifdef REDIRECT_MALLOC
        MANAGED_STACK_ADDRESS_BOEHM_GC_enable();
#     endif
      return MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_start_inner(&sb, arg);
#   else
      return MANAGED_STACK_ADDRESS_BOEHM_GC_call_with_stack_base(MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_start_inner, arg);
#   endif
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API int WRAP_FUNC(pthread_create)(pthread_t *new_thread,
                       MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_CREATE_CONST pthread_attr_t *attr,
                       void *(*start_routine)(void *), void *arg)
  {
    int result;
    struct start_info si;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_DONT_HOLD_LOCK());
    INIT_REAL_SYMS();
    if (!EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized, TRUE)) MANAGED_STACK_ADDRESS_BOEHM_GC_init();
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_thr_initialized);

    if (sem_init(&si.registered, MANAGED_STACK_ADDRESS_BOEHM_GC_SEM_INIT_PSHARED, 0) != 0)
        ABORT("sem_init failed");
    si.flags = 0;
    si.start_routine = start_routine;
    si.arg = arg;

    /* We resist the temptation to muck with the stack size here,       */
    /* even if the default is unreasonably small.  That is the client's */
    /* responsibility.                                                  */
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS
      {
        size_t stack_size = 0;
        if (NULL != attr) {
          if (pthread_attr_getstacksize(attr, &stack_size) != 0)
            ABORT("pthread_attr_getstacksize failed");
        }
        if (0 == stack_size) {
          pthread_attr_t my_attr;

          if (pthread_attr_init(&my_attr) != 0)
            ABORT("pthread_attr_init failed");
          if (pthread_attr_getstacksize(&my_attr, &stack_size) != 0)
            ABORT("pthread_attr_getstacksize failed");
          (void)pthread_attr_destroy(&my_attr);
        }
        /* On Solaris 10 and on Win32 with winpthreads, with the        */
        /* default attr initialization, stack_size remains 0; fudge it. */
        if (EXPECT(0 == stack_size, FALSE)) {
#           if !defined(SOLARIS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_PTHREADS)
              WARN("Failed to get stack size for assertion checking\n", 0);
#           endif
            stack_size = 1000000;
        }
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(stack_size >= 65536);
        /* Our threads may need to do some work for the GC.     */
        /* Ridiculously small threads won't work, and they      */
        /* probably wouldn't work anyway.                       */
      }
#   endif

    if (attr != NULL) {
        int detachstate;

        if (pthread_attr_getdetachstate(attr, &detachstate) != 0)
            ABORT("pthread_attr_getdetachstate failed");
        if (PTHREAD_CREATE_DETACHED == detachstate)
          si.flags |= DETACHED;
    }

#   ifdef PARALLEL_MARK
      if (EXPECT(!MANAGED_STACK_ADDRESS_BOEHM_GC_parallel && MANAGED_STACK_ADDRESS_BOEHM_GC_available_markers_m1 > 0, FALSE))
        MANAGED_STACK_ADDRESS_BOEHM_GC_start_mark_threads();
#   endif
#   ifdef DEBUG_THREADS
      MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("About to start new thread from thread %p\n",
                    (void *)MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_PTRVAL(pthread_self()));
#   endif
    set_need_to_lock();
    result = REAL_FUNC(pthread_create)(new_thread, attr,
                                       MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_start, &si);

    /* Wait until child has been added to the thread table.             */
    /* This also ensures that we hold onto the stack-allocated si       */
    /* until the child is done with it.                                 */
    if (EXPECT(0 == result, TRUE)) {
        IF_CANCEL(int cancel_state;)

        DISABLE_CANCEL(cancel_state);
                /* pthread_create is not a cancellation point.  */
        while (0 != sem_wait(&si.registered)) {
#           if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_HAIKU_THREADS)
              /* To workaround some bug in Haiku semaphores.    */
              if (EACCES == errno) continue;
#           endif
            if (EINTR != errno) ABORT("sem_wait failed");
        }
        RESTORE_CANCEL(cancel_state);
    }
    sem_destroy(&si.registered);
    return result;
  }

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS && !SN_TARGET_ORBIS && !SN_TARGET_PSP2 */

#if ((defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS_PARAMARK) || defined(USE_PTHREAD_LOCKS)) \
     && !defined(NO_PTHREAD_TRYLOCK)) || defined(USE_SPIN_LOCK)
  /* Spend a few cycles in a way that can't introduce contention with   */
  /* other threads.                                                     */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_PAUSE_SPIN_CYCLES 10
  STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_pause(void)
  {
    int i;

    for (i = 0; i < MANAGED_STACK_ADDRESS_BOEHM_GC_PAUSE_SPIN_CYCLES; ++i) {
        /* Something that's unlikely to be optimized away. */
#     if defined(AO_HAVE_compiler_barrier) \
         && !defined(BASE_ATOMIC_OPS_EMULATED)
        AO_compiler_barrier();
#     else
        MANAGED_STACK_ADDRESS_BOEHM_GC_noop1(i);
#     endif
    }
  }
#endif /* USE_SPIN_LOCK || !NO_PTHREAD_TRYLOCK */

#ifndef SPIN_MAX
# define SPIN_MAX 128   /* Maximum number of calls to MANAGED_STACK_ADDRESS_BOEHM_GC_pause before   */
                        /* give up.                                     */
#endif

#if (!defined(USE_SPIN_LOCK) && !defined(NO_PTHREAD_TRYLOCK) \
     && defined(USE_PTHREAD_LOCKS)) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS_PARAMARK)
  /* If we do not want to use the below spinlock implementation, either */
  /* because we don't have a MANAGED_STACK_ADDRESS_BOEHM_GC_test_and_set implementation, or because */
  /* we don't want to risk sleeping, we can still try spinning on       */
  /* pthread_mutex_trylock for a while.  This appears to be very        */
  /* beneficial in many cases.                                          */
  /* I suspect that under high contention this is nearly always better  */
  /* than the spin lock.  But it is a bit slower on a uniprocessor.     */
  /* Hence we still default to the spin lock.                           */
  /* This is also used to acquire the mark lock for the parallel        */
  /* marker.                                                            */

  /* Here we use a strict exponential backoff scheme.  I don't know     */
  /* whether that's better or worse than the above.  We eventually      */
  /* yield by calling pthread_mutex_lock(); it never makes sense to     */
  /* explicitly sleep.                                                  */

# ifdef LOCK_STATS
    /* Note that LOCK_STATS requires AO_HAVE_test_and_set.      */
    volatile AO_t MANAGED_STACK_ADDRESS_BOEHM_GC_spin_count = 0;
    volatile AO_t MANAGED_STACK_ADDRESS_BOEHM_GC_block_count = 0;
    volatile AO_t MANAGED_STACK_ADDRESS_BOEHM_GC_unlocked_count = 0;
# endif

  STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_generic_lock(pthread_mutex_t * lock)
  {
#   ifndef NO_PTHREAD_TRYLOCK
      unsigned pause_length = 1;
      unsigned i;

      if (EXPECT(0 == pthread_mutex_trylock(lock), TRUE)) {
#       ifdef LOCK_STATS
            (void)AO_fetch_and_add1(&MANAGED_STACK_ADDRESS_BOEHM_GC_unlocked_count);
#       endif
        return;
      }
      for (; pause_length <= SPIN_MAX; pause_length <<= 1) {
         for (i = 0; i < pause_length; ++i) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_pause();
        }
        switch (pthread_mutex_trylock(lock)) {
            case 0:
#               ifdef LOCK_STATS
                    (void)AO_fetch_and_add1(&MANAGED_STACK_ADDRESS_BOEHM_GC_spin_count);
#               endif
                return;
            case EBUSY:
                break;
            default:
                ABORT("Unexpected error from pthread_mutex_trylock");
        }
      }
#   endif /* !NO_PTHREAD_TRYLOCK */
#   ifdef LOCK_STATS
        (void)AO_fetch_and_add1(&MANAGED_STACK_ADDRESS_BOEHM_GC_block_count);
#   endif
    pthread_mutex_lock(lock);
  }
#endif /* !USE_SPIN_LOCK || ... */

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS)
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER volatile unsigned char MANAGED_STACK_ADDRESS_BOEHM_GC_collecting = FALSE;
                        /* A hint that we are in the collector and      */
                        /* holding the allocation lock for an           */
                        /* extended period.                             */

# if defined(AO_HAVE_char_load) && !defined(BASE_ATOMIC_OPS_EMULATED)
#   define is_collecting() ((MANAGED_STACK_ADDRESS_BOEHM_GC_bool)AO_char_load(&MANAGED_STACK_ADDRESS_BOEHM_GC_collecting))
# else
    /* MANAGED_STACK_ADDRESS_BOEHM_GC_collecting is a hint, a potential data race between   */
    /* MANAGED_STACK_ADDRESS_BOEHM_GC_lock() and ENTER/EXIT_GC() is OK to ignore.           */
#   define is_collecting() ((MANAGED_STACK_ADDRESS_BOEHM_GC_bool)MANAGED_STACK_ADDRESS_BOEHM_GC_collecting)
# endif
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS && !MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS */

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER unsigned long MANAGED_STACK_ADDRESS_BOEHM_GC_lock_holder = NO_THREAD;
#endif

#if defined(USE_SPIN_LOCK)
  /* Reasonably fast spin locks.  Basically the same implementation     */
  /* as STL alloc.h.  This isn't really the right way to do this.       */
  /* but until the POSIX scheduling mess gets straightened out ...      */

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER volatile AO_TS_t MANAGED_STACK_ADDRESS_BOEHM_GC_allocate_lock = AO_TS_INITIALIZER;

# define low_spin_max 30 /* spin cycles if we suspect uniprocessor  */
# define high_spin_max SPIN_MAX /* spin cycles for multiprocessor   */

  static volatile AO_t spin_max = low_spin_max;
  static volatile AO_t last_spins = 0;
                                /* A potential data race between        */
                                /* threads invoking MANAGED_STACK_ADDRESS_BOEHM_GC_lock which reads */
                                /* and updates spin_max and last_spins  */
                                /* could be ignored because these       */
                                /* variables are hints only.            */

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_lock(void)
  {
    unsigned my_spin_max;
    unsigned my_last_spins;
    unsigned i;

    if (EXPECT(AO_test_and_set_acquire(&MANAGED_STACK_ADDRESS_BOEHM_GC_allocate_lock)
                == AO_TS_CLEAR, TRUE)) {
        return;
    }
    my_spin_max = (unsigned)AO_load(&spin_max);
    my_last_spins = (unsigned)AO_load(&last_spins);
    for (i = 0; i < my_spin_max; i++) {
        if (is_collecting() || MANAGED_STACK_ADDRESS_BOEHM_GC_nprocs == 1)
          goto yield;
        if (i < my_last_spins/2) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_pause();
            continue;
        }
        if (AO_test_and_set_acquire(&MANAGED_STACK_ADDRESS_BOEHM_GC_allocate_lock) == AO_TS_CLEAR) {
            /*
             * got it!
             * Spinning worked.  Thus we're probably not being scheduled
             * against the other process with which we were contending.
             * Thus it makes sense to spin longer the next time.
             */
            AO_store(&last_spins, (AO_t)i);
            AO_store(&spin_max, (AO_t)high_spin_max);
            return;
        }
    }
    /* We are probably being scheduled against the other process.  Sleep. */
    AO_store(&spin_max, (AO_t)low_spin_max);
  yield:
    for (i = 0;; ++i) {
        if (AO_test_and_set_acquire(&MANAGED_STACK_ADDRESS_BOEHM_GC_allocate_lock) == AO_TS_CLEAR) {
            return;
        }
#       define SLEEP_THRESHOLD 12
                /* Under Linux very short sleeps tend to wait until     */
                /* the current time quantum expires.  On old Linux      */
                /* kernels nanosleep (<= 2 ms) just spins.              */
                /* (Under 2.4, this happens only for real-time          */
                /* processes.)  We want to minimize both behaviors      */
                /* here.                                                */
        if (i < SLEEP_THRESHOLD) {
            sched_yield();
        } else {
            struct timespec ts;

            if (i > 24) i = 24;
                        /* Don't wait for more than about 15 ms,        */
                        /* even under extreme contention.               */

            ts.tv_sec = 0;
            ts.tv_nsec = (unsigned32)1 << i;
            nanosleep(&ts, 0);
        }
    }
  }

#elif defined(USE_PTHREAD_LOCKS)
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER pthread_mutex_t MANAGED_STACK_ADDRESS_BOEHM_GC_allocate_ml = PTHREAD_MUTEX_INITIALIZER;

# ifndef NO_PTHREAD_TRYLOCK
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_lock(void)
    {
      if (1 == MANAGED_STACK_ADDRESS_BOEHM_GC_nprocs || is_collecting()) {
        pthread_mutex_lock(&MANAGED_STACK_ADDRESS_BOEHM_GC_allocate_ml);
      } else {
        MANAGED_STACK_ADDRESS_BOEHM_GC_generic_lock(&MANAGED_STACK_ADDRESS_BOEHM_GC_allocate_ml);
      }
    }
# elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS)
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_lock(void)
    {
      pthread_mutex_lock(&MANAGED_STACK_ADDRESS_BOEHM_GC_allocate_ml);
    }
# endif

#endif /* !USE_SPIN_LOCK && USE_PTHREAD_LOCKS */

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS_PARAMARK

# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) \
     && !defined(USE_PTHREAD_LOCKS)
#   define NUMERIC_THREAD_ID(id) (unsigned long)(word)MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_PTRVAL(id)
    /* Id not guaranteed to be unique. */
# endif

# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS
    STATIC unsigned long MANAGED_STACK_ADDRESS_BOEHM_GC_mark_lock_holder = NO_THREAD;
#   define SET_MARK_LOCK_HOLDER \
                (void)(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_lock_holder = NUMERIC_THREAD_ID(pthread_self()))
#   define UNSET_MARK_LOCK_HOLDER \
                do { \
                  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_lock_holder \
                                == NUMERIC_THREAD_ID(pthread_self())); \
                  MANAGED_STACK_ADDRESS_BOEHM_GC_mark_lock_holder = NO_THREAD; \
                } while (0)
# else
#   define SET_MARK_LOCK_HOLDER (void)0
#   define UNSET_MARK_LOCK_HOLDER (void)0
# endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS */

  static pthread_cond_t builder_cv = PTHREAD_COND_INITIALIZER;

# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS
    static void setup_mark_lock(void)
    {
#     ifdef GLIBC_2_19_TSX_BUG
        pthread_mutexattr_t mattr;
        int glibc_minor = -1;
        int glibc_major = MANAGED_STACK_ADDRESS_BOEHM_GC_parse_version(&glibc_minor,
                                           gnu_get_libc_version());

        if (glibc_major > 2 || (glibc_major == 2 && glibc_minor >= 19)) {
          /* TODO: disable this workaround for glibc with fixed TSX */
          /* This disables lock elision to workaround a bug in glibc 2.19+ */
          if (0 != pthread_mutexattr_init(&mattr)) {
            ABORT("pthread_mutexattr_init failed");
          }
          if (0 != pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_NORMAL)) {
            ABORT("pthread_mutexattr_settype failed");
          }
          if (0 != pthread_mutex_init(&mark_mutex, &mattr)) {
            ABORT("pthread_mutex_init failed");
          }
          (void)pthread_mutexattr_destroy(&mattr);
        }
#     endif
    }
# endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS */

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_acquire_mark_lock(void)
  {
#   if defined(NUMERIC_THREAD_ID_UNIQUE) && !defined(THREAD_SANITIZER)
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_lock_holder != NUMERIC_THREAD_ID(pthread_self()));
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_generic_lock(&mark_mutex);
    SET_MARK_LOCK_HOLDER;
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_release_mark_lock(void)
  {
    UNSET_MARK_LOCK_HOLDER;
    if (pthread_mutex_unlock(&mark_mutex) != 0) {
        ABORT("pthread_mutex_unlock failed");
    }
  }

  /* Collector must wait for a freelist builders for 2 reasons:         */
  /* 1) Mark bits may still be getting examined without lock.           */
  /* 2) Partial free lists referenced only by locals may not be scanned */
  /*    correctly, e.g. if they contain "pointer-free" objects, since   */
  /*    the free-list link may be ignored.                              */
  STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_wait_builder(void)
  {
    ASSERT_CANCEL_DISABLED();
    UNSET_MARK_LOCK_HOLDER;
    if (pthread_cond_wait(&builder_cv, &mark_mutex) != 0) {
        ABORT("pthread_cond_wait failed");
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_lock_holder == NO_THREAD);
    SET_MARK_LOCK_HOLDER;
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_wait_for_reclaim(void)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_acquire_mark_lock();
    while (MANAGED_STACK_ADDRESS_BOEHM_GC_fl_builder_count > 0) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_wait_builder();
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_release_mark_lock();
  }

# if defined(CAN_HANDLE_FORK) && defined(THREAD_SANITIZER)
    /* Identical to MANAGED_STACK_ADDRESS_BOEHM_GC_wait_for_reclaim() but with the no_sanitize      */
    /* attribute as a workaround for TSan which does not notice that    */
    /* the GC lock is acquired in fork_prepare_proc().                  */
    MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NO_SANITIZE_THREAD
    static void wait_for_reclaim_atfork(void)
    {
      MANAGED_STACK_ADDRESS_BOEHM_GC_acquire_mark_lock();
      while (MANAGED_STACK_ADDRESS_BOEHM_GC_fl_builder_count > 0)
        MANAGED_STACK_ADDRESS_BOEHM_GC_wait_builder();
      MANAGED_STACK_ADDRESS_BOEHM_GC_release_mark_lock();
    }
# endif /* CAN_HANDLE_FORK && THREAD_SANITIZER */

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_notify_all_builder(void)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_lock_holder == NUMERIC_THREAD_ID(pthread_self()));
    if (pthread_cond_broadcast(&builder_cv) != 0) {
        ABORT("pthread_cond_broadcast failed");
    }
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_wait_marker(void)
  {
    ASSERT_CANCEL_DISABLED();
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_parallel);
    UNSET_MARK_LOCK_HOLDER;
    if (pthread_cond_wait(&mark_cv, &mark_mutex) != 0) {
        ABORT("pthread_cond_wait failed");
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_lock_holder == NO_THREAD);
    SET_MARK_LOCK_HOLDER;
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_notify_all_marker(void)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_parallel);
    if (pthread_cond_broadcast(&mark_cv) != 0) {
        ABORT("pthread_cond_broadcast failed");
    }
  }

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS_PARAMARK */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_on_thread_event_proc MANAGED_STACK_ADDRESS_BOEHM_GC_on_thread_event = 0;

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_on_thread_event(MANAGED_STACK_ADDRESS_BOEHM_GC_on_thread_event_proc fn)
{
  /* fn may be 0 (means no event notifier). */
  LOCK();
  MANAGED_STACK_ADDRESS_BOEHM_GC_on_thread_event = fn;
  UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_on_thread_event_proc MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_on_thread_event(void)
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_on_thread_event_proc fn;

  LOCK();
  fn = MANAGED_STACK_ADDRESS_BOEHM_GC_on_thread_event;
  UNLOCK();
  return fn;
}

#ifdef STACKPTR_CORRECTOR_AVAILABLE
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_sp_corrector_proc MANAGED_STACK_ADDRESS_BOEHM_GC_sp_corrector = 0;
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_sp_corrector(MANAGED_STACK_ADDRESS_BOEHM_GC_sp_corrector_proc fn)
{
# ifdef STACKPTR_CORRECTOR_AVAILABLE
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_sp_corrector = fn;
    UNLOCK();
# else
    UNUSED_ARG(fn);
# endif
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_sp_corrector_proc MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_sp_corrector(void)
{
# ifdef STACKPTR_CORRECTOR_AVAILABLE
    MANAGED_STACK_ADDRESS_BOEHM_GC_sp_corrector_proc fn;

    LOCK();
    fn = MANAGED_STACK_ADDRESS_BOEHM_GC_sp_corrector;
    UNLOCK();
    return fn;
# else
    return 0; /* unsupported */
# endif
}

#ifdef PTHREAD_REGISTER_CANCEL_WEAK_STUBS
  /* Workaround "undefined reference" linkage errors on some targets. */
  EXTERN_C_BEGIN
  extern void __pthread_register_cancel(void) __attribute__((__weak__));
  extern void __pthread_unregister_cancel(void) __attribute__((__weak__));
  EXTERN_C_END

  void __pthread_register_cancel(void) {}
  void __pthread_unregister_cancel(void) {}
#endif

#undef do_blocking_enter

#endif /* THREADS */
