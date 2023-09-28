/*
 * Copyright (c) 2011 Ludovic Courtes
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED. ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose, provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

/* Make sure 'MANAGED_STACK_ADDRESS_BOEHM_GC_INIT' can be called from threads other than the initial
 * thread.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_THREADS
# define MANAGED_STACK_ADDRESS_BOEHM_GC_THREADS
#endif

#define MANAGED_STACK_ADDRESS_BOEHM_GC_NO_THREAD_REDIRECTS 1
                /* Do not redirect thread creation and join calls.      */

#include "gc.h"

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS
# include <pthread.h>
# include <string.h>
#else
# ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN 1
# endif
# define NOSERVICE
# include <windows.h>
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS */

#include <stdlib.h>
#include <stdio.h>

#define CHECK_OUT_OF_MEMORY(p) \
    do { \
        if (NULL == (p)) { \
            fprintf(stderr, "Out of memory\n"); \
            exit(69); \
        } \
    } while (0)

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS
  static void *thread(void *arg)
#else
  static DWORD WINAPI thread(LPVOID arg)
#endif
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_INIT();
  CHECK_OUT_OF_MEMORY(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(123));
  CHECK_OUT_OF_MEMORY(MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC(12345));
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS
    return arg;
# else
    return (DWORD)(MANAGED_STACK_ADDRESS_BOEHM_GC_word)arg;
# endif
}

#include "private/gcconfig.h"

int main(void)
{
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS
    int code;
    pthread_t t;

#   ifdef LINT2
      t = pthread_self(); /* explicitly initialize to some value */
#   endif
# else
    HANDLE t;
    DWORD thread_id;
# endif
# if !(defined(BEOS) || defined(ANY_MSWIN) \
       || (defined(DARWIN) && !defined(NO_PTHREAD_GET_STACKADDR_NP)) \
       || ((defined(FREEBSD) || defined(LINUX) || defined(NETBSD) \
            || defined(HOST_ANDROID)) && !defined(NO_PTHREAD_GETATTR_NP) \
           && !defined(NO_PTHREAD_ATTR_GET_NP)) \
       || (defined(MANAGED_STACK_ADDRESS_BOEHM_GC_SOLARIS_THREADS) && !defined(_STRICT_STDC)) \
       || (!defined(STACKBOTTOM) && (defined(HEURISTIC1) \
          || (!defined(LINUX_STACKBOTTOM) && !defined(FREEBSD_STACKBOTTOM)))))
    /* MANAGED_STACK_ADDRESS_BOEHM_GC_INIT() must be called from main thread only. */
    MANAGED_STACK_ADDRESS_BOEHM_GC_INIT();
# endif
  (void)MANAGED_STACK_ADDRESS_BOEHM_GC_get_suspend_signal(); /* linking fails if no threads support */
  if (MANAGED_STACK_ADDRESS_BOEHM_GC_get_find_leak())
    printf("This test program is not designed for leak detection mode\n");
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS
    if ((code = pthread_create(&t, NULL, thread, NULL)) != 0) {
      fprintf(stderr, "Thread #0 creation failed: %s\n", strerror(code));
      return 1;
    }
    if ((code = pthread_join(t, NULL)) != 0) {
      fprintf(stderr, "Thread #0 join failed: %s\n", strerror(code));
      return 1;
    }
# else
    t = CreateThread(NULL, 0, thread, 0, 0, &thread_id);
    if (t == NULL) {
      fprintf(stderr, "Thread #0 creation failed, errcode= %d\n",
              (int)GetLastError());
      return 1;
    }
    if (WaitForSingleObject(t, INFINITE) != WAIT_OBJECT_0) {
      fprintf(stderr, "Thread #0 join failed, errcode= %d\n",
              (int)GetLastError());
      CloseHandle(t);
      return 1;
    }
    CloseHandle(t);
# endif
  printf("SUCCEEDED\n");
  return 0;
}
