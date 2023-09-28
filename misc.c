/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1999-2001 by Hewlett-Packard Company. All rights reserved.
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

#include "private/gc_pmark.h"

#include <limits.h>
#include <stdarg.h>

#ifndef MSWINCE
# include <signal.h>
#endif

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_SOLARIS_THREADS
# include <sys/syscall.h>
#endif

#if defined(UNIX_LIKE) || defined(CYGWIN32) || defined(SYMBIAN) \
    || (defined(CONSOLE_LOG) && defined(MSWIN32))
# include <fcntl.h>
# include <sys/stat.h>
#endif

#if defined(CONSOLE_LOG) && defined(MSWIN32) && !defined(__GNUC__)
# include <io.h>
#endif

#ifdef NONSTOP
# include <floss.h>
#endif

#ifdef THREADS
# ifdef PCR
#   include "il/PCR_IL.h"
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER PCR_Th_ML MANAGED_STACK_ADDRESS_BOEHM_GC_allocate_ml;
# elif defined(SN_TARGET_PSP2)
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER WapiMutex MANAGED_STACK_ADDRESS_BOEHM_GC_allocate_ml_PSP2 = { 0, NULL };
# elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DEFN_ALLOCATE_ML) || defined(SN_TARGET_PS3)
#   include <pthread.h>
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER pthread_mutex_t MANAGED_STACK_ADDRESS_BOEHM_GC_allocate_ml;
# endif
  /* For other platforms with threads, the lock and possibly            */
  /* MANAGED_STACK_ADDRESS_BOEHM_GC_lock_holder variables are defined in the thread support code.   */
#endif /* THREADS */

#ifdef DYNAMIC_LOADING
  /* We need to register the main data segment.  Returns TRUE unless    */
  /* this is done implicitly as part of dynamic library registration.   */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_REGISTER_MAIN_STATIC_DATA() MANAGED_STACK_ADDRESS_BOEHM_GC_register_main_static_data()
#elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DONT_REGISTER_MAIN_STATIC_DATA)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_REGISTER_MAIN_STATIC_DATA() FALSE
#else
  /* Don't unnecessarily call MANAGED_STACK_ADDRESS_BOEHM_GC_register_main_static_data() in case    */
  /* dyn_load.c isn't linked in.                                        */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_REGISTER_MAIN_STATIC_DATA() TRUE
#endif

#ifdef NEED_CANCEL_DISABLE_COUNT
  __thread unsigned char MANAGED_STACK_ADDRESS_BOEHM_GC_cancel_disable_count = 0;
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_FAR struct _MANAGED_STACK_ADDRESS_BOEHM_GC_arrays MANAGED_STACK_ADDRESS_BOEHM_GC_arrays /* = { 0 } */;

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_n_mark_procs = MANAGED_STACK_ADDRESS_BOEHM_GC_RESERVED_MARK_PROCS;

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_n_kinds = MANAGED_STACK_ADDRESS_BOEHM_GC_N_KINDS_INITIAL_VALUE;

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_debugging_started = FALSE;
                /* defined here so we don't have to load dbg_mlc.o */

ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_stackbottom = 0;

#ifdef IA64
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_register_stackbottom = NULL;
#endif

int MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc = FALSE;

int MANAGED_STACK_ADDRESS_BOEHM_GC_dont_precollect = FALSE;

MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_quiet = 0; /* used also in pcr_interface.c */

#if !defined(NO_CLOCK) || !defined(SMALL_CONFIG)
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER int MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats = 0;
#endif

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PRINT_BACK_HEIGHT
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_print_back_height = TRUE;
#else
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_print_back_height = FALSE;
#endif

#ifndef NO_DEBUGGING
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_DUMP_REGULARLY
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_dump_regularly = TRUE;
                                /* Generate regular debugging dumps. */
# else
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_dump_regularly = FALSE;
# endif
# ifndef NO_CLOCK
    STATIC CLOCK_TYPE MANAGED_STACK_ADDRESS_BOEHM_GC_init_time;
                /* The time that the GC was initialized at.     */
# endif
#endif /* !NO_DEBUGGING */

#ifdef KEEP_BACK_PTRS
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER long MANAGED_STACK_ADDRESS_BOEHM_GC_backtraces = 0;
                /* Number of random backtraces to generate for each GC. */
#endif

#ifdef FIND_LEAK
  int MANAGED_STACK_ADDRESS_BOEHM_GC_find_leak = 1;
#else
  int MANAGED_STACK_ADDRESS_BOEHM_GC_find_leak = 0;
#endif

#ifndef SHORT_DBG_HDRS
# ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_FINDLEAK_DELAY_FREE
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_findleak_delay_free = TRUE;
# else
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_findleak_delay_free = FALSE;
# endif
#endif /* !SHORT_DBG_HDRS */

#ifdef ALL_INTERIOR_POINTERS
  int MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers = 1;
#else
  int MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers = 0;
#endif

#ifdef FINALIZE_ON_DEMAND
  int MANAGED_STACK_ADDRESS_BOEHM_GC_finalize_on_demand = 1;
#else
  int MANAGED_STACK_ADDRESS_BOEHM_GC_finalize_on_demand = 0;
#endif

#ifdef JAVA_FINALIZATION
  int MANAGED_STACK_ADDRESS_BOEHM_GC_java_finalization = 1;
#else
  int MANAGED_STACK_ADDRESS_BOEHM_GC_java_finalization = 0;
#endif

/* All accesses to it should be synchronized to avoid data races.       */
MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_notifier_proc MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_notifier =
                                        (MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_notifier_proc)0;

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_FORCE_UNMAP_ON_GCOLLECT
  /* Has no effect unless USE_MUNMAP.                           */
  /* Has no effect on implicitly-initiated garbage collections. */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_force_unmap_on_gcollect = TRUE;
#else
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_force_unmap_on_gcollect = FALSE;
#endif

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_LARGE_ALLOC_WARN_INTERVAL
# define MANAGED_STACK_ADDRESS_BOEHM_GC_LARGE_ALLOC_WARN_INTERVAL 5
#endif
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER long MANAGED_STACK_ADDRESS_BOEHM_GC_large_alloc_warn_interval = MANAGED_STACK_ADDRESS_BOEHM_GC_LARGE_ALLOC_WARN_INTERVAL;
                        /* Interval between unsuppressed warnings.      */

STATIC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK MANAGED_STACK_ADDRESS_BOEHM_GC_default_oom_fn(size_t bytes_requested)
{
    UNUSED_ARG(bytes_requested);
    return NULL;
}

/* All accesses to it should be synchronized to avoid data races.       */
MANAGED_STACK_ADDRESS_BOEHM_GC_oom_func MANAGED_STACK_ADDRESS_BOEHM_GC_oom_fn = MANAGED_STACK_ADDRESS_BOEHM_GC_default_oom_fn;

#ifdef CAN_HANDLE_FORK
# ifdef HANDLE_FORK
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER int MANAGED_STACK_ADDRESS_BOEHM_GC_handle_fork = 1;
                        /* The value is examined by MANAGED_STACK_ADDRESS_BOEHM_GC_thr_init.        */
# else
    MANAGED_STACK_ADDRESS_BOEHM_GC_INNER int MANAGED_STACK_ADDRESS_BOEHM_GC_handle_fork = FALSE;
# endif

#elif !defined(HAVE_NO_FORK)
  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_atfork_prepare(void)
  {
#   ifdef THREADS
      ABORT("fork() handling unsupported");
#   endif
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_atfork_parent(void)
  {
    /* empty */
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_atfork_child(void)
  {
    /* empty */
  }
#endif /* !CAN_HANDLE_FORK && !HAVE_NO_FORK */

/* Overrides the default automatic handle-fork mode.  Has effect only   */
/* if called before MANAGED_STACK_ADDRESS_BOEHM_GC_INIT.                                            */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_handle_fork(int value)
{
# ifdef CAN_HANDLE_FORK
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized)
      MANAGED_STACK_ADDRESS_BOEHM_GC_handle_fork = value >= -1 ? value : 1;
                /* Map all negative values except for -1 to a positive one. */
# elif defined(THREADS) || (defined(DARWIN) && defined(MPROTECT_VDB))
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized && value) {
#     ifndef SMALL_CONFIG
        MANAGED_STACK_ADDRESS_BOEHM_GC_init(); /* to initialize MANAGED_STACK_ADDRESS_BOEHM_GC_manual_vdb and MANAGED_STACK_ADDRESS_BOEHM_GC_stderr */
#       ifndef THREADS
          if (MANAGED_STACK_ADDRESS_BOEHM_GC_manual_vdb)
            return;
#       endif
#     endif
      ABORT("fork() handling unsupported");
    }
# else
    /* No at-fork handler is needed in the single-threaded mode.        */
    UNUSED_ARG(value);
# endif
}

/* Set things up so that MANAGED_STACK_ADDRESS_BOEHM_GC_size_map[i] >= granules(i),                 */
/* but not too much bigger                                              */
/* and so that size_map contains relatively few distinct entries        */
/* This was originally stolen from Russ Atkinson's Cedar                */
/* quantization algorithm (but we precompute it).                       */
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_init_size_map(void)
{
    size_t i = 1;

    /* Map size 0 to something bigger.                  */
    /* This avoids problems at lower levels.            */
      MANAGED_STACK_ADDRESS_BOEHM_GC_size_map[0] = 1;

    for (; i <= GRANULES_TO_BYTES(MANAGED_STACK_ADDRESS_BOEHM_GC_TINY_FREELISTS-1) - EXTRA_BYTES; i++) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_size_map[i] = ALLOC_REQUEST_GRANS(i);
#       ifndef _MSC_VER
          MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_size_map[i] < MANAGED_STACK_ADDRESS_BOEHM_GC_TINY_FREELISTS);
          /* Seems to tickle bug in VC++ 2008 for x64 */
#       endif
    }
    /* We leave the rest of the array to be filled in on demand. */
}

/*
 * The following is a gross hack to deal with a problem that can occur
 * on machines that are sloppy about stack frame sizes, notably SPARC.
 * Bogus pointers may be written to the stack and not cleared for
 * a LONG time, because they always fall into holes in stack frames
 * that are not written.  We partially address this by clearing
 * sections of the stack whenever we get control.
 */

#ifndef SMALL_CLEAR_SIZE
# define SMALL_CLEAR_SIZE 256   /* Clear this much every time.  */
#endif

#if defined(ALWAYS_SMALL_CLEAR_STACK) || defined(STACK_NOT_SCANNED)
  MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_clear_stack(void *arg)
  {
#   ifndef STACK_NOT_SCANNED
      word volatile dummy[SMALL_CLEAR_SIZE];
      BZERO((/* no volatile */ word *)((word)dummy), sizeof(dummy));
#   endif
    return arg;
  }
#else

# ifdef THREADS
#   define BIG_CLEAR_SIZE 2048  /* Clear this much now and then.        */
# else
    STATIC word MANAGED_STACK_ADDRESS_BOEHM_GC_stack_last_cleared = 0; /* MANAGED_STACK_ADDRESS_BOEHM_GC_no when we last did this */
    STATIC ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_min_sp = NULL;
                        /* Coolest stack pointer value from which       */
                        /* we've already cleared the stack.             */
    STATIC ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_high_water = NULL;
                        /* "hottest" stack pointer value we have seen   */
                        /* recently.  Degrades over time.               */
    STATIC word MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd_at_reset = 0;
#   define DEGRADE_RATE 50
# endif

# if defined(__APPLE_CC__) && !MANAGED_STACK_ADDRESS_BOEHM_GC_CLANG_PREREQ(6, 0)
#   define CLEARSTACK_LIMIT_MODIFIER volatile /* to workaround some bug */
# else
#   define CLEARSTACK_LIMIT_MODIFIER /* empty */
# endif

  EXTERN_C_BEGIN
  void *MANAGED_STACK_ADDRESS_BOEHM_GC_clear_stack_inner(void *, CLEARSTACK_LIMIT_MODIFIER ptr_t);
  EXTERN_C_END

# ifndef ASM_CLEAR_CODE
    /* Clear the stack up to about limit.  Return arg.  This function   */
    /* is not static because it could also be erroneously defined in .S */
    /* file, so this error would be caught by the linker.               */
    void *MANAGED_STACK_ADDRESS_BOEHM_GC_clear_stack_inner(void *arg,
                               CLEARSTACK_LIMIT_MODIFIER ptr_t limit)
    {
#     define CLEAR_SIZE 213 /* granularity */
      volatile word dummy[CLEAR_SIZE];

      BZERO((/* no volatile */ word *)((word)dummy), sizeof(dummy));
      if ((word)MANAGED_STACK_ADDRESS_BOEHM_GC_approx_sp() COOLER_THAN (word)limit) {
        (void)MANAGED_STACK_ADDRESS_BOEHM_GC_clear_stack_inner(arg, limit);
      }
      /* Make sure the recursive call is not a tail call, and the bzero */
      /* call is not recognized as dead code.                           */
#     if defined(CPPCHECK)
        MANAGED_STACK_ADDRESS_BOEHM_GC_noop1(dummy[0]);
#     else
        MANAGED_STACK_ADDRESS_BOEHM_GC_noop1(COVERT_DATAFLOW(dummy));
#     endif
      return arg;
    }
# endif /* !ASM_CLEAR_CODE */

# ifdef THREADS
    /* Used to occasionally clear a bigger chunk.       */
    /* TODO: Should be more random than it is ...       */
    static unsigned next_random_no(void)
    {
#     ifdef AO_HAVE_fetch_and_add1
        static volatile AO_t random_no;

        return (unsigned)AO_fetch_and_add1(&random_no) % 13;
#     else
        static unsigned random_no = 0;

        return (random_no++) % 13;
#     endif
    }
