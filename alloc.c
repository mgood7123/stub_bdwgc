/*
 * Copyright (c) 1988-1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1996 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996-1999 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 1999-2011 Hewlett-Packard Development Company, L.P.
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
 *
 */

#include "private/gc_priv.h"

#if !defined(MACOS) && !defined(MSWINCE)
# include <signal.h>
#endif

/*
 * Separate free lists are maintained for different sized objects
 * up to MAXOBJBYTES.
 * The call MANAGED_STACK_ADDRESS_BOEHM_GC_allocobj(i,k) ensures that the freelist for
 * kind k objects of size i points to a non-empty
 * free list. It returns a pointer to the first entry on the free list.
 * In a single-threaded world, MANAGED_STACK_ADDRESS_BOEHM_GC_allocobj may be called to allocate
 * an object of small size lb (and NORMAL kind) as follows
 * (MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_inner is a wrapper over MANAGED_STACK_ADDRESS_BOEHM_GC_allocobj which also
 * fills in MANAGED_STACK_ADDRESS_BOEHM_GC_size_map if needed):
 *
 *   lg = MANAGED_STACK_ADDRESS_BOEHM_GC_size_map[lb];
 *   op = MANAGED_STACK_ADDRESS_BOEHM_GC_objfreelist[lg];
 *   if (NULL == op) {
 *     op = MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_inner(lb, NORMAL, 0);
 *   } else {
 *     MANAGED_STACK_ADDRESS_BOEHM_GC_objfreelist[lg] = obj_link(op);
 *     MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd += GRANULES_TO_BYTES((word)lg);
 *   }
 *
 * Note that this is very fast if the free list is non-empty; it should
 * only involve the execution of 4 or 5 simple instructions.
 * All composite objects on freelists are cleared, except for
 * their first word.
 */

/*
 * The allocator uses MANAGED_STACK_ADDRESS_BOEHM_GC_allochblk to allocate large chunks of objects.
 * These chunks all start on addresses which are multiples of
 * HBLKSZ.   Each allocated chunk has an associated header,
 * which can be located quickly based on the address of the chunk.
 * (See headers.c for details.)
 * This makes it possible to check quickly whether an
 * arbitrary address corresponds to an object administered by the
 * allocator.
 */

word MANAGED_STACK_ADDRESS_BOEHM_GC_non_gc_bytes = 0;  /* Number of bytes not intended to be collected */

word MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no = 0;

#ifndef NO_CLOCK
  static unsigned long full_gc_total_time = 0; /* in ms, may wrap */
  static unsigned long stopped_mark_total_time = 0;
  static unsigned32 full_gc_total_ns_frac = 0; /* fraction of 1 ms */
  static unsigned32 stopped_mark_total_ns_frac = 0;
  static MANAGED_STACK_ADDRESS_BOEHM_GC_bool measure_performance = FALSE;
                /* Do performance measurements if set to true (e.g.,    */
                /* accumulation of the total time of full collections). */

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_start_performance_measurement(void)
  {
    measure_performance = TRUE;
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API unsigned long MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_full_gc_total_time(void)
  {
    return full_gc_total_time;
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API unsigned long MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_stopped_mark_total_time(void)
  {
    return stopped_mark_total_time;
  }
#endif /* !NO_CLOCK */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_incremental = FALSE; /* By default, stop the world. */
  STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_should_start_incremental_collection = FALSE;
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_is_incremental_mode(void)
{
  return (int)MANAGED_STACK_ADDRESS_BOEHM_GC_incremental;
}

#ifdef THREADS
  int MANAGED_STACK_ADDRESS_BOEHM_GC_parallel = FALSE;      /* By default, parallel GC is off.      */
#endif

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_FULL_FREQ) && !defined(CPPCHECK)
  int MANAGED_STACK_ADDRESS_BOEHM_GC_full_freq = MANAGED_STACK_ADDRESS_BOEHM_GC_FULL_FREQ;
#else
  int MANAGED_STACK_ADDRESS_BOEHM_GC_full_freq = 19;   /* Every 20th collection is a full   */
                           /* collection, whether we need it    */
                           /* or not.                           */
#endif

STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_need_full_gc = FALSE;
                           /* Need full GC due to heap growth.  */

#ifdef THREAD_LOCAL_ALLOC
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_world_stopped = FALSE;
#endif

STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_disable_automatic_collection = FALSE;

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_disable_automatic_collection(int value)
{
  LOCK();
  MANAGED_STACK_ADDRESS_BOEHM_GC_disable_automatic_collection = (MANAGED_STACK_ADDRESS_BOEHM_GC_bool)value;
  UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_disable_automatic_collection(void)
{
  int value;

  LOCK();
  value = (int)MANAGED_STACK_ADDRESS_BOEHM_GC_disable_automatic_collection;
  UNLOCK();
  return value;
}

STATIC word MANAGED_STACK_ADDRESS_BOEHM_GC_used_heap_size_after_full = 0;

/* Version macros are now defined in gc_version.h, which is included by */
/* gc.h, which is included by gc_priv.h.                                */
#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_VERSION_VAR
  EXTERN_C_BEGIN
  extern const MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_VAL_T MANAGED_STACK_ADDRESS_BOEHM_GC_version;
  EXTERN_C_END

  const MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_VAL_T MANAGED_STACK_ADDRESS_BOEHM_GC_version =
                ((MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_VAL_T)MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MAJOR << 16)
                | (MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MINOR << 8) | MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MICRO;
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_VAL_T MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_version(void)
{
  return ((MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_VAL_T)MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MAJOR << 16)
         | (MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MINOR << 8) | MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MICRO;
}

/* some more variables */

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_DONT_EXPAND
  int MANAGED_STACK_ADDRESS_BOEHM_GC_dont_expand = TRUE;
#else
  int MANAGED_STACK_ADDRESS_BOEHM_GC_dont_expand = FALSE;
#endif

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_FREE_SPACE_DIVISOR) && !defined(CPPCHECK)
  word MANAGED_STACK_ADDRESS_BOEHM_GC_free_space_divisor = MANAGED_STACK_ADDRESS_BOEHM_GC_FREE_SPACE_DIVISOR; /* must be > 0 */
#else
  word MANAGED_STACK_ADDRESS_BOEHM_GC_free_space_divisor = 3;
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER int MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK MANAGED_STACK_ADDRESS_BOEHM_GC_never_stop_func(void)
{
  return FALSE;
}

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_TIME_LIMIT) && !defined(CPPCHECK)
  unsigned long MANAGED_STACK_ADDRESS_BOEHM_GC_time_limit = MANAGED_STACK_ADDRESS_BOEHM_GC_TIME_LIMIT;
                           /* We try to keep pause times from exceeding  */
                           /* this by much. In milliseconds.             */
#elif defined(PARALLEL_MARK)
  unsigned long MANAGED_STACK_ADDRESS_BOEHM_GC_time_limit = MANAGED_STACK_ADDRESS_BOEHM_GC_TIME_UNLIMITED;
                        /* The parallel marker cannot be interrupted for */
                        /* now, so the time limit is absent by default.  */
#else
  unsigned long MANAGED_STACK_ADDRESS_BOEHM_GC_time_limit = 15;
#endif

#ifndef NO_CLOCK
  STATIC unsigned long MANAGED_STACK_ADDRESS_BOEHM_GC_time_lim_nsec = 0;
                        /* The nanoseconds add-on to MANAGED_STACK_ADDRESS_BOEHM_GC_time_limit      */
                        /* value.  Not updated by MANAGED_STACK_ADDRESS_BOEHM_GC_set_time_limit().  */
                        /* Ignored if the value of MANAGED_STACK_ADDRESS_BOEHM_GC_time_limit is     */
                        /* MANAGED_STACK_ADDRESS_BOEHM_GC_TIME_UNLIMITED.                           */

# define TV_NSEC_LIMIT (1000UL * 1000) /* amount of nanoseconds in 1 ms */

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_time_limit_tv(struct MANAGED_STACK_ADDRESS_BOEHM_GC_timeval_s tv)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(tv.tv_ms <= MANAGED_STACK_ADDRESS_BOEHM_GC_TIME_UNLIMITED);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(tv.tv_nsec < TV_NSEC_LIMIT);
    MANAGED_STACK_ADDRESS_BOEHM_GC_time_limit = tv.tv_ms;
    MANAGED_STACK_ADDRESS_BOEHM_GC_time_lim_nsec = tv.tv_nsec;
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API struct MANAGED_STACK_ADDRESS_BOEHM_GC_timeval_s MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_time_limit_tv(void)
  {
    struct MANAGED_STACK_ADDRESS_BOEHM_GC_timeval_s tv;

    tv.tv_ms = MANAGED_STACK_ADDRESS_BOEHM_GC_time_limit;
    tv.tv_nsec = MANAGED_STACK_ADDRESS_BOEHM_GC_time_lim_nsec;
    return tv;
  }

  STATIC CLOCK_TYPE MANAGED_STACK_ADDRESS_BOEHM_GC_start_time = CLOCK_TYPE_INITIALIZER;
                                /* Time at which we stopped world.      */
                                /* used only in MANAGED_STACK_ADDRESS_BOEHM_GC_timeout_stop_func.   */
#endif /* !NO_CLOCK */

STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_n_attempts = 0;   /* Number of attempts at finishing      */
                                /* collection within MANAGED_STACK_ADDRESS_BOEHM_GC_time_limit.     */

STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_stop_func MANAGED_STACK_ADDRESS_BOEHM_GC_default_stop_func = MANAGED_STACK_ADDRESS_BOEHM_GC_never_stop_func;
                                /* Accessed holding the allocator lock. */

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_stop_func(MANAGED_STACK_ADDRESS_BOEHM_GC_stop_func stop_func)
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NONNULL_ARG_NOT_NULL(stop_func));
  LOCK();
  MANAGED_STACK_ADDRESS_BOEHM_GC_default_stop_func = stop_func;
  UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_stop_func MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_stop_func(void)
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_stop_func stop_func;

  LOCK();
  stop_func = MANAGED_STACK_ADDRESS_BOEHM_GC_default_stop_func;
  UNLOCK();
  return stop_func;
}

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL) || defined(NO_CLOCK)
# define MANAGED_STACK_ADDRESS_BOEHM_GC_timeout_stop_func MANAGED_STACK_ADDRESS_BOEHM_GC_default_stop_func
#else
  STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK MANAGED_STACK_ADDRESS_BOEHM_GC_timeout_stop_func(void)
  {
    CLOCK_TYPE current_time;
    static unsigned count = 0;
    unsigned long time_diff, nsec_diff;

    if (MANAGED_STACK_ADDRESS_BOEHM_GC_default_stop_func())
      return TRUE;

    if (MANAGED_STACK_ADDRESS_BOEHM_GC_time_limit == MANAGED_STACK_ADDRESS_BOEHM_GC_TIME_UNLIMITED || (count++ & 3) != 0)
      return FALSE;

    GET_TIME(current_time);
    time_diff = MS_TIME_DIFF(current_time, MANAGED_STACK_ADDRESS_BOEHM_GC_start_time);
    nsec_diff = NS_FRAC_TIME_DIFF(current_time, MANAGED_STACK_ADDRESS_BOEHM_GC_start_time);
#   if defined(CPPCHECK)
      MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((word)&nsec_diff);
#   endif
    if (time_diff >= MANAGED_STACK_ADDRESS_BOEHM_GC_time_limit
        && (time_diff > MANAGED_STACK_ADDRESS_BOEHM_GC_time_limit || nsec_diff >= MANAGED_STACK_ADDRESS_BOEHM_GC_time_lim_nsec)) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF("Abandoning stopped marking after %lu ms %lu ns"
                         " (attempt %d)\n",
                         time_diff, nsec_diff, MANAGED_STACK_ADDRESS_BOEHM_GC_n_attempts);
      return TRUE;
    }

    return FALSE;
  }
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL */

#ifdef THREADS
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER word MANAGED_STACK_ADDRESS_BOEHM_GC_total_stacksize = 0; /* updated on every push_all_stacks */
#endif

static size_t min_bytes_allocd_minimum = 1;
                        /* The lowest value returned by min_bytes_allocd(). */

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_min_bytes_allocd(size_t value)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(value > 0);
    min_bytes_allocd_minimum = value;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API size_t MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_min_bytes_allocd(void)
{
    return min_bytes_allocd_minimum;
}

/* Return the minimum number of bytes that must be allocated between    */
/* collections to amortize the collection cost.  Should be non-zero.    */
static word min_bytes_allocd(void)
{
    word result;
    word stack_size;
    word total_root_size;       /* includes double stack size,  */
                                /* since the stack is expensive */
                                /* to scan.                     */
    word scan_size;             /* Estimate of memory to be scanned     */
                                /* during normal GC.                    */

#   ifdef THREADS
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_need_to_lock) {
        /* We are multi-threaded... */
        stack_size = MANAGED_STACK_ADDRESS_BOEHM_GC_total_stacksize;
        /* For now, we just use the value computed during the latest GC. */
#       ifdef DEBUG_THREADS
          MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Total stacks size: %lu\n",
                        (unsigned long)stack_size);
#       endif
      } else
#   endif
    /* else*/ {
#     ifdef STACK_NOT_SCANNED
        stack_size = 0;
#     elif defined(STACK_GROWS_UP)
        stack_size = (word)(MANAGED_STACK_ADDRESS_BOEHM_GC_approx_sp() - MANAGED_STACK_ADDRESS_BOEHM_GC_stackbottom);
#     else
        stack_size = (word)(MANAGED_STACK_ADDRESS_BOEHM_GC_stackbottom - MANAGED_STACK_ADDRESS_BOEHM_GC_approx_sp());
#     endif
    }

    total_root_size = 2 * stack_size + MANAGED_STACK_ADDRESS_BOEHM_GC_root_size;
    scan_size = 2 * MANAGED_STACK_ADDRESS_BOEHM_GC_composite_in_use + MANAGED_STACK_ADDRESS_BOEHM_GC_atomic_in_use / 4
                + total_root_size;
    result = scan_size / MANAGED_STACK_ADDRESS_BOEHM_GC_free_space_divisor;
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_incremental) {
      result /= 2;
    }
    return result > min_bytes_allocd_minimum
            ? result : min_bytes_allocd_minimum;
}

STATIC word MANAGED_STACK_ADDRESS_BOEHM_GC_non_gc_bytes_at_gc = 0;
                /* Number of explicitly managed bytes of storage        */
                /* at last collection.                                  */

/* Return the number of bytes allocated, adjusted for explicit storage  */
/* management, etc.  This number is used in deciding when to trigger    */
/* collections.                                                         */
STATIC word MANAGED_STACK_ADDRESS_BOEHM_GC_adj_bytes_allocd(void)
{
    signed_word result;
    signed_word expl_managed = (signed_word)MANAGED_STACK_ADDRESS_BOEHM_GC_non_gc_bytes
                                - (signed_word)MANAGED_STACK_ADDRESS_BOEHM_GC_non_gc_bytes_at_gc;

    /* Don't count what was explicitly freed, or newly allocated for    */
    /* explicit management.  Note that deallocating an explicitly       */
    /* managed object should not alter result, assuming the client      */
    /* is playing by the rules.                                         */
    result = (signed_word)MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd
             + (signed_word)MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_dropped
             - (signed_word)MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_freed
             + (signed_word)MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_bytes_freed
             - expl_managed;
    if (result > (signed_word)MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd) {
        result = (signed_word)MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd;
        /* probably client bug or unfortunate scheduling */
    }
    result += (signed_word)MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_finalized;
        /* We count objects enqueued for finalization as though they    */
        /* had been reallocated this round. Finalization is user        */
        /* visible progress.  And if we don't count this, we have       */
        /* stability problems for programs that finalize all objects.   */
    if (result < (signed_word)(MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd >> 3)) {
        /* Always count at least 1/8 of the allocations.  We don't want */
        /* to collect too infrequently, since that would inhibit        */
        /* coalescing of free storage blocks.                           */
        /* This also makes us partially robust against client bugs.     */
        result = (signed_word)(MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd >> 3);
    }
    return (word)result;
}


/* Clear up a few frames worth of garbage left at the top of the stack. */
/* This is used to prevent us from accidentally treating garbage left   */
/* on the stack by other parts of the collector as roots.  This         */
/* differs from the code in misc.c, which actually tries to keep the    */
/* stack clear of long-lived, client-generated garbage.                 */
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_clear_a_few_frames(void)
{
#   ifndef CLEAR_NWORDS
#     define CLEAR_NWORDS 64
#   endif
    volatile word frames[CLEAR_NWORDS];
    BZERO((/* no volatile */ word *)((word)frames),
          CLEAR_NWORDS * sizeof(word));
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_start_incremental_collection(void)
{
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_incremental) return;

    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_should_start_incremental_collection = TRUE;
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc) {
      ENTER_GC();
      MANAGED_STACK_ADDRESS_BOEHM_GC_collect_a_little_inner(1);
      EXIT_GC();
    }
    UNLOCK();
# endif
}

/* Have we allocated enough to amortize a collection? */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_should_collect(void)
{
    static word last_min_bytes_allocd;
    static word last_gc_no;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    if (last_gc_no != MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no) {
      last_min_bytes_allocd = min_bytes_allocd();
      last_gc_no = MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no;
    }
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_should_start_incremental_collection) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_should_start_incremental_collection = FALSE;
      return TRUE;
    }
# endif
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_disable_automatic_collection) return FALSE;

    if (MANAGED_STACK_ADDRESS_BOEHM_GC_last_heap_growth_gc_no == MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no)
      return TRUE; /* avoid expanding past limits used by blacklisting  */

    return MANAGED_STACK_ADDRESS_BOEHM_GC_adj_bytes_allocd() >= last_min_bytes_allocd;
}

/* STATIC */ MANAGED_STACK_ADDRESS_BOEHM_GC_start_callback_proc MANAGED_STACK_ADDRESS_BOEHM_GC_start_call_back = 0;
                        /* Called at start of full collections.         */
                        /* Not called if 0.  Called with the allocation */
                        /* lock held.  Not used by GC itself.           */

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_start_callback(MANAGED_STACK_ADDRESS_BOEHM_GC_start_callback_proc fn)
{
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_start_call_back = fn;
    UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_start_callback_proc MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_start_callback(void)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_start_callback_proc fn;

    LOCK();
    fn = MANAGED_STACK_ADDRESS_BOEHM_GC_start_call_back;
    UNLOCK();
    return fn;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE void MANAGED_STACK_ADDRESS_BOEHM_GC_notify_full_gc(void)
{
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_start_call_back != 0) {
        (*MANAGED_STACK_ADDRESS_BOEHM_GC_start_call_back)();
    }
}

STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_is_full_gc = FALSE;

STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_stopped_mark(MANAGED_STACK_ADDRESS_BOEHM_GC_stop_func stop_func);
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_finish_collection(void);

