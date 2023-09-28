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

#include "private/gc_priv.h"

int main(void)
{
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_USE_LD_WRAP)
        printf("-Wl,--wrap -Wl,dlopen "
               "-Wl,--wrap -Wl,pthread_create -Wl,--wrap -Wl,pthread_join "
               "-Wl,--wrap -Wl,pthread_detach -Wl,--wrap -Wl,pthread_sigmask "
               "-Wl,--wrap -Wl,pthread_exit -Wl,--wrap -Wl,pthread_cancel\n");
#   endif
#   if (defined(MANAGED_STACK_ADDRESS_BOEHM_GC_LINUX_THREADS) && !defined(HOST_ANDROID)) \
        || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_IRIX_THREADS) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DARWIN_THREADS) \
        || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_AIX_THREADS) || (defined(HURD) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_THREADS))
#       ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_USE_DLOPEN_WRAP
          printf("-ldl ");
#       endif
        printf("-lpthread\n");
#   endif
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_OPENBSD_THREADS)
        printf("-pthread\n");
#   endif
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_FREEBSD_THREADS)
#       ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_USE_DLOPEN_WRAP
          printf("-ldl ");
#       endif
#       if (__FREEBSD_version < 500000)
          printf("-pthread\n");
#       else /* __FREEBSD__ || __DragonFly__ */
          printf("-lpthread\n");
#       endif
#   endif
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NETBSD_THREADS)
          printf("-lpthread -lrt\n");
#   endif

#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_HPUX_THREADS) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_OSF1_THREADS)
        printf("-lpthread -lrt\n");
#   endif
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_SOLARIS_THREADS)
        printf("-lthread -lposix4\n");
                /* Is this right for recent versions? */
#   endif
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) && defined(CYGWIN32)
        printf("-lpthread\n");
#   endif
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_PTHREADS)
#     ifdef PTW32_STATIC_LIB
        /* assume suffix s for static version of the pthreads-win32 library */
        printf("-lpthreadGC2s -lws2_32\n");
#     else
        printf("-lpthreadGC2\n");
#     endif
#   endif
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_OSF1_THREADS)
        printf("-pthread -lrt\n"); /* DOB: must be -pthread, not -lpthread */
#   endif
    /* You need GCC 3.0.3 to build this one!            */
    /* DG/UX native gcc doesn't know what "-pthread" is */
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DGUX386_THREADS)
        printf("-ldl -pthread\n");
#   endif
    return 0;
}