# endif /* THREADS */

/* Clear some of the inaccessible part of the stack.  Returns its       */
/* argument, so it can be used in a tail call position, hence clearing  */
/* another frame.                                                       */
  MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_clear_stack(void *arg)
  {
    ptr_t sp = MANAGED_STACK_ADDRESS_BOEHM_GC_approx_sp();  /* Hotter than actual sp */
#   ifdef THREADS
        word volatile dummy[SMALL_CLEAR_SIZE];
#   endif

#   define SLOP 400
        /* Extra bytes we clear every time.  This clears our own        */
        /* activation record, and should cause more frequent            */
        /* clearing near the cold end of the stack, a good thing.       */

#   define MANAGED_STACK_ADDRESS_BOEHM_GC_SLOP 4000
        /* We make MANAGED_STACK_ADDRESS_BOEHM_GC_high_water this much hotter than we really saw    */
        /* it, to cover for GC noise etc. above our current frame.      */

#   define CLEAR_THRESHOLD 100000
        /* We restart the clearing process after this many bytes of     */
        /* allocation.  Otherwise very heavily recursive programs       */
        /* with sparse stacks may result in heaps that grow almost      */
        /* without bounds.  As the heap gets larger, collection         */
        /* frequency decreases, thus clearing frequency would decrease, */
        /* thus more junk remains accessible, thus the heap gets        */
        /* larger ...                                                   */

#   ifdef THREADS
      if (next_random_no() == 0) {
        ptr_t limit = sp;

        MAKE_HOTTER(limit, BIG_CLEAR_SIZE*sizeof(word));
        limit = (ptr_t)((word)limit & ~(word)0xf);
                        /* Make it sufficiently aligned for assembly    */
                        /* implementations of MANAGED_STACK_ADDRESS_BOEHM_GC_clear_stack_inner.     */
        return MANAGED_STACK_ADDRESS_BOEHM_GC_clear_stack_inner(arg, limit);
      }
      BZERO((void *)dummy, SMALL_CLEAR_SIZE*sizeof(word));
#   else
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no > MANAGED_STACK_ADDRESS_BOEHM_GC_stack_last_cleared) {
        /* Start things over, so we clear the entire stack again */
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_stack_last_cleared == 0)
          MANAGED_STACK_ADDRESS_BOEHM_GC_high_water = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_stackbottom;
        MANAGED_STACK_ADDRESS_BOEHM_GC_min_sp = MANAGED_STACK_ADDRESS_BOEHM_GC_high_water;
        MANAGED_STACK_ADDRESS_BOEHM_GC_stack_last_cleared = MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no;
        MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd_at_reset = MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd;
      }
      /* Adjust MANAGED_STACK_ADDRESS_BOEHM_GC_high_water */
      MAKE_COOLER(MANAGED_STACK_ADDRESS_BOEHM_GC_high_water, WORDS_TO_BYTES(DEGRADE_RATE) + MANAGED_STACK_ADDRESS_BOEHM_GC_SLOP);
      if ((word)sp HOTTER_THAN (word)MANAGED_STACK_ADDRESS_BOEHM_GC_high_water) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_high_water = sp;
      }
      MAKE_HOTTER(MANAGED_STACK_ADDRESS_BOEHM_GC_high_water, MANAGED_STACK_ADDRESS_BOEHM_GC_SLOP);
      {
        ptr_t limit = MANAGED_STACK_ADDRESS_BOEHM_GC_min_sp;

        MAKE_HOTTER(limit, SLOP);
        if ((word)sp COOLER_THAN (word)limit) {
          limit = (ptr_t)((word)limit & ~(word)0xf);
                          /* Make it sufficiently aligned for assembly  */
                          /* implementations of MANAGED_STACK_ADDRESS_BOEHM_GC_clear_stack_inner.   */
          MANAGED_STACK_ADDRESS_BOEHM_GC_min_sp = sp;
          return MANAGED_STACK_ADDRESS_BOEHM_GC_clear_stack_inner(arg, limit);
        }
      }
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd - MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd_at_reset > CLEAR_THRESHOLD) {
        /* Restart clearing process, but limit how much clearing we do. */
        MANAGED_STACK_ADDRESS_BOEHM_GC_min_sp = sp;
        MAKE_HOTTER(MANAGED_STACK_ADDRESS_BOEHM_GC_min_sp, CLEAR_THRESHOLD/4);
        if ((word)MANAGED_STACK_ADDRESS_BOEHM_GC_min_sp HOTTER_THAN (word)MANAGED_STACK_ADDRESS_BOEHM_GC_high_water)
          MANAGED_STACK_ADDRESS_BOEHM_GC_min_sp = MANAGED_STACK_ADDRESS_BOEHM_GC_high_water;
        MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd_at_reset = MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd;
      }
#   endif
    return arg;
  }

#endif /* !ALWAYS_SMALL_CLEAR_STACK && !STACK_NOT_SCANNED */

/* Return a pointer to the base address of p, given a pointer to a      */
/* an address within an object.  Return 0 o.w.                          */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_base(void * p)
{
    ptr_t r;
    struct hblk *h;
    bottom_index *bi;
    hdr *candidate_hdr;

    r = (ptr_t)p;
    if (!EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized, TRUE)) return NULL;
    h = HBLKPTR(r);
    GET_BI(r, bi);
    candidate_hdr = HDR_FROM_BI(bi, r);
    if (NULL == candidate_hdr) return NULL;
    /* If it's a pointer to the middle of a large object, move it       */
    /* to the beginning.                                                */
        while (IS_FORWARDING_ADDR_OR_NIL(candidate_hdr)) {
           h = FORWARDED_ADDR(h,candidate_hdr);
           r = (ptr_t)h;
           candidate_hdr = HDR(h);
        }
    if (HBLK_IS_FREE(candidate_hdr)) return NULL;
    /* Make sure r points to the beginning of the object */
        r = (ptr_t)((word)r & ~(word)(WORDS_TO_BYTES(1)-1));
        {
            word sz = candidate_hdr -> hb_sz;
            ptr_t limit;

            r -= HBLKDISPL(r) % sz;
            limit = r + sz;
            if (((word)limit > (word)(h + 1) && sz <= HBLKSIZE)
                || (word)p >= (word)limit)
              return NULL;
        }
    return (void *)r;
}

/* Return TRUE if and only if p points to somewhere in GC heap. */
MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_is_heap_ptr(const void *p)
{
    bottom_index *bi;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized);
    GET_BI(p, bi);
    return HDR_FROM_BI(bi, p) != 0;
}

/* Return the size of an object, given a pointer to its base.           */
/* (For small objects this also happens to work from interior pointers, */
/* but that shouldn't be relied upon.)                                  */
MANAGED_STACK_ADDRESS_BOEHM_GC_API size_t MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_size(const void * p)
{
    hdr * hhdr = HDR((/* no const */ void *)(word)p);

    return (size_t)(hhdr -> hb_sz);
}