/* Initiate a garbage collection if appropriate.  Choose judiciously    */
/* between partial, full, and stop-world collections.                   */
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_maybe_gc(void)
{
  static int n_partial_gcs = 0;

  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
  ASSERT_CANCEL_DISABLED();
  if (!MANAGED_STACK_ADDRESS_BOEHM_GC_should_collect()) return;

  if (!MANAGED_STACK_ADDRESS_BOEHM_GC_incremental) {
    MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect_inner();
    return;
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(!MANAGED_STACK_ADDRESS_BOEHM_GC_collection_in_progress());
# ifdef PARALLEL_MARK
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_parallel)
      MANAGED_STACK_ADDRESS_BOEHM_GC_wait_for_reclaim();
# endif
  if (MANAGED_STACK_ADDRESS_BOEHM_GC_need_full_gc || n_partial_gcs >= MANAGED_STACK_ADDRESS_BOEHM_GC_full_freq) {
    MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF(
                "***>Full mark for collection #%lu after %lu allocd bytes\n",
                (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no + 1, (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd);
    MANAGED_STACK_ADDRESS_BOEHM_GC_promote_black_lists();
    (void)MANAGED_STACK_ADDRESS_BOEHM_GC_reclaim_all((MANAGED_STACK_ADDRESS_BOEHM_GC_stop_func)0, TRUE);
    MANAGED_STACK_ADDRESS_BOEHM_GC_notify_full_gc();
    MANAGED_STACK_ADDRESS_BOEHM_GC_clear_marks();
    n_partial_gcs = 0;
    MANAGED_STACK_ADDRESS_BOEHM_GC_is_full_gc = TRUE;
  } else {
    n_partial_gcs++;
  }

  /* Try to mark with the world stopped.  If we run out of      */
  /* time, this turns into an incremental marking.              */
# ifndef NO_CLOCK
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_time_limit != MANAGED_STACK_ADDRESS_BOEHM_GC_TIME_UNLIMITED) GET_TIME(MANAGED_STACK_ADDRESS_BOEHM_GC_start_time);
# endif
  if (MANAGED_STACK_ADDRESS_BOEHM_GC_stopped_mark(MANAGED_STACK_ADDRESS_BOEHM_GC_timeout_stop_func)) {
#   ifdef SAVE_CALL_CHAIN
      MANAGED_STACK_ADDRESS_BOEHM_GC_save_callers(MANAGED_STACK_ADDRESS_BOEHM_GC_last_stack);
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_finish_collection();
  } else if (!MANAGED_STACK_ADDRESS_BOEHM_GC_is_full_gc) {
    /* Count this as the first attempt. */
    MANAGED_STACK_ADDRESS_BOEHM_GC_n_attempts++;
  }
}

STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event_proc MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event = 0;

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_on_collection_event(MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event_proc fn)
{
    /* fn may be 0 (means no event notifier). */
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event = fn;
    UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event_proc MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_on_collection_event(void)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event_proc fn;

    LOCK();
    fn = MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event;
    UNLOCK();
    return fn;
}

/* Stop the world garbage collection.  If stop_func is not      */
/* MANAGED_STACK_ADDRESS_BOEHM_GC_never_stop_func then abort if stop_func returns TRUE.     */
/* Return TRUE if we successfully completed the collection.     */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_try_to_collect_inner(MANAGED_STACK_ADDRESS_BOEHM_GC_stop_func stop_func)
{
#   ifndef NO_CLOCK
      CLOCK_TYPE start_time = CLOCK_TYPE_INITIALIZER;
      MANAGED_STACK_ADDRESS_BOEHM_GC_bool start_time_valid;
#   endif

    ASSERT_CANCEL_DISABLED();
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized);
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc || (*stop_func)()) return FALSE;
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event)
      MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event(MANAGED_STACK_ADDRESS_BOEHM_GC_EVENT_START);
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_incremental && MANAGED_STACK_ADDRESS_BOEHM_GC_collection_in_progress()) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF(
            "MANAGED_STACK_ADDRESS_BOEHM_GC_try_to_collect_inner: finishing collection in progress\n");
      /* Just finish collection already in progress.    */
        do {
            if ((*stop_func)()) {
              /* TODO: Notify MANAGED_STACK_ADDRESS_BOEHM_GC_EVENT_ABANDON */
              return FALSE;
            }
            ENTER_GC();
            MANAGED_STACK_ADDRESS_BOEHM_GC_collect_a_little_inner(1);
            EXIT_GC();
        } while (MANAGED_STACK_ADDRESS_BOEHM_GC_collection_in_progress());
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_notify_full_gc();
#   ifndef NO_CLOCK
      start_time_valid = FALSE;
      if ((MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats | (int)measure_performance) != 0) {
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats)
          MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Initiating full world-stop collection!\n");
        start_time_valid = TRUE;
        GET_TIME(start_time);
      }
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_promote_black_lists();
    /* Make sure all blocks have been reclaimed, so sweep routines      */
    /* don't see cleared mark bits.                                     */
    /* If we're guaranteed to finish, then this is unnecessary.         */
    /* In the find_leak case, we have to finish to guarantee that       */
    /* previously unmarked objects are not reported as leaks.           */
#       ifdef PARALLEL_MARK
          if (MANAGED_STACK_ADDRESS_BOEHM_GC_parallel)
            MANAGED_STACK_ADDRESS_BOEHM_GC_wait_for_reclaim();
#       endif
        if ((MANAGED_STACK_ADDRESS_BOEHM_GC_find_leak || stop_func != MANAGED_STACK_ADDRESS_BOEHM_GC_never_stop_func)
            && !MANAGED_STACK_ADDRESS_BOEHM_GC_reclaim_all(stop_func, FALSE)) {
            /* Aborted.  So far everything is still consistent. */
            /* TODO: Notify MANAGED_STACK_ADDRESS_BOEHM_GC_EVENT_ABANDON */
            return FALSE;
        }
    MANAGED_STACK_ADDRESS_BOEHM_GC_invalidate_mark_state();  /* Flush mark stack.   */
    MANAGED_STACK_ADDRESS_BOEHM_GC_clear_marks();
#   ifdef SAVE_CALL_CHAIN
        MANAGED_STACK_ADDRESS_BOEHM_GC_save_callers(MANAGED_STACK_ADDRESS_BOEHM_GC_last_stack);
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_is_full_gc = TRUE;
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_stopped_mark(stop_func)) {
      if (!MANAGED_STACK_ADDRESS_BOEHM_GC_incremental) {
        /* We're partially done and have no way to complete or use      */
        /* current work.  Reestablish invariants as cheaply as          */
        /* possible.                                                    */
        MANAGED_STACK_ADDRESS_BOEHM_GC_invalidate_mark_state();
        MANAGED_STACK_ADDRESS_BOEHM_GC_unpromote_black_lists();
      } /* else we claim the world is already still consistent.  We'll  */
        /* finish incrementally.                                        */
      /* TODO: Notify MANAGED_STACK_ADDRESS_BOEHM_GC_EVENT_ABANDON */
      return FALSE;
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_finish_collection();
#   ifndef NO_CLOCK
      if (start_time_valid) {
        CLOCK_TYPE current_time;
        unsigned long time_diff, ns_frac_diff;

        GET_TIME(current_time);
        time_diff = MS_TIME_DIFF(current_time, start_time);
        ns_frac_diff = NS_FRAC_TIME_DIFF(current_time, start_time);
        if (measure_performance) {
          full_gc_total_time += time_diff; /* may wrap */
          full_gc_total_ns_frac += (unsigned32)ns_frac_diff;
          if (full_gc_total_ns_frac >= (unsigned32)1000000UL) {
            /* Overflow of the nanoseconds part. */
            full_gc_total_ns_frac -= (unsigned32)1000000UL;
            full_gc_total_time++;
          }
        }
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats)
          MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Complete collection took %lu ms %lu ns\n",
                        time_diff, ns_frac_diff);
      }
#   endif
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event)
      MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event(MANAGED_STACK_ADDRESS_BOEHM_GC_EVENT_END);
    return TRUE;
}

/* The number of extra calls to MANAGED_STACK_ADDRESS_BOEHM_GC_mark_some that we have made. */
STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_deficit = 0;

/* The default value of MANAGED_STACK_ADDRESS_BOEHM_GC_rate.        */
#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_RATE
# define MANAGED_STACK_ADDRESS_BOEHM_GC_RATE 10
#endif

/* When MANAGED_STACK_ADDRESS_BOEHM_GC_collect_a_little_inner() performs n units of GC work, a unit */
/* is intended to touch roughly MANAGED_STACK_ADDRESS_BOEHM_GC_rate pages.  (But, every once in     */
/* a while, we do more than that.)  This needs to be a fairly large     */
/* number with our current incremental GC strategy, since otherwise we  */
/* allocate too much during GC, and the cleanup gets expensive.         */
STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_rate = MANAGED_STACK_ADDRESS_BOEHM_GC_RATE;

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_rate(int value)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(value > 0);
    MANAGED_STACK_ADDRESS_BOEHM_GC_rate = value;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_rate(void)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_rate;
}

/* The default maximum number of prior attempts at world stop marking.  */
#ifndef MAX_PRIOR_ATTEMPTS
# define MAX_PRIOR_ATTEMPTS 3
#endif

/* The maximum number of prior attempts at world stop marking.          */
/* A value of 1 means that we finish the second time, no matter how     */
/* long it takes.  Does not count the initial root scan for a full GC.  */
static int max_prior_attempts = MAX_PRIOR_ATTEMPTS;

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_max_prior_attempts(int value)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(value >= 0);
    max_prior_attempts = value;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_max_prior_attempts(void)
{
    return max_prior_attempts;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_collect_a_little_inner(int n)
{
    IF_CANCEL(int cancel_state;)

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized);
    DISABLE_CANCEL(cancel_state);
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_incremental && MANAGED_STACK_ADDRESS_BOEHM_GC_collection_in_progress()) {
        int i;
        int max_deficit = MANAGED_STACK_ADDRESS_BOEHM_GC_rate * n;

#       ifdef PARALLEL_MARK
            if (MANAGED_STACK_ADDRESS_BOEHM_GC_time_limit != MANAGED_STACK_ADDRESS_BOEHM_GC_TIME_UNLIMITED)
                MANAGED_STACK_ADDRESS_BOEHM_GC_parallel_mark_disabled = TRUE;
#       endif
        for (i = MANAGED_STACK_ADDRESS_BOEHM_GC_deficit; i < max_deficit; i++) {
            if (MANAGED_STACK_ADDRESS_BOEHM_GC_mark_some(NULL))
                break;
        }
#       ifdef PARALLEL_MARK
            MANAGED_STACK_ADDRESS_BOEHM_GC_parallel_mark_disabled = FALSE;
#       endif

        if (i < max_deficit && !MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(!MANAGED_STACK_ADDRESS_BOEHM_GC_collection_in_progress());
            /* Need to follow up with a full collection.        */
#           ifdef SAVE_CALL_CHAIN
                MANAGED_STACK_ADDRESS_BOEHM_GC_save_callers(MANAGED_STACK_ADDRESS_BOEHM_GC_last_stack);
#           endif
#           ifdef PARALLEL_MARK
                if (MANAGED_STACK_ADDRESS_BOEHM_GC_parallel)
                    MANAGED_STACK_ADDRESS_BOEHM_GC_wait_for_reclaim();
#           endif
#           ifndef NO_CLOCK
                if (MANAGED_STACK_ADDRESS_BOEHM_GC_time_limit != MANAGED_STACK_ADDRESS_BOEHM_GC_TIME_UNLIMITED
                        && MANAGED_STACK_ADDRESS_BOEHM_GC_n_attempts < max_prior_attempts)
                    GET_TIME(MANAGED_STACK_ADDRESS_BOEHM_GC_start_time);
#           endif
            if (MANAGED_STACK_ADDRESS_BOEHM_GC_stopped_mark(MANAGED_STACK_ADDRESS_BOEHM_GC_n_attempts < max_prior_attempts ?
                                MANAGED_STACK_ADDRESS_BOEHM_GC_timeout_stop_func : MANAGED_STACK_ADDRESS_BOEHM_GC_never_stop_func)) {
                MANAGED_STACK_ADDRESS_BOEHM_GC_finish_collection();
            } else {
                MANAGED_STACK_ADDRESS_BOEHM_GC_n_attempts++;
            }
        }
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_deficit > 0) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_deficit -= max_deficit;
            if (MANAGED_STACK_ADDRESS_BOEHM_GC_deficit < 0)
                MANAGED_STACK_ADDRESS_BOEHM_GC_deficit = 0;
        }
    } else if (!MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_maybe_gc();
    }
    RESTORE_CANCEL(cancel_state);
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void (*MANAGED_STACK_ADDRESS_BOEHM_GC_check_heap)(void) = 0;
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void (*MANAGED_STACK_ADDRESS_BOEHM_GC_print_all_smashed)(void) = 0;

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_collect_a_little(void)
{
    int result;

    if (!EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized, TRUE)) MANAGED_STACK_ADDRESS_BOEHM_GC_init();
    LOCK();
    ENTER_GC();
    /* Note: if the collection is in progress, this may do marking (not */
    /* stopping the world) even in case of disabled GC.                 */
    MANAGED_STACK_ADDRESS_BOEHM_GC_collect_a_little_inner(1);
    EXIT_GC();
    result = (int)MANAGED_STACK_ADDRESS_BOEHM_GC_collection_in_progress();
    UNLOCK();
    if (!result && MANAGED_STACK_ADDRESS_BOEHM_GC_debugging_started) MANAGED_STACK_ADDRESS_BOEHM_GC_print_all_smashed();
    return result;
}

