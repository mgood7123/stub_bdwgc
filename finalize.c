/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1996 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996-1999 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 2007 Free Software Foundation, Inc.
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

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION
# include "gc/javaxfc.h" /* to get MANAGED_STACK_ADDRESS_BOEHM_GC_finalize_all() as extern "C" */

/* Type of mark procedure used for marking from finalizable object.     */
/* This procedure normally does not mark the object, only its           */
/* descendants.                                                         */
typedef void (* finalization_mark_proc)(ptr_t /* finalizable_obj_ptr */);

#define HASH3(addr,size,log_size) \
        ((((word)(addr) >> 3) ^ ((word)(addr) >> (3 + (log_size)))) \
         & ((size) - 1))
#define HASH2(addr,log_size) HASH3(addr, (word)1 << (log_size), log_size)

struct hash_chain_entry {
    word hidden_key;
    struct hash_chain_entry * next;
};

struct disappearing_link {
    struct hash_chain_entry prolog;
#   define dl_hidden_link prolog.hidden_key
                                /* Field to be cleared.         */
#   define dl_next(x) (struct disappearing_link *)((x) -> prolog.next)
#   define dl_set_next(x, y) \
                (void)((x)->prolog.next = (struct hash_chain_entry *)(y))
    word dl_hidden_obj;         /* Pointer to object base       */
};

struct finalizable_object {
    struct hash_chain_entry prolog;
#   define fo_hidden_base prolog.hidden_key
                                /* Pointer to object base.      */
                                /* No longer hidden once object */
                                /* is on finalize_now queue.    */
#   define fo_next(x) (struct finalizable_object *)((x) -> prolog.next)
#   define fo_set_next(x,y) ((x)->prolog.next = (struct hash_chain_entry *)(y))
    MANAGED_STACK_ADDRESS_BOEHM_GC_finalization_proc fo_fn; /* Finalizer.                   */
    ptr_t fo_client_data;
    word fo_object_size;        /* In bytes.                    */
    finalization_mark_proc fo_mark_proc;        /* Mark-through procedure */
};

#ifdef AO_HAVE_store
  /* Update finalize_now atomically as MANAGED_STACK_ADDRESS_BOEHM_GC_should_invoke_finalizers does */
  /* not acquire the allocation lock.                                   */
# define SET_FINALIZE_NOW(fo) \
            AO_store((volatile AO_t *)&MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.finalize_now, (AO_t)(fo))
#else
# define SET_FINALIZE_NOW(fo) (void)(MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.finalize_now = (fo))
#endif /* !THREADS */

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_push_finalizer_structures(void)
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((word)(&MANAGED_STACK_ADDRESS_BOEHM_GC_dl_hashtbl.head) % sizeof(word) == 0);
  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((word)(&MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots) % sizeof(word) == 0);
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_LONG_REFS_NOT_NEEDED
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((word)(&MANAGED_STACK_ADDRESS_BOEHM_GC_ll_hashtbl.head) % sizeof(word) == 0);
    MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ALL_SYM(MANAGED_STACK_ADDRESS_BOEHM_GC_ll_hashtbl.head);
# endif
  MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ALL_SYM(MANAGED_STACK_ADDRESS_BOEHM_GC_dl_hashtbl.head);
  MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ALL_SYM(MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots);
  /* MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_arr is pushed specially by MANAGED_STACK_ADDRESS_BOEHM_GC_mark_togglerefs.        */
}

/* Threshold of log_size to initiate full collection before growing     */
/* a hash table.                                                        */
#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_ON_GROW_LOG_SIZE_MIN
# define MANAGED_STACK_ADDRESS_BOEHM_GC_ON_GROW_LOG_SIZE_MIN CPP_LOG_HBLKSIZE
#endif

/* Double the size of a hash table. *log_size_ptr is the log of its     */
/* current size.  May be a no-op.  *table is a pointer to an array of   */
/* hash headers.  We update both *table and *log_size_ptr on success.   */
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_grow_table(struct hash_chain_entry ***table,
                          unsigned *log_size_ptr, const word *entries_ptr)
{
    word i;
    struct hash_chain_entry *p;
    unsigned log_old_size = *log_size_ptr;
    unsigned log_new_size = log_old_size + 1;
    word old_size = *table == NULL ? 0 : (word)1 << log_old_size;
    word new_size = (word)1 << log_new_size;
    /* FIXME: Power of 2 size often gets rounded up to one more page. */
    struct hash_chain_entry **new_table;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    /* Avoid growing the table in case of at least 25% of entries can   */
    /* be deleted by enforcing a collection.  Ignored for small tables. */
    /* In incremental mode we skip this optimization, as we want to     */
    /* avoid triggering a full GC whenever possible.                    */
    if (log_old_size >= MANAGED_STACK_ADDRESS_BOEHM_GC_ON_GROW_LOG_SIZE_MIN && !MANAGED_STACK_ADDRESS_BOEHM_GC_incremental) {
      IF_CANCEL(int cancel_state;)

      DISABLE_CANCEL(cancel_state);
      MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect_inner();
      RESTORE_CANCEL(cancel_state);
      /* MANAGED_STACK_ADDRESS_BOEHM_GC_finalize might decrease entries value.  */
      if (*entries_ptr < ((word)1 << log_old_size) - (*entries_ptr >> 2))
        return;
    }

    new_table = (struct hash_chain_entry **)
                    MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_MALLOC_IGNORE_OFF_PAGE(
                        (size_t)new_size * sizeof(struct hash_chain_entry *),
                        NORMAL);
    if (new_table == 0) {
        if (*table == 0) {
            ABORT("Insufficient space for initial table allocation");
        } else {
            return;
        }
    }
    for (i = 0; i < old_size; i++) {
      p = (*table)[i];
      while (p != 0) {
        ptr_t real_key = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_REVEAL_POINTER(p -> hidden_key);
        struct hash_chain_entry *next = p -> next;
        size_t new_hash = HASH3(real_key, new_size, log_new_size);

        p -> next = new_table[new_hash];
        MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(p);
        new_table[new_hash] = p;
        p = next;
      }
    }
    *log_size_ptr = log_new_size;
    *table = new_table;
    MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(new_table); /* entire object */
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_register_disappearing_link(void * * link)
{
    ptr_t base;

    base = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_base(link);
    if (base == 0)
        ABORT("Bad arg to MANAGED_STACK_ADDRESS_BOEHM_GC_register_disappearing_link");
    return MANAGED_STACK_ADDRESS_BOEHM_GC_general_register_disappearing_link(link, base);
}

STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_register_disappearing_link_inner(
                        struct dl_hashtbl_s *dl_hashtbl, void **link,
                        const void *obj, const char *tbl_log_name)
{
    struct disappearing_link *curr_dl;
    size_t index;
    struct disappearing_link * new_dl;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized);
    if (EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_find_leak, FALSE)) return MANAGED_STACK_ADDRESS_BOEHM_GC_UNIMPLEMENTED;
#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS
      MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((word)(*link)); /* check accessibility */
