/*
 * Copyright (c) 2000-2005 by Hewlett-Packard Company.  All rights reserved.
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

#include "private/gc_priv.h"

#if defined(THREAD_LOCAL_ALLOC)

#if !defined(THREADS) && !defined(CPPCHECK)
# error Invalid config - THREAD_LOCAL_ALLOC requires MANAGED_STACK_ADDRESS_BOEHM_GC_THREADS
#endif

#include "private/thread_local_alloc.h"

#if defined(USE_COMPILER_TLS)
  __thread MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_TLS_FAST
#elif defined(USE_WIN32_COMPILER_TLS)
  __declspec(thread) MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_TLS_FAST
#endif
MANAGED_STACK_ADDRESS_BOEHM_GC_key_t MANAGED_STACK_ADDRESS_BOEHM_GC_thread_key;

static MANAGED_STACK_ADDRESS_BOEHM_GC_bool keys_initialized;

/* Return a single nonempty freelist fl to the global one pointed to    */
/* by gfl.                                                              */

static void return_single_freelist(void *fl, void **gfl)
{
    if (*gfl == 0) {
      *gfl = fl;
    } else {
      void *q, **qptr;

      MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_size(fl) == MANAGED_STACK_ADDRESS_BOEHM_GC_size(*gfl));
      /* Concatenate: */
        qptr = &(obj_link(fl));
        while ((word)(q = *qptr) >= HBLKSIZE)
          qptr = &(obj_link(q));
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(0 == q);
        *qptr = *gfl;
        *gfl = fl;
    }
}

/* Recover the contents of the freelist array fl into the global one gfl. */
static void return_freelists(void **fl, void **gfl)
{
    int i;

    for (i = 1; i < MANAGED_STACK_ADDRESS_BOEHM_GC_TINY_FREELISTS; ++i) {
        if ((word)(fl[i]) >= HBLKSIZE) {
          return_single_freelist(fl[i], &gfl[i]);
        }
        /* Clear fl[i], since the thread structure may hang around.     */
        /* Do it in a way that is likely to trap if we access it.       */
        fl[i] = (ptr_t)HBLKSIZE;
    }
    /* The 0 granule freelist really contains 1 granule objects.        */
    if ((word)fl[0] >= HBLKSIZE
#       ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT
          && fl[0] != ERROR_FL
#       endif
       ) {
        return_single_freelist(fl[0], &gfl[1]);
    }
}

#ifdef USE_PTHREAD_SPECIFIC
  /* Re-set the TLS value on thread cleanup to allow thread-local       */
  /* allocations to happen in the TLS destructors.                      */
  /* MANAGED_STACK_ADDRESS_BOEHM_GC_unregister_my_thread (and similar routines) will finally set    */
  /* the MANAGED_STACK_ADDRESS_BOEHM_GC_thread_key to NULL preventing this destructor from being    */
  /* called repeatedly.                                                 */
  static void reset_thread_key(void* v) {
    pthread_setspecific(MANAGED_STACK_ADDRESS_BOEHM_GC_thread_key, v);
  }
#else
# define reset_thread_key 0
#endif

/* Each thread structure must be initialized.   */
/* This call must be made from the new thread.  */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_init_thread_local(MANAGED_STACK_ADDRESS_BOEHM_GC_tlfs p)
{
    int i, j, res;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    if (!EXPECT(keys_initialized, TRUE)) {
#       ifdef USE_CUSTOM_SPECIFIC
          /* Ensure proper alignment of a "pushed" GC symbol.   */
          MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((word)(&MANAGED_STACK_ADDRESS_BOEHM_GC_thread_key) % sizeof(word) == 0);
#       endif
        res = MANAGED_STACK_ADDRESS_BOEHM_GC_key_create(&MANAGED_STACK_ADDRESS_BOEHM_GC_thread_key, reset_thread_key);
        if (COVERT_DATAFLOW(res) != 0) {
            ABORT("Failed to create key for local allocator");
        }
        keys_initialized = TRUE;
    }
    res = MANAGED_STACK_ADDRESS_BOEHM_GC_setspecific(MANAGED_STACK_ADDRESS_BOEHM_GC_thread_key, p);
    if (COVERT_DATAFLOW(res) != 0) {
        ABORT("Failed to set thread specific allocation pointers");
    }
    for (j = 0; j < MANAGED_STACK_ADDRESS_BOEHM_GC_TINY_FREELISTS; ++j) {
        for (i = 0; i < THREAD_FREELISTS_KINDS; ++i) {
            p -> _freelists[i][j] = (void *)(word)1;
        }
#       ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT
            p -> gcj_freelists[j] = (void *)(word)1;
#       endif
    }
    /* The size 0 free lists are handled like the regular free lists,   */
    /* to ensure that the explicit deallocation works.  However,        */
    /* allocation of a size 0 "gcj" object is always an error.          */
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT
        p -> gcj_freelists[0] = ERROR_FL;
#   endif
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_destroy_thread_local(MANAGED_STACK_ADDRESS_BOEHM_GC_tlfs p)
{
    int k;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_getspecific(MANAGED_STACK_ADDRESS_BOEHM_GC_thread_key) == p);
    /* We currently only do this from the thread itself.        */
    MANAGED_STACK_ADDRESS_BOEHM_GC_STATIC_ASSERT(THREAD_FREELISTS_KINDS <= MAXOBJKINDS);
    for (k = 0; k < THREAD_FREELISTS_KINDS; ++k) {
        if (k == (int)MANAGED_STACK_ADDRESS_BOEHM_GC_n_kinds)
            break; /* kind is not created */
        return_freelists(p -> _freelists[k], MANAGED_STACK_ADDRESS_BOEHM_GC_obj_kinds[k].ok_freelist);
    }
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT
        return_freelists(p -> gcj_freelists, (void **)MANAGED_STACK_ADDRESS_BOEHM_GC_gcjobjfreelist);
#   endif
}