/* These getters remain unsynchronized for compatibility (since some    */
/* clients could call some of them from a GC callback holding the       */
/* allocator lock).                                                     */
MANAGED_STACK_ADDRESS_BOEHM_GC_API size_t MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_heap_size(void)
{
    /* ignore the memory space returned to OS (i.e. count only the      */
    /* space owned by the garbage collector)                            */
    return (size_t)(MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize - MANAGED_STACK_ADDRESS_BOEHM_GC_unmapped_bytes);
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API size_t MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_obtained_from_os_bytes(void)
{
    return (size_t)MANAGED_STACK_ADDRESS_BOEHM_GC_our_mem_bytes;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API size_t MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_free_bytes(void)
{
    /* ignore the memory space returned to OS */
    return (size_t)(MANAGED_STACK_ADDRESS_BOEHM_GC_large_free_bytes - MANAGED_STACK_ADDRESS_BOEHM_GC_unmapped_bytes);
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API size_t MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_unmapped_bytes(void)
{
    return (size_t)MANAGED_STACK_ADDRESS_BOEHM_GC_unmapped_bytes;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API size_t MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_bytes_since_gc(void)
{
    return (size_t)MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API size_t MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_total_bytes(void)
{
    return (size_t)(MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd + MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd_before_gc);
}

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_GET_HEAP_USAGE_NOT_NEEDED

MANAGED_STACK_ADDRESS_BOEHM_GC_API size_t MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_size_map_at(int i)
{
  if ((unsigned)i > MAXOBJBYTES)
    return MANAGED_STACK_ADDRESS_BOEHM_GC_SIZE_MAX;
  return GRANULES_TO_BYTES(MANAGED_STACK_ADDRESS_BOEHM_GC_size_map[i]);
}

/* Return the heap usage information.  This is a thread-safe (atomic)   */
/* alternative for the five above getters.  NULL pointer is allowed for */
/* any argument.  Returned (filled in) values are of word type.         */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_heap_usage_safe(MANAGED_STACK_ADDRESS_BOEHM_GC_word *pheap_size,
                        MANAGED_STACK_ADDRESS_BOEHM_GC_word *pfree_bytes, MANAGED_STACK_ADDRESS_BOEHM_GC_word *punmapped_bytes,
                        MANAGED_STACK_ADDRESS_BOEHM_GC_word *pbytes_since_gc, MANAGED_STACK_ADDRESS_BOEHM_GC_word *ptotal_bytes)
{
  LOCK();
  if (pheap_size != NULL)
    *pheap_size = MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize - MANAGED_STACK_ADDRESS_BOEHM_GC_unmapped_bytes;
  if (pfree_bytes != NULL)
    *pfree_bytes = MANAGED_STACK_ADDRESS_BOEHM_GC_large_free_bytes - MANAGED_STACK_ADDRESS_BOEHM_GC_unmapped_bytes;
  if (punmapped_bytes != NULL)
    *punmapped_bytes = MANAGED_STACK_ADDRESS_BOEHM_GC_unmapped_bytes;
  if (pbytes_since_gc != NULL)
    *pbytes_since_gc = MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd;
  if (ptotal_bytes != NULL)
    *ptotal_bytes = MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd + MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd_before_gc;
  UNLOCK();
}

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER word MANAGED_STACK_ADDRESS_BOEHM_GC_reclaimed_bytes_before_gc = 0;

  /* Fill in GC statistics provided the destination is of enough size.  */
  static void fill_prof_stats(struct MANAGED_STACK_ADDRESS_BOEHM_GC_prof_stats_s *pstats)
  {
    pstats->heapsize_full = MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize;
    pstats->free_bytes_full = MANAGED_STACK_ADDRESS_BOEHM_GC_large_free_bytes;
    pstats->unmapped_bytes = MANAGED_STACK_ADDRESS_BOEHM_GC_unmapped_bytes;
    pstats->bytes_allocd_since_gc = MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd;
    pstats->allocd_bytes_before_gc = MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd_before_gc;
    pstats->non_gc_bytes = MANAGED_STACK_ADDRESS_BOEHM_GC_non_gc_bytes;
    pstats->gc_no = MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no; /* could be -1 */
#   ifdef PARALLEL_MARK
      pstats->markers_m1 = (word)((signed_word)MANAGED_STACK_ADDRESS_BOEHM_GC_markers_m1);
#   else
      pstats->markers_m1 = 0; /* one marker */
#   endif
    pstats->bytes_reclaimed_since_gc = MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_found > 0 ?
                                        (word)MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_found : 0;
    pstats->reclaimed_bytes_before_gc = MANAGED_STACK_ADDRESS_BOEHM_GC_reclaimed_bytes_before_gc;
    pstats->expl_freed_bytes_since_gc = MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_freed; /* since gc-7.7 */
    pstats->obtained_from_os_bytes = MANAGED_STACK_ADDRESS_BOEHM_GC_our_mem_bytes; /* since gc-8.2 */
  }

# include <string.h> /* for memset() */

  MANAGED_STACK_ADDRESS_BOEHM_GC_API size_t MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_prof_stats(struct MANAGED_STACK_ADDRESS_BOEHM_GC_prof_stats_s *pstats,
                                          size_t stats_sz)
  {
    struct MANAGED_STACK_ADDRESS_BOEHM_GC_prof_stats_s stats;

    LOCK();
    fill_prof_stats(stats_sz >= sizeof(stats) ? pstats : &stats);
    UNLOCK();

    if (stats_sz == sizeof(stats)) {
      return sizeof(stats);
    } else if (stats_sz > sizeof(stats)) {
      /* Fill in the remaining part with -1.    */
      memset((char *)pstats + sizeof(stats), 0xff, stats_sz - sizeof(stats));
      return sizeof(stats);
    } else {
      if (EXPECT(stats_sz > 0, TRUE))
        BCOPY(&stats, pstats, stats_sz);
      return stats_sz;
    }
  }

# ifdef THREADS
    /* The _unsafe version assumes the caller holds the allocation lock. */
    MANAGED_STACK_ADDRESS_BOEHM_GC_API size_t MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_prof_stats_unsafe(
                                            struct MANAGED_STACK_ADDRESS_BOEHM_GC_prof_stats_s *pstats,
                                            size_t stats_sz)
    {
      struct MANAGED_STACK_ADDRESS_BOEHM_GC_prof_stats_s stats;

      if (stats_sz >= sizeof(stats)) {
        fill_prof_stats(pstats);
        if (stats_sz > sizeof(stats))
          memset((char *)pstats + sizeof(stats), 0xff,
                 stats_sz - sizeof(stats));
        return sizeof(stats);
      } else {
        if (EXPECT(stats_sz > 0, TRUE)) {
          fill_prof_stats(&stats);
          BCOPY(&stats, pstats, stats_sz);
        }
        return stats_sz;
      }
    }
# endif /* THREADS */

#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_GET_HEAP_USAGE_NOT_NEEDED */

#if defined(THREADS) && !defined(SIGNAL_BASED_STOP_WORLD)
  /* GC does not use signals to suspend and restart threads.    */
  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_suspend_signal(int sig)
  {
    UNUSED_ARG(sig);
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_thr_restart_signal(int sig)
  {
    UNUSED_ARG(sig);
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_suspend_signal(void)
  {
    return -1;
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_thr_restart_signal(void)
  {
    return -1;
  }
#endif /* THREADS && !SIGNAL_BASED_STOP_WORLD */

#if !defined(_MAX_PATH) && defined(ANY_MSWIN)
# define _MAX_PATH MAX_PATH
#endif

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_READ_ENV_FILE
  /* This works for Win32/WinCE for now.  Really useful only for WinCE. */
  STATIC char *MANAGED_STACK_ADDRESS_BOEHM_GC_envfile_content = NULL;
                        /* The content of the GC "env" file with CR and */
                        /* LF replaced to '\0'.  NULL if the file is    */
                        /* missing or empty.  Otherwise, always ends    */
                        /* with '\0'.                                   */
  STATIC unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_envfile_length = 0;
                        /* Length of MANAGED_STACK_ADDRESS_BOEHM_GC_envfile_content (if non-NULL).  */

# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_ENVFILE_MAXLEN
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ENVFILE_MAXLEN 0x4000
# endif

# define MANAGED_STACK_ADDRESS_BOEHM_GC_ENV_FILE_EXT ".gc.env"

  /* The routine initializes MANAGED_STACK_ADDRESS_BOEHM_GC_envfile_content from the GC "env" file. */
  STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_envfile_init(void)
  {
#   ifdef ANY_MSWIN
      HANDLE hFile;
      char *content;
      unsigned ofs;
      unsigned len;
      DWORD nBytesRead;
      TCHAR path[_MAX_PATH + 0x10]; /* buffer for path + ext */
      size_t bytes_to_get;

      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
      len = (unsigned)GetModuleFileName(NULL /* hModule */, path,
                                        _MAX_PATH + 1);
      /* If GetModuleFileName() has failed then len is 0. */
      if (len > 4 && path[len - 4] == (TCHAR)'.') {
        len -= 4; /* strip executable file extension */
      }
      BCOPY(TEXT(MANAGED_STACK_ADDRESS_BOEHM_GC_ENV_FILE_EXT), &path[len], sizeof(TEXT(MANAGED_STACK_ADDRESS_BOEHM_GC_ENV_FILE_EXT)));
      hFile = CreateFile(path, GENERIC_READ,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL /* lpSecurityAttributes */, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL, NULL /* hTemplateFile */);
      if (hFile == INVALID_HANDLE_VALUE)
        return; /* the file is absent or the operation is failed */
      len = (unsigned)GetFileSize(hFile, NULL);
      if (len <= 1 || len >= MANAGED_STACK_ADDRESS_BOEHM_GC_ENVFILE_MAXLEN) {
        CloseHandle(hFile);
        return; /* invalid file length - ignoring the file content */
      }
      /* At this execution point, MANAGED_STACK_ADDRESS_BOEHM_GC_setpagesize() and MANAGED_STACK_ADDRESS_BOEHM_GC_init_win32()  */
      /* must already be called (for GET_MEM() to work correctly).      */
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_page_size != 0);
      bytes_to_get = ROUNDUP_PAGESIZE_IF_MMAP((size_t)len + 1);
      content = MANAGED_STACK_ADDRESS_BOEHM_GC_os_get_mem(bytes_to_get);
      if (content == NULL) {
        CloseHandle(hFile);
        return; /* allocation failure */
      }
      ofs = 0;
      nBytesRead = (DWORD)-1L;
          /* Last ReadFile() call should clear nBytesRead on success. */
      while (ReadFile(hFile, content + ofs, len - ofs + 1, &nBytesRead,
                      NULL /* lpOverlapped */) && nBytesRead != 0) {
        if ((ofs += nBytesRead) > len)
          break;
      }
      CloseHandle(hFile);
      if (ofs != len || nBytesRead != 0) {
        /* TODO: recycle content */
        return; /* read operation is failed - ignoring the file content */
      }
      content[ofs] = '\0';
      while (ofs-- > 0) {
       if (content[ofs] == '\r' || content[ofs] == '\n')
         content[ofs] = '\0';
      }
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_envfile_content);
      MANAGED_STACK_ADDRESS_BOEHM_GC_envfile_length = len + 1;
      MANAGED_STACK_ADDRESS_BOEHM_GC_envfile_content = content;
#   endif
  }

  /* This routine scans MANAGED_STACK_ADDRESS_BOEHM_GC_envfile_content for the specified            */
  /* environment variable (and returns its value if found).             */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER char * MANAGED_STACK_ADDRESS_BOEHM_GC_envfile_getenv(const char *name)
  {
    char *p;
    char *end_of_content;
    size_t namelen;

#   ifndef NO_GETENV
      p = getenv(name); /* try the standard getenv() first */
      if (p != NULL)
        return *p != '\0' ? p : NULL;
#   endif
    p = MANAGED_STACK_ADDRESS_BOEHM_GC_envfile_content;
    if (p == NULL)
      return NULL; /* "env" file is absent (or empty) */
    namelen = strlen(name);
    if (namelen == 0) /* a sanity check */
      return NULL;
    for (end_of_content = p + MANAGED_STACK_ADDRESS_BOEHM_GC_envfile_length;
         p != end_of_content; p += strlen(p) + 1) {
      if (strncmp(p, name, namelen) == 0 && *(p += namelen) == '=') {
        p++; /* the match is found; skip '=' */
        return *p != '\0' ? p : NULL;
      }
      /* If not matching then skip to the next line. */
    }
    return NULL; /* no match found */
  }
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_READ_ENV_FILE */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized = FALSE;

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_is_init_called(void)
{
  return (int)MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized;
}

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) \
    && ((defined(MSWIN32) && !defined(CONSOLE_LOG)) || defined(MSWINCE))
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER CRITICAL_SECTION MANAGED_STACK_ADDRESS_BOEHM_GC_write_cs;
#endif

#ifndef DONT_USE_ATEXIT
# if !defined(PCR) && !defined(SMALL_CONFIG)
    /* A dedicated variable to avoid a garbage collection on abort.     */
    /* MANAGED_STACK_ADDRESS_BOEHM_GC_find_leak cannot be used for this purpose as otherwise        */
    /* TSan finds a data race (between MANAGED_STACK_ADDRESS_BOEHM_GC_default_on_abort and, e.g.,   */
    /* MANAGED_STACK_ADDRESS_BOEHM_GC_finish_collection).                                           */
    static MANAGED_STACK_ADDRESS_BOEHM_GC_bool skip_gc_atexit = FALSE;
# else
#   define skip_gc_atexit FALSE
# endif

  STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_exit_check(void)
  {
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_find_leak && !skip_gc_atexit) {
#     ifdef THREADS
        /* Check that the thread executing at-exit functions is     */
        /* the same as the one performed the GC initialization,     */
        /* otherwise the latter thread might already be dead but    */
        /* still registered and this, as a consequence, might       */
        /* cause a signal delivery fail when suspending the threads */
        /* on platforms that do not guarantee ESRCH returned if     */
        /* the signal is not delivered.                             */
        /* It should also prevent "Collecting from unknown thread"  */
        /* abort in MANAGED_STACK_ADDRESS_BOEHM_GC_push_all_stacks().                           */
        if (!MANAGED_STACK_ADDRESS_BOEHM_GC_is_main_thread() || !MANAGED_STACK_ADDRESS_BOEHM_GC_thread_is_registered()) return;
#     endif
      MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect();
    }
  }
#endif

#if defined(UNIX_LIKE) && !defined(NO_DEBUGGING)
  static void looping_handler(int sig)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf("Caught signal %d: looping in handler\n", sig);
    for (;;) {
       /* empty */
    }
  }

  static MANAGED_STACK_ADDRESS_BOEHM_GC_bool installed_looping_handler = FALSE;

  static void maybe_install_looping_handler(void)
  {
    /* Install looping handler before the write fault handler, so we    */
    /* handle write faults correctly.                                   */
    if (!installed_looping_handler && 0 != GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_LOOP_ON_ABORT")) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_set_and_save_fault_handler(looping_handler);
      installed_looping_handler = TRUE;
    }
  }

#else /* !UNIX_LIKE */
# define maybe_install_looping_handler()
#endif

#define MANAGED_STACK_ADDRESS_BOEHM_GC_DEFAULT_STDERR_FD 2
#ifdef KOS
# define MANAGED_STACK_ADDRESS_BOEHM_GC_DEFAULT_STDOUT_FD MANAGED_STACK_ADDRESS_BOEHM_GC_DEFAULT_STDERR_FD
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_DEFAULT_STDOUT_FD 1
#endif

#if !defined(OS2) && !defined(MACOS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ANDROID_LOG) \
    && !defined(NN_PLATFORM_CTR) && !defined(NINTENDO_SWITCH) \
    && (!defined(MSWIN32) || defined(CONSOLE_LOG)) && !defined(MSWINCE)
  STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_stdout = MANAGED_STACK_ADDRESS_BOEHM_GC_DEFAULT_STDOUT_FD;
  STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_stderr = MANAGED_STACK_ADDRESS_BOEHM_GC_DEFAULT_STDERR_FD;
  STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_log = MANAGED_STACK_ADDRESS_BOEHM_GC_DEFAULT_STDERR_FD;

# ifndef MSWIN32
    MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_log_fd(int fd)
    {
      MANAGED_STACK_ADDRESS_BOEHM_GC_log = fd;
    }
# endif
#endif

#ifdef MSGBOX_ON_ERROR
  STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_win32_MessageBoxA(const char *msg, const char *caption,
                                   unsigned flags)
  {
#   ifndef DONT_USE_USER32_DLL
      /* Use static binding to "user32.dll".    */
      (void)MessageBoxA(NULL, msg, caption, flags);
#   else
      /* This simplifies linking - resolve "MessageBoxA" at run-time. */
      HINSTANCE hU32 = LoadLibrary(TEXT("user32.dll"));
      if (hU32) {
        FARPROC pfn = GetProcAddress(hU32, "MessageBoxA");
        if (pfn)
          (void)(*(int (WINAPI *)(HWND, LPCSTR, LPCSTR, UINT))
                 (MANAGED_STACK_ADDRESS_BOEHM_GC_funcptr_uint)pfn)(NULL /* hWnd */, msg, caption, flags);
        (void)FreeLibrary(hU32);
      }
#   endif
  }
#endif /* MSGBOX_ON_ERROR */

#if defined(THREADS) && defined(UNIX_LIKE) && !defined(NO_GETCONTEXT)
  static void callee_saves_pushed_dummy_fn(ptr_t data, void *context)
  {
    UNUSED_ARG(data);
    UNUSED_ARG(context);
  }
#endif

#ifndef SMALL_CONFIG
# ifdef MANUAL_VDB
    static MANAGED_STACK_ADDRESS_BOEHM_GC_bool manual_vdb_allowed = TRUE;
# else
    static MANAGED_STACK_ADDRESS_BOEHM_GC_bool manual_vdb_allowed = FALSE;
# endif

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_manual_vdb_allowed(int value)
  {
    manual_vdb_allowed = (MANAGED_STACK_ADDRESS_BOEHM_GC_bool)value;
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_manual_vdb_allowed(void)
  {
    return (int)manual_vdb_allowed;
  }
#endif /* !SMALL_CONFIG */

STATIC word MANAGED_STACK_ADDRESS_BOEHM_GC_parse_mem_size_arg(const char *str)
{
  word result;
  char *endptr;
  char ch;

  if ('\0' == *str) return MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX; /* bad value */
  result = (word)STRTOULL(str, &endptr, 10);
  ch = *endptr;
  if (ch != '\0') {
    if (*(endptr + 1) != '\0') return MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX;
    /* Allow k, M or G suffix.  */
    switch (ch) {
    case 'K':
    case 'k':
      result <<= 10;
      break;
# if CPP_WORDSZ >= 32
    case 'M':
    case 'm':
      result <<= 20;
      break;
    case 'G':
    case 'g':
      result <<= 30;
      break;
# endif
    default:
      result = MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX;
    }
  }
  return result;
}

#define MANAGED_STACK_ADDRESS_BOEHM_GC_LOG_STD_NAME "gc.log"

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_init(void)
{
    /* LOCK(); -- no longer does anything this early. */
    word initial_heap_sz;
    IF_CANCEL(int cancel_state;)

    if (EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized, TRUE)) return;
#   ifdef REDIRECT_MALLOC
      {
        static MANAGED_STACK_ADDRESS_BOEHM_GC_bool init_started = FALSE;
        if (init_started)
          ABORT("Redirected malloc() called during GC init");
        init_started = TRUE;
      }
#   endif

#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_INITIAL_HEAP_SIZE) && !defined(CPPCHECK)
      initial_heap_sz = MANAGED_STACK_ADDRESS_BOEHM_GC_INITIAL_HEAP_SIZE;
#   else
      initial_heap_sz = MINHINCR * HBLKSIZE;
#   endif

    DISABLE_CANCEL(cancel_state);
    /* Note that although we are nominally called with the */
    /* allocation lock held, the allocation lock is now    */
    /* only really acquired once a second thread is forked.*/
    /* And the initialization code needs to run before     */
    /* then.  Thus we really don't hold any locks, and can */
    /* in fact safely initialize them here.                */
#   ifdef THREADS
#     ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_ALWAYS_MULTITHREADED
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(!MANAGED_STACK_ADDRESS_BOEHM_GC_need_to_lock);
#     endif
#     ifdef SN_TARGET_PS3
        {
          pthread_mutexattr_t mattr;

          if (0 != pthread_mutexattr_init(&mattr)) {
            ABORT("pthread_mutexattr_init failed");
          }
          if (0 != pthread_mutex_init(&MANAGED_STACK_ADDRESS_BOEHM_GC_allocate_ml, &mattr)) {
            ABORT("pthread_mutex_init failed");
          }
          (void)pthread_mutexattr_destroy(&mattr);
        }
#     endif
#   endif /* THREADS */
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS)
#     ifndef SPIN_COUNT
#       define SPIN_COUNT 4000
#     endif
#     ifdef MSWINRT_FLAVOR
        InitializeCriticalSectionAndSpinCount(&MANAGED_STACK_ADDRESS_BOEHM_GC_allocate_ml, SPIN_COUNT);
#     else
        {
#         ifndef MSWINCE
            FARPROC pfn = 0;
            HMODULE hK32 = GetModuleHandle(TEXT("kernel32.dll"));
            if (hK32)
              pfn = GetProcAddress(hK32,
                                   "InitializeCriticalSectionAndSpinCount");
            if (pfn) {
              (*(BOOL (WINAPI *)(LPCRITICAL_SECTION, DWORD))
               (MANAGED_STACK_ADDRESS_BOEHM_GC_funcptr_uint)pfn)(&MANAGED_STACK_ADDRESS_BOEHM_GC_allocate_ml, SPIN_COUNT);
            } else
#         endif /* !MSWINCE */
          /* else */ InitializeCriticalSection(&MANAGED_STACK_ADDRESS_BOEHM_GC_allocate_ml);
        }
#     endif
#   endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS && !MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS */
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) \
       && ((defined(MSWIN32) && !defined(CONSOLE_LOG)) || defined(MSWINCE))
      InitializeCriticalSection(&MANAGED_STACK_ADDRESS_BOEHM_GC_write_cs);
#   endif
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ALWAYS_MULTITHREADED)
      LOCK(); /* just to set MANAGED_STACK_ADDRESS_BOEHM_GC_lock_holder */
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_setpagesize();
#   ifdef MSWIN32
      MANAGED_STACK_ADDRESS_BOEHM_GC_init_win32();
#   endif
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_READ_ENV_FILE
      MANAGED_STACK_ADDRESS_BOEHM_GC_envfile_init();
#   endif
#   if !defined(NO_CLOCK) || !defined(SMALL_CONFIG)
#     ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_PRINT_VERBOSE_STATS
        /* This is useful for debugging and profiling on platforms with */
        /* missing getenv() (like WinCE).                               */
        MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats = VERBOSE;
#     else
        if (0 != GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_PRINT_VERBOSE_STATS")) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats = VERBOSE;
        } else if (0 != GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_PRINT_STATS")) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats = 1;
        }
#     endif
#   endif
#   if ((defined(UNIX_LIKE) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ANDROID_LOG)) \
        || (defined(CONSOLE_LOG) && defined(MSWIN32)) \
        || defined(CYGWIN32) || defined(SYMBIAN)) && !defined(SMALL_CONFIG)
        {
          char * file_name = TRUSTED_STRING(GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_LOG_FILE"));
#         ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_LOG_TO_FILE_ALWAYS
            if (NULL == file_name)
              file_name = MANAGED_STACK_ADDRESS_BOEHM_GC_LOG_STD_NAME;
#         else
            if (0 != file_name)
#         endif
          {
#           if defined(_MSC_VER)
              int log_d = _open(file_name, O_CREAT | O_WRONLY | O_APPEND);
#           else
              int log_d = open(file_name, O_CREAT | O_WRONLY | O_APPEND, 0644);
#           endif
            if (log_d < 0) {
              MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf("Failed to open %s as log file\n", file_name);
            } else {
              char *str;
              MANAGED_STACK_ADDRESS_BOEHM_GC_log = log_d;
              str = GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_ONLY_LOG_TO_FILE");
#             ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ONLY_LOG_TO_FILE
                /* The similar environment variable set to "0"  */
                /* overrides the effect of the macro defined.   */
                if (str != NULL && *str == '0' && *(str + 1) == '\0')
#             else
                /* Otherwise setting the environment variable   */
                /* to anything other than "0" will prevent from */
                /* redirecting stdout/err to the log file.      */
                if (str == NULL || (*str == '0' && *(str + 1) == '\0'))
#             endif
              {
                MANAGED_STACK_ADDRESS_BOEHM_GC_stdout = log_d;
                MANAGED_STACK_ADDRESS_BOEHM_GC_stderr = log_d;
              }
            }
          }
        }
#   endif
#   if !defined(NO_DEBUGGING) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DUMP_REGULARLY)
      if (0 != GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_DUMP_REGULARLY")) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_dump_regularly = TRUE;
      }
#   endif
#   ifdef KEEP_BACK_PTRS
      {
        char * backtraces_string = GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_BACKTRACES");
        if (0 != backtraces_string) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_backtraces = atol(backtraces_string);
          if (backtraces_string[0] == '\0') MANAGED_STACK_ADDRESS_BOEHM_GC_backtraces = 1;
        }
      }