#   endif
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(obj != NULL && MANAGED_STACK_ADDRESS_BOEHM_GC_base_C(obj) == obj);
    if (EXPECT(NULL == dl_hashtbl -> head, FALSE)
        || EXPECT(dl_hashtbl -> entries
                  > ((word)1 << dl_hashtbl -> log_size), FALSE)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_grow_table((struct hash_chain_entry ***)&dl_hashtbl -> head,
                      &dl_hashtbl -> log_size, &dl_hashtbl -> entries);
        MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF("Grew %s table to %u entries\n", tbl_log_name,
                           1U << dl_hashtbl -> log_size);
    }
    index = HASH2(link, dl_hashtbl -> log_size);
    for (curr_dl = dl_hashtbl -> head[index]; curr_dl != 0;
         curr_dl = dl_next(curr_dl)) {
        if (curr_dl -> dl_hidden_link == MANAGED_STACK_ADDRESS_BOEHM_GC_HIDE_POINTER(link)) {
            /* Alternatively, MANAGED_STACK_ADDRESS_BOEHM_GC_HIDE_NZ_POINTER() could be used instead. */
            curr_dl -> dl_hidden_obj = MANAGED_STACK_ADDRESS_BOEHM_GC_HIDE_POINTER(obj);
            UNLOCK();
            return MANAGED_STACK_ADDRESS_BOEHM_GC_DUPLICATE;
        }
    }
    new_dl = (struct disappearing_link *)
        MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_MALLOC(sizeof(struct disappearing_link), NORMAL);
    if (EXPECT(NULL == new_dl, FALSE)) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_oom_func oom_fn = MANAGED_STACK_ADDRESS_BOEHM_GC_oom_fn;
      UNLOCK();
      new_dl = (struct disappearing_link *)
                (*oom_fn)(sizeof(struct disappearing_link));
      if (0 == new_dl) {
        return MANAGED_STACK_ADDRESS_BOEHM_GC_NO_MEMORY;
      }
      /* It's not likely we'll make it here, but ... */
      LOCK();
      /* Recalculate index since the table may grow.    */
      index = HASH2(link, dl_hashtbl -> log_size);
      /* Check again that our disappearing link not in the table. */
      for (curr_dl = dl_hashtbl -> head[index]; curr_dl != 0;
           curr_dl = dl_next(curr_dl)) {
        if (curr_dl -> dl_hidden_link == MANAGED_STACK_ADDRESS_BOEHM_GC_HIDE_POINTER(link)) {
          curr_dl -> dl_hidden_obj = MANAGED_STACK_ADDRESS_BOEHM_GC_HIDE_POINTER(obj);
          UNLOCK();
#         ifndef DBG_HDRS_ALL
            /* Free unused new_dl returned by MANAGED_STACK_ADDRESS_BOEHM_GC_oom_fn() */
            MANAGED_STACK_ADDRESS_BOEHM_GC_free((void *)new_dl);
#         endif
          return MANAGED_STACK_ADDRESS_BOEHM_GC_DUPLICATE;
        }
      }
    }
    new_dl -> dl_hidden_obj = MANAGED_STACK_ADDRESS_BOEHM_GC_HIDE_POINTER(obj);
    new_dl -> dl_hidden_link = MANAGED_STACK_ADDRESS_BOEHM_GC_HIDE_POINTER(link);
    dl_set_next(new_dl, dl_hashtbl -> head[index]);
    MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(new_dl);
    dl_hashtbl -> head[index] = new_dl;
    dl_hashtbl -> entries++;
    MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(dl_hashtbl->head + index);
    UNLOCK();
    return MANAGED_STACK_ADDRESS_BOEHM_GC_SUCCESS;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_general_register_disappearing_link(void * * link,
                                                         const void * obj)
{
    if (((word)link & (ALIGNMENT-1)) != 0 || !NONNULL_ARG_NOT_NULL(link))
        ABORT("Bad arg to MANAGED_STACK_ADDRESS_BOEHM_GC_general_register_disappearing_link");
    return MANAGED_STACK_ADDRESS_BOEHM_GC_register_disappearing_link_inner(&MANAGED_STACK_ADDRESS_BOEHM_GC_dl_hashtbl, link, obj,
                                               "dl");
}

#ifdef DBG_HDRS_ALL
# define FREE_DL_ENTRY(curr_dl) dl_set_next(curr_dl, NULL)
#else
# define FREE_DL_ENTRY(curr_dl) MANAGED_STACK_ADDRESS_BOEHM_GC_free(curr_dl)
#endif

/* Unregisters given link and returns the link entry to free.   */
MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE struct disappearing_link *MANAGED_STACK_ADDRESS_BOEHM_GC_unregister_disappearing_link_inner(
                                struct dl_hashtbl_s *dl_hashtbl, void **link)
{
    struct disappearing_link *curr_dl;
    struct disappearing_link *prev_dl = NULL;
    size_t index;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    if (EXPECT(NULL == dl_hashtbl -> head, FALSE)) return NULL;

    index = HASH2(link, dl_hashtbl -> log_size);
    for (curr_dl = dl_hashtbl -> head[index]; curr_dl;
         curr_dl = dl_next(curr_dl)) {
        if (curr_dl -> dl_hidden_link == MANAGED_STACK_ADDRESS_BOEHM_GC_HIDE_POINTER(link)) {
            /* Remove found entry from the table. */
            if (NULL == prev_dl) {
                dl_hashtbl -> head[index] = dl_next(curr_dl);
                MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(dl_hashtbl->head + index);
            } else {
                dl_set_next(prev_dl, dl_next(curr_dl));
                MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(prev_dl);
            }
            dl_hashtbl -> entries--;
            break;
        }
        prev_dl = curr_dl;
    }
    return curr_dl;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_unregister_disappearing_link(void * * link)
{
    struct disappearing_link *curr_dl;

    if (((word)link & (ALIGNMENT-1)) != 0) return 0; /* Nothing to do. */

    LOCK();
    curr_dl = MANAGED_STACK_ADDRESS_BOEHM_GC_unregister_disappearing_link_inner(&MANAGED_STACK_ADDRESS_BOEHM_GC_dl_hashtbl, link);
    UNLOCK();
    if (NULL == curr_dl) return 0;
    FREE_DL_ENTRY(curr_dl);
    return 1;
}

/* Mark from one finalizable object using the specified mark proc.      */
/* May not mark the object pointed to by real_ptr (i.e, it is the job   */
/* of the caller, if appropriate).  Note that this is called with the   */
/* mutator running.  This is safe only if the mutator (client) gets     */
/* the allocation lock to reveal hidden pointers.                       */
MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE void MANAGED_STACK_ADDRESS_BOEHM_GC_mark_fo(ptr_t real_ptr, finalization_mark_proc fo_mark_proc)
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
  fo_mark_proc(real_ptr);
  /* Process objects pushed by the mark procedure.      */
  while (!MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_empty())
    MARK_FROM_MARK_STACK();
}

/* Complete a collection in progress, if any.   */
MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE void MANAGED_STACK_ADDRESS_BOEHM_GC_complete_ongoing_collection(void) {
  if (EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_collection_in_progress(), FALSE)) {
    while (!MANAGED_STACK_ADDRESS_BOEHM_GC_mark_some(NULL)) { /* empty */ }
  }
}