STATIC void *MANAGED_STACK_ADDRESS_BOEHM_GC_get_tlfs(void)
{
# if !defined(USE_PTHREAD_SPECIFIC) && !defined(USE_WIN32_SPECIFIC)
    MANAGED_STACK_ADDRESS_BOEHM_GC_key_t k = MANAGED_STACK_ADDRESS_BOEHM_GC_thread_key;

    if (EXPECT(0 == k, FALSE)) {
      /* We have not yet run MANAGED_STACK_ADDRESS_BOEHM_GC_init_parallel.  That means we also  */
      /* are not locking, so MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind_global is fairly cheap. */
      return NULL;
    }
    return MANAGED_STACK_ADDRESS_BOEHM_GC_getspecific(k);
# else
    if (EXPECT(!keys_initialized, FALSE)) return NULL;

    return MANAGED_STACK_ADDRESS_BOEHM_GC_getspecific(MANAGED_STACK_ADDRESS_BOEHM_GC_thread_key);
# endif
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind(size_t bytes, int kind)
{
    size_t granules;
    void *tsd;
    void *result;

#   if MAXOBJKINDS > THREAD_FREELISTS_KINDS
      if (EXPECT(kind >= THREAD_FREELISTS_KINDS, FALSE)) {
        return MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind_global(bytes, kind);
      }
#   endif
    tsd = MANAGED_STACK_ADDRESS_BOEHM_GC_get_tlfs();
    if (EXPECT(NULL == tsd, FALSE)) {
        return MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind_global(bytes, kind);
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized);
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_thread_tsd_valid(tsd));
    granules = ALLOC_REQUEST_GRANS(bytes);
#   if defined(CPPCHECK)
#     define MALLOC_KIND_PTRFREE_INIT (void*)1
#   else
#     define MALLOC_KIND_PTRFREE_INIT NULL
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_FAST_MALLOC_GRANS(result, granules,
                         ((MANAGED_STACK_ADDRESS_BOEHM_GC_tlfs)tsd) -> _freelists[kind], DIRECT_GRANULES,
                         kind, MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind_global(bytes, kind),
                         (void)(kind == PTRFREE ? MALLOC_KIND_PTRFREE_INIT
                                               : (obj_link(result) = 0)));
#   ifdef LOG_ALLOCS
      MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_kind(%lu, %d) returned %p, recent GC #%lu\n",
                    (unsigned long)bytes, kind, result,
                    (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no);
#   endif
    return result;
}

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT

# include "gc/gc_gcj.h"

