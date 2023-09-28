/*
 * Copyright (c) 2011 by Hewlett-Packard Company.  All rights reserved.
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

#ifdef ENABLE_DISCLAIM

#include "gc/gc_disclaim.h"
#include "private/dbg_mlc.h" /* for oh type */

#if defined(KEEP_BACK_PTRS) || defined(MAKE_BACK_GRAPH)
  /* The first bit is already used for a debug purpose. */
# define FINALIZER_CLOSURE_FLAG 0x2
#else
# define FINALIZER_CLOSURE_FLAG 0x1
#endif

STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK MANAGED_STACK_ADDRESS_BOEHM_GC_finalized_disclaim(void *obj)
{
#   ifdef AO_HAVE_load
        word fc_word = (word)AO_load((volatile AO_t *)obj);
#   else
        word fc_word = *(word *)obj;
#   endif

    if ((fc_word & FINALIZER_CLOSURE_FLAG) != 0) {
       /* The disclaim function may be passed fragments from the        */
       /* free-list, on which it should not run finalization.           */
       /* To recognize this case, we use the fact that the first word   */
       /* on such fragments is always multiple of 4 (a link to the next */
       /* fragment, or NULL).  If it is desirable to have a finalizer   */
       /* which does not use the first word for storing finalization    */
       /* info, MANAGED_STACK_ADDRESS_BOEHM_GC_disclaim_and_reclaim() must be extended to clear     */
       /* fragments so that the assumption holds for the selected word. */
        const struct MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_closure *fc
                        = (struct MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_closure *)(fc_word
                                        & ~(word)FINALIZER_CLOSURE_FLAG);
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(!MANAGED_STACK_ADDRESS_BOEHM_GC_find_leak);
        (*fc->proc)((word *)obj + 1, fc->cd);
    }
    return 0;
}

STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_register_disclaim_proc_inner(unsigned kind,
                                            MANAGED_STACK_ADDRESS_BOEHM_GC_disclaim_proc proc,
                                            MANAGED_STACK_ADDRESS_BOEHM_GC_bool mark_unconditionally)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(kind < MAXOBJKINDS);
    if (EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_find_leak, FALSE)) return;

    MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[kind].ok_disclaim_proc = proc;
    MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[kind].ok_mark_unconditionally = mark_unconditionally;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_init_finalized_malloc(void)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_init();  /* In case it's not already done.       */
    LOCK();
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_finalized_kind != 0) {
        UNLOCK();
        return;
    }

    /* The finalizer closure is placed in the first word in order to    */
    /* use the lower bits to distinguish live objects from objects on   */
    /* the free list.  The downside of this is that we need one-word    */
    /* offset interior pointers, and that MANAGED_STACK_ADDRESS_BOEHM_GC_base does not return the   */
    /* start of the user region.                                        */
    MANAGED_STACK_ADDRESS_BOEHM_GC_register_displacement_inner(sizeof(word));

    /* And, the pointer to the finalizer closure object itself is       */
    /* displaced due to baking in this indicator.                       */
    MANAGED_STACK_ADDRESS_BOEHM_GC_register_displacement_inner(FINALIZER_CLOSURE_FLAG);
    MANAGED_STACK_ADDRESS_BOEHM_GC_register_displacement_inner(sizeof(oh) + FINALIZER_CLOSURE_FLAG);

    MANAGED_STACK_ADDRESS_BOEHM_GC_finalized_kind = MANAGED_STACK_ADDRESS_BOEHM_GC_new_kind_inner(MANAGED_STACK_ADDRESS_BOEHM_GC_new_free_list_inner(),
                                          MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH, TRUE, TRUE);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_finalized_kind != 0);
    MANAGED_STACK_ADDRESS_BOEHM_GC_register_disclaim_proc_inner(MANAGED_STACK_ADDRESS_BOEHM_GC_finalized_kind, MANAGED_STACK_ADDRESS_BOEHM_GC_finalized_disclaim,
                                    TRUE);
    UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_register_disclaim_proc(int kind, MANAGED_STACK_ADDRESS_BOEHM_GC_disclaim_proc proc,
                                              int mark_unconditionally)
{
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_register_disclaim_proc_inner((unsigned)kind, proc,
                                    (MANAGED_STACK_ADDRESS_BOEHM_GC_bool)mark_unconditionally);
    UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_finalized_malloc(size_t lb,
                                const struct MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_closure *fclos)
{
    void *op;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_finalized_kind != 0);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NONNULL_ARG_NOT_NULL(fclos));
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(((word)fclos & FINALIZER_CLOSURE_FLAG) == 0);
    op = MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind(SIZET_SAT_ADD(lb, sizeof(word)),
                        (int)MANAGED_STACK_ADDRESS_BOEHM_GC_finalized_kind);
    if (EXPECT(NULL == op, FALSE))
        return NULL;
#   ifdef AO_HAVE_store
        AO_store((volatile AO_t *)op, (AO_t)fclos | FINALIZER_CLOSURE_FLAG);
#   else
        *(word *)op = (word)fclos | FINALIZER_CLOSURE_FLAG;
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(op);
    REACHABLE_AFTER_DIRTY(fclos);
    return (word *)op + 1;
}

#endif /* ENABLE_DISCLAIM */
