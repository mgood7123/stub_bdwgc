/*
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
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
 *
 */

#include "private/gc_pmark.h"  /* includes gc_priv.h */

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT

/*
 * This is an allocator interface tuned for gcj (the GNU static
 * java compiler).
 *
 * Each allocated object has a pointer in its first word to a vtable,
 * which for our purposes is simply a structure describing the type of
 * the object.
 * This descriptor structure contains a GC marking descriptor at offset
 * MARK_DESCR_OFFSET.
 *
 * It is hoped that this interface may also be useful for other systems,
 * possibly with some tuning of the constants.  But the immediate goal
 * is to get better gcj performance.
 *
 * We assume: counting on explicit initialization of this interface is OK.
 */

#include "gc/gc_gcj.h"
#include "private/dbg_mlc.h"

int MANAGED_STACK_ADDRESS_BOEHM_GC_gcj_kind = 0;    /* Object kind for objects with descriptors     */
                        /* in "vtable".                                 */
int MANAGED_STACK_ADDRESS_BOEHM_GC_gcj_debug_kind = 0;
                        /* The kind of objects that is always marked    */
                        /* with a mark proc call.                       */

STATIC struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry *MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK MANAGED_STACK_ADDRESS_BOEHM_GC_gcj_fake_mark_proc(word *addr,
                        struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry *mark_stack_ptr,
                        struct MANAGED_STACK_ADDRESS_BOEHM_GC_ms_entry * mark_stack_limit, word env)
{
    UNUSED_ARG(addr);
    UNUSED_ARG(mark_stack_limit);
    UNUSED_ARG(env);
#   if defined(FUNCPTR_IS_WORD) && defined(CPPCHECK)
        MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((word)&MANAGED_STACK_ADDRESS_BOEHM_GC_init_gcj_malloc);
#   endif
    ABORT_RET("No client gcj mark proc is specified");
    return mark_stack_ptr;
}

#ifdef FUNCPTR_IS_WORD
  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_init_gcj_malloc(int mp_index, void *mp)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_init_gcj_malloc_mp((unsigned)mp_index, (MANAGED_STACK_ADDRESS_BOEHM_GC_mark_proc)(word)mp);
  }
#endif /* FUNCPTR_IS_WORD */

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_init_gcj_malloc_mp(unsigned mp_index, MANAGED_STACK_ADDRESS_BOEHM_GC_mark_proc mp)
{
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_IGNORE_GCJ_INFO
      MANAGED_STACK_ADDRESS_BOEHM_GC_bool ignore_gcj_info;
#   endif

    if (mp == 0)        /* In case MANAGED_STACK_ADDRESS_BOEHM_GC_DS_PROC is unused.        */
      mp = MANAGED_STACK_ADDRESS_BOEHM_GC_gcj_fake_mark_proc;

    MANAGED_STACK_ADDRESS_BOEHM_GC_init();  /* In case it's not already done.       */
    LOCK();
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_gcjobjfreelist != NULL) {
      /* Already initialized.   */
      UNLOCK();
      return;
    }
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_IGNORE_GCJ_INFO
      /* This is useful for debugging on platforms with missing getenv(). */
#     define ignore_gcj_info TRUE
#   else
      ignore_gcj_info = (0 != GETENV("MANAGED_STACK_ADDRESS_BOEHM_GC_IGNORE_GCJ_INFO"));
#   endif
    if (ignore_gcj_info) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF("Gcj-style type information is disabled!\n");
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_mark_procs[mp_index] == (MANAGED_STACK_ADDRESS_BOEHM_GC_mark_proc)0); /* unused */
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_procs[mp_index] = mp;
    if (mp_index >= MANAGED_STACK_ADDRESS_BOEHM_GC_n_mark_procs)
        ABORT("MANAGED_STACK_ADDRESS_BOEHM_GC_init_gcj_malloc_mp: bad index");
    /* Set up object kind gcj-style indirect descriptor. */
    MANAGED_STACK_ADDRESS_BOEHM_GC_gcjobjfreelist = (ptr_t *)MANAGED_STACK_ADDRESS_BOEHM_GC_new_free_list_inner();
    if (ignore_gcj_info) {
        /* Use a simple length-based descriptor, thus forcing a fully   */
        /* conservative scan.                                           */
        MANAGED_STACK_ADDRESS_BOEHM_GC_gcj_kind = (int)MANAGED_STACK_ADDRESS_BOEHM_GC_new_kind_inner((void **)MANAGED_STACK_ADDRESS_BOEHM_GC_gcjobjfreelist,
                                             /* 0 | */ MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH,
                                             TRUE, TRUE);
        MANAGED_STACK_ADDRESS_BOEHM_GC_gcj_debug_kind = MANAGED_STACK_ADDRESS_BOEHM_GC_gcj_kind;
    } else {
        MANAGED_STACK_ADDRESS_BOEHM_GC_gcj_kind = (int)MANAGED_STACK_ADDRESS_BOEHM_GC_new_kind_inner(
                        (void **)MANAGED_STACK_ADDRESS_BOEHM_GC_gcjobjfreelist,
                        (((word)(-(signed_word)MARK_DESCR_OFFSET
                                 - MANAGED_STACK_ADDRESS_BOEHM_GC_INDIR_PER_OBJ_BIAS))
                         | MANAGED_STACK_ADDRESS_BOEHM_GC_DS_PER_OBJECT),
                        FALSE, TRUE);
        /* Set up object kind for objects that require mark proc call.  */
        MANAGED_STACK_ADDRESS_BOEHM_GC_gcj_debug_kind = (int)MANAGED_STACK_ADDRESS_BOEHM_GC_new_kind_inner(MANAGED_STACK_ADDRESS_BOEHM_GC_new_free_list_inner(),
                                MANAGED_STACK_ADDRESS_BOEHM_GC_MAKE_PROC(mp_index,
                                        1 /* allocated with debug info */),
                                FALSE, TRUE);
    }
    UNLOCK();