/* Toggle-ref support.  */
#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_TOGGLE_REFS_NOT_NEEDED
  typedef union toggle_ref_u GCToggleRef;

  STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_func MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_callback = 0;

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_process_togglerefs(void)
  {
    size_t i;
    size_t new_size = 0;
    MANAGED_STACK_ADDRESS_BOEHM_GC_bool needs_barrier = FALSE;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    for (i = 0; i < MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_size; ++i) {
      GCToggleRef *r = &MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_arr[i];
      void *obj = r -> strong_ref;

      if (((word)obj & 1) != 0) {
        obj = MANAGED_STACK_ADDRESS_BOEHM_GC_REVEAL_POINTER(r -> weak_ref);
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(((word)obj & 1) == 0);
      }
      if (NULL == obj) continue;

      switch (MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_callback(obj)) {
      case MANAGED_STACK_ADDRESS_BOEHM_GC_TOGGLE_REF_DROP:
        break;
      case MANAGED_STACK_ADDRESS_BOEHM_GC_TOGGLE_REF_STRONG:
        MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_arr[new_size++].strong_ref = obj;
        needs_barrier = TRUE;
        break;
      case MANAGED_STACK_ADDRESS_BOEHM_GC_TOGGLE_REF_WEAK:
        MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_arr[new_size++].weak_ref = MANAGED_STACK_ADDRESS_BOEHM_GC_HIDE_POINTER(obj);
        break;
      default:
        ABORT("Bad toggle-ref status returned by callback");
      }
    }

    if (new_size < MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_size) {
      BZERO(&MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_arr[new_size],
            (MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_size - new_size) * sizeof(GCToggleRef));
      MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_size = new_size;
    }
    if (needs_barrier)
      MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_arr); /* entire object */
  }

  STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_normal_finalize_mark_proc(ptr_t);

  STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_mark_togglerefs(void)
  {
    size_t i;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    if (NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_arr)
      return;

    MANAGED_STACK_ADDRESS_BOEHM_GC_set_mark_bit(MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_arr);
    for (i = 0; i < MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_size; ++i) {
      void *obj = MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_arr[i].strong_ref;
      if (obj != NULL && ((word)obj & 1) == 0) {
        /* Push and mark the object.    */
        MANAGED_STACK_ADDRESS_BOEHM_GC_mark_fo((ptr_t)obj, MANAGED_STACK_ADDRESS_BOEHM_GC_normal_finalize_mark_proc);
        MANAGED_STACK_ADDRESS_BOEHM_GC_set_mark_bit(obj);
        MANAGED_STACK_ADDRESS_BOEHM_GC_complete_ongoing_collection();
      }
    }
  }

  STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_clear_togglerefs(void)
  {
    size_t i;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    for (i = 0; i < MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_size; ++i) {
      GCToggleRef *r = &MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_arr[i];

      if (((word)(r -> strong_ref) & 1) != 0) {
        if (!MANAGED_STACK_ADDRESS_BOEHM_GC_is_marked(MANAGED_STACK_ADDRESS_BOEHM_GC_REVEAL_POINTER(r -> weak_ref))) {
          r -> weak_ref = 0;
        } else {
          /* No need to copy, BDWGC is a non-moving collector.    */
        }
      }
    }
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_toggleref_func(MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_func fn)
  {
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_callback = fn;
    UNLOCK();
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_func MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_toggleref_func(void)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_func fn;

    LOCK();
    fn = MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_callback;
    UNLOCK();
    return fn;
  }

  static MANAGED_STACK_ADDRESS_BOEHM_GC_bool ensure_toggleref_capacity(size_t capacity_inc)
  {
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    if (NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_arr) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_capacity = 32; /* initial capacity */
      MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_arr = (GCToggleRef *)MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_MALLOC_IGNORE_OFF_PAGE(
                        MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_capacity * sizeof(GCToggleRef),
                        NORMAL);
      if (NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_arr)
        return FALSE;
    }
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_size + capacity_inc
        >= MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_capacity) {
      GCToggleRef *new_array;
      while (MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_capacity
              < MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_size + capacity_inc) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_capacity *= 2;
        if ((MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_capacity
             & ((size_t)1 << (sizeof(size_t) * 8 - 1))) != 0)
          return FALSE; /* overflow */
      }

      new_array = (GCToggleRef *)MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_MALLOC_IGNORE_OFF_PAGE(
                        MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_capacity * sizeof(GCToggleRef),
                        NORMAL);
      if (NULL == new_array)
        return FALSE;
      if (EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_size > 0, TRUE))
        BCOPY(MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_arr, new_array,
              MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_size * sizeof(GCToggleRef));
      MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_FREE(MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_arr);
      MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_arr = new_array;
    }
    return TRUE;
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_add(void *obj, int is_strong_ref)
  {
    int res = MANAGED_STACK_ADDRESS_BOEHM_GC_SUCCESS;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NONNULL_ARG_NOT_NULL(obj));
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(((word)obj & 1) == 0 && obj == MANAGED_STACK_ADDRESS_BOEHM_GC_base(obj));
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_callback != 0) {
      if (!ensure_toggleref_capacity(1)) {
        res = MANAGED_STACK_ADDRESS_BOEHM_GC_NO_MEMORY;
      } else {
        GCToggleRef *r = &MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_arr[MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_size];

        if (is_strong_ref) {
          r -> strong_ref = obj;
          MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_arr + MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_size);
        } else {
          r -> weak_ref = MANAGED_STACK_ADDRESS_BOEHM_GC_HIDE_POINTER(obj);
          MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT((r -> weak_ref & 1) != 0);
        }
        MANAGED_STACK_ADDRESS_BOEHM_GC_toggleref_array_size++;
      }
    }
    UNLOCK();
    return res;
  }
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_TOGGLE_REFS_NOT_NEEDED */

