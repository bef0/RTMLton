/* Copyright (C) 2009-2010,2012 Matthew Fluet.
 * Copyright (C) 1999-2008 Henry Cejtin, Matthew Fluet, Suresh
 *    Jagannathan, and Stephen Weeks.
 * Copyright (C) 1997-2000 NEC Research Institute.
 *
 * MLton is released under a BSD-style license.
 * See the file MLton-LICENSE for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <errno.h>

#pragma GCC diagnostic push  // require GCC 4.6
#pragma GCC diagnostic ignored "-Wformat"

struct thrctrl
{
    pthread_mutex_t lock;
    pthread_cond_t cond;
    pthread_cond_t safepoint_cond;      // "paused" threads will wait on this condition
    pthread_mutex_t safepoint_lock;

    int running_threads;
    int booted;
    int gc_needed;
    int requested_by;
} TC;

#define IFED(X) do { if (X) { perror("perror " #X); exit(-1); } } while(0)

#define LOCK IFED(pthread_mutex_lock(&s->fl_lock))
#define UNLOCK IFED(pthread_mutex_unlock(&s->fl_lock))

#define BLOCK IFED(pthread_cond_wait(&s->fl_empty_cond,&s->fl_lock))
#define BROADCAST IFED(pthread_cond_broadcast(&s->fl_empty_cond))

#define RTSYNC_LOCK IFED(pthread_mutex_lock(&s->rtSync_lock))
#define RTSYNC_UNLOCK IFED(pthread_mutex_unlock(&s->rtSync_lock))
#define RTSYNC_SIGNAL IFED(pthread_cond_signal(&s->rtSync_cond))
#define RTSYNC_BLOCK IFED(pthread_cond_wait(&s->rtSync_cond,&s->rtSync_lock))
#define RTSYNC_TRYLOCK pthread_mutex_trylock(&s->rtSync_lock)

#define TC_LOCK if (DEBUG) fprintf(stderr, "%d] TC_LOCK thr:%d boot:%d\n", PTHREAD_NUM, TC.running_threads, TC.booted); IFED(pthread_mutex_lock(&TC.lock))
#define TC_UNLOCK if (DEBUG) fprintf(stderr, "%d] TC_UNLOCK thr:%d boot:%d\n", PTHREAD_NUM, TC.running_threads, TC.booted); IFED(pthread_mutex_unlock(&TC.lock))
#define TCSP_LOCK if (DEBUG) fprintf(stderr, "%d] TCSP_LOCK thr:%d boot:%d\n", PTHREAD_NUM, TC.running_threads, TC.booted); IFED(pthread_mutex_lock(&TC.safepoint_lock))
#define TCSP_UNLOCK if (DEBUG) fprintf(stderr, "%d] TCSP_UNLOCK thr:%d boot:%d\n", PTHREAD_NUM, TC.running_threads, TC.booted); IFED(pthread_mutex_unlock(&TC.safepoint_lock))

/*
 * - threads can ask for GC's by setting gc_needed to 1
 *   and requested_by to their threadid. in the current
 *   model, only the most recent requester (eg the one 
 *   right before running_threads hits zero) is noted.
 * - threads can enter safe point (for a GC) by decrementing
 *   running_threads by one.
 * - A thread may leave a safe point only if gc_needed is zero.
 * - Upon leaving the safe point, running_threads is incremented by one.
 *
 * ask_for_gc:
 *    lock(gc_needed = 1)
 *
 * enter_safe_point:
 *    lock(running_threads --)
 *
 * leave_safe_point:
 *    spin while (gc_needed == 1)
 *    lock(running_threads ++)
 *
 */

#define pthread_yield sched_yield

#define REQUESTGC do { \
        if (DEBUG) fprintf(stderr, "%d] REQUESTGC start thr:%d\n", PTHREAD_NUM, TC.running_threads); \
        TC_LOCK; TC.gc_needed = 1; TC.requested_by = PTHREAD_NUM; setup_for_gc(s); TC_UNLOCK; \
        if (DEBUG) fprintf(stderr, "%d] REQUESTGC end thr:%d\n", PTHREAD_NUM, TC.running_threads); \
        } while(0)
#define COMPLETEGC do { \
        if (DEBUG) fprintf(stderr, "%d] COMPLETEGC start thr:%d\n", PTHREAD_NUM, TC.running_threads); \
        finish_for_gc(s); \
        if (DEBUG) fprintf(stderr, "%d] COMPLETEGC end thr:%d\n", PTHREAD_NUM, TC.running_threads); \
        } while(0)
#define ENTER_SAFEPOINT do { \
        if (DEBUG) fprintf(stderr, "%d] ENTER_SAFEPOINT start thr:%d\n", PTHREAD_NUM, TC.running_threads); \
        TC_LOCK; TC.running_threads--; pthread_cond_signal(&TC.cond); TC_UNLOCK; \
        if (DEBUG) fprintf(stderr, "%d] ENTER_SAFEPOINT end thr:%d\n", PTHREAD_NUM, TC.running_threads); \
        } while(0)
#define LEAVE_SAFEPOINT do { \
        if (DEBUG) fprintf(stderr, "%d] LEAVE_SAFEPOINT start thr:%d\n", PTHREAD_NUM, TC.running_threads); \
        TCSP_LOCK; while (TC.gc_needed) pthread_cond_wait(&TC.safepoint_cond, &TC.safepoint_lock); TCSP_UNLOCK; \
        TC_LOCK; TC.running_threads++; TC_UNLOCK; \
        if (DEBUG) fprintf(stderr, "%d] LEAVE_SAFEPOINT end thr:%d\n", PTHREAD_NUM, TC.running_threads); \
        } while(0)


#ifndef CHECKDISABLEGC
#define CHECKDISABLEGC do { if (getenv("DISABLEGC")) { fprintf(stderr, "GC is disabled\n"); return; } } while(0)
#endif
void minorGC (GC_state s) {
    //minorCheneyCopyGC (s);
}


void
growStackCurrent (GC_state s)
{
    size_t reserved;
    GC_stack stack;

    reserved = sizeofStackGrowReserved (s, getStackCurrent (s));
    if (DEBUG_STACKS or s->controls.messages)
        fprintf (stderr,
                 "[GC: Growing stack of size %s bytes to size %s bytes, using %s bytes.]\n",
                 uintmaxToCommaString (getStackCurrent (s)->reserved),
                 uintmaxToCommaString (reserved),
                 uintmaxToCommaString (getStackCurrent (s)->used));

    /* TODO insufficient heap will cause grow to fail since we've now separated
     * stack ops from heap ops
     */

    if (not hasHeapBytesFree (s, sizeofStackWithHeader (s, reserved), 0)) {
        if (DEBUG_STACKS or s->controls.messages)
            fprintf (stderr,
                     "%d]No heap bytes free to grow stack hence calling GC\n",
                     PTHREAD_NUM);
        //             resizeHeap (s, s->lastMajorStatistics.bytesLive + sizeofStackWithHeader(s,reserved));
    }


//  assert (hasHeapBytesFree (s, sizeofStackWithHeader (s, reserved), 0));
    stack = newStack (s, reserved, TRUE);
    copyStack (s, getStackCurrent (s), stack);
    getThreadCurrent (s)->stack =
        pointerToObjptr ((pointer) stack, s->heap.start);

    s->stackBottom[PTHREAD_NUM] = getStackBottom (s, stack);
    s->stackTop[PTHREAD_NUM] = getStackTop (s, stack);
    s->stackLimit[PTHREAD_NUM] = getStackLimit (s, stack);
}

void
maybe_growstack (GC_state s)
{
    bool stackTopOk;
    if (isStackEmpty (getStackCurrent (s)))
        return;
    stackTopOk = invariantForMutatorStack (s);
#if 0
    int stackBytesRequested =
        stackTopOk
        ? 0
        : sizeofStackWithHeader (s,
                                 sizeofStackGrowReserved (s,
                                                          getStackCurrent
                                                       (s)));
#endif
    unless (stackTopOk) growStackCurrent (s);
}

void
enterGC (GC_state s)
{
    if (s->profiling.isOn) {
        /* We don't need to profileEnter for count profiling because it
         * has already bumped the counter.  If we did allow the bump, then
         * the count would look like function(s) had run an extra time.
         */
        if (s->profiling.stack and not (PROFILE_COUNT == s->profiling.kind))
            GC_profileEnter (s);
    }
    s->amInGC = TRUE;
}

/* TODO
 * define THREADED to enable threading (right now its linear just like
 * the original code)
 */
void
leaveGC (GC_state s)
{
    if (s->profiling.isOn) {
        if (s->profiling.stack and not (PROFILE_COUNT == s->profiling.kind))
            GC_profileLeave (s);
    }
    s->amInGC = FALSE;
}

#define THREADED

#undef THREADED
#define CONCURRENT

#undef GCTHRDEBUG

#ifdef GCTHRDEBUG
#define DBG(X) fprintf X
#else
#define DBG(X)
#endif

#define MYASSERT(T, X, COMP, RV) {                               \
	  T __rv__ = (T)X;                                           \
        if (!(__rv__ COMP (T)RV)) {                              \
                fprintf(stderr, #X " failed, %x " #COMP " %x\n", \
                                __rv__, (T)RV);                  \
                exit(-1);                                        \
        }                                                        \
}

#define COPYIN(EL) s->EL[1] = s->EL[TC.requested_by]
#define COPYOUT(EL) s->EL[TC.requested_by] = s->EL[1]
#define SANITY(EL) if (s->EL[TC.requested_by] == s->EL[1]) if (DEBUG) fprintf(stderr,"%d] " #EL " changed!\n", PTHREAD_NUM);

inline static void
setup_for_gc (GC_state s)
{
    COPYIN (stackTop);
    if (DEBUG)
        fprintf (stderr,
                 "%d] GCREqBy = %d , before copy stackBottom = " FMTPTR
                 " , should become = " FMTPTR " , actually = " FMTPTR
                 " \n", PTHREAD_NUM, TC.requested_by, s->stackBottom[1],
                 s->stackBottom[TC.requested_by], s->stackBottom[0]);
    COPYIN (stackBottom);
    if (DEBUG)
        fprintf (stderr,
                 "%d] GCReqBy= %d,  after copy StackBottom = " FMTPTR " \n",
                 PTHREAD_NUM, TC.requested_by, s->stackBottom[1]);
    COPYIN (stackLimit);
    COPYIN (exnStack);
    if (DEBUG)
        fprintf (stderr,
                 "%d] GCREqBy = %d , before copy currentThread = "FMTPTR" , should become = "FMTPTR" , main thread = "FMTPTR" \n", PTHREAD_NUM, TC.requested_by,
                 s->currentThread[1],s->currentThread[TC.requested_by],s->currentThread[0]);
    COPYIN (currentThread);
    if (DEBUG)
        fprintf (stderr,
                 "%d] GCReqBy= %d,  after copy currentThread = "FMTPTR" \n", PTHREAD_NUM, TC.requested_by,s->currentThread[1]);
    COPYIN (savedThread);
    COPYIN (signalHandlerThread);
    COPYIN (ffiOpArgsResPtr);
}

inline static void
sanity_check_array (GC_state s)
{
    SANITY (stackTop);
    SANITY (stackBottom);
    SANITY (stackLimit);
    SANITY (exnStack);
    SANITY (currentThread);
    SANITY (savedThread);
    SANITY (signalHandlerThread);
    SANITY (ffiOpArgsResPtr);
}

inline static void
finish_for_gc (GC_state s)
{
    sanity_check_array (s);
    COPYOUT (stackTop);
    COPYOUT (stackBottom);
    COPYOUT (stackLimit);
    COPYOUT (exnStack);
    COPYOUT (currentThread);
    COPYOUT (savedThread);
    COPYOUT (signalHandlerThread);
    COPYOUT (ffiOpArgsResPtr);
}



__attribute__ ((noreturn))
     void *GCrunner (void *_s)
{
    GC_state s = (GC_state) _s;

    set_pthread_num (1);        // by definition
    TC.running_threads = 1;     // main thr
    TC.gc_needed = 0;
    TC.booted = 0;
    TC.requested_by = 0;

    pthread_mutex_init (&TC.lock, NULL);
    pthread_mutex_init (&TC.safepoint_lock, NULL);
    pthread_cond_init (&TC.cond, NULL);
    pthread_cond_init (&TC.safepoint_cond, NULL);

    if (DEBUG)
        fprintf (stderr, "%d] GC_Runner Thread running.\n", PTHREAD_NUM);

    s->GCrunnerRunning = TRUE;
    
  
   /* if(s->currentThread[PTHREAD_NUM] == BOGUS_OBJPTR)
    {
        if(DEBUG_THREADS)
            fprintf(stderr,"%d] creating green thread to link with RT thread\n");
     
        GC_thread thread = newThread (s, sizeofStackInitialReserved (s));
        switchToThread (s, pointerToObjptr((pointer)thread - offsetofThread (s), s->heap.start));
    }
    */

#ifdef CONCURRENT
    while(1){
        if(DEBUG)
            fprintf(stderr,"%d] GC thread is Idle: FC=%d \n",PTHREAD_NUM,s->fl_chunks);
       
       /* GC sweep is performed under RTSYNC_LOCK because this lock also prevents the mutators from marking their stacks*/ 
        RTSYNC_LOCK;
        
        RTSYNC_BLOCK;
        

        assert(s->dirty);


        if(DEBUG_RTGC)
            fprintf(stderr,"%d] GC going to sweep, free chunks = %d\n",PTHREAD_NUM,s->fl_chunks);
        
        s->isGCRunning = true;
        performGC_helper (s,
                          s->oldGenBytesRequested,
                          s->nurseryBytesRequested,
                          s->forceMajor, s->mayResize);
        


        s->dirty = false;
        /*Change this to reset all rtSync values for all RT threads*/
        s->rtSync[0] = false;
     
       /*Need to acquire s->fl_lock before braodcast to have predictable scheduling behavior. man pthread_cond_broadcast*/ 
        LOCK;
        BROADCAST;
        UNLOCK; 

        s->isGCRunning = false;
        RTSYNC_UNLOCK;
        
        
    
    }


#endif
    


#ifdef THREADED
    while (1) {
        if (DEBUG)
            fprintf (stderr, "%d] GCrunner: waiting for GC request.\n",
                     PTHREAD_NUM);

        TC_LOCK;
        do {
            do {
                if (DEBUG)
                    fprintf (stderr,
                             "%d] TC_UNLOCK (implied) thr:%d boot:%d\n",
                             PTHREAD_NUM, TC.running_threads, TC.booted);
                pthread_cond_wait (&TC.cond, &TC.lock); // implicit TC_UNLOCK
            }
            while (TC.booted && TC.running_threads);
        }
        while (!TC.gc_needed);

        // TC_LOCK is re-acquired here as a result of cond_wait succeeding

        if (DEBUG)
            fprintf (stderr,
                     "%d] GCrunner: GC requested. all threads should be paused.\n",
                     PTHREAD_NUM);

        // at this point, all threads should be paused and the GC can proceed    

        if (s->isRealTimeThreadInitialized) {
            if (DEBUG) {
                fprintf (stderr,
                         "%d] GCrunner: threads paused. GC'ing\n",
                         PTHREAD_NUM);
                fprintf (stderr,
                         "%d] GC running needed=%d threads=%d\n",
                         PTHREAD_NUM, TC.gc_needed, TC.running_threads);
            }

            performUMGC(s, 3000, 0, true);


            if (DEBUG)
                fprintf (stderr,
                         "%d] GCrunner: finished. unpausing threads.\n",
                         PTHREAD_NUM);

            TC.gc_needed = 0;
            pthread_cond_broadcast (&TC.safepoint_cond);        // unpause all threads
            TC_UNLOCK;
        }
        else {
            fprintf (stderr,
                     "%d] GCrunner: skipping thread pause bc RTT is not yet initialized\n",
                     PTHREAD_NUM);
        }
    }
#endif

 /*NOTREACHED*/}

void
performGC (GC_state s,
           size_t oldGenBytesRequested,
           size_t nurseryBytesRequested, bool forceMajor, bool mayResize)
{
    if (DEBUG)
        fprintf (stderr, "%d] performGC: starting..\n", PTHREAD_NUM);

    CHECKDISABLEGC;

    /* In our MT formulation of realtime MLton, we move the
     * GC into a separate pthread. In order to keep things
     * orderly, we implement a STW approach to GC. The GC
     * waits on mutex acquisition. When it acquires it, that
     * indicates a GC was requested by one of the other threads.
     * Before progressing, the GC must ask all of the threads to
     * pause. We do this by using a condition variable that the 
     * threads wait on when they are at a safe point.
     * 
     * Once all threads are paused on the condition, the GC can 
     * proceed. Once the GC finishes, it signals all of the 
     * threads waiting on the condition, causing them to wakeup.
     */
    s->oldGenBytesRequested = oldGenBytesRequested;
    s->nurseryBytesRequested = nurseryBytesRequested;
    s->forceMajor = forceMajor;
    s->mayResize = mayResize;

#ifdef THREADED
   
    while (s->isRealTimeThreadInitialized == FALSE) {
        if (DEBUG) {
            fprintf (stderr, "%d] spin [can't performGC yet] ..\n",
                     PTHREAD_NUM);
        }
        ssleep (1, 0);
    }

    REQUESTGC;
    ENTER_SAFEPOINT;
    sched_yield ();
    LEAVE_SAFEPOINT;
    COMPLETEGC;

#else
    DBG ((stderr, "non-threaded mode, passing thru to performGC_helper\n"));

    s->rtSync[PTHREAD_NUM]= true;
    //performGC_helper (s, oldGenBytesRequested, nurseryBytesRequested,
      //                forceMajor, mayResize);
#endif
}

static void markStack(GC_state s,GC_stack currentStack)
{

            //fprintf(stderr,"%d] Marking stack \n",PTHREAD_NUM);
            assert(!s->dirty);

            foreachGlobalObjptr (s, umDfsMarkObjectsMark);
            foreachObjptrInObject(s, (pointer) currentStack, umDfsMarkObjectsMark, FALSE);
    
            //fprintf(stderr,"%d] Marked its stack,FC=%d. dirty=%d\n",PTHREAD_NUM,s->fl_chunks,s->dirty?1:0) ;

}

static void sweep(GC_state s, size_t ensureObjectChunksAvailable,
                 size_t ensureArrayChunksAvailable,
                 bool fullGC)
{


    pointer pchunk;
    size_t step = sizeof(struct GC_UM_Chunk)+sizeof(UM_header); /*account for size of chunktype header*/
    //pointer end = s->umheap.start + s->umheap.size - step;


    int freed  = 0;

    assert(PTHREAD_NUM ==1);

#if 0
    //    if (s->umheap.fl_chunks <= 2000) {
    for (pchunk=s->umheap.start;
         pchunk < end;
         pchunk+=step) {
        GC_UM_Chunk pc = (GC_UM_Chunk)pchunk;
        if ((pc->chunk_header & UM_CHUNK_IN_USE) &&
            (!(pc->chunk_header & UM_CHUNK_MARK_MASK))) {
            if (DEBUG_MEM) {
                fprintf(stderr, "Collecting: "FMTPTR", %d, %d\n",
                        (uintptr_t)pc, pc->sentinel, pc->chunk_header);
            }
            insertFreeChunk(s, &(s->umheap), pchunk);
        }

        if (!fullGC && s->fl_chunks >= ensureObjectChunksAvailable) {
            break;
        }
    }
        //    }

    step = sizeof(struct GC_UM_Array_Chunk);
    end = s->umarheap.start + s->umarheap.size - step;

    for (pchunk=s->umarheap.start;
         pchunk < end;
         pchunk += step) {
        GC_UM_Array_Chunk pc = (GC_UM_Array_Chunk)pchunk;
        if ((pc->array_chunk_header & UM_CHUNK_IN_USE) &&
            (!(pc->array_chunk_header & UM_CHUNK_MARK_MASK))) {
            if (DEBUG_MEM) {
                fprintf(stderr, "Collecting array: "FMTPTR", %d, %d\n",
                        (uintptr_t)pc, pc->array_chunk_magic,
                        pc->array_chunk_header);
            }
            insertFreeChunk(s, &(s->umarheap), pchunk);
        }

        /* if (!fullGC && */
        /*     s->fl_chunks >= ensureArrayChunksAvailable) { */
        /*     fprintf(stderr, "Array chunk ensured\n"); */
        /*     break; */
        /* } */
    }

#endif


    for (pchunk=s->umheap.start;
         pchunk < s->umheap.limit;
         pchunk+=step) {
        assert(pchunk != s->umheap.limit);
        if(((UM_Mem_Chunk)pchunk)->chunkType == UM_NORMAL_CHUNK)
        {
        GC_UM_Chunk pc = (GC_UM_Chunk)(pchunk+sizeof(UM_header)); /*account for size of chunktype*/
        if ((pc->chunk_header & UM_CHUNK_IN_USE) &&
            (!(pc->chunk_header & UM_CHUNK_MARK_MASK) && !(pc->chunk_header & UM_CHUNK_GREY_MASK))) {
            if (DEBUG_MEM) {
                fprintf(stderr, "Collecting: "FMTPTR", %d, %d\n",
                        (uintptr_t)pc, pc->sentinel, pc->chunk_header);
            }
            insertFreeChunk(s, &(s->umheap), pchunk);
            s->cGCStats.numChunksFreed++;
            freed++;
        }
        else if ((pc->chunk_header & UM_CHUNK_IN_USE) &&
            ((pc->chunk_header & UM_CHUNK_MARK_MASK) || (pc->chunk_header & UM_CHUNK_GREY_MASK))) {

               /*Unmark Chunk*/ 
                    pc->chunk_header &= ~UM_CHUNK_MARK_MASK;
                /*Unmark MLton Object*/
                    /*pointer p  =  (pointer)(pc + GC_NORMAL_HEADER_SIZE);
                    GC_header* headerp = getHeaderp(p);
                    GC_header header = *headerp;
                    header = header & ~MARK_MASK;
                    (*headerp) = header;*/
        }

        }
        else if(((UM_Mem_Chunk)pchunk)->chunkType == UM_ARRAY_CHUNK)
        {

        GC_UM_Array_Chunk pc = (GC_UM_Array_Chunk)(pchunk + sizeof(UM_header)); /*account for size of chunktype*/
        if ((pc->array_chunk_header & UM_CHUNK_IN_USE) &&
            (!(pc->array_chunk_header & UM_CHUNK_MARK_MASK) && !(pc->array_chunk_header & UM_CHUNK_GREY_MASK))) {
            if (DEBUG_MEM) {
                fprintf(stderr, "Collecting array: "FMTPTR", %d, %d\n",
                        (uintptr_t)pc, pc->array_chunk_magic,
                        pc->array_chunk_header);
            }
            insertFreeChunk(s, &(s->umheap), pchunk);
            s->cGCStats.numChunksFreed++;
            freed++;
        }
       if ((pc->array_chunk_header & UM_CHUNK_IN_USE) &&
            ((pc->array_chunk_header & UM_CHUNK_MARK_MASK) || (pc->array_chunk_header & UM_CHUNK_GREY_MASK))) {

            /*Unmark Array chunk*/
            pc->array_chunk_header &= ~UM_CHUNK_MARK_MASK;
           
            /*Unmark MLton Object*/
            /*pointer p  =  (pointer)(pc + GC_HEADER_SIZE+GC_HEADER_SIZE);
            GC_header* headerp = getHeaderp(p);
            GC_header header = *headerp;
            header = header & ~MARK_MASK;
            (*headerp) = header;*/

        }
        }

    }


    assert(pchunk == s->umheap.limit);

    s->cGCStats.numSweeps++;

    if(DEBUG_RTGC)
        fprintf(stderr,"%d] Finished one sweep cycle and freed %d chunks\n",PTHREAD_NUM,freed);

}

void performUMGC(GC_state s,
                 size_t ensureObjectChunksAvailable,
                 size_t ensureArrayChunksAvailable,
                 bool fullGC) {

    if (DEBUG_MEM) {
        fprintf(stderr, "PerformUMGC\n");
        dumpUMHeap(s);
    }


#ifdef PROFILE_UMGC
    long t_start = getCurrentTime();
    fprintf(stderr, "[GC] Free chunk: %d, Free array chunk: %d\n",
            s->fl_chunks,
            s->fl_chunks);
#endif
    
   // GC_stack currentStack = getStackCurrent(s);

// Mark phase
    //markStack(s,currentStack);
//Sweep phase
    sweep(s,ensureObjectChunksAvailable,ensureArrayChunksAvailable,fullGC);


#ifdef PROFILE_UMGC
    long t_end = getCurrentTime();
    fprintf(stderr, "[GC] Time: %ld, Free chunk: %d, Free array chunk: %d, "
            "ensureArrayChunk: %d\n",
            t_end - t_start,
            s->fl_chunks,
            s->fl_chunks,
            ensureArrayChunksAvailable);
#endif

}

static void performGC_helper (GC_state s,
                              size_t oldGenBytesRequested,
                              size_t nurseryBytesRequested,
                              bool forceMajor,
                              __attribute__ ((unused)) bool mayResize) {
    uintmax_t gcTime;
    //bool stackTopOk;
    //size_t stackBytesRequested;
    struct rusage ru_start;
    //size_t totalBytesRequested;

    if (s->gc_module == GC_NONE) {
        return;
    }

    enterGC (s);
    s->cumulativeStatistics.numGCs++;
    if (DEBUG or s->controls.messages) {
        size_t nurserySize = s->heap.size - ((size_t)(s->heap.nursery - s->heap.start));
        size_t nurseryUsed = (size_t)(s->frontier - s->heap.nursery);
        fprintf (stderr,
                 "[GC: Starting gc #%s; requesting %s nursery bytes and %s old-gen bytes,]\n",
                 uintmaxToCommaString(s->cumulativeStatistics.numGCs),
                 uintmaxToCommaString(nurseryBytesRequested),
                 uintmaxToCommaString(oldGenBytesRequested));
        fprintf (stderr,
                 "[GC:\theap at "FMTPTR" of size %s bytes (+ %s bytes card/cross map),]\n",
                (uintptr_t)(s->heap.start),
                uintmaxToCommaString(s->heap.size),
                uintmaxToCommaString(s->heap.withMapsSize - s->heap.size));
        fprintf (stderr,
                 "[GC:\twith old-gen of size %s bytes (%.1f%% of heap),]\n",
                 uintmaxToCommaString(s->heap.oldGenSize),
                 100.0 * ((double)(s->heap.oldGenSize) / (double)(s->heap.size)));
        fprintf (stderr,
                 "[GC:\tand nursery of size %s bytes (%.1f%% of heap),]\n",
                 uintmaxToCommaString(nurserySize),
                 100.0 * ((double)(nurserySize) / (double)(s->heap.size)));
        fprintf (stderr,
                 "[GC:\tand nursery using %s bytes (%.1f%% of heap, %.1f%% of nursery).]\n",
                 uintmaxToCommaString(nurseryUsed),
                 100.0 * ((double)(nurseryUsed) / (double)(s->heap.size)),
                 100.0 * ((double)(nurseryUsed) / (double)(nurserySize)));
    }


/*TODO: Assess invariant for GC check in chunked CMS GC*/
    //assert (invariantForGC (s));

    if (needGCTime (s))
        startTiming (&ru_start);

    //  minorGC (s);

    /*What stack? GC has no stack.*/
    //stackTopOk = invariantForMutatorStack (s);


#if 0
    stackBytesRequested =
    stackTopOk
    ? 0
    : sizeofStackWithHeader (s, sizeofStackGrowReserved (s, getStackCurrent (s)));
#endif
    //totalBytesRequested = oldGenBytesRequested + nurseryBytesRequested + stackBytesRequested;

    /*if (forceMajor
        or totalBytesRequested > s->heap.size - s->heap.oldGenSize) {
        performUMGC(s, 3000, 0, true);
    }*/

    performUMGC(s, 3000, 0, true);

    /*The chunked heap does not grow, so we dont need to reset heap */
    //setGCStateCurrentHeap (s, oldGenBytesRequested + stackBytesRequested,nurseryBytesRequested);



    //assert (hasHeapBytesFree (s, oldGenBytesRequested + stackBytesRequested, nurseryBytesRequested));
    //assert (s->fl_chunks > 3000);


#if 0
    unless (stackTopOk)
    growStackCurrent (s);
#endif

/*Setting current thread and stack not required for chunked CMS GC*/
    //setGCStateCurrentThreadAndStack (s);


    if (needGCTime (s)) {
        gcTime = stopTiming (&ru_start, &s->cumulativeStatistics.ru_gc);
        s->cumulativeStatistics.maxPauseTime =
                max (s->cumulativeStatistics.maxPauseTime, gcTime);
    } else
        gcTime = 0;  /* Assign gcTime to quell gcc warning. */
    if (DEBUG or s->controls.messages) {
        size_t nurserySize = s->heap.size - (size_t)(s->heap.nursery - s->heap.start);
        fprintf (stderr,
                 "[GC: Finished gc #%s; time %s ms,]\n",
                 uintmaxToCommaString(s->cumulativeStatistics.numGCs),
                 uintmaxToCommaString(gcTime));
        fprintf (stderr,
                 "[GC:\theap at "FMTPTR" of size %s bytes (+ %s bytes card/cross map),]\n",
                (uintptr_t)(s->heap.start),
                uintmaxToCommaString(s->heap.size),
                uintmaxToCommaString(s->heap.withMapsSize - s->heap.size));
        fprintf (stderr,
                 "[GC:\twith old-gen of size %s bytes (%.1f%% of heap),]\n",
                 uintmaxToCommaString(s->heap.oldGenSize),
                 100.0 * ((double)(s->heap.oldGenSize) / (double)(s->heap.size)));
        fprintf (stderr,
                 "[GC:\tand nursery of size %s bytes (%.1f%% of heap).]\n",
                 uintmaxToCommaString(nurserySize),
                 100.0 * ((double)(nurserySize) / (double)(s->heap.size)));
    }
    /* Send a GC signal. */
    if (s->signalsInfo.gcSignalHandled
        and s->signalHandlerThread[PTHREAD_NUM] != BOGUS_OBJPTR) {
        if (DEBUG_SIGNALS)
            fprintf (stderr, "GC Signal pending.\n");
        s->signalsInfo.gcSignalPending = TRUE;
        unless (s->signalsInfo.amInSignalHandler)
        s->signalsInfo.signalIsPending = TRUE;
    }
    if (DEBUG)
        displayGCState (s, stderr);

    //assert (hasHeapBytesFree (s, oldGenBytesRequested, nurseryBytesRequested));
    //assert (invariantForGC (s));
    assert(invariantForRTGC(s));



    leaveGC (s);
}


void ensureInvariantForMutator (GC_state s, bool force) {
    force = true;
    markStack(s,getStackCurrent(s));
}

bool ensureChunksAvailable(GC_state s)
{
  if(DEBUG_RTGC)
    fprintf(stderr,"%d]ensureChunksAvailable: FC = %d, Max Chunks = %d\n",PTHREAD_NUM,s->fl_chunks,s->maxChunksAvailable);
  
  if(s->fl_chunks > (size_t)((30 * s->maxChunksAvailable)/100 ))
      return true;
  else
      return false;
     
}


void GC_collect_real(GC_state s, size_t bytesRequested, bool force) {
  enter (s);
  /* When the mutator requests zero bytes, it may actually need as
   * much as GC_HEAP_LIMIT_SLOP.
   */
  if (0 == bytesRequested)
    bytesRequested = GC_HEAP_LIMIT_SLOP;
  getThreadCurrent(s)->bytesNeeded = bytesRequested;
  switchToSignalHandlerThreadIfNonAtomicAndSignalPending (s);
  ensureInvariantForMutator (s, force);

  assert (invariantForMutatorFrontier(s));
  assert (invariantForMutatorStack(s));
  leave (s);

  if (DEBUG_MEM) {
      fprintf(stderr, "GC_collect done\n");
  }
}

void GC_collect (GC_state s, size_t bytesRequested, bool force) {

    if(!force)
    {
        /*Try to get RTSync lock, if success means a) GC not Sweeping, b) no other thread is in GC_collect
         * if fails: dont execute section, continue with work
         * pthread_mutex_trylock returns 0 when mutex is acquired*/
        if(RTSYNC_TRYLOCK == 0)
        {
            /*Assert if GC has aquired RTSync lock and is running
             * This assert should never fail*/
            assert(!s->isGCRunning);
            /*Mark stack and the rest if the dirty but has been set by another mutator or enough chunks are available and 
             * own thread's rtsync has not been set i.e. it hasnt marked its stack already*/
            if((s->dirty || !ensureChunksAvailable(s)) && !s->rtSync[PTHREAD_NUM])
            {
                if(DEBUG_RTGC)
                    fprintf(stderr,"%d]GC_collect: Is dirty bit set? %s, Are enough Chunks Avialable? %s\n",PTHREAD_NUM,s->dirty?"Y":"N",ensureChunksAvailable(s)?"Y":"N");
                GC_collect_real(s,bytesRequested,true); /*marks stack*/
                s->dirty= true;
                s->rtSync[PTHREAD_NUM] = true;
                /*Check if all  other RT threads have set their values*/
                int i;
                for(i=0;i<MAXPRI;i++)
                {
                    if(i==1 || i == PTHREAD_NUM)
                        continue;
                    if(!s->rtSync[i])
                        break;
                }
                if(i == MAXPRI)
                {
                    RTSYNC_SIGNAL;
                }
                else
                {
                    fprintf(stderr,"%d] All Threads not synced\n",PTHREAD_NUM);
                } 
            }
            RTSYNC_UNLOCK;
        }
    }
    else
    {
        die("Why are you forcing GC_collect?\n");
    }

}

void ensureHasHeapBytesFree (GC_state s,
                        size_t oldGenBytesRequested,
                        size_t nurseryBytesRequested)
{
    assert (s->heap.nursery <= s->limitPlusSlop);
    assert (s->frontier <= s->limitPlusSlop);

    if (DEBUG) {
        displayHeap (s, &(s->heap), stderr);
    }

    if (not hasHeapBytesFree (s, oldGenBytesRequested, nurseryBytesRequested)) {
        markStack(s,getStackCurrent(s));
        //performGC (s, oldGenBytesRequested, nurseryBytesRequested, FALSE,TRUE);
        if (DEBUG) {
            fprintf (stderr,
                     "%d] Back after GCin and going to check assert. oldgen size=%d\n",
                     PTHREAD_NUM, s->heap.oldGenSize);
            displayHeap (s, &(s->heap), stderr);
            assert (s->stackBottom[PTHREAD_NUM] ==
                    getStackBottom (s, getStackCurrent (s)));
        }
    }
    assert (hasHeapBytesFree
            (s, oldGenBytesRequested, nurseryBytesRequested));
}


#undef LOCK
#undef UNLOCK

#pragma GCC diagnostic pop  // require GCC 4.6