#ifdef THREADS
  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_stop_world_external(void)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized);
    LOCK();
#   ifdef THREAD_LOCAL_ALLOC
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(!MANAGED_STACK_ADDRESS_BOEHM_GC_world_stopped);
#   endif
    STOP_WORLD();
#   ifdef THREAD_LOCAL_ALLOC
      MANAGED_STACK_ADDRESS_BOEHM_GC_world_stopped = TRUE;
#   endif
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_start_world_external(void)
  {
#   ifdef THREAD_LOCAL_ALLOC
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_world_stopped);
      MANAGED_STACK_ADDRESS_BOEHM_GC_world_stopped = FALSE;
#   else
      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized);
#   endif
    START_WORLD();
    UNLOCK();
  }
#endif /* THREADS */

#ifndef NO_CLOCK
  /* Variables for world-stop average delay time statistic computation. */
  /* "divisor" is incremented every world-stop and halved when reached  */
  /* its maximum (or upon "total_time" overflow).                       */
  static unsigned world_stopped_total_time = 0;
  static unsigned world_stopped_total_divisor = 0;
# ifndef MAX_TOTAL_TIME_DIVISOR
    /* We shall not use big values here (so "outdated" delay time       */
    /* values would have less impact on "average" delay time value than */
    /* newer ones).                                                     */
#   define MAX_TOTAL_TIME_DIVISOR 1000
# endif
#endif /* !NO_CLOCK */

#ifdef USE_MUNMAP
# ifndef MUNMAP_THRESHOLD
#   define MUNMAP_THRESHOLD 7
# endif
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_unmap_threshold = MUNMAP_THRESHOLD;

# define IF_USE_MUNMAP(x) x
# define COMMA_IF_USE_MUNMAP(x) /* comma */, x
#else
# define IF_USE_MUNMAP(x) /* empty */
# define COMMA_IF_USE_MUNMAP(x) /* empty */
#endif

/* We stop the world and mark from all roots.  If stop_func() ever      */
/* returns TRUE, we may fail and return FALSE.  Increment MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no if   */
/* we succeed.                                                          */
STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_stopped_mark(MANAGED_STACK_ADDRESS_BOEHM_GC_stop_func stop_func)
{
    int abandoned_at = 0;
    ptr_t cold_gc_frame = MANAGED_STACK_ADDRESS_BOEHM_GC_approx_sp();
#   ifndef NO_CLOCK
      CLOCK_TYPE start_time = CLOCK_TYPE_INITIALIZER;
      MANAGED_STACK_ADDRESS_BOEHM_GC_bool start_time_valid = FALSE;
#   endif

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized);
#   if !defined(REDIRECT_MALLOC) && defined(USE_WINALLOC)
        MANAGED_STACK_ADDRESS_BOEHM_GC_add_current_malloc_heap();
#   endif
#   if defined(REGISTER_LIBRARIES_EARLY)
        MANAGED_STACK_ADDRESS_BOEHM_GC_cond_register_dynamic_libraries();
#   endif

#   if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_TOGGLE_REFS_NOT_NEEDED)
      MANAGED_STACK_ADDRESS_BOEHM_GC_process_togglerefs();
#   endif

        /* Output blank line for convenience here.      */
    MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF(
              "\n--> Marking for collection #%lu after %lu allocated bytes\n",
              (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no + 1, (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd);
#   ifndef NO_CLOCK
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_PRINT_STATS_FLAG || measure_performance) {
        GET_TIME(start_time);
        start_time_valid = TRUE;
      }
#   endif
#   ifdef THREADS
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event)
        MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event(MANAGED_STACK_ADDRESS_BOEHM_GC_EVENT_PRE_STOP_WORLD);
#   endif
    STOP_WORLD();
#   ifdef THREADS
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event)
        MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event(MANAGED_STACK_ADDRESS_BOEHM_GC_EVENT_POST_STOP_WORLD);
#   endif
#   ifdef THREAD_LOCAL_ALLOC
      MANAGED_STACK_ADDRESS_BOEHM_GC_world_stopped = TRUE;
#   endif

#   ifdef MAKE_BACK_GRAPH
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_print_back_height) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_build_back_graph();
      }
#   endif

    /* Notify about marking from all roots.     */
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event)
          MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event(MANAGED_STACK_ADDRESS_BOEHM_GC_EVENT_MARK_START);

    /* Minimize junk left in my registers and on the stack.     */
            MANAGED_STACK_ADDRESS_BOEHM_GC_clear_a_few_frames();
            MANAGED_STACK_ADDRESS_BOEHM_GC_noop6(0,0,0,0,0,0);

        MANAGED_STACK_ADDRESS_BOEHM_GC_initiate_gc();
#       ifdef PARALLEL_MARK
          if (stop_func != MANAGED_STACK_ADDRESS_BOEHM_GC_never_stop_func)
            MANAGED_STACK_ADDRESS_BOEHM_GC_parallel_mark_disabled = TRUE;
#       endif
        for (abandoned_at = 0; !(*stop_func)(); abandoned_at++) {
          if (MANAGED_STACK_ADDRESS_BOEHM_GC_mark_some(cold_gc_frame)) {
#           ifdef PARALLEL_MARK
              if (MANAGED_STACK_ADDRESS_BOEHM_GC_parallel && MANAGED_STACK_ADDRESS_BOEHM_GC_parallel_mark_disabled) {
                MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF("Stopped marking done after %d iterations"
                                   " with disabled parallel marker\n",
                                   abandoned_at);
              }
#           endif
            abandoned_at = -1;
            break;
          }
        }
#       ifdef PARALLEL_MARK
          MANAGED_STACK_ADDRESS_BOEHM_GC_parallel_mark_disabled = FALSE;
#       endif

    if (abandoned_at >= 0) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_deficit = abandoned_at; /* Give the mutator a chance. */
      /* TODO: Notify MANAGED_STACK_ADDRESS_BOEHM_GC_EVENT_MARK_ABANDON */
    } else {
      MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no++;
      /* Check all debugged objects for consistency.    */
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_debugging_started) {
        (*MANAGED_STACK_ADDRESS_BOEHM_GC_check_heap)();
      }
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event)
        MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event(MANAGED_STACK_ADDRESS_BOEHM_GC_EVENT_MARK_END);
    }

#   ifdef THREADS
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event)
        MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event(MANAGED_STACK_ADDRESS_BOEHM_GC_EVENT_PRE_START_WORLD);
#   endif
#   ifdef THREAD_LOCAL_ALLOC
      MANAGED_STACK_ADDRESS_BOEHM_GC_world_stopped = FALSE;
#   endif
    START_WORLD();
#   ifdef THREADS
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event)
        MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event(MANAGED_STACK_ADDRESS_BOEHM_GC_EVENT_POST_START_WORLD);
#   endif

#   ifndef NO_CLOCK
      if (start_time_valid) {
        CLOCK_TYPE current_time;
        unsigned long time_diff, ns_frac_diff;

        /* TODO: Avoid code duplication from MANAGED_STACK_ADDRESS_BOEHM_GC_try_to_collect_inner */
        GET_TIME(current_time);
        time_diff = MS_TIME_DIFF(current_time, start_time);
        ns_frac_diff = NS_FRAC_TIME_DIFF(current_time, start_time);
        if (measure_performance) {
          stopped_mark_total_time += time_diff; /* may wrap */
          stopped_mark_total_ns_frac += (unsigned32)ns_frac_diff;
          if (stopped_mark_total_ns_frac >= (unsigned32)1000000UL) {
            stopped_mark_total_ns_frac -= (unsigned32)1000000UL;
            stopped_mark_total_time++;
          }
        }

        if (MANAGED_STACK_ADDRESS_BOEHM_GC_PRINT_STATS_FLAG) {
          unsigned total_time = world_stopped_total_time;
          unsigned divisor = world_stopped_total_divisor;

          /* Compute new world-stop delay total time.   */
          if (total_time > (((unsigned)-1) >> 1)
              || divisor >= MAX_TOTAL_TIME_DIVISOR) {
            /* Halve values if overflow occurs. */
            total_time >>= 1;
            divisor >>= 1;
          }
          total_time += time_diff < (((unsigned)-1) >> 1) ?
                        (unsigned)time_diff : ((unsigned)-1) >> 1;
          /* Update old world_stopped_total_time and its divisor.   */
          world_stopped_total_time = total_time;
          world_stopped_total_divisor = ++divisor;
          if (abandoned_at < 0) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(divisor != 0);
            MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("World-stopped marking took %lu ms %lu ns"
                          " (%u ms in average)\n", time_diff, ns_frac_diff,
                          total_time / divisor);
          }
        }
      }
#   endif

    if (abandoned_at >= 0) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF("Abandoned stopped marking after %d iterations\n",
                         abandoned_at);
      return FALSE;
    }
    return TRUE;
}