/* Finalizer callback support. */
STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_await_finalize_proc MANAGED_STACK_ADDRESS_BOEHM_GC_object_finalized_proc = 0;

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_await_finalize_proc(MANAGED_STACK_ADDRESS_BOEHM_GC_await_finalize_proc fn)
{
  LOCK();
  MANAGED_STACK_ADDRESS_BOEHM_GC_object_finalized_proc = fn;
  UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_await_finalize_proc MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_await_finalize_proc(void)
{
  MANAGED_STACK_ADDRESS_BOEHM_GC_await_finalize_proc fn;

  LOCK();
  fn = MANAGED_STACK_ADDRESS_BOEHM_GC_object_finalized_proc;
  UNLOCK();
  return fn;
}

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_LONG_REFS_NOT_NEEDED
  MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_register_long_link(void * * link, const void * obj)
  {
    if (((word)link & (ALIGNMENT-1)) != 0 || !NONNULL_ARG_NOT_NULL(link))
        ABORT("Bad arg to MANAGED_STACK_ADDRESS_BOEHM_GC_register_long_link");
    return MANAGED_STACK_ADDRESS_BOEHM_GC_register_disappearing_link_inner(&MANAGED_STACK_ADDRESS_BOEHM_GC_ll_hashtbl, link, obj,
                                               "long dl");
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_unregister_long_link(void * * link)
  {
    struct disappearing_link *curr_dl;

    if (((word)link & (ALIGNMENT-1)) != 0) return 0; /* Nothing to do. */

    LOCK();
    curr_dl = MANAGED_STACK_ADDRESS_BOEHM_GC_unregister_disappearing_link_inner(&MANAGED_STACK_ADDRESS_BOEHM_GC_ll_hashtbl, link);
    UNLOCK();
    if (NULL == curr_dl) return 0;
    FREE_DL_ENTRY(curr_dl);
    return 1;
  }
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_LONG_REFS_NOT_NEEDED */

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_MOVE_DISAPPEARING_LINK_NOT_NEEDED
  STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_move_disappearing_link_inner(
                                struct dl_hashtbl_s *dl_hashtbl,
                                void **link, void **new_link)
  {
    struct disappearing_link *curr_dl, *new_dl;
    struct disappearing_link *prev_dl = NULL;
    size_t curr_index, new_index;
    word curr_hidden_link, new_hidden_link;

#   ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS
      MANAGED_STACK_ADDRESS_BOEHM_GC_noop1((word)(*new_link));
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    if (EXPECT(NULL == dl_hashtbl -> head, FALSE)) return MANAGED_STACK_ADDRESS_BOEHM_GC_NOT_FOUND;

    /* Find current link.       */
    curr_index = HASH2(link, dl_hashtbl -> log_size);
    curr_hidden_link = MANAGED_STACK_ADDRESS_BOEHM_GC_HIDE_POINTER(link);
    for (curr_dl = dl_hashtbl -> head[curr_index]; curr_dl;
         curr_dl = dl_next(curr_dl)) {
      if (curr_dl -> dl_hidden_link == curr_hidden_link)
        break;
      prev_dl = curr_dl;
    }
    if (EXPECT(NULL == curr_dl, FALSE)) {
      return MANAGED_STACK_ADDRESS_BOEHM_GC_NOT_FOUND;
    } else if (link == new_link) {
      return MANAGED_STACK_ADDRESS_BOEHM_GC_SUCCESS; /* Nothing to do.      */
    }

    /* link found; now check new_link not present.      */
    new_index = HASH2(new_link, dl_hashtbl -> log_size);
    new_hidden_link = MANAGED_STACK_ADDRESS_BOEHM_GC_HIDE_POINTER(new_link);
    for (new_dl = dl_hashtbl -> head[new_index]; new_dl;
         new_dl = dl_next(new_dl)) {
      if (new_dl -> dl_hidden_link == new_hidden_link) {
        /* Target already registered; bail.     */
        return MANAGED_STACK_ADDRESS_BOEHM_GC_DUPLICATE;
      }
    }

    /* Remove from old, add to new, update link.        */
    if (NULL == prev_dl) {
      dl_hashtbl -> head[curr_index] = dl_next(curr_dl);
    } else {
      dl_set_next(prev_dl, dl_next(curr_dl));
      MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(prev_dl);
    }
    curr_dl -> dl_hidden_link = new_hidden_link;
    dl_set_next(curr_dl, dl_hashtbl -> head[new_index]);
    dl_hashtbl -> head[new_index] = curr_dl;
    MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(curr_dl);
    MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(dl_hashtbl->head); /* entire object */
    return MANAGED_STACK_ADDRESS_BOEHM_GC_SUCCESS;
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_move_disappearing_link(void **link, void **new_link)
  {
    int result;

    if (((word)new_link & (ALIGNMENT-1)) != 0
        || !NONNULL_ARG_NOT_NULL(new_link))
      ABORT("Bad new_link arg to MANAGED_STACK_ADDRESS_BOEHM_GC_move_disappearing_link");
    if (((word)link & (ALIGNMENT-1)) != 0)
      return MANAGED_STACK_ADDRESS_BOEHM_GC_NOT_FOUND; /* Nothing to do. */

    LOCK();
    result = MANAGED_STACK_ADDRESS_BOEHM_GC_move_disappearing_link_inner(&MANAGED_STACK_ADDRESS_BOEHM_GC_dl_hashtbl, link, new_link);
    UNLOCK();
    return result;
  }

# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_LONG_REFS_NOT_NEEDED
    MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_move_long_link(void **link, void **new_link)
    {
      int result;

      if (((word)new_link & (ALIGNMENT-1)) != 0
          || !NONNULL_ARG_NOT_NULL(new_link))
        ABORT("Bad new_link arg to MANAGED_STACK_ADDRESS_BOEHM_GC_move_long_link");
      if (((word)link & (ALIGNMENT-1)) != 0)
        return MANAGED_STACK_ADDRESS_BOEHM_GC_NOT_FOUND; /* Nothing to do. */

      LOCK();
      result = MANAGED_STACK_ADDRESS_BOEHM_GC_move_disappearing_link_inner(&MANAGED_STACK_ADDRESS_BOEHM_GC_ll_hashtbl, link, new_link);
      UNLOCK();
      return result;
    }
# endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_LONG_REFS_NOT_NEEDED */
#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_MOVE_DISAPPEARING_LINK_NOT_NEEDED */

/* Possible finalization_marker procedures.  Note that mark stack       */
/* overflow is handled by the caller, and is not a disaster.            */
#if defined(_MSC_VER) && defined(I386)
  MANAGED_STACK_ADDRESS_BOEHM_GC_ATTR_NOINLINE
  /* Otherwise some optimizer bug is tickled in VC for x86 (v19, at least). */
#endif
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_normal_finalize_mark_proc(ptr_t p)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top = MANAGED_STACK_ADDRESS_BOEHM_GC_push_obj(p, HDR(p), MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top,
                                    MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack + MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_size);
}

/* This only pays very partial attention to the mark descriptor.        */
/* It does the right thing for normal and atomic objects, and treats    */
/* most others as normal.                                               */
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_ignore_self_finalize_mark_proc(ptr_t p)
{
    hdr * hhdr = HDR(p);
    word descr = hhdr -> hb_descr;
    ptr_t current_p;
    ptr_t scan_limit;
    ptr_t target_limit = p + hhdr -> hb_sz - 1;

    if ((descr & MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS) == MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH) {
       scan_limit = p + descr - sizeof(word);
    } else {
       scan_limit = target_limit + 1 - sizeof(word);
    }
    for (current_p = p; (word)current_p <= (word)scan_limit;
         current_p += ALIGNMENT) {
        word q;

        LOAD_WORD_OR_CONTINUE(q, current_p);
        if (q < (word)p || q > (word)target_limit) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_PUSH_ONE_HEAP(q, current_p, MANAGED_STACK_ADDRESS_BOEHM_GC_mark_stack_top);
        }
    }
}

STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_null_finalize_mark_proc(ptr_t p)
{
    UNUSED_ARG(p);
}

/* Possible finalization_marker procedures.  Note that mark stack       */
/* overflow is handled by the caller, and is not a disaster.            */

/* MANAGED_STACK_ADDRESS_BOEHM_GC_unreachable_finalize_mark_proc is an alias for normal marking,    */
/* but it is explicitly tested for, and triggers different              */
/* behavior.  Objects registered in this way are not finalized          */
/* if they are reachable by other finalizable objects, even if those    */
/* other objects specify no ordering.                                   */
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_unreachable_finalize_mark_proc(ptr_t p)
{
    /* A dummy comparison to ensure the compiler not to optimize two    */
    /* identical functions into a single one (thus, to ensure a unique  */
    /* address of each).  Alternatively, MANAGED_STACK_ADDRESS_BOEHM_GC_noop1(p) could be used.     */
    if (EXPECT(NULL == p, FALSE)) return;

    MANAGED_STACK_ADDRESS_BOEHM_GC_normal_finalize_mark_proc(p);
}

static MANAGED_STACK_ADDRESS_BOEHM_GC_bool need_unreachable_finalization = FALSE;
        /* Avoid the work if this is not used.  */
        /* TODO: turn need_unreachable_finalization into a counter */

/* Register a finalization function.  See gc.h for details.     */
/* The last parameter is a procedure that determines            */
/* marking for finalization ordering.  Any objects marked       */
/* by that procedure will be guaranteed to not have been        */
/* finalized when this finalizer is invoked.                    */
STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_register_finalizer_inner(void * obj,
                                        MANAGED_STACK_ADDRESS_BOEHM_GC_finalization_proc fn, void *cd,
                                        MANAGED_STACK_ADDRESS_BOEHM_GC_finalization_proc *ofn, void **ocd,
                                        finalization_mark_proc mp)
{
    struct finalizable_object * curr_fo;
    size_t index;
    struct finalizable_object *new_fo = 0;
    hdr *hhdr = NULL; /* initialized to prevent warning. */

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized);
    if (EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_find_leak, FALSE)) {
      /* No-op.  *ocd and *ofn remain unchanged.    */
      return;
    }
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(obj != NULL && MANAGED_STACK_ADDRESS_BOEHM_GC_base_C(obj) == obj);
    if (mp == MANAGED_STACK_ADDRESS_BOEHM_GC_unreachable_finalize_mark_proc)
        need_unreachable_finalization = TRUE;
    if (EXPECT(NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head, FALSE)
        || EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_fo_entries > ((word)1 << MANAGED_STACK_ADDRESS_BOEHM_GC_log_fo_table_size), FALSE)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_grow_table((struct hash_chain_entry ***)&MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head,
                      &MANAGED_STACK_ADDRESS_BOEHM_GC_log_fo_table_size, &MANAGED_STACK_ADDRESS_BOEHM_GC_fo_entries);
        MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF("Grew fo table to %u entries\n",
                           1U << MANAGED_STACK_ADDRESS_BOEHM_GC_log_fo_table_size);
    }
    for (;;) {
      struct finalizable_object *prev_fo = NULL;
      MANAGED_STACK_ADDRESS_BOEHM_GC_oom_func oom_fn;

      index = HASH2(obj, MANAGED_STACK_ADDRESS_BOEHM_GC_log_fo_table_size);
      curr_fo = MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head[index];
      while (curr_fo != 0) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_size(curr_fo) >= sizeof(struct finalizable_object));
        if (curr_fo -> fo_hidden_base == MANAGED_STACK_ADDRESS_BOEHM_GC_HIDE_POINTER(obj)) {
          /* Interruption by a signal in the middle of this     */
          /* should be safe.  The client may see only *ocd      */
          /* updated, but we'll declare that to be his problem. */
          if (ocd) *ocd = (void *)(curr_fo -> fo_client_data);
          if (ofn) *ofn = curr_fo -> fo_fn;
          /* Delete the structure for obj.      */
          if (prev_fo == 0) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head[index] = fo_next(curr_fo);
          } else {
            fo_set_next(prev_fo, fo_next(curr_fo));
            MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(prev_fo);
          }
          if (fn == 0) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_fo_entries--;
            /* May not happen if we get a signal.  But a high   */
            /* estimate will only make the table larger than    */
            /* necessary.                                       */
#           if !defined(THREADS) && !defined(DBG_HDRS_ALL)
              MANAGED_STACK_ADDRESS_BOEHM_GC_free((void *)curr_fo);