/* Gcj-style allocation without locks is extremely tricky.  The         */
/* fundamental issue is that we may end up marking a free list, which   */
/* has freelist links instead of "vtable" pointers.  That is usually    */
/* OK, since the next object on the free list will be cleared, and      */
/* will thus be interpreted as containing a zero descriptor.  That's    */
/* fine if the object has not yet been initialized.  But there are      */
/* interesting potential races.                                         */
/* In the case of incremental collection, this seems hopeless, since    */
/* the marker may run asynchronously, and may pick up the pointer to    */
/* the next freelist entry (which it thinks is a vtable pointer), get   */
/* suspended for a while, and then see an allocated object instead      */
/* of the vtable.  This may be avoidable with either a handshake with   */
/* the collector or, probably more easily, by moving the free list      */
/* links to the second word of each object.  The latter isn't a         */
/* universal win, since on architecture like Itanium, nonzero offsets   */
/* are not necessarily free.  And there may be cache fill order issues. */
/* For now, we punt with incremental GC.  This probably means that      */
/* incremental GC should be enabled before we fork a second thread.     */
/* Unlike the other thread local allocation calls, we assume that the   */
/* collector has been explicitly initialized.                           */
MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_MALLOC void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_gcj_malloc(size_t bytes,
                                    void * ptr_to_struct_containing_descr)
{
  if (EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_incremental, FALSE)) {
    return MANAGED_STACK_ADDRESS_BOEHM_GC_core_gcj_malloc(bytes, ptr_to_struct_containing_descr, 0);
  } else {
    size_t granules = ALLOC_REQUEST_GRANS(bytes);
    void *result;
    void **tiny_fl;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_gcjobjfreelist != NULL);
    tiny_fl = ((MANAGED_STACK_ADDRESS_BOEHM_GC_tlfs)MANAGED_STACK_ADDRESS_BOEHM_GC_getspecific(MANAGED_STACK_ADDRESS_BOEHM_GC_thread_key))->gcj_freelists;
    MANAGED_STACK_ADDRESS_BOEHM_GC_FAST_MALLOC_GRANS(result, granules, tiny_fl, DIRECT_GRANULES,
                         MANAGED_STACK_ADDRESS_BOEHM_GC_gcj_kind,
                         MANAGED_STACK_ADDRESS_BOEHM_GC_core_gcj_malloc(bytes,
                                            ptr_to_struct_containing_descr,
                                            0 /* flags */),
                         {AO_compiler_barrier();
                          *(void **)result = ptr_to_struct_containing_descr;});
        /* This forces the initialization of the "method ptr".          */
        /* This is necessary to ensure some very subtle properties      */
        /* required if a GC is run in the middle of such an allocation. */
        /* Here we implicitly also assume atomicity for the free list.  */
        /* and method pointer assignments.                              */
        /* We must update the freelist before we store the pointer.     */
        /* Otherwise a GC at this point would see a corrupted           */
        /* free list.                                                   */
        /* A real memory barrier is not needed, since the               */
        /* action of stopping this thread will cause prior writes       */
        /* to complete.                                                 */
        /* We assert that any concurrent marker will stop us.           */
        /* Thus it is impossible for a mark procedure to see the        */
        /* allocation of the next object, but to see this object        */
        /* still containing a free list pointer.  Otherwise the         */
        /* marker, by misinterpreting the freelist link as a vtable     */
        /* pointer, might find a random "mark descriptor" in the next   */
        /* object.                                                      */
    return result;
  }
}

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT */

/* The thread support layer must arrange to mark thread-local   */
/* free lists explicitly, since the link field is often         */
/* invisible to the marker.  It knows how to find all threads;  */
/* we take care of an individual thread freelist structure.     */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_mark_thread_local_fls_for(MANAGED_STACK_ADDRESS_BOEHM_GC_tlfs p)
{
    ptr_t q;
    int i, j;

    for (j = 0; j < MANAGED_STACK_ADDRESS_BOEHM_GC_TINY_FREELISTS; ++j) {
      for (i = 0; i < THREAD_FREELISTS_KINDS; ++i) {
        /* Load the pointer atomically as it might be updated   */
        /* concurrently by MANAGED_STACK_ADDRESS_BOEHM_GC_FAST_MALLOC_GRANS.                */
        q = (ptr_t)AO_load((volatile AO_t *)&p->_freelists[i][j]);
        if ((word)q > HBLKSIZE)
          MANAGED_STACK_ADDRESS_BOEHM_GC_set_fl_marks(q);
      }
#     ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT
        if (EXPECT(j > 0, TRUE)) {
          q = (ptr_t)AO_load((volatile AO_t *)&p->gcj_freelists[j]);
          if ((word)q > HBLKSIZE)
            MANAGED_STACK_ADDRESS_BOEHM_GC_set_fl_marks(q);
        }
#     endif
    }
}

#if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS)
    /* Check that all thread-local free-lists in p are completely marked. */
    void MANAGED_STACK_ADDRESS_BOEHM_GC_check_tls_for(MANAGED_STACK_ADDRESS_BOEHM_GC_tlfs p)
    {
        int i, j;

        for (j = 1; j < MANAGED_STACK_ADDRESS_BOEHM_GC_TINY_FREELISTS; ++j) {
          for (i = 0; i < THREAD_FREELISTS_KINDS; ++i) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_check_fl_marks(&p->_freelists[i][j]);
          }
#         ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_GCJ_SUPPORT
            MANAGED_STACK_ADDRESS_BOEHM_GC_check_fl_marks(&p->gcj_freelists[j]);
#         endif
        }
    }
#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS */

#endif /* THREAD_LOCAL_ALLOC */