#   endif
    if (0 != GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_FIND_LEAK")) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_find_leak = 1;
    }
#   ifndef SHORT_DBG_HDRS
      if (0 != GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_FINDLEAK_DELAY_FREE")) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_findleak_delay_free = TRUE;
      }
#   endif
    if (0 != GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_ALL_INTERIOR_POINTERS")) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers = 1;
    }
    if (0 != GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_DONT_GC")) {
#     if defined(LINT2) \
         && !(defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ALWAYS_MULTITHREADED))
        MANAGED_STACK_ADDRESS_BOEHM_GC_disable();
#     else
        MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc = 1;
#     endif
    }
    if (0 != GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_PRINT_BACK_HEIGHT")) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_print_back_height = TRUE;
    }
    if (0 != GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_NO_BLACKLIST_WARNING")) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_large_alloc_warn_interval = LONG_MAX;
    }
    {
      char * addr_string = GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_TRACE");
      if (0 != addr_string) {
#       ifndef ENABLE_TRACE
          WARN("Tracing not enabled: Ignoring MANAGED_STACK_ADDRESS_BOEHM_GC_TRACE value\n", 0);
#       else
          word addr = (word)STRTOULL(addr_string, NULL, 16);
          if (addr < 0x1000)
              WARN("Unlikely trace address: %p\n", (void *)addr);
          MANAGED_STACK_ADDRESS_BOEHM_GC_trace_addr = (ptr_t)addr;
#       endif
      }
    }
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_COLLECT_AT_MALLOC
      {
        char * string = GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_COLLECT_AT_MALLOC");
        if (0 != string) {
          size_t min_lb = (size_t)STRTOULL(string, NULL, 10);
          if (min_lb > 0)
            MANAGED_STACK_ADDRESS_BOEHM_GC_dbg_collect_at_malloc_min_lb = min_lb;
        }
      }
#   endif
#   if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL) && !defined(NO_CLOCK)
      {
        char * time_limit_string = GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_PAUSE_TIME_TARGET");
        if (0 != time_limit_string) {
          long time_limit = atol(time_limit_string);
          if (time_limit > 0) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_time_limit = (unsigned long)time_limit;
          }
        }
      }
#   endif
#   ifndef SMALL_CONFIG
      {
        char * full_freq_string = GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_FULL_FREQUENCY");
        if (full_freq_string != NULL) {
          int full_freq = atoi(full_freq_string);
          if (full_freq > 0)
            MANAGED_STACK_ADDRESS_BOEHM_GC_full_freq = full_freq;
        }
      }
#   endif
    {
      char * interval_string = GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_LARGE_ALLOC_WARN_INTERVAL");
      if (0 != interval_string) {
        long interval = atol(interval_string);
        if (interval <= 0) {
          WARN("MANAGED_STACK_ADDRESS_BOEHM_GC_LARGE_ALLOC_WARN_INTERVAL environment variable has"
               " bad value - ignoring\n", 0);
        } else {
          MANAGED_STACK_ADDRESS_BOEHM_GC_large_alloc_warn_interval = interval;
        }
      }
    }
    {
        char * space_divisor_string = GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_FREE_SPACE_DIVISOR");
        if (space_divisor_string != NULL) {
          int space_divisor = atoi(space_divisor_string);
          if (space_divisor > 0)
            MANAGED_STACK_ADDRESS_BOEHM_GC_free_space_divisor = (unsigned)space_divisor;
        }
    }
#   ifdef USE_MUNMAP
      {
        char * string = GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_UNMAP_THRESHOLD");
        if (string != NULL) {
          if (*string == '0' && *(string + 1) == '\0') {
            /* "0" is used to disable unmapping. */
            MANAGED_STACK_ADDRESS_BOEHM_GC_unmap_threshold = 0;
          } else {
            int unmap_threshold = atoi(string);
            if (unmap_threshold > 0)
              MANAGED_STACK_ADDRESS_BOEHM_GC_unmap_threshold = (unsigned)unmap_threshold;
          }
        }
      }
      {
        char * string = GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_FORCE_UNMAP_ON_GCOLLECT");
        if (string != NULL) {
          if (*string == '0' && *(string + 1) == '\0') {
            /* "0" is used to turn off the mode. */
            MANAGED_STACK_ADDRESS_BOEHM_GC_force_unmap_on_gcollect = FALSE;
          } else {
            MANAGED_STACK_ADDRESS_BOEHM_GC_force_unmap_on_gcollect = TRUE;
          }
        }
      }
      {
        char * string = GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_USE_ENTIRE_HEAP");
        if (string != NULL) {
          if (*string == '0' && *(string + 1) == '\0') {
            /* "0" is used to turn off the mode. */
            MANAGED_STACK_ADDRESS_BOEHM_GC_use_entire_heap = FALSE;
          } else {
            MANAGED_STACK_ADDRESS_BOEHM_GC_use_entire_heap = TRUE;
          }
        }
      }
#   endif
#   if !defined(NO_DEBUGGING) && !defined(NO_CLOCK)
      GET_TIME(MANAGED_STACK_ADDRESS_BOEHM_GC_init_time);
#   endif
    maybe_install_looping_handler();
#   if ALIGNMENT > MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS
      /* Adjust normal object descriptor for extra allocation.  */
      if (EXTRA_BYTES != 0)
        MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[NORMAL].ok_descriptor =
                        ((~(word)ALIGNMENT) + 1) | MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH;
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_exclude_static_roots_inner(beginMANAGED_STACK_ADDRESS_BOEHM_GC_arrays, endMANAGED_STACK_ADDRESS_BOEHM_GC_arrays);
    MANAGED_STACK_ADDRESS_BOEHM_GC_exclude_static_roots_inner(beginMANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds, endMANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds);
#   ifdef SEPARATE_GLOBALS
      MANAGED_STACK_ADDRESS_BOEHM_GC_exclude_static_roots_inner(beginMANAGED_STACK_ADDRESS_BOEHM_GC_objfreelist, endMANAGED_STACK_ADDRESS_BOEHM_GC_objfreelist);
      MANAGED_STACK_ADDRESS_BOEHM_GC_exclude_static_roots_inner(beginMANAGED_STACK_ADDRESS_BOEHM_GC_aobjfreelist, endMANAGED_STACK_ADDRESS_BOEHM_GC_aobjfreelist);
#   endif
#   if defined(USE_PROC_FOR_LIBRARIES) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_LINUX_THREADS)
        WARN("USE_PROC_FOR_LIBRARIES + MANAGED_STACK_ADDRESS_BOEHM_GC_LINUX_THREADS performs poorly\n", 0);
        /* If thread stacks are cached, they tend to be scanned in      */
        /* entirety as part of the root set.  This will grow them to    */
        /* maximum size, and is generally not desirable.                */
#   endif
#   if !defined(THREADS) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS) \
        || defined(NN_PLATFORM_CTR) || defined(NINTENDO_SWITCH) \
        || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) || defined(MANAGED_STACK_ADDRESS_BOEHM_GC_SOLARIS_THREADS)
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_stackbottom == 0) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_stackbottom = MANAGED_STACK_ADDRESS_BOEHM_GC_get_main_stack_base();
#       if (defined(LINUX) || defined(HPUX)) && defined(IA64)
          MANAGED_STACK_ADDRESS_BOEHM_GC_register_stackbottom = MANAGED_STACK_ADDRESS_BOEHM_GC_get_register_stack_base();
#       endif
      } else {
#       if (defined(LINUX) || defined(HPUX)) && defined(IA64)
          if (MANAGED_STACK_ADDRESS_BOEHM_GC_register_stackbottom == 0) {
            WARN("MANAGED_STACK_ADDRESS_BOEHM_GC_register_stackbottom should be set with MANAGED_STACK_ADDRESS_BOEHM_GC_stackbottom\n",
                 0);
            /* The following may fail, since we may rely on             */
            /* alignment properties that may not hold with a user set   */
            /* MANAGED_STACK_ADDRESS_BOEHM_GC_stackbottom.                                          */
            MANAGED_STACK_ADDRESS_BOEHM_GC_register_stackbottom = MANAGED_STACK_ADDRESS_BOEHM_GC_get_register_stack_base();
          }
#       endif
      }
#   endif
#   if !defined(CPPCHECK)
      MANAGED_STACK_ADDRESS_BOEHM_GC_STATIC_ASSERT(sizeof(ptr_t) == sizeof(word));
      MANAGED_STACK_ADDRESS_BOEHM_GC_STATIC_ASSERT(sizeof(signed_word) == sizeof(word));
      MANAGED_STACK_ADDRESS_BOEHM_GC_STATIC_ASSERT(sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_oom_func) == sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_funcptr_uint));
#     ifdef FUNCPTR_IS_WORD
        MANAGED_STACK_ADDRESS_BOEHM_GC_STATIC_ASSERT(sizeof(word) == sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_funcptr_uint));
#     endif
#     if !defined(_AUX_SOURCE) || defined(__GNUC__)
        MANAGED_STACK_ADDRESS_BOEHM_GC_STATIC_ASSERT((word)(-1) > (word)0);
        /* word should be unsigned */
#     endif
      /* We no longer check for ((void*)(-1) > NULL) since all pointers */
      /* are explicitly cast to word in every less/greater comparison.  */
      MANAGED_STACK_ADDRESS_BOEHM_GC_STATIC_ASSERT((signed_word)(-1) < (signed_word)0);
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_STATIC_ASSERT(sizeof(struct hblk) == HBLKSIZE);
#   ifndef THREADS
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(!((word)MANAGED_STACK_ADDRESS_BOEHM_GC_stackbottom HOTTER_THAN (word)MANAGED_STACK_ADDRESS_BOEHM_GC_approx_sp()));
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_init_headers();
#   ifdef SEARCH_FOR_DATA_START
      /* For MPROTECT_VDB, the temporary fault handler should be        */
      /* installed first, before the write fault one in MANAGED_STACK_ADDRESS_BOEHM_GC_dirty_init.  */
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_REGISTER_MAIN_STATIC_DATA()) MANAGED_STACK_ADDRESS_BOEHM_GC_init_linux_data_start();
#   endif
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_incremental || 0 != GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_ENABLE_INCREMENTAL")) {
#       if defined(BASE_ATOMIC_OPS_EMULATED) || defined(CHECKSUMS) \
           || defined(REDIRECT_MALLOC) || defined(REDIRECT_MALLOC_IN_HEADER) \
           || defined(SMALL_CONFIG)
          /* TODO: Implement CHECKSUMS for manual VDB. */
#       else
          if (manual_vdb_allowed) {
              MANAGED_STACK_ADDRESS_BOEHM_GC_manual_vdb = TRUE;
              MANAGED_STACK_ADDRESS_BOEHM_GC_incremental = TRUE;
          } else
#       endif
        /* else */ {
          /* For GWW_VDB on Win32, this needs to happen before any      */
          /* heap memory is allocated.                                  */
          MANAGED_STACK_ADDRESS_BOEHM_GC_incremental = MANAGED_STACK_ADDRESS_BOEHM_GC_dirty_init();
          MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd == 0);
        }
      }
#   endif

    /* Add initial guess of root sets.  Do this first, since sbrk(0)    */
    /* might be used.                                                   */
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_REGISTER_MAIN_STATIC_DATA()) MANAGED_STACK_ADDRESS_BOEHM_GC_register_data_segments();

    MANAGED_STACK_ADDRESS_BOEHM_GC_bl_init();
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_init();
    {
        char * sz_str = GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_INITIAL_HEAP_SIZE");
        if (sz_str != NULL) {
          word value = MANAGED_STACK_ADDRESS_BOEHM_GC_parse_mem_size_arg(sz_str);
          if (MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX == value) {
            WARN("Bad initial heap size %s - ignoring\n", sz_str);
          } else {
            initial_heap_sz = value;
          }
        }
    }
    {
        char * sz_str = GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_MAXIMUM_HEAP_SIZE");
        if (sz_str != NULL) {
          word max_heap_sz = MANAGED_STACK_ADDRESS_BOEHM_GC_parse_mem_size_arg(sz_str);
          if (max_heap_sz < initial_heap_sz || MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX == max_heap_sz) {
            WARN("Bad maximum heap size %s - ignoring\n", sz_str);
          } else {
            if (0 == MANAGED_STACK_ADDRESS_BOEHM_GC_max_retries) MANAGED_STACK_ADDRESS_BOEHM_GC_max_retries = 2;
            MANAGED_STACK_ADDRESS_BOEHM_GC_set_max_heap_size(max_heap_sz);
          }
        }
    }
    if (initial_heap_sz != 0) {
      if (!MANAGED_STACK_ADDRESS_BOEHM_GC_expand_hp_inner(divHBLKSZ(initial_heap_sz))) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf("Can't start up: not enough memory\n");
        EXIT();
      } else {
        MANAGED_STACK_ADDRESS_BOEHM_GC_requested_heapsize += initial_heap_sz;
      }
    }
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers)
      MANAGED_STACK_ADDRESS_BOEHM_GC_initialize_offsets();
    MANAGED_STACK_ADDRESS_BOEHM_GC_register_displacement_inner(0L);
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_LINUX_THREADS) && defined(REDIRECT_MALLOC)
      if (!MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers) {
        /* TLS ABI uses pointer-sized offsets for dtv. */
        MANAGED_STACK_ADDRESS_BOEHM_GC_register_displacement_inner(sizeof(void *));
      }
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_init_size_map();
#   ifdef PCR
      if (PCR_IL_Lock(PCR_Bool_false, PCR_allSigsBlocked, PCR_waitForever)
          != PCR_ERes_okay) {
          ABORT("Can't lock load state");
      } else if (PCR_IL_Unlock() != PCR_ERes_okay) {
          ABORT("Can't unlock load state");
      }
      PCR_IL_Unlock();
      MANAGED_STACK_ADDRESS_BOEHM_GC_pcr_install();
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized = TRUE;
#   ifdef THREADS
#       if defined(LINT2) \
           && !(defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ALWAYS_MULTITHREADED))
          LOCK();
          MANAGED_STACK_ADDRESS_BOEHM_GC_thr_init();
          UNLOCK();
#       else
          MANAGED_STACK_ADDRESS_BOEHM_GC_thr_init();
#       endif
#   endif
    COND_DUMP;
    /* Get black list set up and/or incremental GC started */
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_dont_precollect || MANAGED_STACK_ADDRESS_BOEHM_GC_incremental) {
#       if defined(DYNAMIC_LOADING) && defined(DARWIN)
          MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(0 == MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd);
#       endif
        MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect_inner();
    }
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ALWAYS_MULTITHREADED)
        UNLOCK();
#   endif
#   if defined(THREADS) && defined(UNIX_LIKE) && !defined(NO_GETCONTEXT)
      /* Ensure getcontext_works is set to avoid potential data race.   */
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc || MANAGED_STACK_ADDRESS_BOEHM_GC_dont_precollect)
        MANAGED_STACK_ADDRESS_BOEHM_GC_with_callee_saves_pushed(callee_saves_pushed_dummy_fn, NULL);
#   endif
#   ifndef DONT_USE_ATEXIT
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_find_leak) {
        /* This is to give us at least one chance to detect leaks.        */
        /* This may report some very benign leaks, but ...                */
        atexit(MANAGED_STACK_ADDRESS_BOEHM_GC_exit_check);
      }
#   endif

    /* The rest of this again assumes we don't really hold      */
    /* the allocation lock.                                     */

#   ifdef THREADS
      /* Initialize thread-local allocation.    */
      MANAGED_STACK_ADDRESS_BOEHM_GC_init_parallel();
#   endif

#   if defined(DYNAMIC_LOADING) && defined(DARWIN)
        /* This must be called WITHOUT the allocation lock held */
        /* and before any threads are created.                  */
        MANAGED_STACK_ADDRESS_BOEHM_GC_init_dyld();
#   endif
    RESTORE_CANCEL(cancel_state);
    /* It is not safe to allocate any object till completion of MANAGED_STACK_ADDRESS_BOEHM_GC_init */
    /* (in particular by MANAGED_STACK_ADDRESS_BOEHM_GC_thr_init), i.e. before MANAGED_STACK_ADDRESS_BOEHM_GC_init_dyld() call  */
    /* and initialization of the incremental mode (if any).             */
#   if defined(GWW_VDB) && !defined(KEEP_BACK_PTRS)
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd + MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd_before_gc == 0);
#   endif
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_enable_incremental(void)
{
# if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL) && !defined(KEEP_BACK_PTRS)
    /* If we are keeping back pointers, the GC itself dirties all */
    /* pages on which objects have been marked, making            */
    /* incremental GC pointless.                                  */
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_find_leak && 0 == GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL")) {
      LOCK();
      if (!MANAGED_STACK_ADDRESS_BOEHM_GC_incremental) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_setpagesize();
        /* TODO: Should we skip enabling incremental if win32s? */
        maybe_install_looping_handler(); /* Before write fault handler! */
        if (!MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized) {
          UNLOCK();
          MANAGED_STACK_ADDRESS_BOEHM_GC_incremental = TRUE; /* indicate intention to turn it on */
          MANAGED_STACK_ADDRESS_BOEHM_GC_init();
          LOCK();
        } else {
#         if !defined(BASE_ATOMIC_OPS_EMULATED) && !defined(CHECKSUMS) \
             && !defined(REDIRECT_MALLOC) \
             && !defined(REDIRECT_MALLOC_IN_HEADER) && !defined(SMALL_CONFIG)
            if (manual_vdb_allowed) {
              MANAGED_STACK_ADDRESS_BOEHM_GC_manual_vdb = TRUE;
              MANAGED_STACK_ADDRESS_BOEHM_GC_incremental = TRUE;
            } else
#         endif
          /* else */ {
            MANAGED_STACK_ADDRESS_BOEHM_GC_incremental = MANAGED_STACK_ADDRESS_BOEHM_GC_dirty_init();
          }
        }
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_incremental && !MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc) {
                                /* Can't easily do it if MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc.    */
          IF_CANCEL(int cancel_state;)

          DISABLE_CANCEL(cancel_state);
          if (MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd > 0) {
            /* There may be unmarked reachable objects. */
            MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect_inner();
          } else {
            /* We are OK in assuming everything is      */
            /* clean since nothing can point to an      */
            /* unmarked object.                         */
#           ifdef CHECKSUMS
              MANAGED_STACK_ADDRESS_BOEHM_GC_read_dirty(FALSE);
#           else
              MANAGED_STACK_ADDRESS_BOEHM_GC_read_dirty(TRUE);
#           endif
          }
          RESTORE_CANCEL(cancel_state);
        }
      }
      UNLOCK();
      return;
    }
# endif
  MANAGED_STACK_ADDRESS_BOEHM_GC_init();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_start_mark_threads(void)
{
#   ifdef PARALLEL_MARK
      IF_CANCEL(int cancel_state;)

      DISABLE_CANCEL(cancel_state);
      LOCK();
      MANAGED_STACK_ADDRESS_BOEHM_GC_start_mark_threads_inner();
      UNLOCK();
      RESTORE_CANCEL(cancel_state);
#   else
      /* No action since parallel markers are disabled (or no POSIX fork). */
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_DONT_HOLD_LOCK());
#   endif
}

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_deinit(void)
  {
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized) {
      /* Prevent duplicate resource close.  */
      MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized = FALSE;
      MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd = 0;
      MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd_before_gc = 0;
#     if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) && (defined(MSWIN32) || defined(MSWINCE))
#       if !defined(CONSOLE_LOG) || defined(MSWINCE)
          DeleteCriticalSection(&MANAGED_STACK_ADDRESS_BOEHM_GC_write_cs);
#       endif
#       ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_PTHREADS
          DeleteCriticalSection(&MANAGED_STACK_ADDRESS_BOEHM_GC_allocate_ml);
#       endif
#     endif
    }
  }

#if (defined(MSWIN32) && !defined(CONSOLE_LOG)) || defined(MSWINCE)

  STATIC HANDLE MANAGED_STACK_ADDRESS_BOEHM_GC_log = 0;

# ifdef THREADS
#   if defined(PARALLEL_MARK) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ALWAYS_MULTITHREADED)
#     define IF_NEED_TO_LOCK(x) if (MANAGED_STACK_ADDRESS_BOEHM_GC_parallel || MANAGED_STACK_ADDRESS_BOEHM_GC_need_to_lock) x
#   else
#     define IF_NEED_TO_LOCK(x) if (MANAGED_STACK_ADDRESS_BOEHM_GC_need_to_lock) x
#   endif
# else
#   define IF_NEED_TO_LOCK(x)
# endif /* !THREADS */