#           endif
          } else {
            curr_fo -> fo_fn = fn;
            curr_fo -> fo_client_data = (ptr_t)cd;
            curr_fo -> fo_mark_proc = mp;
            MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(curr_fo);
            /* Reinsert it.  We deleted it first to maintain    */
            /* consistency in the event of a signal.            */
            if (prev_fo == 0) {
              MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head[index] = curr_fo;
            } else {
              fo_set_next(prev_fo, curr_fo);
              MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(prev_fo);
            }
          }
          if (NULL == prev_fo)
            MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head + index);
          UNLOCK();
#         ifndef DBG_HDRS_ALL
              /* Free unused new_fo returned by MANAGED_STACK_ADDRESS_BOEHM_GC_oom_fn() */
              MANAGED_STACK_ADDRESS_BOEHM_GC_free((void *)new_fo);
#         endif
          return;
        }
        prev_fo = curr_fo;
        curr_fo = fo_next(curr_fo);
      }
      if (EXPECT(new_fo != 0, FALSE)) {
        /* new_fo is returned by MANAGED_STACK_ADDRESS_BOEHM_GC_oom_fn().   */
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(fn != 0);
#       ifdef LINT2
          if (NULL == hhdr) ABORT("Bad hhdr in MANAGED_STACK_ADDRESS_BOEHM_GC_register_finalizer_inner");
#       endif
        break;
      }
      if (fn == 0) {
        if (ocd) *ocd = 0;
        if (ofn) *ofn = 0;
        UNLOCK();
        return;
      }
      GET_HDR(obj, hhdr);
      if (EXPECT(0 == hhdr, FALSE)) {
        /* We won't collect it, hence finalizer wouldn't be run. */
        if (ocd) *ocd = 0;
        if (ofn) *ofn = 0;
        UNLOCK();
        return;
      }
      new_fo = (struct finalizable_object *)
        MANAGED_STACK_ADDRESS_BOEHM_GC_INTERNAL_MALLOC(sizeof(struct finalizable_object), NORMAL);
      if (EXPECT(new_fo != 0, TRUE))
        break;
      oom_fn = MANAGED_STACK_ADDRESS_BOEHM_GC_oom_fn;
      UNLOCK();
      new_fo = (struct finalizable_object *)
                (*oom_fn)(sizeof(struct finalizable_object));
      if (0 == new_fo) {
        /* No enough memory.  *ocd and *ofn remain unchanged.   */
        return;
      }
      /* It's not likely we'll make it here, but ... */
      LOCK();
      /* Recalculate index since the table may grow and         */
      /* check again that our finalizer is not in the table.    */
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_size(new_fo) >= sizeof(struct finalizable_object));
    if (ocd) *ocd = 0;
    if (ofn) *ofn = 0;
    new_fo -> fo_hidden_base = MANAGED_STACK_ADDRESS_BOEHM_GC_HIDE_POINTER(obj);
    new_fo -> fo_fn = fn;
    new_fo -> fo_client_data = (ptr_t)cd;
    new_fo -> fo_object_size = hhdr -> hb_sz;
    new_fo -> fo_mark_proc = mp;
    fo_set_next(new_fo, MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head[index]);
    MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(new_fo);
    MANAGED_STACK_ADDRESS_BOEHM_GC_fo_entries++;
    MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head[index] = new_fo;
    MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head + index);
    UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_register_finalizer(void * obj,
                                  MANAGED_STACK_ADDRESS_BOEHM_GC_finalization_proc fn, void * cd,
                                  MANAGED_STACK_ADDRESS_BOEHM_GC_finalization_proc *ofn, void ** ocd)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_register_finalizer_inner(obj, fn, cd, ofn,
                                ocd, MANAGED_STACK_ADDRESS_BOEHM_GC_normal_finalize_mark_proc);
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_register_finalizer_ignore_self(void * obj,
                               MANAGED_STACK_ADDRESS_BOEHM_GC_finalization_proc fn, void * cd,
                               MANAGED_STACK_ADDRESS_BOEHM_GC_finalization_proc *ofn, void ** ocd)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_register_finalizer_inner(obj, fn, cd, ofn,
                                ocd, MANAGED_STACK_ADDRESS_BOEHM_GC_ignore_self_finalize_mark_proc);
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_register_finalizer_no_order(void * obj,
                               MANAGED_STACK_ADDRESS_BOEHM_GC_finalization_proc fn, void * cd,
                               MANAGED_STACK_ADDRESS_BOEHM_GC_finalization_proc *ofn, void ** ocd)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_register_finalizer_inner(obj, fn, cd, ofn,
                                ocd, MANAGED_STACK_ADDRESS_BOEHM_GC_null_finalize_mark_proc);
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_register_finalizer_unreachable(void * obj,
                               MANAGED_STACK_ADDRESS_BOEHM_GC_finalization_proc fn, void * cd,
                               MANAGED_STACK_ADDRESS_BOEHM_GC_finalization_proc *ofn, void ** ocd)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_java_finalization);
    MANAGED_STACK_ADDRESS_BOEHM_GC_register_finalizer_inner(obj, fn, cd, ofn,
                                ocd, MANAGED_STACK_ADDRESS_BOEHM_GC_unreachable_finalize_mark_proc);
}

#ifndef NO_DEBUGGING
  STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_dump_finalization_links(
                                const struct dl_hashtbl_s *dl_hashtbl)
  {
    size_t dl_size = (size_t)1 << dl_hashtbl -> log_size;
    size_t i;

    if (NULL == dl_hashtbl -> head) return; /* empty table  */

    for (i = 0; i < dl_size; i++) {
      struct disappearing_link *curr_dl;

      for (curr_dl = dl_hashtbl -> head[i]; curr_dl != 0;
           curr_dl = dl_next(curr_dl)) {
        ptr_t real_ptr = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_REVEAL_POINTER(curr_dl -> dl_hidden_obj);
        ptr_t real_link = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_REVEAL_POINTER(curr_dl -> dl_hidden_link);

        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Object: %p, link value: %p, link addr: %p\n",
                  (void *)real_ptr, *(void **)real_link, (void *)real_link);
      }
    }
  }

  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_dump_finalization(void)
  {
    struct finalizable_object * curr_fo;
    size_t i;
    size_t fo_size = MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head == NULL ? 0 :
                                (size_t)1 << MANAGED_STACK_ADDRESS_BOEHM_GC_log_fo_table_size;

    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("\n***Disappearing (short) links:\n");
    MANAGED_STACK_ADDRESS_BOEHM_GC_dump_finalization_links(&MANAGED_STACK_ADDRESS_BOEHM_GC_dl_hashtbl);
#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_LONG_REFS_NOT_NEEDED
      MANAGED_STACK_ADDRESS_BOEHM_GC_printf("\n***Disappearing long links:\n");
      MANAGED_STACK_ADDRESS_BOEHM_GC_dump_finalization_links(&MANAGED_STACK_ADDRESS_BOEHM_GC_ll_hashtbl);
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_printf("\n***Finalizers:\n");
    for (i = 0; i < fo_size; i++) {
      for (curr_fo = MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head[i];
           curr_fo != NULL; curr_fo = fo_next(curr_fo)) {
        ptr_t real_ptr = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_REVEAL_POINTER(curr_fo -> fo_hidden_base);

        MANAGED_STACK_ADDRESS_BOEHM_GC_printf("Finalizable object: %p\n", (void *)real_ptr);
      }
    }
  }
#endif /* !NO_DEBUGGING */

#ifndef SMALL_CONFIG
  STATIC word MANAGED_STACK_ADDRESS_BOEHM_GC_old_dl_entries = 0; /* for stats printing */
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_LONG_REFS_NOT_NEEDED
    STATIC word MANAGED_STACK_ADDRESS_BOEHM_GC_old_ll_entries = 0;
# endif
#endif /* !SMALL_CONFIG */

