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

/* We want to make sure that MANAGED_STACK_ADDRESS_BOEHM_GC_thread_exit_proc() is unconditionally   */
/* invoked, even if the client is not compiled with -fexceptions, but   */
/* the GC is.  The workaround is to put MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_start_inner() in its */
/* own file (pthread_start.c), and undefine __EXCEPTIONS in the GCC     */
/* case at the top of the file.  FIXME: it's still unclear whether this */
/* will actually cause the exit handler to be invoked last when         */
/* thread_exit is called (and if -fexceptions is used).                 */
#if !defined(DONT_UNDEF_EXCEPTIONS) && defined(__GNUC__) && defined(__linux__)
  /* We undefine __EXCEPTIONS to avoid using GCC __cleanup__ attribute. */
  /* The current NPTL implementation of pthread_cleanup_push uses       */
  /* __cleanup__ attribute when __EXCEPTIONS is defined (-fexceptions). */
  /* The stack unwinding and cleanup with __cleanup__ attributes work   */
  /* correctly when everything is compiled with -fexceptions, but it is */
  /* not the requirement for this library clients to use -fexceptions   */
  /* everywhere.  With __EXCEPTIONS undefined, the cleanup routines are */
  /* registered with __pthread_register_cancel thus should work anyway. */
# undef __EXCEPTIONS
#endif

#include "private/pthread_support.h"

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS) \
    && !defined(SN_TARGET_ORBIS) && !defined(SN_TARGET_PSP2)

/* Invoked from MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_start. */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER_PTHRSTART void *MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK MANAGED_STACK_ADDRESS_BOEHM_GC_pthread_start_inner(
                                        struct MANAGED_STACK_ADDRESS_BOEHM_GC_stack_base *sb, void *arg)
{
  void * (*start)(void *);
  void * start_arg;
  void * result;
  volatile MANAGED_STACK_ADDRESS_BOEHM_GC_thread me =
                MANAGED_STACK_ADDRESS_BOEHM_GC_start_rtn_prepare_thread(&start, &start_arg, sb, arg);

# ifndef NACL
    pthread_cleanup_push(MANAGED_STACK_ADDRESS_BOEHM_GC_thread_exit_proc, (void *)me);
# endif
  result = (*start)(start_arg);
# if defined(DEBUG_THREADS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_START_STANDALONE)
    MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Finishing thread %p\n",
                  (void *)MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREAD_PTRVAL(pthread_self()));
# endif
  me -> status = result;
  MANAGED_STACK_ADDRESS_BOEHM_GC_end_stubborn_change(me); /* cannot use MANAGED_STACK_ADDRESS_BOEHM_GC_dirty */
  /* Cleanup acquires lock, ensuring that we can't exit while   */
  /* a collection that thinks we're alive is trying to stop us. */
# ifdef NACL
    MANAGED_STACK_ADDRESS_BOEHM_GC_thread_exit_proc((void *)me);
# else
    pthread_cleanup_pop(1);
# endif
  return result;
}

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS */
