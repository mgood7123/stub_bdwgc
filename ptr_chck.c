/*
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
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

/*
 * These are checking routines calls to which could be inserted by a
 * preprocessor to validate C pointer arithmetic.
 */

STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK MANAGED_STACK_ADDRESS_BOEHM_GC_default_same_obj_print_proc(void * p, void * q)
{
    ABORT_ARG2("MANAGED_STACK_ADDRESS_BOEHM_GC_same_obj test failed",
               ": %p and %p are not in the same object", p, q);
}

MANAGED_STACK_ADDRESS_BOEHM_GC_same_obj_print_proc_t MANAGED_STACK_ADDRESS_BOEHM_GC_same_obj_print_proc =
                MANAGED_STACK_ADDRESS_BOEHM_GC_default_same_obj_print_proc;

MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_same_obj(void *p, void *q)
{
    struct hblk *h;
    hdr *hhdr;
    ptr_t base, limit;
    word sz;

    if (!EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized, TRUE)) MANAGED_STACK_ADDRESS_BOEHM_GC_init();
    hhdr = HDR((word)p);
    if (NULL == hhdr) {
        if (divHBLKSZ((word)p) != divHBLKSZ((word)q)
                && HDR((word)q) != NULL) {
            goto fail;
        }
        return p;
    }
    /* If it's a pointer to the middle of a large object, move it       */
    /* to the beginning.                                                */
    if (IS_FORWARDING_ADDR_OR_NIL(hhdr)) {
        h = HBLKPTR(p) - (word)hhdr;
        hhdr = HDR(h);
        while (IS_FORWARDING_ADDR_OR_NIL(hhdr)) {
           h = FORWARDED_ADDR(h, hhdr);
           hhdr = HDR(h);
        }
        limit = (ptr_t)h + hhdr -> hb_sz;
        if ((word)p >= (word)limit || (word)q >= (word)limit
            || (word)q < (word)h) {
            goto fail;
        }
        return p;
    }
    sz = hhdr -> hb_sz;
    if (sz > MAXOBJBYTES) {
      base = (ptr_t)HBLKPTR(p);
      limit = base + sz;
      if ((word)p >= (word)limit) {
        goto fail;
      }
    } else {
      size_t offset;
      size_t pdispl = HBLKDISPL(p);

      offset = pdispl % sz;
      if (HBLKPTR(p) != HBLKPTR(q)) goto fail;
                /* W/o this check, we might miss an error if    */
                /* q points to the first object on a page, and  */
                /* points just before the page.                 */
      base = (ptr_t)p - offset;
      limit = base + sz;
    }
    /* [base, limit) delimits the object containing p, if any.  */
    /* If p is not inside a valid object, then either q is      */
    /* also outside any valid object, or it is outside          */
    /* [base, limit).                                           */
    if ((word)q >= (word)limit || (word)q < (word)base) {
        goto fail;
    }
    return p;
fail:
    (*MANAGED_STACK_ADDRESS_BOEHM_GC_same_obj_print_proc)((ptr_t)p, (ptr_t)q);
    return p;
}

STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK MANAGED_STACK_ADDRESS_BOEHM_GC_default_is_valid_displacement_print_proc(void *p)
{
    ABORT_ARG1("MANAGED_STACK_ADDRESS_BOEHM_GC_is_valid_displacement test failed", ": %p not valid", p);
}

MANAGED_STACK_ADDRESS_BOEHM_GC_valid_ptr_print_proc_t MANAGED_STACK_ADDRESS_BOEHM_GC_is_valid_displacement_print_proc =
                MANAGED_STACK_ADDRESS_BOEHM_GC_default_is_valid_displacement_print_proc;

MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_is_valid_displacement(void *p)
{
    hdr *hhdr;
    word pdispl;
    word offset;
    struct hblk *h;
    word sz;

    if (!EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized, TRUE)) MANAGED_STACK_ADDRESS_BOEHM_GC_init();
    if (NULL == p) return NULL;
    hhdr = HDR((word)p);
    if (NULL == hhdr) return p;
    h = HBLKPTR(p);
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers) {
        while (IS_FORWARDING_ADDR_OR_NIL(hhdr)) {
           h = FORWARDED_ADDR(h, hhdr);
           hhdr = HDR(h);
        }
    } else if (IS_FORWARDING_ADDR_OR_NIL(hhdr)) {
        goto fail;
    }
    sz = hhdr -> hb_sz;
    pdispl = HBLKDISPL(p);
    offset = pdispl % sz;
    if ((sz > MAXOBJBYTES && (word)p >= (word)h + sz)
        || !MANAGED_STACK_ADDRESS_BOEHM_GC_valid_offsets[offset]
        || ((word)p + (sz - offset) > (word)(h + 1)
            && !IS_FORWARDING_ADDR_OR_NIL(HDR(h + 1)))) {
        goto fail;
    }
    return p;
fail:
    (*MANAGED_STACK_ADDRESS_BOEHM_GC_is_valid_displacement_print_proc)((ptr_t)p);
    return p;
}

STATIC void MANAGED_STACK_ADDRESS_BOEHM_GC_CALLBACK MANAGED_STACK_ADDRESS_BOEHM_GC_default_is_visible_print_proc(void * p)
{
    ABORT_ARG1("MANAGED_STACK_ADDRESS_BOEHM_GC_is_visible test failed", ": %p not GC-visible", p);
}

MANAGED_STACK_ADDRESS_BOEHM_GC_valid_ptr_print_proc_t MANAGED_STACK_ADDRESS_BOEHM_GC_is_visible_print_proc =
                MANAGED_STACK_ADDRESS_BOEHM_GC_default_is_visible_print_proc;

#ifndef THREADS
/* Could p be a stack address? */
  STATIC MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_on_stack(void *p)
  {
    return (word)p HOTTER_THAN (word)MANAGED_STACK_ADDRESS_BOEHM_GC_stackbottom
            && !((word)p HOTTER_THAN (word)MANAGED_STACK_ADDRESS_BOEHM_GC_approx_sp());
  }
#endif /* !THREADS */

MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_is_visible(void *p)
{
    hdr *hhdr;

    if ((word)p & (ALIGNMENT - 1)) goto fail;
    if (!EXPECT(MANAGED_STACK_ADDRESS_BOEHM_GC_is_initialized, TRUE)) MANAGED_STACK_ADDRESS_BOEHM_GC_init();
#   ifdef THREADS
        hhdr = HDR((word)p);
        if (hhdr != NULL && NULL == MANAGED_STACK_ADDRESS_BOEHM_GC_base(p)) {
            goto fail;
        } else {
            /* May be inside thread stack.  We can't do much. */
            return p;
        }
#   else
        /* Check stack first: */
          if (MANAGED_STACK_ADDRESS_BOEHM_GC_on_stack(p)) return p;
        hhdr = HDR((word)p);
        if (NULL == hhdr) {
            if (MANAGED_STACK_ADDRESS_BOEHM_GC_is_static_root(p)) return p;
            /* Else do it again correctly:      */
#           if defined(DYNAMIC_LOADING) || defined(ANY_MSWIN) || defined(PCR)
              if (!MANAGED_STACK_ADDRESS_BOEHM_GC_no_dls) {
                MANAGED_STACK_ADDRESS_BOEHM_GC_register_dynamic_libraries();
                if (MANAGED_STACK_ADDRESS_BOEHM_GC_is_static_root(p)) return p;
              }
#           endif
            goto fail;
        } else {
            /* p points to the heap. */
            word descr;
            ptr_t base = (ptr_t)MANAGED_STACK_ADDRESS_BOEHM_GC_base(p);
                        /* TODO: should MANAGED_STACK_ADDRESS_BOEHM_GC_base be manually inlined? */

            if (NULL == base) goto fail;
            if (HBLKPTR(base) != HBLKPTR(p))
                hhdr = HDR(base);
            descr = hhdr -> hb_descr;
    retry:
            switch(descr & MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS) {
                case MANAGED_STACK_ADDRESS_BOEHM_GC_DS_LENGTH:
                    if ((word)p - (word)base > descr) goto fail;
                    break;
                case MANAGED_STACK_ADDRESS_BOEHM_GC_DS_BITMAP:
                    if ((ptr_t)p - base >= WORDS_TO_BYTES(BITMAP_BITS)
                        || ((word)p & (sizeof(word)-1)) != 0) goto fail;
                    if (!(((word)1 << (CPP_WORDSZ-1 - ((word)p - (word)base)))
                          & descr)) goto fail;
                    break;
                case MANAGED_STACK_ADDRESS_BOEHM_GC_DS_PROC:
                    /* We could try to decipher this partially.         */
                    /* For now we just punt.                            */
                    break;
                case MANAGED_STACK_ADDRESS_BOEHM_GC_DS_PER_OBJECT:
                    if (!(descr & SIGNB)) {
                      descr = *(word *)((ptr_t)base
                                        + (descr & ~(word)MANAGED_STACK_ADDRESS_BOEHM_GC_DS_TAGS));
                    } else {
                      ptr_t type_descr = *(ptr_t *)base;
                      descr = *(word *)(type_descr
                                        - (descr - (word)(MANAGED_STACK_ADDRESS_BOEHM_GC_DS_PER_OBJECT
                                           - MANAGED_STACK_ADDRESS_BOEHM_GC_INDIR_PER_OBJ_BIAS)));
                    }
                    goto retry;
            }
            return p;
        }
#   endif
fail:
    (*MANAGED_STACK_ADDRESS_BOEHM_GC_is_visible_print_proc)((ptr_t)p);
    return p;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_pre_incr(void **p, ptrdiff_t how_much)
{
    void * initial = *p;
    void * result = MANAGED_STACK_ADDRESS_BOEHM_GC_same_obj((void *)((ptr_t)initial + how_much), initial);

    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers) {
        (void)MANAGED_STACK_ADDRESS_BOEHM_GC_is_valid_displacement(result);
    }
    *p = result;
    return result; /* updated pointer */
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void * MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_post_incr(void **p, ptrdiff_t how_much)
{
    void * initial = *p;
    void * result = MANAGED_STACK_ADDRESS_BOEHM_GC_same_obj((void *)((ptr_t)initial + how_much), initial);

    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers) {
        (void)MANAGED_STACK_ADDRESS_BOEHM_GC_is_valid_displacement(result);
    }
    *p = result;
    return initial; /* original *p */
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_same_obj_print_proc(MANAGED_STACK_ADDRESS_BOEHM_GC_same_obj_print_proc_t fn)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NONNULL_ARG_NOT_NULL(fn));
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_same_obj_print_proc = fn;
    UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_same_obj_print_proc_t MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_same_obj_print_proc(void)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_same_obj_print_proc_t fn;

    LOCK();
    fn = MANAGED_STACK_ADDRESS_BOEHM_GC_same_obj_print_proc;
    UNLOCK();
    return fn;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_is_valid_displacement_print_proc(
                                        MANAGED_STACK_ADDRESS_BOEHM_GC_valid_ptr_print_proc_t fn)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NONNULL_ARG_NOT_NULL(fn));
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_is_valid_displacement_print_proc = fn;
    UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_valid_ptr_print_proc_t MANAGED_STACK_ADDRESS_BOEHM_GC_CALL
MANAGED_STACK_ADDRESS_BOEHM_GC_get_is_valid_displacement_print_proc(void)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_valid_ptr_print_proc_t fn;

    LOCK();
    fn = MANAGED_STACK_ADDRESS_BOEHM_GC_is_valid_displacement_print_proc;
    UNLOCK();
    return fn;
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_set_is_visible_print_proc(MANAGED_STACK_ADDRESS_BOEHM_GC_valid_ptr_print_proc_t fn)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(NONNULL_ARG_NOT_NULL(fn));
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_is_visible_print_proc = fn;
    UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_API MANAGED_STACK_ADDRESS_BOEHM_GC_valid_ptr_print_proc_t MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_get_is_visible_print_proc(void)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_valid_ptr_print_proc_t fn;

    LOCK();
    fn = MANAGED_STACK_ADDRESS_BOEHM_GC_is_visible_print_proc;
    UNLOCK();
    return fn;
}
