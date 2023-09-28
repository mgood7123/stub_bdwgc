

/******************************************************************

  AmigaOS-specific routines for GC.
  This file is normally included from os_dep.c

******************************************************************/


#if !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_DEF) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_SB) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_DS) && !defined(MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_AM)
# include "private/gc_priv.h"
# include <stdio.h>
# include <signal.h>
# define MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_DEF
# define MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_SB
# define MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_DS
# define MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_AM
#endif


#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_DEF

# ifndef __GNUC__
#   include <exec/exec.h>
# endif
# include <proto/exec.h>
# include <proto/dos.h>
# include <dos/dosextens.h>
# include <workbench/startup.h>

#endif




#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_SB

/******************************************************************
   Find the base of the stack.
******************************************************************/

ptr_t MANAGED_STACK_ADDRESS_BOEHM_GC_get_main_stack_base(void)
{
    struct Process *proc = (struct Process*)SysBase->ThisTask;

    /* Reference: Amiga Guru Book Pages: 42,567,574 */
    if (proc->pr_Task.tc_Node.ln_Type==NT_PROCESS
        && proc->pr_CLI != NULL) {
        /* first ULONG is StackSize */
        /*longPtr = proc->pr_ReturnAddr;
        size = longPtr[0];*/

        return (char *)proc->pr_ReturnAddr + sizeof(ULONG);
    } else {
        return (char *)proc->pr_Task.tc_SPUpper;
    }
}

#endif


#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_DS
/******************************************************************
   Register data segments.
******************************************************************/

   void MANAGED_STACK_ADDRESS_BOEHM_GC_register_data_segments(void)
   {
     struct Process     *proc;
     struct CommandLineInterface *cli;
     BPTR myseglist;
     ULONG *data;

#     ifdef __GNUC__
        ULONG dataSegSize;
        MANAGED_STACK_ADDRESS_BOEHM_GC_bool found_segment = FALSE;
        extern char __data_size[];

        dataSegSize=__data_size+8;
        /* Can`t find the Location of __data_size, because
           it`s possible that is it, inside the segment. */

#     endif

        proc= (struct Process*)SysBase->ThisTask;

        /* Reference: Amiga Guru Book Pages: 538ff,565,573
                     and XOper.asm */
        myseglist = proc->pr_SegList;
        if (proc->pr_Task.tc_Node.ln_Type==NT_PROCESS) {
          if (proc->pr_CLI != NULL) {
            /* ProcLoaded       'Loaded as a command: '*/
            cli = BADDR(proc->pr_CLI);
            myseglist = cli->cli_Module;
          }
        } else {
          ABORT("Not a Process.");
        }

        if (myseglist == NULL) {
            ABORT("Arrrgh.. can't find segments, aborting");
        }

        /* xoper hunks Shell Process */

        for (data = (ULONG *)BADDR(myseglist); data != NULL;
             data = (ULONG *)BADDR(data[0])) {
          if ((ULONG)MANAGED_STACK_ADDRESS_BOEHM_GC_register_data_segments < (ULONG)(&data[1])
              || (ULONG)MANAGED_STACK_ADDRESS_BOEHM_GC_register_data_segments > (ULONG)(&data[1])
                                                    + data[-1]) {
#             ifdef __GNUC__
                if (dataSegSize == data[-1]) {
                  found_segment = TRUE;
                }
#             endif
              MANAGED_STACK_ADDRESS_BOEHM_GC_add_roots_inner((char *)&data[1],
                                 ((char *)&data[1]) + data[-1], FALSE);
          }
        } /* for */
#       ifdef __GNUC__
           if (!found_segment) {
             ABORT("Can`t find correct Segments.\nSolution: Use an newer version of ixemul.library");
           }
#       endif
   }

#endif



#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_AM

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_FASTALLOC

void *MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_allocwrapper(size_t size,void *(*AllocFunction)(size_t size2)){
        return (*AllocFunction)(size);
}

void *(*MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_allocwrapper_do)(size_t size,void *(*AllocFunction)(size_t size2))
        =MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_allocwrapper;

#else




void *MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_allocwrapper_firsttime(size_t size,void *(*AllocFunction)(size_t size2));

void *(*MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_allocwrapper_do)(size_t size,void *(*AllocFunction)(size_t size2))
        =MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_allocwrapper_firsttime;


/******************************************************************
   Amiga-specific routines to obtain memory, and force GC to give
   back fast-mem whenever possible.
        These hacks makes gc-programs go many times faster when
   the Amiga is low on memory, and are therefore strictly necessary.

   -Kjetil S. Matheussen, 2000.
******************************************************************/



/* List-header for all allocated memory. */

struct MANAGED_STACK_ADDRESS_BOEHM_GC_Amiga_AllocedMemoryHeader{
        ULONG size;
        struct MANAGED_STACK_ADDRESS_BOEHM_GC_Amiga_AllocedMemoryHeader *next;
};
struct MANAGED_STACK_ADDRESS_BOEHM_GC_Amiga_AllocedMemoryHeader *MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGAMEM=(struct MANAGED_STACK_ADDRESS_BOEHM_GC_Amiga_AllocedMemoryHeader *)(int)~(NULL);



/* Type of memory. Once in the execution of a program, this might change to MEMF_ANY|MEMF_CLEAR */

ULONG MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_MEMF = MEMF_FAST | MEMF_CLEAR;


/* Prevents MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_get_mem from allocating memory if this one is TRUE. */
#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_ONLYFAST
BOOL MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_dontalloc=FALSE;
#endif

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_PRINTSTATS
int succ=0,succ2=0;
int nsucc=0,nsucc2=0;
int nullretries=0;
int numcollects=0;
int chipa=0;
int allochip=0;
int allocfast=0;
int cur0=0;
int cur1=0;
int cur10=0;
int cur50=0;
int cur150=0;
int cur151=0;
int ncur0=0;
int ncur1=0;
int ncur10=0;
int ncur50=0;
int ncur150=0;
int ncur151=0;
#endif

/* Free everything at program-end. */

void MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_free_all_mem(void){
        struct MANAGED_STACK_ADDRESS_BOEHM_GC_Amiga_AllocedMemoryHeader *gc_am=(struct MANAGED_STACK_ADDRESS_BOEHM_GC_Amiga_AllocedMemoryHeader *)(~(int)(MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGAMEM));

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_PRINTSTATS
        printf("\n\n"
                "%d bytes of chip-mem, and %d bytes of fast-mem where allocated from the OS.\n",
                allochip,allocfast
        );
        printf(
                "%d bytes of chip-mem were returned from the MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_FASTALLOC supported allocating functions.\n",
                chipa
        );
        printf("\n");
        printf("MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect was called %d times to avoid returning NULL or start allocating with the MEMF_ANY flag.\n",numcollects);
        printf("%d of them was a success. (the others had to use allocation from the OS.)\n",nullretries);
        printf("\n");
        printf("Succeeded forcing %d gc-allocations (%d bytes) of chip-mem to be fast-mem.\n",succ,succ2);
        printf("Failed forcing %d gc-allocations (%d bytes) of chip-mem to be fast-mem.\n",nsucc,nsucc2);
        printf("\n");
        printf(
                "Number of retries before succeeding a chip->fast force:\n"
                "0: %d, 1: %d, 2-9: %d, 10-49: %d, 50-149: %d, >150: %d\n",
                cur0,cur1,cur10,cur50,cur150,cur151
        );
        printf(
                "Number of retries before giving up a chip->fast force:\n"
                "0: %d, 1: %d, 2-9: %d, 10-49: %d, 50-149: %d, >150: %d\n",
                ncur0,ncur1,ncur10,ncur50,ncur150,ncur151
        );
#endif

        while(gc_am!=NULL){
                struct MANAGED_STACK_ADDRESS_BOEHM_GC_Amiga_AllocedMemoryHeader *temp = gc_am->next;
                FreeMem(gc_am,gc_am->size);
                gc_am=(struct MANAGED_STACK_ADDRESS_BOEHM_GC_Amiga_AllocedMemoryHeader *)(~(int)(temp));
        }
}

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_ONLYFAST

/* All memory with address lower than this one is chip-mem. */

char *chipmax;


/*
 * Always set to the last size of memory tried to be allocated.
 * Needed to ensure allocation when the size is bigger than 100000.
 *
 */
size_t latestsize;

#endif


#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_FASTALLOC

/*
 * The actual function that is called with the GET_MEM macro.
 *
 */

void *MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_get_mem(size_t size){
        struct MANAGED_STACK_ADDRESS_BOEHM_GC_Amiga_AllocedMemoryHeader *gc_am;

#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_ONLYFAST
        if(MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_dontalloc==TRUE){
                return NULL;
        }

        /* We really don't want to use chip-mem, but if we must, then as little as possible. */
        if(MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_MEMF==(MEMF_ANY|MEMF_CLEAR) && size>100000 && latestsize<50000) return NULL;
#endif

        gc_am=AllocMem((ULONG)(size + sizeof(struct MANAGED_STACK_ADDRESS_BOEHM_GC_Amiga_AllocedMemoryHeader)),MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_MEMF);
        if(gc_am==NULL) return NULL;

        gc_am->next=MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGAMEM;
        gc_am->size=size + sizeof(struct MANAGED_STACK_ADDRESS_BOEHM_GC_Amiga_AllocedMemoryHeader);
        MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGAMEM=(struct MANAGED_STACK_ADDRESS_BOEHM_GC_Amiga_AllocedMemoryHeader *)(~(int)(gc_am));

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_PRINTSTATS
        if((char *)gc_am<chipmax){
                allochip+=size;
        }else{
                allocfast+=size;
        }
#endif

        return gc_am+1;

}

#endif


#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_ONLYFAST

/* Tries very hard to force GC to find fast-mem to return. Done recursively
 * to hold the rejected memory-pointers reachable from the collector in an
 * easy way.
 *
 */
#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_RETRY
void *MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_rec_alloc(size_t size,void *(*AllocFunction)(size_t size2),const int rec){
        void *ret;

        ret=(*AllocFunction)(size);

#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_PRINTSTATS
        if((char *)ret>chipmax || ret==NULL){
                if(ret==NULL){
                        nsucc++;
                        nsucc2+=size;
                        if(rec==0) ncur0++;
                        if(rec==1) ncur1++;
                        if(rec>1 && rec<10) ncur10++;
                        if(rec>=10 && rec<50) ncur50++;
                        if(rec>=50 && rec<150) ncur150++;
                        if(rec>=150) ncur151++;
                }else{
                        succ++;
                        succ2+=size;
                        if(rec==0) cur0++;
                        if(rec==1) cur1++;
                        if(rec>1 && rec<10) cur10++;
                        if(rec>=10 && rec<50) cur50++;
                        if(rec>=50 && rec<150) cur150++;
                        if(rec>=150) cur151++;
                }
        }
#endif

        if (((char *)ret)<=chipmax && ret!=NULL && (rec<(size>500000?9:size/5000))){
                ret=MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_rec_alloc(size,AllocFunction,rec+1);
        }

        return ret;
}
#endif


/* The allocating-functions defined inside the Amiga-blocks in gc.h is called
 * via these functions.
 */


void *MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_allocwrapper_any(size_t size,void *(*AllocFunction)(size_t size2)){
        void *ret;

        MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_dontalloc=TRUE; /* Pretty tough thing to do, but it's indeed necessary. */
        latestsize=size;

        ret=(*AllocFunction)(size);

        if(((char *)ret) <= chipmax){
                if(ret==NULL){
                        /* Give GC access to allocate memory. */
#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_GC
                        if(!MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc){
                                MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect();
#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_PRINTSTATS
                                numcollects++;
#endif
                                ret=(*AllocFunction)(size);
                        }
                        if(ret==NULL)
#endif
                        {
                                MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_dontalloc=FALSE;
                                ret=(*AllocFunction)(size);
                                if(ret==NULL){
                                        WARN("Out of Memory!  Returning NIL!\n", 0);
                                }
                        }
#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_PRINTSTATS
                        else{
                                nullretries++;
                        }
                        if(ret!=NULL && (char *)ret<=chipmax) chipa+=size;
#endif
                }
#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_RETRY
                else{
                        void *ret2;
                        /* We got chip-mem. Better try again and again and again etc., we might get fast-mem sooner or later... */
                        /* Using gctest to check the effectiveness of doing this, does seldom give a very good result. */
                        /* However, real programs doesn't normally rapidly allocate and deallocate. */
                        if(
                                AllocFunction!=MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_uncollectable
#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_ATOMIC_UNCOLLECTABLE
                                && AllocFunction!=MANAGED_STACK_ADDRESS_BOEHM_GC_malloc_atomic_uncollectable
#endif
                        ){
                                ret2=MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_rec_alloc(size,AllocFunction,0);
                        }else{
                                ret2=(*AllocFunction)(size);
#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_PRINTSTATS
                                if((char *)ret2<chipmax || ret2==NULL){
                                        nsucc++;
                                        nsucc2+=size;
                                        ncur0++;
                                }else{
                                        succ++;
                                        succ2+=size;
                                        cur0++;
                                }
#endif
                        }
                        if(((char *)ret2)>chipmax){
                                MANAGED_STACK_ADDRESS_BOEHM_GC_free(ret);
                                ret=ret2;
                        }else{
                                MANAGED_STACK_ADDRESS_BOEHM_GC_free(ret2);
                        }
                }
#endif
        }

#   if defined(CPPCHECK)
      if (MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_dontalloc) /* variable is actually used by AllocFunction */
#   endif
        MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_dontalloc=FALSE;

        return ret;
}



void (*MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_toany)(void)=NULL;

void MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_set_toany(void (*func)(void)){
        MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_toany=func;
}

#endif /* !MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_ONLYFAST */


void *MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_allocwrapper_fast(size_t size,void *(*AllocFunction)(size_t size2)){
        void *ret;

        ret=(*AllocFunction)(size);

        if(ret==NULL){
                /* Enable chip-mem allocation. */
#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_GC
                if(!MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc){
                        MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect();
#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_PRINTSTATS
                        numcollects++;
#endif
                        ret=(*AllocFunction)(size);
                }
                if(ret==NULL)
#endif
                {
#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_ONLYFAST
                        MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_MEMF=MEMF_ANY | MEMF_CLEAR;
                        if(MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_toany!=NULL) (*MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_toany)();
                        MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_allocwrapper_do=MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_allocwrapper_any;
                        return MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_allocwrapper_any(size,AllocFunction);
#endif
                }
#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_PRINTSTATS
                else{
                        nullretries++;
                }
#endif
        }

        return ret;
}

void *MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_allocwrapper_firsttime(size_t size,void *(*AllocFunction)(size_t size2)){
        atexit(&MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_free_all_mem);
        chipmax=(char *)SysBase->MaxLocMem; /* For people still having SysBase in chip-mem, this might speed up a bit. */
        MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_allocwrapper_do=MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_allocwrapper_fast;
        return MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_allocwrapper_fast(size,AllocFunction);
}


#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_FASTALLOC */



/*
 * The wrapped realloc function.
 *
 */
void *MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_realloc(void *old_object,size_t new_size_in_bytes){
#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_FASTALLOC
        return MANAGED_STACK_ADDRESS_BOEHM_GC_realloc(old_object,new_size_in_bytes);
#else
        void *ret;
        latestsize=new_size_in_bytes;
        ret=MANAGED_STACK_ADDRESS_BOEHM_GC_realloc(old_object,new_size_in_bytes);
        if(ret==NULL && new_size_in_bytes != 0
           && MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_MEMF==(MEMF_FAST | MEMF_CLEAR)){
                /* Out of fast-mem. */
#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_GC
                if(!MANAGED_STACK_ADDRESS_BOEHM_GC_dont_gc){
                        MANAGED_STACK_ADDRESS_BOEHM_GC_gcollect();
#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_PRINTSTATS
                        numcollects++;
#endif
                        ret=MANAGED_STACK_ADDRESS_BOEHM_GC_realloc(old_object,new_size_in_bytes);
                }
                if(ret==NULL)
#endif
                {
#ifndef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_ONLYFAST
                        MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_MEMF=MEMF_ANY | MEMF_CLEAR;
                        if(MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_toany!=NULL) (*MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_toany)();
                        MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_allocwrapper_do=MANAGED_STACK_ADDRESS_BOEHM_GC_amiga_allocwrapper_any;
                        ret=MANAGED_STACK_ADDRESS_BOEHM_GC_realloc(old_object,new_size_in_bytes);
#endif
                }
#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_PRINTSTATS
                else{
                        nullretries++;
                }
#endif
        }
        if(ret==NULL && new_size_in_bytes != 0){
                WARN("Out of Memory!  Returning NIL!\n", 0);
        }
#ifdef MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_PRINTSTATS
        if(((char *)ret)<chipmax && ret!=NULL){
                chipa+=new_size_in_bytes;
        }
#endif
        return ret;
#endif
}

#endif /* MANAGED_STACK_ADDRESS_BOEHM_GC_AMIGA_AM */
