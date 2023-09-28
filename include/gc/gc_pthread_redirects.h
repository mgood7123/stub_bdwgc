/*
 * Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 1998 by Fergus Henderson.  All rights reserved.
 * Copyright (c) 2000-2010 by Hewlett-Packard Development Company.
 * All rights reserved.
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

/* Our pthread support normally needs to intercept a number of thread   */
/* calls.  We arrange to do that here, if appropriate.                  */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_REDIRECTS_H
#define MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_REDIRECTS_H

/* Included from gc.h only.  Included only if MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS.              */
#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_H) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS)

/* We need to intercept calls to many of the threads primitives, so     */
/* that we can locate thread stacks and stop the world.                 */
/* Note also that the collector cannot always see thread specific data. */
/* Thread specific data should generally consist of pointers to         */
/* uncollectible objects (allocated with MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_uncollectable,       */
/* not the system malloc), which are deallocated using the destructor   */
/* facility in thr_keycreate.  Alternatively, keep a redundant pointer  */
/* to thread specific data on the thread stack.                         */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_REDIRECTS_ONLY

# include <pthread.h>
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_DLOPEN
#   include <dlfcn.h>
# endif
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_SIGMASK
#   include <signal.h>  /* needed anyway for proper redirection */
# endif

# ifdef __cplusplus
    extern "C" {
# endif

# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_SUSPEND_THREAD_ID
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_SUSPEND_THREAD_ID pthread_t
# endif

# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_DLOPEN
    MANAGED_STACK_ADDRESS_BOEHM_GC_API void *MANAGED_STACK_ADDRESS_BOEHM_GC_dlopen(const char * /* path */, int /* mode */);
# endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_NO_DLOPEN */

# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_SIGMASK
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_SIGMASK_NEEDED) \
        || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_HAVE_PTHREAD_SIGMASK) || defined(_BSD_SOURCE) \
        || defined(_GNU_SOURCE) || defined(_NETBSD_SOURCE) \
        || (_POSIX_C_SOURCE >= 199506L) || (_XOPEN_SOURCE >= 500) \
        || (__POSIX_VISIBLE >= 199506) /* xBSD internal macro */
      MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_sigmask(int /* how */, const sigset_t *,
                                    sigset_t * /* oset */);
#   else
#     define MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_SIGMASK
#   endif
# endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_SIGMASK */

# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_CREATE_CONST
    /* This is used for pthread_create() only.    */
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_CREATE_CONST const
# endif

  MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_create(pthread_t *,
                               MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_CREATE_CONST pthread_attr_t *,
                               void *(*)(void *), void * /* arg */);
  MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_join(pthread_t, void ** /* retval */);
  MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_detach(pthread_t);

# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_CANCEL
    MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_cancel(pthread_t);
# endif

# if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_HAVE_PTHREAD_EXIT) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_EXIT_DECLARED)
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_EXIT_DECLARED
    MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_exit(void *) MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_EXIT_ATTRIBUTE;
# endif

# ifdef __cplusplus
    } /* extern "C" */
# endif

#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_REDIRECTS_ONLY */

#if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_THREAD_REDIRECTS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_USE_LD_WRAP)
  /* Unless the compiler supports #pragma extern_prefix, the Tru64      */
  /* UNIX pthread.h redefines some POSIX thread functions to use        */
  /* mangled names.  Anyway, it's safe to undef them before redefining. */
# undef pthread_create
# undef pthread_join
# undef pthread_detach
# define pthread_create MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_create
# define pthread_join MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_join
# define pthread_detach MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_detach

# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_SIGMASK
#   undef pthread_sigmask
#   define pthread_sigmask MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_sigmask
# endif
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_DLOPEN
#   undef dlopen
#   define dlopen MANAGED_STACK_ADDRESS_BOEHM_GC_dlopen
# endif
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_PTHREAD_CANCEL
#   undef pthread_cancel
#   define pthread_cancel MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_cancel
# endif
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_HAVE_PTHREAD_EXIT
#   undef pthread_exit
#   define pthread_exit MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_exit
# endif
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_NO_THREAD_REDIRECTS */

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS */

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_REDIRECTS_H */
