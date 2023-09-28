/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991, 1992 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1999-2001 by Hewlett-Packard Company. All rights reserved.
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

/* Routines for maintaining maps describing heap block
 * layouts for various object sizes.  Allows fast pointer validity checks
 * and fast location of object start locations on machines (such as SPARC)
 * with slow division.
 */

/* Consider pointers that are offset bytes displaced from the beginning */
/* of an object to be valid.                                            */

MANAGED_STACK_ADDRESS_BOEHM_GC_API void MANAGED_STACK_ADDRESS_BOEHM_GC_CALL MANAGED_STACK_ADDRESS_BOEHM_GC_register_displacement(size_t offset)
{
    LOCK();
    MANAGED_STACK_ADDRESS_BOEHM_GC_register_displacement_inner(offset);
    UNLOCK();
}

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_register_displacement_inner(size_t offset)
{
    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    if (offset >= VALID_OFFSET_SZ) {
        ABORT("Bad argument to MANAGED_STACK_ADDRESS_BOEHM_GC_register_displacement");
    }
    if (!MANAGED_STACK_ADDRESS_BOEHM_GC_valid_offsets[offset]) {
      MANAGED_STACK_ADDRESS_BOEHM_GC_valid_offsets[offset] = TRUE;
      MANAGED_STACK_ADDRESS_BOEHM_GC_modws_valid_offsets[offset % sizeof(word)] = TRUE;
    }
}

#ifndef MARK_BIT_PER_OBJ
  /* Add a heap block map for objects of size granules to obj_map.      */
  /* A size of 0 is used for large objects.  Return FALSE on failure.   */
  MANAGED_STACK_ADDRESS_BOEHM_GC_INNER MANAGED_STACK_ADDRESS_BOEHM_GC_bool MANAGED_STACK_ADDRESS_BOEHM_GC_add_map_entry(size_t granules)
  {
    unsigned displ;
    unsigned short * new_map;

    MANAGED_STACK_ADDRESS_BOEHM_GC_ASSERT(I_HOLD_LOCK());
    if (granules > BYTES_TO_GRANULES(MAXOBJBYTES)) granules = 0;
    if (MANAGED_STACK_ADDRESS_BOEHM_GC_obj_map[granules] != 0) return TRUE;

    new_map = (unsigned short *)MANAGED_STACK_ADDRESS_BOEHM_GC_scratch_alloc(OBJ_MAP_LEN * sizeof(short));
    if (EXPECT(NULL == new_map, FALSE)) return FALSE;

    MANAGED_STACK_ADDRESS_BOEHM_GC_COND_LOG_PRINTF(
                "Adding block map for size of %u granules (%u bytes)\n",
                (unsigned)granules, (unsigned)GRANULES_TO_BYTES(granules));
    if (granules == 0) {
      for (displ = 0; displ < OBJ_MAP_LEN; displ++) {
        new_map[displ] = 1;  /* Nonzero to get us out of marker fast path. */
      }
    } else {
      for (displ = 0; displ < OBJ_MAP_LEN; displ++) {
        new_map[displ] = (unsigned short)(displ % granules);
      }
    }
    MANAGED_STACK_ADDRESS_BOEHM_GC_obj_map[granules] = new_map;
    return TRUE;
  }
#endif /* !MARK_BIT_PER_OBJ */

MANAGED_STACK_ADDRESS_BOEHM_GC_INNER void MANAGED_STACK_ADDRESS_BOEHM_GC_initialize_offsets(void)
{
  unsigned i;
  if (MANAGED_STACK_ADDRESS_BOEHM_GC_all_interior_pointers) {
    for (i = 0; i < VALID_OFFSET_SZ; ++i)
      MANAGED_STACK_ADDRESS_BOEHM_GC_valid_offsets[i] = TRUE;
  } else {
    BZERO(MANAGED_STACK_ADDRESS_BOEHM_GC_valid_offsets, sizeof(MANAGED_STACK_ADDRESS_BOEHM_GC_valid_offsets));
    for (i = 0; i < sizeof(word); ++i)
      MANAGED_STACK_ADDRESS_BOEHM_GC_modws_valid_offsets[i] = FALSE;
  }
}