#   undef ignore_gcj_info
}

/* We need a mechanism to release the lock and invoke finalizers.       */
/* We don't really have an opportunity to do this on a rarely executed  */
/* path on which the lock is not held.  Thus we check at a              */
/* rarely executed point at which it is safe to release the lock.       */
/* We do this even where we could just call MANAGED_STACK_ADDRESS_BOEHM_GC_INVOKE_FINALIZERS,       */
/* since it's probably cheaper and certainly more uniform.              */
/* TODO: Consider doing the same elsewhere? */
static void maybe_finalize(void)
{
   static word last_finalized_no = 0;

   MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
   if (MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no == last_finalized_no ||
       !EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized, TRUE)) return;
   UNLOCK();
   MANAGED_STACK_ADDRESS_BOEHM_GC_INVOKE_FINALIZERS();
   LOCK();
   last_finalized_no = MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no;
}

/* Allocate an object, clear it, and store the pointer to the   */
/* type structure (vtable in gcj).  This adds a byte at the     */
/* end of the object if MANAGED_STACK_ADDRESS_BOEHM_GC_malloc would.                        */
#ifdef THREAD_LOCAL_ALLOC
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER
#else
  STATIC
#endif
void * MANAGED_STACK_ADDRESS_BOEHM_GC_core_gcj_malloc(size_t lb, void * ptr_to_struct_containing_descr,
                          unsigned flags)
{
    ptr_t op;
    size_t lg;

    MANAGED_STACK_ADDRESS_BOEHM_GC_DBG_COLLECT_AT_MALLOC(lb);
    LOCK();
    if (SMALL_OBJ(lb) && (op = MANAGED_STACK_ADDRESS_BOEHM_GC_gcjobjfreelist[lg = MANAGED_STACK_ADDRESS_BOEHM_GC_size_map[lb]],
                          EXPECT(op != NULL, TRUE))) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_gcjobjfreelist[lg] = (ptr_t)obj_link(op);
        MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_allocd += GRANULES_TO_BYTES((word)lg);
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NULL == ((void **)op)[1]);
    } else {
        maybe_finalize();
        op = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_clear_stack(MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_inner(lb, MANAGED_STACK_ADDRESS_BOEHM_GC_gcj_kind,
                                                           flags));
        if (NULL == op) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_oom_func oom_fn = MANAGED_STACK_ADDRESS_BOEHM_GC_oom_fn;
            UNLOCK();
            return (*oom_fn)(lb);
        }
    }
    *(void **)op = ptr_to_struct_containing_descr;
    UNLOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(op);
    REACHABLE_AFTER_DIRTY(ptr_to_struct_containing_descr);
    return (void *)op;
}

#ifndef THREAD_LOCAL_ALLOC
  MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_gcj_malloc(size_t lb,
                                      void * ptr_to_struct_containing_descr)
  {
    return MANAGED_STACK_ADDRESS_BOEHM_GC_core_gcj_malloc(lb, ptr_to_struct_containing_descr, 0);
  }
#endif /* !THREAD_LOCAL_ALLOC */

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_gcj_malloc_ignore_off_page(size_t lb,
                                        void * ptr_to_struct_containing_descr)
{
    return MANAGED_STACK_ADDRESS_BOEHM_GC_core_gcj_malloc(lb, ptr_to_struct_containing_descr,
                              IGNORE_OFF_PAGE);
}

/* Similar to MANAGED_STACK_ADDRESS_BOEHM_GC_gcj_malloc, but add debug info.  This is allocated     */
/* with MANAGED_STACK_ADDRESS_BOEHM_GC_gcj_debug_kind.                                              */
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_debug_gcj_malloc(size_t lb,
                void * ptr_to_struct_containing_descr, MANAGED_STACK_ADDRESS_BOEHM_GC_EXTRA_PARAMS)
{
    void * result;

    /* We're careful to avoid extra calls, which could          */
    /* confuse the backtrace.                                   */
    LOCK();
    maybe_finalize();
    result = MANAGED_STACK_ADDRESS_BOEHM_GC_generic_malloc_inner(SIZET_SAT_ADD(lb, DEBUG_BYTES),
                                     MANAGED_STACK_ADDRESS_BOEHM_GC_gcj_debug_kind, 0 /* flags */);
    if (NULL == result) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_oom_func oom_fn = MANAGED_STACK_ADDRESS_BOEHM_GC_oom_fn;
        UNLOCK();
        MANAGED_STACK_ADDRESS_BOEHM_GC_err_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_debug_gcj_malloc(%lu, %p) returning NULL (%s:%d)\n",
                (unsigned long)lb, ptr_to_struct_containing_descr, s, i);
        return (*oom_fn)(lb);
    }
    *((void **)((ptr_t)result + sizeof(oh))) = ptr_to_struct_containing_descr;
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_debugging_started) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_start_debugging_inner();
    }
    ADD_CALL_CHAIN(result, ra);
    result = MANAGED_STACK_ADDRESS_BOEHM_GC_store_debug_info_inner(result, (word)lb, s, i);
    UNLOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(result);
    REACHABLE_AFTER_DIRTY(ptr_to_struct_containing_descr);
    return result;
}

#endif  /* MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT */
