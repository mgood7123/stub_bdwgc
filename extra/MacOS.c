/*
        MacOS.c

        Some routines for the Macintosh OS port of the Hans-J. Boehm, Alan J. Demers
        garbage collector.

        <Revision History>

        11/22/94  pcb  StripAddress the temporary memory handle for 24-bit mode.
        11/30/94  pcb  Tracking all memory usage so we can deallocate it all at once.
        02/10/96  pcb  Added routine to perform a final collection when
unloading shared library.

        by Patrick C. Beard.
 */
/* Boehm, February 15, 1996 2:55 pm PST */

#include <Resources.h>
#include <Memory.h>
#include <LowMem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MANAGED_STACK_ADDRESS_BOEHM_GC_BUILD
#include "gc.h"
#include "private/gc_priv.h"

/* use 'CODE' resource 0 to get exact location of the beginning of global space. */

typedef struct {
        unsigned long aboveA5;
        unsigned long belowA5;
        unsigned long JTSize;
        unsigned long JTOffset;
} *CodeZeroPtr, **CodeZeroHandle;

void* MANAGED_STACK_ADDRESS_BOEHM_GC_MacGetDataStart(void)
{
        CodeZeroHandle code0 = (CodeZeroHandle)GetResource('CODE', 0);
        if (code0) {
                long belowA5Size = (**code0).belowA5;
                ReleaseResource((Handle)code0);
                return (LMGetCurrentA5() - belowA5Size);
        }
        fprintf(stderr, "Couldn't load the jump table.");
        exit(-1);
# if !defined(CPPCHECK)
        return 0; /* to avoid compiler complain about missing return */
# endif
}

#ifdef USE_TEMPORARY_MEMORY

/* track the use of temporary memory so it can be freed all at once. */

typedef struct TemporaryMemoryBlock TemporaryMemoryBlock, **TemporaryMemoryHandle;

struct TemporaryMemoryBlock {
        TemporaryMemoryHandle nextBlock;
        char data[];
};

static TemporaryMemoryHandle theTemporaryMemory = NULL;

void MANAGED_STACK_ADDRESS_BOEHM_GC_MacFreeTemporaryMemory(void);

Ptr MANAGED_STACK_ADDRESS_BOEHM_GC_MacTemporaryNewPtr(size_t size, Boolean clearMemory)
{
#     if !defined(SHARED_LIBRARY_BUILD)
        static Boolean firstTime = true;
#     endif
        OSErr result;
        TemporaryMemoryHandle tempMemBlock;
        Ptr tempPtr = nil;

        tempMemBlock = (TemporaryMemoryHandle)TempNewHandle(size + sizeof(TemporaryMemoryBlock), &result);
        if (tempMemBlock && result == noErr) {
                HLockHi((Handle)tempMemBlock);
                tempPtr = (**tempMemBlock).data;
                if (clearMemory) memset(tempPtr, 0, size);
                tempPtr = StripAddress(tempPtr);

                /* keep track of the allocated blocks. */
                (**tempMemBlock).nextBlock = theTemporaryMemory;
                theTemporaryMemory = tempMemBlock;
        }

#     if !defined(SHARED_LIBRARY_BUILD)
        /* install an exit routine to clean up the memory used at the end. */
        if (firstTime) {
                atexit(&MANAGED_STACK_ADDRESS_BOEHM_GC_MacFreeTemporaryMemory);
                firstTime = false;
        }
#     endif

        return tempPtr;
}

static void perform_final_collection(void)
{
  unsigned i;
  word last_fo_entries = 0;

  /* adjust the stack bottom, because CFM calls us from another stack
     location. */
     MANAGED_STACK_ADDRESS_BOEHM_GC_stackbottom = (ptr_t)&i;

  /* try to collect and finalize everything in sight */
    for (i = 0; i < 2 || MANAGED_STACK_ADDRESS_BOEHM_GC_fo_entries < last_fo_entries; i++) {
        last_fo_entries = MANAGED_STACK_ADDRESS_BOEHM_GC_fo_entries;
        MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect();
    }
}


void MANAGED_STACK_ADDRESS_BOEHM_GC_MacFreeTemporaryMemory(void)
{
# if defined(SHARED_LIBRARY_BUILD)
    /* if possible, collect all memory, and invoke all finalizers. */
      perform_final_collection();
# endif

    if (theTemporaryMemory != NULL) {
        TemporaryMemoryHandle tempMemBlock = theTemporaryMemory;
        while (tempMemBlock /* != NULL */) {
                TemporaryMemoryHandle nextBlock = (**tempMemBlock).nextBlock;
                DisposeHandle((Handle)tempMemBlock);
                tempMemBlock = nextBlock;
        }
        theTemporaryMemory = NULL;
    }
}

#endif /* USE_TEMPORARY_MEMORY */

#if __option(far_data)

  void* MANAGED_STACK_ADDRESS_BOEHM_GC_MacGetDataEnd(void)
  {
        CodeZeroHandle code0 = (CodeZeroHandle)GetResource('CODE', 0);
        if (code0) {
                long aboveA5Size = (**code0).aboveA5;
                ReleaseResource((Handle)code0);
                return (LMGetCurrentA5() + aboveA5Size);
        }
        fprintf(stderr, "Couldn't load the jump table.");
        exit(-1);
#   if !defined(CPPCHECK)
        return 0;
#   endif
  }

#endif /* __option(far_data) */