#ifndef THREADS
  /* Global variables to minimize the level of recursion when a client  */
  /* finalizer allocates memory.                                        */
  STATIC int MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_nested = 0;
                        /* Only the lowest byte is used, the rest is    */
                        /* padding for proper global data alignment     */
                        /* required for some compilers (like Watcom).   */
  STATIC unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_skipped = 0;

  /* Checks and updates the level of finalizers recursion.              */
  /* Returns NULL if MANAGED_STACK_ADDRESS_BOEHM_GC_invoke_finalizers() should not be called by the */
  /* collector (to minimize the risk of a deep finalizers recursion),   */
  /* otherwise returns a pointer to MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_nested.                */
  STATIC unsigned char *MANAGED_STACK_ADDRESS_BOEHM_GC_check_finalizer_nested(void)
  {
    unsigned nesting_level = *(unsigned char *)&MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_nested;
    if (nesting_level) {
      /* We are inside another MANAGED_STACK_ADDRESS_BOEHM_GC_invoke_finalizers().          */
      /* Skip some implicitly-called MANAGED_STACK_ADDRESS_BOEHM_GC_invoke_finalizers()     */
      /* depending on the nesting (recursion) level.            */
      if (++MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_skipped < (1U << nesting_level)) return NULL;
      MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_skipped = 0;
    }
    *(char *)&MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_nested = (char)(nesting_level + 1);
    return (unsigned char *)&MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_nested;
  }
#endif /* !THREADS */

MANAGED_STACK_ADDRESS_BOEHM_GC_INLINE void MANAGED_STACK_ADDRESS_BOEHM_GC_make_disappearing_links_disappear(
                                        struct dl_hashtbl_s* dl_hashtbl,
                                        MANAGED_STACK_ADDRESS_BOEHM_GC_bool is_remove_dangling)
{
  size_t i;
  size_t dl_size = (size_t)1 << dl_hashtbl -> log_size;
  MANAGED_STACK_ADDRESS_BOEHM_GC_bool needs_barrier = FALSE;

  MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
  if (NULL == dl_hashtbl -> head) return; /* empty table  */

  for (i = 0; i < dl_size; i++) {
    struct disappearing_link *curr_dl, *next_dl;
    struct disappearing_link *prev_dl = NULL;

    for (curr_dl = dl_hashtbl->head[i]; curr_dl != NULL; curr_dl = next_dl) {
      next_dl = dl_next(curr_dl);
#     if defined(MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERTIONS) && !defined(THREAD_SANITIZER)
         /* Check accessibility of the location pointed by link. */
        MANAGED_STACK_ADDRESS_BOEHM_GC_noop1(*(word *)MANAGED_STACK_ADDRESS_BOEHM_GC_REVEAL_POINTER(curr_dl -> dl_hidden_link));
#     endif
      if (is_remove_dangling) {
        ptr_t real_link = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_base(MANAGED_STACK_ADDRESS_BOEHM_GC_REVEAL_POINTER(
                                                curr_dl -> dl_hidden_link));

        if (NULL == real_link || EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_marked(real_link), TRUE)) {
          prev_dl = curr_dl;
          continue;
        }
      } else {
        if (EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_marked((ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_REVEAL_POINTER(
                                        curr_dl -> dl_hidden_obj)), TRUE)) {
          prev_dl = curr_dl;
          continue;
        }
        *(ptr_t *)MANAGED_STACK_ADDRESS_BOEHM_GC_REVEAL_POINTER(curr_dl -> dl_hidden_link) = NULL;
      }

      /* Delete curr_dl entry from dl_hashtbl.  */
      if (NULL == prev_dl) {
        dl_hashtbl -> head[i] = next_dl;
        needs_barrier = TRUE;
      } else {
        dl_set_next(prev_dl, next_dl);
        MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(prev_dl);
      }
      MANAGED_STACK_ADDRESS_BOEHM_GC_clear_mark_bit(curr_dl);
      dl_hashtbl -> entries--;
    }
  }
  if (needs_barrier)
    MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(dl_hashtbl -> head); /* entire object */
}

/* Cause disappearing links to disappear and unreachable objects to be  */
/* enqueued for finalization.  Called with the world running.           */
MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_finalize(void)
{
    struct finalizable_object * curr_fo, * prev_fo, * next_fo;
    ptr_t real_ptr;
    size_t i;
    size_t fo_size = MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head == NULL ? 0 :
                                (size_t)1 << MANAGED_STACK_ADDRESS_BOEHM_GC_log_fo_table_size;
    MANAGED_STACK_ADDRESS_BOEHM_GC_bool needs_barrier = FALSE;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
#   ifndef SMALL_CONFIG
      /* Save current MANAGED_STACK_ADDRESS_BOEHM_GC_[dl/ll]_entries value for stats printing */
      MANAGED_STACK_ADDRESS_BOEHM_GC_old_dl_entries = MANAGED_STACK_ADDRESS_BOEHM_GC_dl_hashtbl.entries;
#     ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_LONG_REFS_NOT_NEEDED
        MANAGED_STACK_ADDRESS_BOEHM_GC_old_ll_entries = MANAGED_STACK_ADDRESS_BOEHM_GC_ll_hashtbl.entries;
#     endif
#   endif

#   ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_TOGGLE_REFS_NOT_NEEDED
      MANAGED_STACK_ADDRESS_BOEHM_GC_mark_togglerefs();
#   endif
    MANAGED_STACK_ADDRESS_BOEHM_GC_make_disappearing_links_disappear(&MANAGED_STACK_ADDRESS_BOEHM_GC_dl_hashtbl, FALSE);

  /* Mark all objects reachable via chains of 1 or more pointers        */
  /* from finalizable objects.                                          */
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(!MANAGED_STACK_ADDRESS_BOEHM_GC_collection_in_progress());
    for (i = 0; i < fo_size; i++) {
      for (curr_fo = MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head[i];
           curr_fo != NULL; curr_fo = fo_next(curr_fo)) {
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_size(curr_fo) >= sizeof(struct finalizable_object));
        real_ptr = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_REVEAL_POINTER(curr_fo -> fo_hidden_base);
        if (!MANAGED_STACK_ADDRESS_BOEHM_GC_is_marked(real_ptr)) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_MARKED_FOR_FINALIZATION(real_ptr);
            MANAGED_STACK_ADDRESS_BOEHM_GC_mark_fo(real_ptr, curr_fo -> fo_mark_proc);
            if (MANAGED_STACK_ADDRESS_BOEHM_GC_is_marked(real_ptr)) {
                WARN("Finalization cycle involving %p\n", real_ptr);
            }
        }
      }
    }
  /* Enqueue for finalization all objects that are still                */
  /* unreachable.                                                       */
    MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_finalized = 0;
    for (i = 0; i < fo_size; i++) {
      curr_fo = MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head[i];
      prev_fo = NULL;
      while (curr_fo != NULL) {
        real_ptr = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_REVEAL_POINTER(curr_fo -> fo_hidden_base);
        if (!MANAGED_STACK_ADDRESS_BOEHM_GC_is_marked(real_ptr)) {
            if (!MANAGED_STACK_ADDRESS_BOEHM_GC_java_finalization) {
              MANAGED_STACK_ADDRESS_BOEHM_GC_set_mark_bit(real_ptr);
            }
            /* Delete from hash table.  */
              next_fo = fo_next(curr_fo);
              if (NULL == prev_fo) {
                MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head[i] = next_fo;
                if (MANAGED_STACK_ADDRESS_BOEHM_GC_object_finalized_proc) {
                  MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head + i);
                } else {
                  needs_barrier = TRUE;
                }
              } else {
                fo_set_next(prev_fo, next_fo);
                MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(prev_fo);
              }
              MANAGED_STACK_ADDRESS_BOEHM_GC_fo_entries--;
              if (MANAGED_STACK_ADDRESS_BOEHM_GC_object_finalized_proc)
                MANAGED_STACK_ADDRESS_BOEHM_GC_object_finalized_proc(real_ptr);

            /* Add to list of objects awaiting finalization.    */
              fo_set_next(curr_fo, MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.finalize_now);
              MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(curr_fo);
              SET_FINALIZE_NOW(curr_fo);
            /* Unhide object pointer so any future collections will   */
            /* see it.                                                */
              curr_fo -> fo_hidden_base =
                        (word)MANAGED_STACK_ADDRESS_BOEHM_GC_REVEAL_POINTER(curr_fo -> fo_hidden_base);
              MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_finalized +=
                        curr_fo -> fo_object_size
                        + sizeof(struct finalizable_object);
            MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_marked(MANAGED_STACK_ADDRESS_BOEHM_GC_base(curr_fo)));
            curr_fo = next_fo;
        } else {
            prev_fo = curr_fo;
            curr_fo = fo_next(curr_fo);
        }
      }
    }

  if (MANAGED_STACK_ADDRESS_BOEHM_GC_java_finalization) {
    /* Make sure we mark everything reachable from objects finalized    */
    /* using the no-order fo_mark_proc.                                 */
      for (curr_fo = MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.finalize_now;
           curr_fo != NULL; curr_fo = fo_next(curr_fo)) {
        real_ptr = (ptr_t)(curr_fo -> fo_hidden_base); /* revealed */
        if (!MANAGED_STACK_ADDRESS_BOEHM_GC_is_marked(real_ptr)) {
            if (curr_fo -> fo_mark_proc == MANAGED_STACK_ADDRESS_BOEHM_GC_null_finalize_mark_proc) {
                MANAGED_STACK_ADDRESS_BOEHM_GC_mark_fo(real_ptr, MANAGED_STACK_ADDRESS_BOEHM_GC_normal_finalize_mark_proc);
            }
            if (curr_fo -> fo_mark_proc != MANAGED_STACK_ADDRESS_BOEHM_GC_unreachable_finalize_mark_proc) {
                MANAGED_STACK_ADDRESS_BOEHM_GC_set_mark_bit(real_ptr);
            }
        }
      }

    /* Now revive finalize-when-unreachable objects reachable from      */
    /* other finalizable objects.                                       */
      if (need_unreachable_finalization) {
        curr_fo = MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.finalize_now;
        MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NULL == curr_fo || MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head != NULL);
        for (prev_fo = NULL; curr_fo != NULL;
             prev_fo = curr_fo, curr_fo = next_fo) {
          next_fo = fo_next(curr_fo);
          if (curr_fo -> fo_mark_proc != MANAGED_STACK_ADDRESS_BOEHM_GC_unreachable_finalize_mark_proc)
            continue;

          real_ptr = (ptr_t)(curr_fo -> fo_hidden_base); /* revealed */
          if (!MANAGED_STACK_ADDRESS_BOEHM_GC_is_marked(real_ptr)) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_set_mark_bit(real_ptr);
            continue;
          }
          if (NULL == prev_fo) {
            SET_FINALIZE_NOW(next_fo);
          } else {
            fo_set_next(prev_fo, next_fo);
            MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(prev_fo);
          }
          curr_fo -> fo_hidden_base = MANAGED_STACK_ADDRESS_BOEHM_GC_HIDE_POINTER(real_ptr);
          MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_finalized -=
              (curr_fo -> fo_object_size) + sizeof(struct finalizable_object);

          i = HASH2(real_ptr, MANAGED_STACK_ADDRESS_BOEHM_GC_log_fo_table_size);
          fo_set_next(curr_fo, MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head[i]);
          MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(curr_fo);
          MANAGED_STACK_ADDRESS_BOEHM_GC_fo_entries++;
          MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head[i] = curr_fo;
          curr_fo = prev_fo;
          needs_barrier = TRUE;
        }
      }
  }
  if (needs_barrier)
    MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head); /* entire object */

  /* Remove dangling disappearing links. */
  MANAGED_STACK_ADDRESS_BOEHM_GC_make_disappearing_links_disappear(&MANAGED_STACK_ADDRESS_BOEHM_GC_dl_hashtbl, TRUE);

# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_TOGGLE_REFS_NOT_NEEDED
    MANAGED_STACK_ADDRESS_BOEHM_GC_clear_togglerefs();
# endif
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_LONG_REFS_NOT_NEEDED
    MANAGED_STACK_ADDRESS_BOEHM_GC_make_disappearing_links_disappear(&MANAGED_STACK_ADDRESS_BOEHM_GC_ll_hashtbl, FALSE);
    MANAGED_STACK_ADDRESS_BOEHM_GC_make_disappearing_links_disappear(&MANAGED_STACK_ADDRESS_BOEHM_GC_ll_hashtbl, TRUE);
# endif

  if (MANAGED_STACK_ADDRESS_BOEHM_GC_fail_count) {
    /* Don't prevent running finalizers if there has been an allocation */
    /* failure recently.                                                */
#   ifdef THREADS
      MANAGED_STACK_ADDRESS_BOEHM_GC_reset_finalizer_nested();
#   else
      MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_nested = 0;
#   endif
  }
}

/* Count of finalizers to run, at most, during a single invocation      */
/* of MANAGED_STACK_ADDRESS_BOEHM_GC_invoke_finalizers(); zero means no limit.  Accessed with the   */
/* allocation lock held.                                                */
STATIC unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_interrupt_finalizers = 0;

#ifndef JAVA_FINALIZATION_NOT_NEEDED

  /* Enqueue all remaining finalizers to be run.        */
  /* A collection in progress, if any, is completed     */
  /* when the first finalizer is enqueued.              */
  STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_enqueue_all_finalizers(void)
  {
    size_t i;
    size_t fo_size = MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head == NULL ? 0 :
                                (size_t)1 << MANAGED_STACK_ADDRESS_BOEHM_GC_log_fo_table_size;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_finalized = 0;
    for (i = 0; i < fo_size; i++) {
      struct finalizable_object * curr_fo = MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head[i];

      MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.fo_head[i] = NULL;
      while (curr_fo != NULL) {
          struct finalizable_object * next_fo;
          ptr_t real_ptr = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_REVEAL_POINTER(curr_fo -> fo_hidden_base);

          MANAGED_STACK_ADDRESS_BOEHM_GC_mark_fo(real_ptr, MANAGED_STACK_ADDRESS_BOEHM_GC_normal_finalize_mark_proc);
          MANAGED_STACK_ADDRESS_BOEHM_GC_set_mark_bit(real_ptr);
          MANAGED_STACK_ADDRESS_BOEHM_GC_complete_ongoing_collection();
          next_fo = fo_next(curr_fo);

          /* Add to list of objects awaiting finalization.      */
          fo_set_next(curr_fo, MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.finalize_now);
          MANAGED_STACK_ADDRESS_BOEHM_GC_dirty(curr_fo);
          SET_FINALIZE_NOW(curr_fo);

          /* Unhide object pointer so any future collections will       */
          /* see it.                                                    */
          curr_fo -> fo_hidden_base =
                        (word)MANAGED_STACK_ADDRESS_BOEHM_GC_REVEAL_POINTER(curr_fo -> fo_hidden_base);
          MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_finalized +=
                curr_fo -> fo_object_size + sizeof(struct finalizable_object);
          curr_fo = next_fo;
      }
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_fo_entries = 0;  /* all entries deleted from the hash table */
  }

  /* Invoke all remaining finalizers that haven't yet been run.
   * This is needed for strict compliance with the Java standard,
   * which can make the runtime guarantee that all finalizers are run.
   * Unfortunately, the Java standard implies we have to keep running
   * finalizers until there are no more left, a potential infinite loop.
   * YUCK.
   * Note that this is even more dangerous than the usual Java
   * finalizers, in that objects reachable from static variables
   * may have been finalized when these finalizers are run.
   * Finalizers run at this point must be prepared to deal with a
   * mostly broken world.
   */
  MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_finalize_all(void)
  {
    LOCK();
    while (MANAGED_STACK_ADDRESS_BOEHM_GC_fo_entries > 0) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_enqueue_all_finalizers();
      MANAGED_STACK_ADDRESS_BOEHM_GC_interrupt_finalizers = 0; /* reset */
      UNLOCK();
      MANAGED_STACK_ADDRESS_BOEHM_GC_invoke_finalizers();
      /* Running the finalizers in this thread is arguably not a good   */
      /* idea when we should be notifying another thread to run them.   */
      /* But otherwise we don't have a great way to wait for them to    */
      /* run.                                                           */
      LOCK();
    }
    UNLOCK();
  }