# ifdef MSWINRT_FLAVOR
#   include <windows.storage.h>

    /* This API is defined in roapi.h, but we cannot include it here    */
    /* since it does not compile in C.                                  */
    DECLSPEC_IMPORT HRESULT WINAPI RoGetActivationFactory(
                                        HSTRING activatableClassId,
                                        REFIID iid, void** factory);

    static MANAGED_STACK_ADDRESS_BOEHM_GC_bool getWinRTLogPath(wchar_t* buf, size_t bufLen)
    {
      static const GUID kIID_IApplicationDataStatics = {
        0x5612147B, 0xE843, 0x45E3,
        0x94, 0xD8, 0x06, 0x16, 0x9E, 0x3C, 0x8E, 0x17
      };
      static const GUID kIID_IStorageItem = {
        0x4207A996, 0xCA2F, 0x42F7,
        0xBD, 0xE8, 0x8B, 0x10, 0x45, 0x7A, 0x7F, 0x30
      };
      MANAGED_STACK_ADDRESS_BOEHM_GC_bool result = FALSE;
      HSTRING_HEADER appDataClassNameHeader;
      HSTRING appDataClassName;
      __x_ABI_CWindows_CStorage_CIApplicationDataStatics* appDataStatics = 0;

      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(bufLen > 0);
      if (SUCCEEDED(WindowsCreateStringReference(
                      RuntimeClass_Windows_Storage_ApplicationData,
                      (sizeof(RuntimeClass_Windows_Storage_ApplicationData)-1)
                        / sizeof(wchar_t),
                      &appDataClassNameHeader, &appDataClassName))
          && SUCCEEDED(RoGetActivationFactory(appDataClassName,
                                              &kIID_IApplicationDataStatics,
                                              &appDataStatics))) {
        __x_ABI_CWindows_CStorage_CIApplicationData* appData = NULL;
        __x_ABI_CWindows_CStorage_CIStorageFolder* tempFolder = NULL;
        __x_ABI_CWindows_CStorage_CIStorageItem* tempFolderItem = NULL;
        HSTRING tempPath = NULL;

        if (SUCCEEDED(appDataStatics->lpVtbl->get_Current(appDataStatics,
                                                          &appData))
            && SUCCEEDED(appData->lpVtbl->get_TemporaryFolder(appData,
                                                              &tempFolder))
            && SUCCEEDED(tempFolder->lpVtbl->QueryInterface(tempFolder,
                                                        &kIID_IStorageItem,
                                                        &tempFolderItem))
            && SUCCEEDED(tempFolderItem->lpVtbl->get_Path(tempFolderItem,
                                                          &tempPath))) {
          UINT32 tempPathLen;
          const wchar_t* tempPathBuf =
                          WindowsGetStringRawBuffer(tempPath, &tempPathLen);

          buf[0] = '\0';
          if (wcsncat_s(buf, bufLen, tempPathBuf, tempPathLen) == 0
              && wcscat_s(buf, bufLen, L"\\") == 0
              && wcscat_s(buf, bufLen, TEXT(MANAGED_STACK_ADDRESS_BOEHM_GC_LOG_STD_NAME)) == 0)
            result = TRUE;
          WindowsDeleteString(tempPath);
        }

        if (tempFolderItem != NULL)
          tempFolderItem->lpVtbl->Release(tempFolderItem);
        if (tempFolder != NULL)
          tempFolder->lpVtbl->Release(tempFolder);
        if (appData != NULL)
          appData->lpVtbl->Release(appData);
        appDataStatics->lpVtbl->Release(appDataStatics);
      }
      return result;
    }
# endif /* MSWINRT_FLAVOR */

  STATIC HANDLE MANAGED_STACK_ADDRESS_BOEHM_GC_CreateLogFile(void)
  {
    HANDLE hFile;
# ifdef MSWINRT_FLAVOR
      TCHAR pathBuf[_MAX_PATH + 0x10]; /* buffer for path + ext */

      hFile = INVALID_HANDLE_VALUE;
      if (getWinRTLogPath(pathBuf, _MAX_PATH + 1)) {
        CREATEFILE2_EXTENDED_PARAMETERS extParams;

        BZERO(&extParams, sizeof(extParams));
        extParams.dwSize = sizeof(extParams);
        extParams.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        extParams.dwFileFlags = MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats == VERBOSE ? 0
                                    : FILE_FLAG_WRITE_THROUGH;
        hFile = CreateFile2(pathBuf, GENERIC_WRITE, FILE_SHARE_READ,
                            CREATE_ALWAYS, &extParams);
      }

# else
    TCHAR *logPath;
#   if defined(NO_GETENV_WIN32) && defined(CPPCHECK)
#     define appendToFile FALSE
#   else
      BOOL appendToFile = FALSE;
#   endif
#   if !defined(NO_GETENV_WIN32) || !defined(OLD_WIN32_LOG_FILE)
      TCHAR pathBuf[_MAX_PATH + 0x10]; /* buffer for path + ext */

      logPath = pathBuf;
#   endif

    /* Use GetEnvironmentVariable instead of GETENV() for unicode support. */
#   ifndef NO_GETENV_WIN32
      if (GetEnvironmentVariable(TEXT("MANAGED_STACK_ADDRESS_BOEHM_GC_LOG_FILE"), pathBuf,
                                 _MAX_PATH + 1) - 1U < (DWORD)_MAX_PATH) {
        appendToFile = TRUE;
      } else
#   endif
    /* else */ {
      /* Env var not found or its value too long.       */
#     ifdef OLD_WIN32_LOG_FILE
        logPath = TEXT(MANAGED_STACK_ADDRESS_BOEHM_GC_LOG_STD_NAME);
#     else
        int len = (int)GetModuleFileName(NULL /* hModule */, pathBuf,
                                         _MAX_PATH + 1);
        /* If GetModuleFileName() has failed then len is 0. */
        if (len > 4 && pathBuf[len - 4] == (TCHAR)'.') {
          len -= 4; /* strip executable file extension */
        }
        BCOPY(TEXT(".") TEXT(MANAGED_STACK_ADDRESS_BOEHM_GC_LOG_STD_NAME), &pathBuf[len],
              sizeof(TEXT(".") TEXT(MANAGED_STACK_ADDRESS_BOEHM_GC_LOG_STD_NAME)));
#     endif
    }

    hFile = CreateFile(logPath, GENERIC_WRITE, FILE_SHARE_READ,
                       NULL /* lpSecurityAttributes */,
                       appendToFile ? OPEN_ALWAYS : CREATE_ALWAYS,
                       MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats == VERBOSE ? FILE_ATTRIBUTE_NORMAL :
                            /* immediately flush writes unless very verbose */
                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                       NULL /* hTemplateFile */);

#   ifndef NO_GETENV_WIN32
      if (appendToFile && hFile != INVALID_HANDLE_VALUE) {
        LONG posHigh = 0;
        (void)SetFilePointer(hFile, 0, &posHigh, FILE_END);
                                  /* Seek to file end (ignoring any error) */
      }
#   endif
#   undef appendToFile
# endif
    return hFile;
  }

  STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_write(const char *buf, size_t len)
  {
      BOOL res;
      DWORD written;
#     if defined(THREADS) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS)
        static MANAGED_STACK_ADDRESS_BOEHM_GC_bool inside_write = FALSE;
                        /* to prevent infinite recursion at abort.      */
        if (inside_write)
          return -1;
#     endif

      if (len == 0)
          return 0;
      IF_NEED_TO_LOCK(EnterCriticalSection(&MANAGED_STACK_ADDRESS_BOEHM_GC_write_cs));
#     if defined(THREADS) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS)
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_write_disabled) {
          inside_write = TRUE;
          ABORT("Assertion failure: MANAGED_STACK_ADDRESS_BOEHM_GC_write called with write_disabled");
        }
#     endif
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_log == 0) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_log = MANAGED_STACK_ADDRESS_BOEHM_GC_CreateLogFile();
      }
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_log == INVALID_HANDLE_VALUE) {
        IF_NEED_TO_LOCK(LeaveCriticalSection(&MANAGED_STACK_ADDRESS_BOEHM_GC_write_cs));
#       ifdef NO_DEBUGGING
          /* Ignore open log failure (e.g., it might be caused by       */
          /* read-only folder of the client application).               */
          return 0;
#       else
          return -1;
#       endif
      }
      res = WriteFile(MANAGED_STACK_ADDRESS_BOEHM_GC_log, buf, (DWORD)len, &written, NULL);
#     if defined(_MSC_VER) && defined(_DEBUG) && !defined(NO_CRT)
#         ifdef MSWINCE
              /* There is no CrtDbgReport() in WinCE */
              {
                  WCHAR wbuf[1024];
                  /* Always use Unicode variant of OutputDebugString() */
                  wbuf[MultiByteToWideChar(CP_ACP, 0 /* dwFlags */,
                                buf, len, wbuf,
                                sizeof(wbuf) / sizeof(wbuf[0]) - 1)] = 0;
                  OutputDebugStringW(wbuf);
              }
#         else
              _CrtDbgReport(_CRT_WARN, NULL, 0, NULL, "%.*s", len, buf);
#         endif
#     endif
      IF_NEED_TO_LOCK(LeaveCriticalSection(&MANAGED_STACK_ADDRESS_BOEHM_GC_write_cs));
      return res ? (int)written : -1;
  }

  /* TODO: This is pretty ugly ... */
# define WRITE(f, buf, len) MANAGED_STACK_ADDRESS_BOEHM_GC_write(buf, len)

#elif defined(OS2) || defined(MACOS)
  STATIC FILE * MANAGED_STACK_ADDRESS_BOEHM_GC_stdout = NULL;
  STATIC FILE * MANAGED_STACK_ADDRESS_BOEHM_GC_stderr = NULL;
  STATIC FILE * MANAGED_STACK_ADDRESS_BOEHM_GC_log = NULL;

  /* Initialize MANAGED_STACK_ADDRESS_BOEHM_GC_log (and the friends) passed to MANAGED_STACK_ADDRESS_BOEHM_GC_write().  */
  STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_set_files(void)
  {
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_stdout == NULL) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_stdout = stdout;
    }
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_stderr == NULL) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_stderr = stderr;
    }
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_log == NULL) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_log = stderr;
    }
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE int MANAGED_STACK_ADDRESS_BOEHM_GC_write(FILE *f, const char *buf, size_t len)
  {
    int res = fwrite(buf, 1, len, f);
    fflush(f);
    return res;
  }

# define WRITE(f, buf, len) (MANAGED_STACK_ADDRESS_BOEHM_GC_set_files(), MANAGED_STACK_ADDRESS_BOEHM_GC_write(f, buf, len))

#elif defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ANDROID_LOG)

# include <android/log.h>

# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_ANDROID_LOG_TAG
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_ANDROID_LOG_TAG "BDWGC"
# endif

# define MANAGED_STACK_ADDRESS_BOEHM_GC_stdout ANDROID_LOG_DEBUG
# define MANAGED_STACK_ADDRESS_BOEHM_GC_stderr ANDROID_LOG_ERROR
# define MANAGED_STACK_ADDRESS_BOEHM_GC_log MANAGED_STACK_ADDRESS_BOEHM_GC_stdout

# define WRITE(level, buf, unused_len) \
                __android_log_write(level, MANAGED_STACK_ADDRESS_BOEHM_GC_ANDROID_LOG_TAG, buf)

#elif defined(NN_PLATFORM_CTR)
  int n3ds_log_write(const char* text, int length);
# define WRITE(level, buf, len) n3ds_log_write(buf, len)

#elif defined(NINTENDO_SWITCH)
  int switch_log_write(const char* text, int length);
# define WRITE(level, buf, len) switch_log_write(buf, len)

#else

# if !defined(ECOS) && !defined(NOSYS) && !defined(PLATFORM_WRITE) \
     && !defined(SN_TARGET_PSP2)
#   include <errno.h>
# endif

  STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_write(int fd, const char *buf, size_t len)
  {
#   if defined(ECOS) || defined(PLATFORM_WRITE) || defined(SN_TARGET_PSP2) \
       || defined(NOSYS)
#     ifdef ECOS
        /* FIXME: This seems to be defined nowhere at present.  */
        /* _Jv_diag_write(buf, len); */
#     else
        /* No writing.  */
#     endif
      return (int)len;
#   else
      int bytes_written = 0;
      IF_CANCEL(int cancel_state;)

      DISABLE_CANCEL(cancel_state);
      while ((unsigned)bytes_written < len) {
#        ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_SOLARIS_THREADS
             int result = syscall(SYS_write, fd, buf + bytes_written,
                                             len - bytes_written);
#        elif defined(_MSC_VER)
             int result = _write(fd, buf + bytes_written,
                                 (unsigned)(len - bytes_written));
#        else
             int result = (int)write(fd, buf + bytes_written,
                                     len - (size_t)bytes_written);
#        endif

         if (-1 == result) {
             if (EAGAIN == errno) /* Resource temporarily unavailable */
               continue;
             RESTORE_CANCEL(cancel_state);
             return result;
         }
         bytes_written += result;
      }
      RESTORE_CANCEL(cancel_state);
      return bytes_written;
#   endif
  }

# define WRITE(f, buf, len) MANAGED_STACK_ADDRESS_BOEHM_GC_write(f, buf, len)
#endif /* !MSWINCE && !OS2 && !MACOS && !MANAGED_STACK_ADDRESS_BOEHM_GC_ANDROID_LOG */

#define BUFSZ 1024

#if defined(DJGPP) || defined(__STRICT_ANSI__)
  /* vsnprintf is missing in DJGPP (v2.0.3) */
# define MANAGED_STACK_ADDRESS_BOEHM_GC_VSNPRINTF(buf, bufsz, format, args) vsprintf(buf, format, args)
#elif defined(_MSC_VER)
# ifdef MSWINCE
    /* _vsnprintf is deprecated in WinCE */
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_VSNPRINTF StringCchVPrintfA
# else
#   define MANAGED_STACK_ADDRESS_BOEHM_GC_VSNPRINTF _vsnprintf
# endif
#else
# define MANAGED_STACK_ADDRESS_BOEHM_GC_VSNPRINTF vsnprintf
#endif

/* A version of printf that is unlikely to call malloc, and is thus safer */
/* to call from the collector in case malloc has been bound to MANAGED_STACK_ADDRESS_BOEHM_GC_malloc. */
/* Floating point arguments and formats should be avoided, since FP       */
/* conversion is more likely to allocate memory.                          */
/* Assumes that no more than BUFSZ-1 characters are written at once.      */
#define MANAGED_STACK_ADDRESS_BOEHM_GC_PRINTF_FILLBUF(buf, format) \
        do { \
          va_list args; \
          va_start(args, format); \
          (buf)[sizeof(buf) - 1] = 0x15; /* guard */ \
          (void)MANAGED_STACK_ADDRESS_BOEHM_GC_VSNPRINTF(buf, sizeof(buf) - 1, format, args); \
          va_end(args); \
          if ((buf)[sizeof(buf) - 1] != 0x15) \
            ABORT("MANAGED_STACK_ADDRESS_BOEHM_GC_printf clobbered stack"); \
        } while (0)