/* Set all mark bits for the free list whose first entry is q   */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_set_fl_marks(ptr_t q)
{
    if (q /* != NULL */) { /* CPPCHECK */
      struct hblk *h = HBLKPTR(q);
      struct hblk *last_h = h;
      hdr *hhdr = HDR(h);
      IF_PER_OBJ(word sz = hhdr->hb_sz;)

      for (;;) {
        word bit_no = MARK_BIT_NO((ptr_t)q - (ptr_t)h, sz);

        if (!mark_bit_from_hdr(hhdr, bit_no)) {
          set_mark_bit_from_hdr(hhdr, bit_no);
          ++hhdr -> hb_n_marks;
        }

        q = (ptr_t)obj_link(q);
        if (q == NULL)
          break;

        h = HBLKPTR(q);
        if (h != last_h) {
          last_h = h;
          hhdr = HDR(h);
          IF_PER_OBJ(sz = hhdr->hb_sz;)
        }
      }
    }
}

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) && defined(THREAD_LOCAL_ALLOC)
  /* Check that all mark bits for the free list whose first entry is    */
  /* (*pfreelist) are set.  Check skipped if points to a special value. */
  void MANAGED_STACK_ADDRESS_BOEHM_GC_check_fl_marks(void **pfreelist)
  {
    /* TODO: There is a data race with MANAGED_STACK_ADDRESS_BOEHM_GC_FAST_MALLOC_GRANS (which does */
    /* not do atomic updates to the free-list).  The race seems to be   */
    /* harmless, and for now we just skip this check in case of TSan.   */
#   if defined(AO_HAVE_load_acquire_read) && !defined(THREAD_SANITIZER)
      AO_t *list = (AO_t *)AO_load_acquire_read((AO_t *)pfreelist);
                /* Atomic operations are used because the world is running. */
      AO_t *prev;
      AO_t *p;

      if ((word)list <= HBLKSIZE) return;

      prev = (AO_t *)pfreelist;
      for (p = list; p != NULL;) {
        AO_t *next;

        if (!MANAGED_STACK_ADDRESS_BOEHM_GC_is_marked(p)) {
          ABORT_ARG2("Unmarked local free list entry",
                     ": object %p on list %p", (void *)p, (void *)list);
        }

        /* While traversing the free-list, it re-reads the pointer to   */
        /* the current node before accepting its next pointer and       */
        /* bails out if the latter has changed.  That way, it won't     */
        /* try to follow the pointer which might be been modified       */
        /* after the object was returned to the client.  It might       */
        /* perform the mark-check on the just allocated object but      */
        /* that should be harmless.                                     */
        next = (AO_t *)AO_load_acquire_read(p);
        if (AO_load(prev) != (AO_t)p)
          break;
        prev = p;
        p = next;
      }
#   else
      /* FIXME: Not implemented (just skipped). */
      (void)pfreelist;
#   endif
  }
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS && THREAD_LOCAL_ALLOC */

/* Clear all mark bits for the free list whose first entry is q */
/* Decrement MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_found by number of bytes on free list.    */
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_clear_fl_marks(ptr_t q)
{
      struct hblk *h = HBLKPTR(q);
      struct hblk *last_h = h;
      hdr *hhdr = HDR(h);
      word sz = hhdr->hb_sz; /* Normally set only once. */

      for (;;) {
        word bit_no = MARK_BIT_NO((ptr_t)q - (ptr_t)h, sz);

        if (mark_bit_from_hdr(hhdr, bit_no)) {
          size_t n_marks = hhdr -> hb_n_marks;

          MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(n_marks != 0);
          clear_mark_bit_from_hdr(hhdr, bit_no);
          n_marks--;
#         ifdef PARALLEL_MARK
            /* Appr. count, don't decrement to zero! */
            if (0 != n_marks || !MANAGED_STACK_ADDRESS_BOEHM_GC_parallel) {
              hhdr -> hb_n_marks = n_marks;
            }
#         else
            hhdr -> hb_n_marks = n_marks;
#         endif
        }
        MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_found -= (signed_word)sz;

        q = (ptr_t)obj_link(q);
        if (q == NULL)
          break;

        h = HBLKPTR(q);
        if (h != last_h) {
          last_h = h;
          hhdr = HDR(h);
          sz = hhdr->hb_sz;
        }
      }
}

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) && defined(THREAD_LOCAL_ALLOC)
  void MANAGED_STACK_ADDRESS_BOEHM_GC_check_tls(void);
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_on_heap_resize_proc MANAGED_STACK_ADDRESS_BOEHM_GC_on_heap_resize = 0;

/* Used for logging only. */
MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE int MANAGED_STACK_ADDRESS_BOEHM_GC_compute_heap_usage_percent(void)
{
  word used = MANAGED_STACK_ADDRESS_BOEHM_GC_composite_in_use + MANAGED_STACK_ADDRESS_BOEHM_GC_atomic_in_use + MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd;
  word heap_sz = MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize - MANAGED_STACK_ADDRESS_BOEHM_GC_unmapped_bytes;
# if defined(CPPCHECK)
    word limit = (MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX >> 1) / 50; /* to avoid a false positive */
# else
    const word limit = MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX / 100;
# endif

  return used >= heap_sz ? 0 : used < limit ?
                (int)((used * 100) / heap_sz) : (int)(used / (heap_sz / 100));
}

#define MANAGED_STACK_ADDRESS_BOEHM_GC_DBGLOG_PRINT_HEAP_IN_USE() \
  MANAGED_STACK_ADDRESS_BOEHM_GC_DBGLOG_PRINTF("In-use heap: %d%% (%lu KiB pointers + %lu KiB other)\n", \
                   MANAGED_STACK_ADDRESS_BOEHM_GC_compute_heap_usage_percent(), \
                   TO_KiB_UL(MANAGED_STACK_ADDRESS_BOEHM_GC_composite_in_use), \
                   TO_KiB_UL(MANAGED_STACK_ADDRESS_BOEHM_GC_atomic_in_use + MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd))

/* Finish up a collection.  Assumes mark bits are consistent, but the   */
/* world is otherwise running.                                          */
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_finish_collection(void)
{
#   ifndef NO_CLOCK
      CLOCK_TYPE start_time = CLOCK_TYPE_INITIALIZER;
      CLOCK_TYPE finalize_time = CLOCK_TYPE_INITIALIZER;
#   endif

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
#   if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) \
       && defined(THREAD_LOCAL_ALLOC) && !defined(DBG_HDRS_ALL)
        /* Check that we marked some of our own data.           */
        /* TODO: Add more checks. */
        MANAGED_STACK_ADDRESS_BOEHM_GC_check_tls();
#   endif

#   ifndef NO_CLOCK
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats)
        GET_TIME(start_time);
#   endif
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event)
      MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event(MANAGED_STACK_ADDRESS_BOEHM_GC_EVENT_RECLAIM_START);

#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_GET_HEAP_USAGE_NOT_NEEDED
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_found > 0)
        MANAGED_STACK_ADDRESS_BOEHM_GC_reclaimed_bytes_before_gc += (word)MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_found;
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_found = 0;
#   if defined(LINUX) && defined(__ELF__) && !defined(SMALL_CONFIG)
        if (GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_PRINT_ADDRESS_MAP") != 0) {
          MANAGED_STACK_ADDRESS_BOEHM_GC_print_address_map();
        }
#   endif
    COND_DUMP;
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_find_leak) {
      /* Mark all objects on the free list.  All objects should be      */
      /* marked when we're done.                                        */
      word size;        /* current object size  */
      unsigned kind;
      ptr_t q;

      for (kind = 0; kind < MANAGED_STACK_ADDRESS_BOEHM_GC_n_kinds; kind++) {
        for (size = 1; size <= MAXOBJGRANULES; size++) {
          q = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[kind].ok_freelist[size];
          if (q != NULL)
            MANAGED_STACK_ADDRESS_BOEHM_GC_set_fl_marks(q);
        }
      }
      MANAGED_STACK_ADDRESS_BOEHM_GC_start_reclaim(TRUE);
        /* The above just checks; it doesn't really reclaim anything.   */
    }

#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
      MANAGED_STACK_ADDRESS_BOEHM_GC_finalize();
#   endif
#   ifndef NO_CLOCK
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats)
        GET_TIME(finalize_time);
#   endif

    if (MANAGED_STACK_ADDRESS_BOEHM_GC_print_back_height) {
#     ifdef MAKE_BACK_GRAPH
        MANAGED_STACK_ADDRESS_BOEHM_GC_traverse_back_graph();
#     elif !defined(SMALL_CONFIG)
        MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf("Back height not available: "
                      "Rebuild collector with -DMAKE_BACK_GRAPH\n");
#     endif
    }

    /* Clear free list mark bits, in case they got accidentally marked   */
    /* (or MANAGED_STACK_ADDRESS_BOEHM_GC_find_leak is set and they were intentionally marked).      */
    /* Also subtract memory remaining from MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_found count.         */
    /* Note that composite objects on free list are cleared.             */
    /* Thus accidentally marking a free list is not a problem;  only     */
    /* objects on the list itself will be marked, and that's fixed here. */
    {
      word size;        /* current object size          */
      ptr_t q;          /* pointer to current object    */
      unsigned kind;

      for (kind = 0; kind < MANAGED_STACK_ADDRESS_BOEHM_GC_n_kinds; kind++) {
        for (size = 1; size <= MAXOBJGRANULES; size++) {
          q = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[kind].ok_freelist[size];
          if (q != NULL)
            MANAGED_STACK_ADDRESS_BOEHM_GC_clear_fl_marks(q);
        }
      }
    }

    MANAGED_STACK_ADDRESS_BOEHM_GC_VERBOSE_LOG_PRINTF("Bytes recovered before sweep - f.l. count = %ld\n",
                          (long)MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_found);

    /* Reconstruct free lists to contain everything not marked */
    MANAGED_STACK_ADDRESS_BOEHM_GC_start_reclaim(FALSE);

#   ifdef USE_MUNMAP
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_unmap_threshold > 0 /* unmapping enabled? */
          && EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no != 1, TRUE)) /* do not unmap during GC init */
        MANAGED_STACK_ADDRESS_BOEHM_GC_unmap_old(MANAGED_STACK_ADDRESS_BOEHM_GC_unmap_threshold);

      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize >= MANAGED_STACK_ADDRESS_BOEHM_GC_unmapped_bytes);
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_our_mem_bytes >= MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize);
    MANAGED_STACK_ADDRESS_BOEHM_GC_DBGLOG_PRINTF("GC #%lu freed %ld bytes, heap %lu KiB ("
                     IF_USE_MUNMAP("+ %lu KiB unmapped ")
                     "+ %lu KiB internal)\n",
                     (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no, (long)MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_found,
                     TO_KiB_UL(MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize - MANAGED_STACK_ADDRESS_BOEHM_GC_unmapped_bytes) /*, */
                     COMMA_IF_USE_MUNMAP(TO_KiB_UL(MANAGED_STACK_ADDRESS_BOEHM_GC_unmapped_bytes)),
                     TO_KiB_UL(MANAGED_STACK_ADDRESS_BOEHM_GC_our_mem_bytes - MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize
                               + sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_arrays)));
    MANAGED_STACK_ADDRESS_BOEHM_GC_DBGLOG_PRINT_HEAP_IN_USE();
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_is_full_gc) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_used_heap_size_after_full = MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize - MANAGED_STACK_ADDRESS_BOEHM_GC_large_free_bytes;
        MANAGED_STACK_ADDRESS_BOEHM_GC_need_full_gc = FALSE;
    } else {
        MANAGED_STACK_ADDRESS_BOEHM_GC_need_full_gc = MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize - MANAGED_STACK_ADDRESS_BOEHM_GC_used_heap_size_after_full
                          > min_bytes_allocd() + MANAGED_STACK_ADDRESS_BOEHM_GC_large_free_bytes;
    }

    /* Reset or increment counters for next cycle */
    MANAGED_STACK_ADDRESS_BOEHM_GC_n_attempts = 0;
    MANAGED_STACK_ADDRESS_BOEHM_GC_is_full_gc = FALSE;
    MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd_before_gc += MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd;
    MANAGED_STACK_ADDRESS_BOEHM_GC_non_gc_bytes_at_gc = MANAGED_STACK_ADDRESS_BOEHM_GC_non_gc_bytes;
    MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd = 0;
    MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_dropped = 0;
    MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_freed = 0;
    MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_bytes_freed = 0;

    if (MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event)
      MANAGED_STACK_ADDRESS_BOEHM_GC_on_collection_event(MANAGED_STACK_ADDRESS_BOEHM_GC_EVENT_RECLAIM_END);
#   ifndef NO_CLOCK
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats) {
        CLOCK_TYPE done_time;

        GET_TIME(done_time);
#       if !defined(SMALL_CONFIG) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION)
          /* A convenient place to output finalization statistics.      */
          MANAGED_STACK_ADDRESS_BOEHM_GC_print_finalization_stats();
#       endif
        MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("Finalize and initiate sweep took %lu ms %lu ns"
                      " + %lu ms %lu ns\n",
                      MS_TIME_DIFF(finalize_time, start_time),
                      NS_FRAC_TIME_DIFF(finalize_time, start_time),
                      MS_TIME_DIFF(done_time, finalize_time),
                      NS_FRAC_TIME_DIFF(done_time, finalize_time));
      }
#   elif !defined(SMALL_CONFIG) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION)
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_print_stats)
        MANAGED_STACK_ADDRESS_BOEHM_GC_print_finalization_stats();
#   endif
}

STATIC word MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize_at_forced_unmap = 0;
                                /* accessed with the allocation lock held */

/* If stop_func == 0 then MANAGED_STACK_ADDRESS_BOEHM_GC_default_stop_func is used instead.         */
STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_try_to_collect_general(MANAGED_STACK_ADDRESS_BOEHM_GC_stop_func stop_func,
                                         MANAGED_STACK_ADDRESS_BOEHM_GC_bool force_unmap)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_bool result;
    IF_USE_MUNMAP(unsigned old_unmap_threshold;)
    IF_CANCEL(int cancel_state;)

    if (!EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized, TRUE)) MANAGED_STACK_ADDRESS_BOEHM_GC_init();
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_debugging_started) MANAGED_STACK_ADDRESS_BOEHM_GC_print_all_smashed();
    MANAGED_STACK_ADDRESS_BOEHM_GC_INVOKE_FINALIZERS();
    LOCK();
    if (force_unmap) {
      /* Record current heap size to make heap growth more conservative */
      /* afterwards (as if the heap is growing from zero size again).   */
      MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize_at_forced_unmap = MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize;
    }
    DISABLE_CANCEL(cancel_state);
#   ifdef USE_MUNMAP
      old_unmap_threshold = MANAGED_STACK_ADDRESS_BOEHM_GC_unmap_threshold;
      if (force_unmap ||
          (MANAGED_STACK_ADDRESS_BOEHM_GC_force_unmap_on_gcollect && old_unmap_threshold > 0))
        MANAGED_STACK_ADDRESS_BOEHM_GC_unmap_threshold = 1; /* unmap as much as possible */
#   endif
    ENTER_GC();
    /* Minimize junk left in my registers */
      MANAGED_STACK_ADDRESS_BOEHM_GC_noop6(0,0,0,0,0,0);
    result = MANAGED_STACK_ADDRESS_BOEHM_GC_try_to_collect_inner(stop_func != 0 ? stop_func :
                                     MANAGED_STACK_ADDRESS_BOEHM_GC_default_stop_func);
    EXIT_GC();
    IF_USE_MUNMAP(MANAGED_STACK_ADDRESS_BOEHM_GC_unmap_threshold = old_unmap_threshold); /* restore */
    RESTORE_CANCEL(cancel_state);
    UNLOCK();
    if (result) {
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_debugging_started) MANAGED_STACK_ADDRESS_BOEHM_GC_print_all_smashed();
        MANAGED_STACK_ADDRESS_BOEHM_GC_INVOKE_FINALIZERS();
    }
    return result;
}

/* Externally callable routines to invoke full, stop-the-world collection. */

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_try_to_collect(MANAGED_STACK_ADDRESS_BOEHM_GC_stop_func stop_func)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NONNULL_ARG_NOT_NULL(stop_func));
    return (int)MANAGED_STACK_ADDRESS_BOEHM_GC_try_to_collect_general(stop_func, FALSE);
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect(void)
{
    /* 0 is passed as stop_func to get MANAGED_STACK_ADDRESS_BOEHM_GC_default_stop_func value       */
    /* while holding the allocation lock (to prevent data races).       */
    (void)MANAGED_STACK_ADDRESS_BOEHM_GC_try_to_collect_general(0, FALSE);
    if (get_have_errors())
      MANAGED_STACK_ADDRESS_BOEHM_GC_print_all_errors();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect_and_unmap(void)
{
    /* Collect and force memory unmapping to OS. */
    (void)MANAGED_STACK_ADDRESS_BOEHM_GC_try_to_collect_general(MANAGED_STACK_ADDRESS_BOEHM_GC_never_stop_func, TRUE);
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_os_get_mem(size_t bytes)
{
  struct hblk *space = GET_MEM(bytes); /* HBLKSIZE-aligned */

  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
  if (EXPECT(NULL == space, FALSE)) return NULL;
# ifdef USE_PROC_FOR_LIBRARIES
    /* Add HBLKSIZE aligned, GET_MEM-generated block to MANAGED_STACK_ADDRESS_BOEHM_GC_our_memory. */
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_n_memory >= MAX_HEAP_SECTS)
      ABORT("Too many GC-allocated memory sections: Increase MAX_HEAP_SECTS");
    MANAGED_STACK_ADDRESS_BOEHM_GC_our_memory[MANAGED_STACK_ADDRESS_BOEHM_GC_n_memory].hs_start = (ptr_t)space;
    MANAGED_STACK_ADDRESS_BOEHM_GC_our_memory[MANAGED_STACK_ADDRESS_BOEHM_GC_n_memory].hs_bytes = bytes;
    MANAGED_STACK_ADDRESS_BOEHM_GC_n_memory++;
# endif
  MANAGED_STACK_ADDRESS_BOEHM_GC_our_mem_bytes += bytes;
  MANAGED_STACK_ADDRESS_BOEHM_GC_VERBOSE_LOG_PRINTF("Got %lu bytes from OS\n", (unsigned long)bytes);
  return (ptr_t)space;
}

/* Use the chunk of memory starting at p of size bytes as part of the heap. */
/* Assumes p is HBLKSIZE aligned, bytes argument is a multiple of HBLKSIZE. */
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_heap(struct hblk *p, size_t bytes)
{
    hdr * phdr;
    word endp;
    size_t old_capacity = 0;
    void *old_heap_sects = NULL;
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS
      unsigned i;
#   endif

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((word)p % HBLKSIZE == 0);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(bytes % HBLKSIZE == 0);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(bytes > 0);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_all_nils != NULL);

    if (EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_n_heap_sects == MANAGED_STACK_ADDRESS_BOEHM_GC_capacity_heap_sects, FALSE)) {
      /* Allocate new MANAGED_STACK_ADDRESS_BOEHM_GC_heap_sects with sufficient capacity.   */
#     ifndef INITIAL_HEAP_SECTS
#       define INITIAL_HEAP_SECTS 32
#     endif
      size_t new_capacity = MANAGED_STACK_ADDRESS_BOEHM_GC_n_heap_sects > 0 ?
                (size_t)MANAGED_STACK_ADDRESS_BOEHM_GC_n_heap_sects * 2 : INITIAL_HEAP_SECTS;
      void *new_heap_sects =
                MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_alloc(new_capacity * sizeof(struct HeapSect));

      if (NULL == new_heap_sects) {
        /* Retry with smaller yet sufficient capacity.  */
        new_capacity = (size_t)MANAGED_STACK_ADDRESS_BOEHM_GC_n_heap_sects + INITIAL_HEAP_SECTS;
        new_heap_sects =
                MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_alloc(new_capacity * sizeof(struct HeapSect));
        if (NULL == new_heap_sects)
          ABORT("Insufficient memory for heap sections");
      }
      old_capacity = MANAGED_STACK_ADDRESS_BOEHM_GC_capacity_heap_sects;
      old_heap_sects = MANAGED_STACK_ADDRESS_BOEHM_GC_heap_sects;
      /* Transfer MANAGED_STACK_ADDRESS_BOEHM_GC_heap_sects contents to the newly allocated array.  */
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_n_heap_sects > 0)
        BCOPY(old_heap_sects, new_heap_sects,
              MANAGED_STACK_ADDRESS_BOEHM_GC_n_heap_sects * sizeof(struct HeapSect));
      MANAGED_STACK_ADDRESS_BOEHM_GC_capacity_heap_sects = new_capacity;
      MANAGED_STACK_ADDRESS_BOEHM_GC_heap_sects = (struct HeapSect *)new_heap_sects;
      MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF("Grew heap sections array to %lu elements\n",
                         (unsigned long)new_capacity);
    }

    while (EXPECT((word)p <= HBLKSIZE, FALSE)) {
        /* Can't handle memory near address zero. */
        ++p;
        bytes -= HBLKSIZE;
        if (0 == bytes) return;
    }
    endp = (word)p + bytes;
    if (EXPECT(endp <= (word)p, FALSE)) {
        /* Address wrapped. */
        bytes -= HBLKSIZE;
        if (0 == bytes) return;
        endp -= HBLKSIZE;
    }
    phdr = MANAGED_STACK_ADDRESS_BOEHM_GC_install_header(p);
    if (EXPECT(NULL == phdr, FALSE)) {
        /* This is extremely unlikely. Can't add it.  This will         */
        /* almost certainly result in a 0 return from the allocator,    */
        /* which is entirely appropriate.                               */
        return;
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(endp > (word)p && endp == (word)p + bytes);
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS
      /* Ensure no intersection between sections.       */
      for (i = 0; i < MANAGED_STACK_ADDRESS_BOEHM_GC_n_heap_sects; i++) {
        word hs_start = (word)MANAGED_STACK_ADDRESS_BOEHM_GC_heap_sects[i].hs_start;
        word hs_end = hs_start + MANAGED_STACK_ADDRESS_BOEHM_GC_heap_sects[i].hs_bytes;

        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(!((hs_start <= (word)p && (word)p < hs_end)
                    || (hs_start < endp && endp <= hs_end)
                    || ((word)p < hs_start && hs_end < endp)));
      }
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_heap_sects[MANAGED_STACK_ADDRESS_BOEHM_GC_n_heap_sects].hs_start = (ptr_t)p;
    MANAGED_STACK_ADDRESS_BOEHM_GC_heap_sects[MANAGED_STACK_ADDRESS_BOEHM_GC_n_heap_sects].hs_bytes = bytes;
    MANAGED_STACK_ADDRESS_BOEHM_GC_n_heap_sects++;
    phdr -> hb_sz = bytes;
    phdr -> hb_flags = 0;
    MANAGED_STACK_ADDRESS_BOEHM_GC_freehblk(p);
    MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize += bytes;

    if ((word)p <= (word)MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr
        || EXPECT(NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr, FALSE)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr = (void *)((ptr_t)p - sizeof(word));
                /* Making it a little smaller than necessary prevents   */
                /* us from getting a false hit from the variable        */
                /* itself.  There's some unintentional reflection       */
                /* here.                                                */
    }
    if (endp > (word)MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr = (void *)endp;
    }
#   ifdef SET_REAL_HEAP_BOUNDS
      if ((word)p < MANAGED_STACK_ADDRESS_BOEHM_GC_least_real_heap_addr
          || EXPECT(0 == MANAGED_STACK_ADDRESS_BOEHM_GC_least_real_heap_addr, FALSE))
        MANAGED_STACK_ADDRESS_BOEHM_GC_least_real_heap_addr = (word)p - sizeof(word);
      if (endp > MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_real_heap_addr) {
#       ifdef INCLUDE_LINUX_THREAD_DESCR
          /* Avoid heap intersection with the static data roots. */
          MANAGED_STACK_ADDRESS_BOEHM_GC_exclude_static_roots_inner((void *)p, (void *)endp);
#       endif
        MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_real_heap_addr = endp;
      }
#   endif
    if (EXPECT(old_capacity > 0, FALSE)) {
#     ifndef GWW_VDB
        /* Recycling may call MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_heap() again but should not     */
        /* cause resizing of MANAGED_STACK_ADDRESS_BOEHM_GC_heap_sects.                             */
        MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_recycle_no_gww(old_heap_sects,
                                  old_capacity * sizeof(struct HeapSect));
#     else
        /* TODO: implement GWW-aware recycling as in alloc_mark_stack */
        MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((word)old_heap_sects);
#     endif
    }
}