#endif /* !JAVA_FINALIZATION_NOT_NEEDED */

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_interrupt_finalizers(unsigned value)
{
  LOCK();
  MANAGED_STACK_ADDRESS_BOEHM_GC_interrupt_finalizers = value;
  UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API unsigned MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_interrupt_finalizers(void)
{
  unsigned value;

  LOCK();
  value = MANAGED_STACK_ADDRESS_BOEHM_GC_interrupt_finalizers;
  UNLOCK();
  return value;
}

/* Returns true if it is worth calling MANAGED_STACK_ADDRESS_BOEHM_GC_invoke_finalizers. (Useful if */
/* finalizers can only be called from some kind of "safe state" and     */
/* getting into that safe state is expensive.)                          */
MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_should_invoke_finalizers(void)
{
# ifdef AO_HAVE_load
    return AO_load((volatile AO_t *)&MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.finalize_now) != 0;
# else
    return MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.finalize_now != NULL;
# endif /* !THREADS */
}

/* Invoke finalizers for all objects that are ready to be finalized.    */
MANAGED_STACK_ADDRESS_BOEHM_GC_API int MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_invoke_finalizers(void)
{
    int count = 0;
    word bytes_freed_before = 0; /* initialized to prevent warning. */

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_DONT_HOLD_LOCK());
    while (MANAGED_STACK_ADDRESS_BOEHM_GC_should_invoke_finalizers()) {
        struct finalizable_object * curr_fo;
        ptr_t real_ptr;

        LOCK();
        if (count == 0) {
            bytes_freed_before = MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_freed;
            /* Don't do this outside, since we need the lock. */
        } else if (EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_interrupt_finalizers != 0, FALSE)
                   && (unsigned)count >= MANAGED_STACK_ADDRESS_BOEHM_GC_interrupt_finalizers) {
            UNLOCK();
            break;
        }
        curr_fo = MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.finalize_now;
#       ifdef THREADS
            if (EXPECT(NULL == curr_fo, FALSE)) {
                UNLOCK();
                break;
            }
#       endif
        SET_FINALIZE_NOW(fo_next(curr_fo));
        UNLOCK();
        fo_set_next(curr_fo, 0);
        real_ptr = (ptr_t)(curr_fo -> fo_hidden_base); /* revealed */
        (*(curr_fo -> fo_fn))(real_ptr, curr_fo -> fo_client_data);
        curr_fo -> fo_client_data = 0;
        ++count;
        /* Explicit freeing of curr_fo is probably a bad idea.  */
        /* It throws off accounting if nearly all objects are   */
        /* finalizable.  Otherwise it should not matter.        */
    }
    /* bytes_freed_before is initialized whenever count != 0 */
    if (count != 0
#         if defined(THREADS) && !defined(THREAD_SANITIZER)
            /* A quick check whether some memory was freed.     */
            /* The race with MANAGED_STACK_ADDRESS_BOEHM_GC_free() is safe to be ignored    */
            /* because we only need to know if the current      */
            /* thread has deallocated something.                */
            && bytes_freed_before != MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_freed
#         endif
       ) {
        LOCK();
        MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_bytes_freed += (MANAGED_STACK_ADDRESS_BOEHM_GC_bytes_freed - bytes_freed_before);
        UNLOCK();
    }
    return count;
}

static word last_finalizer_notification = 0;

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_notify_or_invoke_finalizers(void)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_notifier_proc notifier_fn = 0;
#   if defined(KEEP_BACK_PTRS) || defined(MAKE_BACK_GRAPH)
      static word last_back_trace_gc_no = 1;    /* Skip first one. */
#   endif

#   if defined(THREADS) && !defined(KEEP_BACK_PTRS) \
       && !defined(MAKE_BACK_GRAPH)
      /* Quick check (while unlocked) for an empty finalization queue.  */
      if (!MANAGED_STACK_ADDRESS_BOEHM_GC_should_invoke_finalizers())
        return;
#   endif
    LOCK();

    /* This is a convenient place to generate backtraces if appropriate, */
    /* since that code is not callable with the allocation lock.         */
#   if defined(KEEP_BACK_PTRS) || defined(MAKE_BACK_GRAPH)
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no > last_back_trace_gc_no) {
#       ifdef KEEP_BACK_PTRS
          long i;
          /* Stops when MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no wraps; that's OK.      */
          last_back_trace_gc_no = MANAGED_STACK_ADDRESS_BOEHM_GC_WORD_MAX;  /* disable others. */
          for (i = 0; i < MANAGED_STACK_ADDRESS_BOEHM_GC_backtraces; ++i) {
              /* FIXME: This tolerates concurrent heap mutation,        */
              /* which may cause occasional mysterious results.         */
              /* We need to release the GC lock, since MANAGED_STACK_ADDRESS_BOEHM_GC_print_callers */
              /* acquires it.  It probably shouldn't.                   */
              void *current = MANAGED_STACK_ADDRESS_BOEHM_GC_generate_random_valid_address();

              UNLOCK();
              MANAGED_STACK_ADDRESS_BOEHM_GC_printf("\n****Chosen address %p in object\n", current);
              MANAGED_STACK_ADDRESS_BOEHM_GC_print_backtrace(current);
              LOCK();
          }
          last_back_trace_gc_no = MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no;
#       endif
#       ifdef MAKE_BACK_GRAPH
          if (MANAGED_STACK_ADDRESS_BOEHM_GC_print_back_height) {
            MANAGED_STACK_ADDRESS_BOEHM_GC_print_back_graph_stats();
          }
#       endif
      }
#   endif
    if (NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.finalize_now) {
      UNLOCK();
      return;
    }

    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_finalize_on_demand) {
      unsigned char *pnested;

#     ifdef THREADS
        if (EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_in_thread_creation, FALSE)) {
          UNLOCK();
          return;
        }
#     endif
      pnested = MANAGED_STACK_ADDRESS_BOEHM_GC_check_finalizer_nested();
      UNLOCK();
      /* Skip MANAGED_STACK_ADDRESS_BOEHM_GC_invoke_finalizers() if nested. */
      if (pnested != NULL) {
        (void)MANAGED_STACK_ADDRESS_BOEHM_GC_invoke_finalizers();
        *pnested = 0; /* Reset since no more finalizers or interrupted. */
#       ifndef THREADS
          MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.finalize_now
                    || MANAGED_STACK_ADDRESS_BOEHM_GC_interrupt_finalizers > 0);
#       endif   /* Otherwise GC can run concurrently and add more */
      }
      return;
    }

    /* These variables require synchronization to avoid data races.     */
    if (last_finalizer_notification != MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no) {
        notifier_fn = MANAGED_STACK_ADDRESS_BOEHM_GC_finalizer_notifier;
        last_finalizer_notification = MANAGED_STACK_ADDRESS_BOEHM_GC_gc_no;
    }
    UNLOCK();
    if (notifier_fn != 0)
        (*notifier_fn)(); /* Invoke the notifier */
}

#ifndef SMALL_CONFIG
# ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_LONG_REFS_NOT_NEEDED
#   define IF_LONG_REFS_PRESENT_ELSE(x,y) (x)
# else
#   define IF_LONG_REFS_PRESENT_ELSE(x,y) (y)
# endif

  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_print_finalization_stats(void)
  {
    struct finalizable_object *fo;
    unsigned long ready = 0;

    MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("%lu finalization entries;"
                  " %lu/%lu short/long disappearing links alive\n",
                  (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_fo_entries,
                  (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_dl_hashtbl.entries,
                  (unsigned long)IF_LONG_REFS_PRESENT_ELSE(
                                                MANAGED_STACK_ADDRESS_BOEHM_GC_ll_hashtbl.entries, 0));

    for (fo = MANAGED_STACK_ADDRESS_BOEHM_GC_fnlz_roots.finalize_now; fo != NULL; fo = fo_next(fo))
      ++ready;
    MANAGED_STACK_ADDRESS_BOEHM_GC_log_printf("%lu finalization-ready objects;"
                  " %ld/%ld short/long links cleared\n",
                  ready,
                  (long)MANAGED_STACK_ADDRESS_BOEHM_GC_old_dl_entries - (long)MANAGED_STACK_ADDRESS_BOEHM_GC_dl_hashtbl.entries,
                  (long)IF_LONG_REFS_PRESENT_ELSE(
                              MANAGED_STACK_ADDRESS_BOEHM_GC_old_ll_entries - MANAGED_STACK_ADDRESS_BOEHM_GC_ll_hashtbl.entries, 0));
  }
#endif /* !SMALL_CONFIG */

#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_NO_FINALIZATION */