void MANAGED_STACK_ADDRESS_BOEHM_GC_printf(const char *format, ...)
{
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_quiet) {
      char buf[BUFSZ + 1];

      MANAGED_STACK_ADDRESS_BOEHM_GC_PRINTF_FILLBUF(buf, format);
#     ifdef NACL
        (void)WRITE(MANAGED_STACK_ADDRESS_BOEHM_GC_stdout, buf, strlen(buf));
        /* Ignore errors silently.      */
#     else
        if (WRITE(MANAGED_STACK_ADDRESS_BOEHM_GC_stdout, buf, strlen(buf)) < 0
#           if defined(CYGWIN32) || (defined(CONSOLE_LOG) && defined(MSWIN32))
              && MANAGED_STACK_ADDRESS_BOEHM_GC_stdout != MANAGED_STACK_ADDRESS_BOEHM_GC_DEFAULT_STDOUT_FD
#           endif
           ) {
          ABORT("write to stdout failed");
        }
#     endif
    }
}

void MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf(const char *format, ...)
{
    char buf[BUFSZ + 1];

    MANAGED_STACK_ADDRESS_BOEHM_GC_PRINTF_FILLBUF(buf, format);
    MANAGED_STACK_ADDRESS_BOEHM_GC_err_puts(buf);
}

void MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf(const char *format, ...)
{
    char buf[BUFSZ + 1];

    MANAGED_STACK_ADDRESS_BOEHM_GC_PRINTF_FILLBUF(buf, format);
#   ifdef NACL
      (void)WRITE(MANAGED_STACK_ADDRESS_BOEHM_GC_log, buf, strlen(buf));
#   else
      if (WRITE(MANAGED_STACK_ADDRESS_BOEHM_GC_log, buf, strlen(buf)) < 0
#         if defined(CYGWIN32) || (defined(CONSOLE_LOG) && defined(MSWIN32))
            && MANAGED_STACK_ADDRESS_BOEHM_GC_log != MANAGED_STACK_ADDRESS_BOEHM_GC_DEFAULT_STDERR_FD
#         endif
         ) {
        ABORT("write to GC log failed");
      }
#   endif
}

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_ANDROID_LOG

# define MANAGED_STACK_ADDRESS_BOEHM_GC_warn_printf MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf

#else

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_info_log_printf(const char *format, ...)
  {
    char buf[BUFSZ + 1];

    MANAGED_STACK_ADDRESS_BOEHM_GC_PRINTF_FILLBUF(buf, format);
    (void)WRITE(ANDROID_LOG_INFO, buf, 0 /* unused */);
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_verbose_log_printf(const char *format, ...)
  {
    char buf[BUFSZ + 1];

    MANAGED_STACK_ADDRESS_BOEHM_GC_PRINTF_FILLBUF(buf, format);
    (void)WRITE(ANDROID_LOG_VERBOSE, buf, 0); /* ignore write errors */
  }

  STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_warn_printf(const char *format, ...)
  {
    char buf[BUFSZ + 1];

    MANAGED_STACK_ADDRESS_BOEHM_GC_PRINTF_FILLBUF(buf, format);
    (void)WRITE(ANDROID_LOG_WARN, buf, 0);
  }

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_ANDROID_LOG */

void MANAGED_STACK_ADDRESS_BOEHM_GC_err_puts(const char *s)
{
    (void)WRITE(MANAGED_STACK_ADDRESS_BOEHM_GC_stderr, s, strlen(s)); /* ignore errors */
}

STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK MANAGED_STACK_ADDRESS_BOEHM_GC_default_warn_proc(char *msg, MANAGED_STACK_ADDRESS_BOEHM_GC_word arg)
{
    /* TODO: Add assertion on arg comply with msg (format).     */
    MANAGED_STACK_ADDRESS_BOEHM_GC_warn_printf(msg, arg);
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_warn_proc MANAGED_STACK_ADDRESS_BOEHM_GC_current_warn_proc = MANAGED_STACK_ADDRESS_BOEHM_GC_default_warn_proc;

/* This is recommended for production code (release). */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK MANAGED_STACK_ADDRESS_BOEHM_GC_ignore_warn_proc(char *msg, MANAGED_STACK_ADDRESS_BOEHM_GC_word arg)
{
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats) {
      /* Don't ignore warnings if stats printing is on. */
      MANAGED_STACK_ADDRESS_BOEHM_GC_default_warn_proc(msg, arg);
    }
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_warn_proc(MANAGED_STACK_ADDRESS_BOEHM_GC_warn_proc p)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NONNULL_ARG_NOT_NULL(p));
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS
#     ifdef CYGWIN32
        /* Need explicit MANAGED_STACK_ADDRESS_BOEHM_GC_INIT call */
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized);
#     else
        if (!MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized) MANAGED_STACK_ADDRESS_BOEHM_GC_init();
#     endif
#   endif
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_current_warn_proc = p;
    UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_warn_proc MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_warn_proc(void)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_warn_proc result;

    LOCK();
    result = MANAGED_STACK_ADDRESS_BOEHM_GC_current_warn_proc;
    UNLOCK();
    return result;
}

#if !defined(PCR) && !defined(SMALL_CONFIG)
  /* Print (or display) a message before abnormal exit (including       */
  /* abort).  Invoked from ABORT(msg) macro (there msg is non-NULL)     */
  /* and from EXIT() macro (msg is NULL in that case).                  */
  STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK MANAGED_STACK_ADDRESS_BOEHM_GC_default_on_abort(const char *msg)
  {
#   ifndef DONT_USE_ATEXIT
      skip_gc_atexit = TRUE; /* disable at-exit MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect() */
#   endif

    if (msg != NULL) {
#     ifdef MSGBOX_ON_ERROR
        MANAGED_STACK_ADDRESS_BOEHM_GC_win32_MessageBoxA(msg, "Fatal error in GC", MB_ICONERROR | MB_OK);
        /* Also duplicate msg to GC log file.   */
#     endif

#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_ANDROID_LOG
      /* Avoid calling MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf() here, as MANAGED_STACK_ADDRESS_BOEHM_GC_on_abort() could be  */
      /* called from it.  Note 1: this is not an atomic output.         */
      /* Note 2: possible write errors are ignored.                     */
#     if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_WIN32_THREADS) && defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) \
         && ((defined(MSWIN32) && !defined(CONSOLE_LOG)) || defined(MSWINCE))
        if (!MANAGED_STACK_ADDRESS_BOEHM_GC_write_disabled)
#     endif
      {
        if (WRITE(MANAGED_STACK_ADDRESS_BOEHM_GC_stderr, msg, strlen(msg)) >= 0)
          (void)WRITE(MANAGED_STACK_ADDRESS_BOEHM_GC_stderr, "\n", 1);
      }
#   else
      __android_log_assert("*" /* cond */, MANAGED_STACK_ADDRESS_BOEHM_GC_ANDROID_LOG_TAG, "%s\n", msg);
#   endif
    }

#   if !defined(NO_DEBUGGING) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ANDROID_LOG)
      if (GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_LOOP_ON_ABORT") != NULL) {
            /* In many cases it's easier to debug a running process.    */
            /* It's arguably nicer to sleep, but that makes it harder   */
            /* to look at the thread if the debugger doesn't know much  */
            /* about threads.                                           */
            for(;;) {
              /* Empty */
            }
      }
#   endif
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_abort_func MANAGED_STACK_ADDRESS_BOEHM_GC_on_abort = MANAGED_STACK_ADDRESS_BOEHM_GC_default_on_abort;

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_abort_func(MANAGED_STACK_ADDRESS_BOEHM_GC_abort_func fn)
  {
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NONNULL_ARG_NOT_NULL(fn));
      LOCK();
      MANAGED_STACK_ADDRESS_BOEHM_GC_on_abort = fn;
      UNLOCK();
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_abort_func MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_abort_func(void)
  {
      MANAGED_STACK_ADDRESS_BOEHM_GC_abort_func fn;

      LOCK();
      fn = MANAGED_STACK_ADDRESS_BOEHM_GC_on_abort;
      UNLOCK();
      return fn;
  }
#endif /* !SMALL_CONFIG */

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_enable(void)
{
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc != 0); /* ensure no counter underflow */
    MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc--;
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc && MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize > MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize_on_gc_disable)
      WARN("Heap grown by %" WARN_PRIuPTR " KiB while GC was disabled\n",
           (MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize - MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize_on_gc_disable) >> 10);
    UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_disable(void)
{
    LOCK();
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc)
      MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize_on_gc_disable = MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize;
    MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc++;
    UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_is_disabled(void)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc != 0;
}

/* Helper procedures for new kind creation.     */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void ** MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_new_free_list_inner(void)
{
    void *result;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    result = MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_MALLOC((MAXOBJGRANULES+1) * sizeof(ptr_t), PTRFREE);
    if (NULL == result) ABORT("Failed to allocate freelist for new kind");
    BZERO(result, (MAXOBJGRANULES+1)*sizeof(ptr_t));
    return (void **)result;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void ** MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_new_free_list(void)
{
    void ** result;

    LOCK();
    result = MANAGED_STACK_ADDRESS_BOEHM_GC_new_free_list_inner();
    UNLOCK();
    return result;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_new_kind_inner(void **fl, MANAGED_STACK_ADDRESS_BOEHM_GC_word descr,
                                          int adjust, int clear)
{
    unsigned result = MANAGED_STACK_ADDRESS_BOEHM_GC_n_kinds;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NONNULL_ARG_NOT_NULL(fl));
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(adjust == FALSE || adjust == TRUE);
    /* If an object is not needed to be cleared (when moved to the      */
    /* free list) then its descriptor should be zero to denote          */
    /* a pointer-free object (and, as a consequence, the size of the    */
    /* object should not be added to the descriptor template).          */
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(clear == TRUE
              || (descr == 0 && adjust == FALSE && clear == FALSE));
    if (result < MAXOBJKINDS) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(result > 0);
      MANAGED_STACK_ADDRESS_BOEHM_GC_n_kinds++;
      MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[result].ok_freelist = fl;
      MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[result].ok_reclaim_list = 0;
      MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[result].ok_descriptor = descr;
      MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[result].ok_relocate_descr = adjust;
      MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[result].ok_init = (MANAGED_STACK_ADDRESS_BOEHM_GC_bool)clear;
#     ifdef ENABLE_DISCLAIM
        MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[result].ok_mark_unconditionally = FALSE;
        MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[result].ok_disclaim_proc = 0;
#     endif
    } else {
      ABORT("Too many kinds");
    }
    return result;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_new_kind(void **fl, MANAGED_STACK_ADDRESS_BOEHM_GC_word descr, int adjust,
                                    int clear)
{
    unsigned result;

    LOCK();
    result = MANAGED_STACK_ADDRESS_BOEHM_GC_new_kind_inner(fl, descr, adjust, clear);
    UNLOCK();
    return result;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_new_proc_inner(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_proc proc)
{
    unsigned result = MANAGED_STACK_ADDRESS_BOEHM_GC_n_mark_procs;

    if (result < MAX_MARK_PROCS) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_n_mark_procs++;
      MANAGED_STACK_ADDRESS_BOEHM_GC_mark_procs[result] = proc;
    } else {
      ABORT("Too many mark procedures");
    }
    return result;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_new_proc(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_proc proc)
{
    unsigned result;

    LOCK();
    result = MANAGED_STACK_ADDRESS_BOEHM_GC_new_proc_inner(proc);
    UNLOCK();
    return result;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_call_with_alloc_lock(MANAGED_STACK_ADDRESS_BOEHM_GC_fn_type fn, void *client_data)
{
    void * result;

    LOCK();
    result = fn(client_data);
    UNLOCK();
    return result;
}

#ifdef THREADS
  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_alloc_lock(void)
  {
    LOCK();
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_alloc_unlock(void)
  {
    UNLOCK();
  }
#endif /* THREADS */

MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_call_with_stack_base(MANAGED_STACK_ADDRESS_BOEHM_GC_stack_base_func fn, void *arg)
{
    struct MANAGED_STACK_ADDRESS_BOEHM_GC_stack_base base;
    void *result;

    base.mem_base = (void *)&base;
#   ifdef IA64
      base.reg_base = (void *)MANAGED_STACK_ADDRESS_BOEHM_GC_save_regs_in_stack();
      /* TODO: Unnecessarily flushes register stack,    */
      /* but that probably doesn't hurt.                */
#   elif defined(E2K)
      base.reg_base = NULL; /* not used by GC currently */
#   endif
    result = (*fn)(&base, arg);
    /* Strongly discourage the compiler from treating the above */
    /* as a tail call.                                          */
    MANAGED_STACK_ADDRESS_BOEHM_GC_noop1(COVERT_DATAFLOW(&base));
    return result;
}

#ifndef THREADS

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_sp = NULL;
        /* NULL value means we are not inside MANAGED_STACK_ADDRESS_BOEHM_GC_do_blocking() call. */
# ifdef IA64
    STATIC ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_register_sp = NULL;
# endif

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER struct MANAGED_STACK_ADDRESS_BOEHM_GC_traced_stack_sect_s *MANAGED_STACK_ADDRESS_BOEHM_GC_traced_stack_sect = NULL;

/* This is nearly the same as in pthread_support.c.     */
MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_call_with_gc_active(MANAGED_STACK_ADDRESS_BOEHM_GC_fn_type fn,
                                             void * client_data)
{
    struct MANAGED_STACK_ADDRESS_BOEHM_GC_traced_stack_sect_s stacksect;
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized);

    /* Adjust our stack bottom pointer (this could happen if    */
    /* MANAGED_STACK_ADDRESS_BOEHM_GC_get_main_stack_base() is unimplemented or broken for  */
    /* the platform).                                           */
    if ((word)MANAGED_STACK_ADDRESS_BOEHM_GC_stackbottom HOTTER_THAN (word)(&stacksect))
      MANAGED_STACK_ADDRESS_BOEHM_GC_stackbottom = (ptr_t)COVERT_DATAFLOW(&stacksect);

    if (MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_sp == NULL) {
      /* We are not inside MANAGED_STACK_ADDRESS_BOEHM_GC_do_blocking() - do nothing more.  */
      client_data = fn(client_data);
      /* Prevent treating the above as a tail call.     */
      MANAGED_STACK_ADDRESS_BOEHM_GC_noop1(COVERT_DATAFLOW(&stacksect));
      return client_data; /* result */
    }

    /* Setup new "stack section".       */
    stacksect.saved_stack_ptr = MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_sp;
#   ifdef IA64
      /* This is the same as in MANAGED_STACK_ADDRESS_BOEHM_GC_call_with_stack_base().      */
      stacksect.backing_store_end = MANAGED_STACK_ADDRESS_BOEHM_GC_save_regs_in_stack();
      /* Unnecessarily flushes register stack,          */
      /* but that probably doesn't hurt.                */
      stacksect.saved_backing_store_ptr = MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_register_sp;
#   endif
    stacksect.prev = MANAGED_STACK_ADDRESS_BOEHM_GC_traced_stack_sect;
    MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_sp = NULL;
    MANAGED_STACK_ADDRESS_BOEHM_GC_traced_stack_sect = &stacksect;

    client_data = fn(client_data);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_sp == NULL);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_traced_stack_sect == &stacksect);

#   if defined(CPPCHECK)
      MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((word)MANAGED_STACK_ADDRESS_BOEHM_GC_traced_stack_sect - (word)MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_sp);
#   endif
    /* Restore original "stack section".        */
    MANAGED_STACK_ADDRESS_BOEHM_GC_traced_stack_sect = stacksect.prev;
#   ifdef IA64
      MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_register_sp = stacksect.saved_backing_store_ptr;
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_sp = stacksect.saved_stack_ptr;

    return client_data; /* result */
}

/* This is nearly the same as in pthread_support.c.     */
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_do_blocking_inner(ptr_t data, void *context)
{
    struct blocking_data * d = (struct blocking_data *)data;

    UNUSED_ARG(context);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_sp == NULL);
#   ifdef SPARC
        MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_sp = MANAGED_STACK_ADDRESS_BOEHM_GC_save_regs_in_stack();
#   else
        MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_sp = (ptr_t) &d; /* save approx. sp */
#       ifdef IA64
            MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_register_sp = MANAGED_STACK_ADDRESS_BOEHM_GC_save_regs_in_stack();
#       elif defined(E2K)
            (void)MANAGED_STACK_ADDRESS_BOEHM_GC_save_regs_in_stack();
#       endif
#   endif

    d -> client_data = (d -> fn)(d -> client_data);

#   ifdef SPARC
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_sp != NULL);
#   else
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_sp == (ptr_t)(&d));
#   endif
#   if defined(CPPCHECK)
      MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((word)MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_sp);
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_sp = NULL;
}

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_stackbottom(void *gc_thread_handle,
                                         const struct MANAGED_STACK_ADDRESS_BOEHM_GC_stack_base *sb)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(sb -> mem_base != NULL);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NULL == gc_thread_handle || &MANAGED_STACK_ADDRESS_BOEHM_GC_stackbottom == gc_thread_handle);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_blocked_sp
              && NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_traced_stack_sect); /* for now */
    (void)gc_thread_handle;

    MANAGED_STACK_ADDRESS_BOEHM_GC_stackbottom = (char *)sb->mem_base;