#if !defined(NO_DEBUGGING)
  void MANAGED_STACK_ADDRESS_BOEHM_GC_print_heap_sects(void)
  {
    unsigned i;

    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Total heap size: %lu" IF_USE_MUNMAP(" (%lu unmapped)") "\n",
              (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize /*, */
              COMMA_IF_USE_MUNMAP((unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_unmapped_bytes));

    for (i = 0; i < MANAGED_STACK_ADDRESS_BOEHM_GC_n_heap_sects; i++) {
      ptr_t start = MANAGED_STACK_ADDRESS_BOEHM_GC_heap_sects[i].hs_start;
      size_t len = MANAGED_STACK_ADDRESS_BOEHM_GC_heap_sects[i].hs_bytes;
      struct hblk *h;
      unsigned nbl = 0;

      for (h = (struct hblk *)start; (word)h < (word)(start + len); h++) {
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_is_black_listed(h, HBLKSIZE)) nbl++;
      }
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Section %d from %p to %p %u/%lu blacklisted\n",
                i, (void *)start, (void *)&start[len],
                nbl, (unsigned long)divHBLKSZ(len));
    }
  }
#endif

void * MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr = (void *)MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX;
void * MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr = 0;

STATIC word MANAGED_STACK_ADDRESS_BOEHM_GC_max_heapsize = 0;

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_max_heap_size(MANAGED_STACK_ADDRESS_BOEHM_GC_word n)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_max_heapsize = n;
}

word MANAGED_STACK_ADDRESS_BOEHM_GC_max_retries = 0;

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_recycle_inner(void *ptr, size_t bytes)
{
  size_t page_offset;
  size_t displ = 0;
  size_t recycled_bytes;

  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
  if (NULL == ptr) return;

  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(bytes != 0);
  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_page_size != 0);
  /* TODO: Assert correct memory flags if GWW_VDB */
  page_offset = (word)ptr & (MANAGED_STACK_ADDRESS_BOEHM_GC_page_size - 1);
  if (page_offset != 0)
    displ = MANAGED_STACK_ADDRESS_BOEHM_GC_page_size - page_offset;
  recycled_bytes = bytes > displ ? (bytes - displ) & ~(MANAGED_STACK_ADDRESS_BOEHM_GC_page_size - 1) : 0;
  MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF("Recycle %lu/%lu scratch-allocated bytes at %p\n",
                (unsigned long)recycled_bytes, (unsigned long)bytes, ptr);
  if (recycled_bytes > 0)
    MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_heap((struct hblk *)((word)ptr + displ), recycled_bytes);
}

/* This explicitly increases the size of the heap.  It is used          */
/* internally, but may also be invoked from MANAGED_STACK_ADDRESS_BOEHM_GC_expand_hp by the user.   */
/* The argument is in units of HBLKSIZE (zero is treated as 1).         */
/* Returns FALSE on failure.                                            */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_expand_hp_inner(word n)
{
    size_t bytes;
    struct hblk * space;
    word expansion_slop;        /* Number of bytes by which we expect   */
                                /* the heap to expand soon.             */

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_page_size != 0);
    if (0 == n) n = 1;
    bytes = ROUNDUP_PAGESIZE((size_t)n * HBLKSIZE);
    MANAGED_STACK_ADDRESS_BOEHM_GC_DBGLOG_PRINT_HEAP_IN_USE();
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_max_heapsize != 0
        && (MANAGED_STACK_ADDRESS_BOEHM_GC_max_heapsize < (word)bytes
            || MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize > MANAGED_STACK_ADDRESS_BOEHM_GC_max_heapsize - (word)bytes)) {
        /* Exceeded self-imposed limit */
        return FALSE;
    }
    space = (struct hblk *)MANAGED_STACK_ADDRESS_BOEHM_GC_os_get_mem(bytes);
    if (EXPECT(NULL == space, FALSE)) {
        WARN("Failed to expand heap by %" WARN_PRIuPTR " KiB\n", bytes >> 10);
        return FALSE;
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_last_heap_growth_gc_no = MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no;
    MANAGED_STACK_ADDRESS_BOEHM_GC_INFOLOG_PRINTF("Grow heap to %lu KiB after %lu bytes allocated\n",
                      TO_KiB_UL(MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize + bytes),
                      (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd);

    /* Adjust heap limits generously for blacklisting to work better.   */
    /* MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_heap performs minimal adjustment needed for            */
    /* correctness.                                                     */
    expansion_slop = min_bytes_allocd() + 4 * MAXHINCR * HBLKSIZE;
    if ((MANAGED_STACK_ADDRESS_BOEHM_GC_last_heap_addr == 0 && !((word)space & SIGNB))
        || (MANAGED_STACK_ADDRESS_BOEHM_GC_last_heap_addr != 0
            && (word)MANAGED_STACK_ADDRESS_BOEHM_GC_last_heap_addr < (word)space)) {
        /* Assume the heap is growing up. */
        word new_limit = (word)space + (word)bytes + expansion_slop;
        if (new_limit > (word)space
            && (word)MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr < new_limit)
          MANAGED_STACK_ADDRESS_BOEHM_GC_greatest_plausible_heap_addr = (void *)new_limit;
    } else {
        /* Heap is growing down. */
        word new_limit = (word)space - expansion_slop - sizeof(word);
        if (new_limit < (word)space
            && (word)MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr > new_limit)
          MANAGED_STACK_ADDRESS_BOEHM_GC_least_plausible_heap_addr = (void *)new_limit;
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_last_heap_addr = (ptr_t)space;

    MANAGED_STACK_ADDRESS_BOEHM_GC_add_to_heap(space, bytes);
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_on_heap_resize)
        (*MANAGED_STACK_ADDRESS_BOEHM_GC_on_heap_resize)(MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize);

    return TRUE;
}

/* Really returns a bool, but it's externally visible, so that's clumsy. */
MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_expand_hp(size_t bytes)
{
    word n_blocks = OBJ_SZ_TO_BLOCKS_CHECKED(bytes);
    word old_heapsize;
    MANAGED_STACK_ADDRESS_BOEHM_GC_bool result;

    if (!EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized, TRUE)) MANAGED_STACK_ADDRESS_BOEHM_GC_init();
    LOCK();
    old_heapsize = MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize;
    result = MANAGED_STACK_ADDRESS_BOEHM_GC_expand_hp_inner(n_blocks);
    if (result) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_requested_heapsize += bytes;
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc) {
        /* Do not call WARN if the heap growth is intentional.  */
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize >= old_heapsize);
        MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize_on_gc_disable += MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize - old_heapsize;
      }
    }
    UNLOCK();
    return (int)result;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_fail_count = 0;
                        /* How many consecutive GC/expansion failures?  */
                        /* Reset by MANAGED_STACK_ADDRESS_BOEHM_GC_allochblk.                       */

/* The minimum value of the ratio of allocated bytes since the latest   */
/* GC to the amount of finalizers created since that GC which triggers  */
/* the collection instead heap expansion.  Has no effect in the         */
/* incremental mode.                                                    */
#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOCD_BYTES_PER_FINALIZER) && !defined(CPPCHECK)
  STATIC word MANAGED_STACK_ADDRESS_BOEHM_GC_allocd_bytes_per_finalizer = MANAGED_STACK_ADDRESS_BOEHM_GC_ALLOCD_BYTES_PER_FINALIZER;
#else
  STATIC word MANAGED_STACK_ADDRESS_BOEHM_GC_allocd_bytes_per_finalizer = 10000;
#endif

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_allocd_bytes_per_finalizer(MANAGED_STACK_ADDRESS_BOEHM_GC_word value)
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_allocd_bytes_per_finalizer = value;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_word MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_allocd_bytes_per_finalizer(void)
{
  return MANAGED_STACK_ADDRESS_BOEHM_GC_allocd_bytes_per_finalizer;
}

static word last_fo_entries = 0;
static word last_bytes_finalized = 0;

/* Collect or expand heap in an attempt make the indicated number of    */
/* free blocks available.  Should be called until the blocks are        */
/* available (setting retry value to TRUE unless this is the first call */
/* in a loop) or until it fails by returning FALSE.  The flags argument */
/* should be IGNORE_OFF_PAGE or 0.                                      */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_collect_or_expand(word needed_blocks,
                                      unsigned flags,
                                      MANAGED_STACK_ADDRESS_BOEHM_GC_bool retry)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_bool gc_not_stopped = TRUE;
    word blocks_to_get;
    IF_CANCEL(int cancel_state;)

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized);
    DISABLE_CANCEL(cancel_state);
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_incremental && !MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc &&
        ((MANAGED_STACK_ADDRESS_BOEHM_GC_dont_expand && MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd > 0)
         || (MANAGED_STACK_ADDRESS_BOEHM_GC_fo_entries > last_fo_entries
             && (last_bytes_finalized | MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_finalized) != 0
             && (MANAGED_STACK_ADDRESS_BOEHM_GC_fo_entries - last_fo_entries)
                * MANAGED_STACK_ADDRESS_BOEHM_GC_allocd_bytes_per_finalizer > MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd)
         || MANAGED_STACK_ADDRESS_BOEHM_GC_should_collect())) {
      /* Try to do a full collection using 'default' stop_func (unless  */
      /* nothing has been allocated since the latest collection or heap */
      /* expansion is disabled).                                        */
      gc_not_stopped = MANAGED_STACK_ADDRESS_BOEHM_GC_try_to_collect_inner(
                        MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd > 0 && (!MANAGED_STACK_ADDRESS_BOEHM_GC_dont_expand || !retry) ?
                        MANAGED_STACK_ADDRESS_BOEHM_GC_default_stop_func : MANAGED_STACK_ADDRESS_BOEHM_GC_never_stop_func);
      if (gc_not_stopped == TRUE || !retry) {
        /* Either the collection hasn't been aborted or this is the     */
        /* first attempt (in a loop).                                   */
        last_fo_entries = MANAGED_STACK_ADDRESS_BOEHM_GC_fo_entries;
        last_bytes_finalized = MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_finalized;
        RESTORE_CANCEL(cancel_state);
        return TRUE;
      }
    }

    blocks_to_get = (MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize - MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize_at_forced_unmap)
                        / (HBLKSIZE * MANAGED_STACK_ADDRESS_BOEHM_GC_free_space_divisor)
                    + needed_blocks;
    if (blocks_to_get > MAXHINCR) {
      word slop;

      /* Get the minimum required to make it likely that we can satisfy */
      /* the current request in the presence of black-listing.          */
      /* This will probably be more than MAXHINCR.                      */
      if ((flags & IGNORE_OFF_PAGE) != 0) {
        slop = 4;
      } else {
        slop = 2 * divHBLKSZ(BL_LIMIT);
        if (slop > needed_blocks) slop = needed_blocks;
      }
      if (needed_blocks + slop > MAXHINCR) {
        blocks_to_get = needed_blocks + slop;
      } else {
        blocks_to_get = MAXHINCR;
      }
      if (blocks_to_get > divHBLKSZ(MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX))
        blocks_to_get = divHBLKSZ(MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX);
    } else if (blocks_to_get < MINHINCR) {
      blocks_to_get = MINHINCR;
    }

    if (MANAGED_STACK_ADDRESS_BOEHM_GC_max_heapsize > MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize) {
      word max_get_blocks = divHBLKSZ(MANAGED_STACK_ADDRESS_BOEHM_GC_max_heapsize - MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize);
      if (blocks_to_get > max_get_blocks)
        blocks_to_get = max_get_blocks > needed_blocks
                        ? max_get_blocks : needed_blocks;
    }

#   ifdef USE_MUNMAP
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_unmap_threshold > 1) {
        /* Return as much memory to the OS as possible before   */
        /* trying to get memory from it.                        */
        MANAGED_STACK_ADDRESS_BOEHM_GC_unmap_old(0);
      }
#   endif
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_expand_hp_inner(blocks_to_get)
        && (blocks_to_get == needed_blocks
            || !MANAGED_STACK_ADDRESS_BOEHM_GC_expand_hp_inner(needed_blocks))) {
      if (gc_not_stopped == FALSE) {
        /* Don't increment MANAGED_STACK_ADDRESS_BOEHM_GC_fail_count here (and no warning).     */
        MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect_inner();
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd == 0);
      } else if (MANAGED_STACK_ADDRESS_BOEHM_GC_fail_count++ < MANAGED_STACK_ADDRESS_BOEHM_GC_max_retries) {
        WARN("Out of Memory!  Trying to continue...\n", 0);
        MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect_inner();
      } else {
#       if !defined(AMIGA) || !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_FASTALLOC)
#         ifdef USE_MUNMAP
            MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize >= MANAGED_STACK_ADDRESS_BOEHM_GC_unmapped_bytes);
#         endif
#         if !defined(SMALL_CONFIG) && (CPP_WORDSZ >= 32)
#           define MAX_HEAPSIZE_WARNED_IN_BYTES (5 << 20) /* 5 MB */
            if (MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize > (word)MAX_HEAPSIZE_WARNED_IN_BYTES) {
              WARN("Out of Memory! Heap size: %" WARN_PRIuPTR " MiB."
                   " Returning NULL!\n",
                   (MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize - MANAGED_STACK_ADDRESS_BOEHM_GC_unmapped_bytes) >> 20);
            } else
#         endif
          /* else */ {
              WARN("Out of Memory! Heap size: %" WARN_PRIuPTR " bytes."
                   " Returning NULL!\n", MANAGED_STACK_ADDRESS_BOEHM_GC_heapsize - MANAGED_STACK_ADDRESS_BOEHM_GC_unmapped_bytes);
          }
#       endif
        RESTORE_CANCEL(cancel_state);
        return FALSE;
      }
    } else if (MANAGED_STACK_ADDRESS_BOEHM_GC_fail_count) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF("Memory available again...\n");
    }
    RESTORE_CANCEL(cancel_state);
    return TRUE;
}

/*
 * Make sure the object free list for size gran (in granules) is not empty.
 * Return a pointer to the first object on the free list.
 * The object MUST BE REMOVED FROM THE FREE LIST BY THE CALLER.
 */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_allocobj(size_t gran, int kind)
{
    void ** flh = &MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[kind].ok_freelist[gran];
    MANAGED_STACK_ADDRESS_BOEHM_GC_bool tried_minor = FALSE;
    MANAGED_STACK_ADDRESS_BOEHM_GC_bool retry = FALSE;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized);
    if (0 == gran) return NULL;

    while (NULL == *flh) {
      ENTER_GC();
#     ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_DISABLE_INCREMENTAL
        if (MANAGED_STACK_ADDRESS_BOEHM_GC_incremental && MANAGED_STACK_ADDRESS_BOEHM_GC_time_limit != MANAGED_STACK_ADDRESS_BOEHM_GC_TIME_UNLIMITED
            && !MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc) {
          /* True incremental mode, not just generational.      */
          /* Do our share of marking work.                      */
          MANAGED_STACK_ADDRESS_BOEHM_GC_collect_a_little_inner(1);
        }
#     endif
      /* Sweep blocks for objects of this size */
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(!MANAGED_STACK_ADDRESS_BOEHM_GC_is_full_gc
                  || NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[kind].ok_reclaim_list
                  || NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[kind].ok_reclaim_list[gran]);
        MANAGED_STACK_ADDRESS_BOEHM_GC_continue_reclaim(gran, kind);
      EXIT_GC();
#     if defined(CPPCHECK)
        MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((word)&flh);
#     endif
      if (NULL == *flh) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_new_hblk(gran, kind);
#       if defined(CPPCHECK)
          MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((word)&flh);
#       endif
        if (NULL == *flh) {
          ENTER_GC();
          if (MANAGED_STACK_ADDRESS_BOEHM_GC_incremental && MANAGED_STACK_ADDRESS_BOEHM_GC_time_limit == MANAGED_STACK_ADDRESS_BOEHM_GC_TIME_UNLIMITED
              && !tried_minor && !MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_collect_a_little_inner(1);
            tried_minor = TRUE;
          } else {
            if (!MANAGED_STACK_ADDRESS_BOEHM_GC_collect_or_expand(1, 0 /* flags */, retry)) {
              EXIT_GC();
              return NULL;
            }
            retry = TRUE;
          }
          EXIT_GC();
        }
      }
    }
    /* Successful allocation; reset failure count.      */
    MANAGED_STACK_ADDRESS_BOEHM_GC_fail_count = 0;

    return (ptr_t)(*flh);
}