#   ifdef IA64
      MANAGED_STACK_ADDRESS_BOEHM_GC_register_stackbottom = (ptr_t)sb->reg_base;
#   endif
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_my_stackbottom(struct MANAGED_STACK_ADDRESS_BOEHM_GC_stack_base *sb)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized);
    sb -> mem_base = MANAGED_STACK_ADDRESS_BOEHM_GC_stackbottom;
#   ifdef IA64
      sb -> reg_base = MANAGED_STACK_ADDRESS_BOEHM_GC_register_stackbottom;
#   elif defined(E2K)
      sb -> reg_base = NULL;
#   endif
    return &MANAGED_STACK_ADDRESS_BOEHM_GC_stackbottom; /* gc_thread_handle */
  }
#endif /* !THREADS */

MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_do_blocking(MANAGED_STACK_ADDRESS_BOEHM_GC_fn_type fn, void * client_data)
{
    struct blocking_data my_data;

    my_data.fn = fn;
    my_data.client_data = client_data;
    MANAGED_STACK_ADDRESS_BOEHM_GC_with_callee_saves_pushed(MANAGED_STACK_ADDRESS_BOEHM_GC_do_blocking_inner, (ptr_t)(&my_data));
    return my_data.client_data; /* result */
}

#if !defined(NO_DEBUGGING)
  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_dump(void)
  {
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_dump_named(NULL);
    UNLOCK();
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_dump_named(const char *name)
  {
#   ifndef NO_CLOCK
      CLOCK_TYPE current_time;

      GET_TIME(current_time);
#   endif
    if (name != NULL) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("\n***GC Dump %s\n", name);
    } else {
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("\n***GC Dump collection #%lu\n", (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no);
    }
#   ifndef NO_CLOCK
      /* Note that the time is wrapped in ~49 days if sizeof(long)==4.  */
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Time since GC init: %lu ms\n",
                MS_TIME_DIFF(current_time, MANAGED_STACK_ADDRESS_BOEHM_GC_init_time));
#   endif

    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("\n***Static roots:\n");
    MANAGED_STACK_ADDRESS_BOEHM_GC_print_static_roots();
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("\n***Heap sections:\n");
    MANAGED_STACK_ADDRESS_BOEHM_GC_print_heap_sects();
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("\n***Free blocks:\n");
    MANAGED_STACK_ADDRESS_BOEHM_GC_print_hblkfreelist();
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("\n***Blocks in use:\n");
    MANAGED_STACK_ADDRESS_BOEHM_GC_print_block_list();
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
      MANAGED_STACK_ADDRESS_BOEHM_GC_dump_finalization();
#   endif
  }
#endif /* !NO_DEBUGGING */

static void MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK block_add_size(struct hblk *h, MANAGED_STACK_ADDRESS_BOEHM_GC_word pbytes)
{
  hdr *hhdr = HDR(h);
  *(word *)pbytes += (WORDS_TO_BYTES(hhdr->hb_sz) + HBLKSIZE-1)
                        & ~(word)(HBLKSIZE-1);
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API size_t MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_memory_use(void)
{
  word bytes = 0;

  LOCK();
  MANAGED_STACK_ADDRESS_BOEHM_GC_apply_to_all_blocks(block_add_size, (word)(&bytes));
  UNLOCK();
  return (size_t)bytes;
}

/* Getter functions for the public Read-only variables.                 */

/* MANAGED_STACK_ADDRESS_BOEHM_GC_get_gc_no() is unsynchronized and should be typically called      */
/* inside the context of MANAGED_STACK_ADDRESS_BOEHM_GC_call_with_alloc_lock() to prevent data      */
/* races (on multiprocessors).                                          */
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_word MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_gc_no(void)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no;
}

#ifndef PARALLEL_MARK
  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_markers_count(unsigned markers)
  {
    UNUSED_ARG(markers);
  }
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_parallel(void)
{
# ifdef THREADS
    return MANAGED_STACK_ADDRESS_BOEHM_GC_parallel;
# else
    return 0;
# endif
}

/* Setter and getter functions for the public R/W function variables.   */
/* These functions are synchronized (like MANAGED_STACK_ADDRESS_BOEHM_GC_set_warn_proc() and        */
/* MANAGED_STACK_ADDRESS_BOEHM_GC_get_warn_proc()).                                                 */

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_oom_fn(MANAGED_STACK_ADDRESS_BOEHM_GC_oom_func fn)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NONNULL_ARG_NOT_NULL(fn));
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_oom_fn = fn;
    UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_oom_func MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_oom_fn(void)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_oom_func fn;

    LOCK();
    fn = MANAGED_STACK_ADDRESS_BOEHM_GC_oom_fn;
    UNLOCK();
    return fn;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_on_heap_resize(MANAGED_STACK_ADDRESS_BOEHM_GC_on_heap_resize_proc fn)
{
    /* fn may be 0 (means no event notifier). */
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_on_heap_resize = fn;
    UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_on_heap_resize_proc MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_on_heap_resize(void)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_on_heap_resize_proc fn;

    LOCK();
    fn = MANAGED_STACK_ADDRESS_BOEHM_GC_on_heap_resize;
    UNLOCK();
    return fn;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_finalizer_notifier(MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_notifier_proc fn)
{
    /* fn may be 0 (means no finalizer notifier). */
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_notifier = fn;
    UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_notifier_proc MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_finalizer_notifier(void)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_notifier_proc fn;

    LOCK();
    fn = MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_notifier;
    UNLOCK();
    return fn;
}

/* Setter and getter functions for the public numeric R/W variables.    */
/* It is safe to call these functions even before MANAGED_STACK_ADDRESS_BOEHM_GC_INIT().            */
/* These functions are unsynchronized and should be typically called    */
/* inside the context of MANAGED_STACK_ADDRESS_BOEHM_GC_call_with_alloc_lock() (if called after     */
/* MANAGED_STACK_ADDRESS_BOEHM_GC_INIT()) to prevent data races (unless it is guaranteed the        */
/* collector is not multi-threaded at that execution point).            */

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_find_leak(int value)
{
    /* value is of boolean type. */
    MANAGED_STACK_ADDRESS_BOEHM_GC_find_leak = value;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_find_leak(void)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_find_leak;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_all_interior_pointers(int value)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers = value ? 1 : 0;
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized) {
      /* It is not recommended to change MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers value */
      /* after GC is initialized but it seems GC could work correctly   */
      /* even after switching the mode.                                 */
      LOCK();
      MANAGED_STACK_ADDRESS_BOEHM_GC_initialize_offsets(); /* NOTE: this resets manual offsets as well */
      if (!MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers)
        MANAGED_STACK_ADDRESS_BOEHM_GC_bl_init_no_interiors();
      UNLOCK();
    }
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_all_interior_pointers(void)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_finalize_on_demand(int value)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(value != -1); /* -1 was used to retrieve old value in gc-7.2 */
    /* value is of boolean type. */
    MANAGED_STACK_ADDRESS_BOEHM_GC_finalize_on_demand = value;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_finalize_on_demand(void)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_finalize_on_demand;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_java_finalization(int value)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(value != -1); /* -1 was used to retrieve old value in gc-7.2 */
    /* value is of boolean type. */
    MANAGED_STACK_ADDRESS_BOEHM_GC_java_finalization = value;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_java_finalization(void)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_java_finalization;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_dont_expand(int value)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(value != -1); /* -1 was used to retrieve old value in gc-7.2 */
    /* value is of boolean type. */
    MANAGED_STACK_ADDRESS_BOEHM_GC_dont_expand = value;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_dont_expand(void)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_dont_expand;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_no_dls(int value)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(value != -1); /* -1 was used to retrieve old value in gc-7.2 */
    /* value is of boolean type. */
    MANAGED_STACK_ADDRESS_BOEHM_GC_no_dls = value;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_no_dls(void)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_no_dls;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_non_gc_bytes(MANAGED_STACK_ADDRESS_BOEHM_GC_word value)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_non_gc_bytes = value;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_word MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_non_gc_bytes(void)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_non_gc_bytes;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_free_space_divisor(MANAGED_STACK_ADDRESS_BOEHM_GC_word value)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(value > 0);
    MANAGED_STACK_ADDRESS_BOEHM_GC_free_space_divisor = value;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_word MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_free_space_divisor(void)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_free_space_divisor;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_max_retries(MANAGED_STACK_ADDRESS_BOEHM_GC_word value)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((MANAGED_STACK_ADDRESS_BOEHM_GC_signed_word)value != -1);
                        /* -1 was used to retrieve old value in gc-7.2 */
    MANAGED_STACK_ADDRESS_BOEHM_GC_max_retries = value;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_word MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_max_retries(void)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_max_retries;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_dont_precollect(int value)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(value != -1); /* -1 was used to retrieve old value in gc-7.2 */
    /* value is of boolean type. */
    MANAGED_STACK_ADDRESS_BOEHM_GC_dont_precollect = value;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_dont_precollect(void)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_dont_precollect;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_full_freq(int value)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(value >= 0);
    MANAGED_STACK_ADDRESS_BOEHM_GC_full_freq = value;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_full_freq(void)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_full_freq;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_time_limit(unsigned long value)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((long)value != -1L);
                        /* -1 was used to retrieve old value in gc-7.2 */
    MANAGED_STACK_ADDRESS_BOEHM_GC_time_limit = value;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API unsigned long MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_time_limit(void)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_time_limit;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_force_unmap_on_gcollect(int value)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_force_unmap_on_gcollect = (MANAGED_STACK_ADDRESS_BOEHM_GC_bool)value;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_force_unmap_on_gcollect(void)
{
    return (int)MANAGED_STACK_ADDRESS_BOEHM_GC_force_unmap_on_gcollect;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_abort_on_oom(void)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf("Insufficient memory for the allocation\n");
    EXIT();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API size_t MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_hblk_size(void)
{
    return (size_t)HBLKSIZE;
}
