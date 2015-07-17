#include "realtime_thread.h"

// pri 0 is non RT threads handled by "main"
// pri 1 is reserved for GC
// so we subtract 2 when allocating/accessing tracking structures
#define RESPRI  2
#define MAXPRI 10 /* 0 .. 10 */

static pthread_mutex_t thread_queue_lock;
static int thread_queue[MAXPRI-2];

volatile bool RTThreads_beginInit = FALSE;
volatile int32_t RTThreads_initialized= 0;
volatile int32_t Proc_criticalCount;
volatile int32_t Proc_criticalTicket;

pthread_mutex_t AllocatedThreadLock;

int RTThread_addThreadToQueue(GC_thread t, int priority) {
	if (priority < RESPRI || priority > MAXPRI) return -1;
	thread_queue[priority - RESPRI] = 1;
	MYASSERT(1, ==, 1);
	return 0;
}

void RTThread_waitForInitialization (GC_state s) {
  int32_t unused;

  while (!RTThreads_beginInit) { }

  unused = __sync_fetch_and_add (&RTThreads_initialized, 1);

  while (!RTThread_isInitialized(s)) { }
  initProfiling (s);
}


bool RTThread_isInitialized (GC_state s) {
  return RTThreads_initialized == (MAXPRI-RESPRI);
}

void realtimeThreadInit(struct GC_state *state) {
	int rv = 0;

	rv = pthread_mutex_init(&thread_queue_lock, NULL);
	assert(rv == 0);
	memset(&thread_queue, 0, sizeof(int)*(MAXPRI-2));

        pthread_t *realtimeThreads =
                malloc(MAXPRI-RESPRI * sizeof(pthread_t));
        assert(realtimeThreads != NULL);

        state->realtimeThreadConts =
                malloc((MAXPRI-RESPRI) * sizeof(struct cont));

	//Initializing the locks for each realtime thread created,
	state->realtimeThreadLocks= malloc((MAXPRI-RESPRI) * sizeof(pthread_mutex_t));
        int tNum;
        for (tNum = 0; tNum < (MAXPRI-RESPRI); tNum++) {
	    fprintf(stderr, "spawning thread %d\n", tNum);

            struct realtimeRunnerParameters* params =
                malloc(sizeof(struct realtimeRunnerParameters));
            params->tNum = tNum;
            params->state = state;

printf("1\n");
            state->realtimeThreadConts[tNum].nextChunk = NULL;
printf("2\n");

            if (pthread_create(&realtimeThreads[tNum], NULL, &realtimeRunner,
                        (void*)params)) {
                fprintf(stderr, "pthread_create failed: %s\n", strerror (errno));
                exit (1);
            }
        }

        state->realtimeThreads = realtimeThreads;

        state->realtimeThreadAllocated =
            malloc((MAXPRI-RESPRI) * sizeof(bool));
        for (tNum = 0; tNum < (MAXPRI-RESPRI); tNum++) {
            state->realtimeThreadAllocated[tNum] = false;
        }
}

void* realtimeRunner(void* paramsPtr) {

    struct realtimeRunnerParameters *params = paramsPtr;

    while (1) {
        // Trampoline
        int tNum = params->tNum;
        printf("\t%x] realtimeRunner[%d] running.\n", pthread_self(), tNum);

        // copy the cont struct to this local variable
        struct GC_state *state = params->state;
        printf("state = %x\n", state);

        // TODO lock lock[tNum]
	//Acquiring lock associated with pThread from GC state
	pthread_mutex_lock(&state->realtimeThreadLocks[tNum]);


        printf("Acquired thread lock\n");

        struct cont* realtimeThreadConts = state->realtimeThreadConts;
        printf("realtimeThreadConts = %x\n", realtimeThreadConts);

        printf("cont.nextChunk: if next line doesn't print, cont is null\n");
        struct cont cont = realtimeThreadConts[tNum];

        printf("cont.nextChunk = %x\n", cont.nextChunk);
        if (cont.nextChunk != NULL) {
            printf("\t%x] realtimeRunner trampolining.\n", pthread_self());
            cont=(*(struct cont(*)(void))cont.nextChunk)();
            cont=(*(struct cont(*)(void))cont.nextChunk)();
            cont=(*(struct cont(*)(void))cont.nextChunk)();
            cont=(*(struct cont(*)(void))cont.nextChunk)();
            cont=(*(struct cont(*)(void))cont.nextChunk)();
            cont=(*(struct cont(*)(void))cont.nextChunk)();
            cont=(*(struct cont(*)(void))cont.nextChunk)();
            cont=(*(struct cont(*)(void))cont.nextChunk)();

            // copy local variable back to gcstate
            params->state->realtimeThreadConts[tNum] = cont;
	
        } else {
            printf("\t%x] realtimeRunner has nothing to trampoline.\n", pthread_self());
        }

        // TODO unlock lock[tNum]
	pthread_mutex_unlock(&state->realtimeThreadLocks[tNum]);
        printf("Released thread lock \n");
    }
    return NULL;
}

int allocate_pthread(struct GC_state *state, struct cont *cont) {

	//grab a lock for accessing the allocated threads structure

	pthread_mutex_lock(&AllocatedThreadLock);
	
	//Loop through allocate thread structure to find free spot
	for(int i = 0 ; i < (MAXPRI-RESPRI) ; i++)
	{
		if(!state->realtimeThreadAllocated[i])
		  {	
			pthread_mutex_lock(&state->realtimeThreadLocks[i]);
			state->realtimeThreadConts[i] = *cont;
			pthread_mutex_unlock(&state->realtimeThreadLocks[i]);
			
			//Mark thread as allocated
			state->realtimeThreadAllocated[i] = true;
			pthread_mutex_unlock(&AllocatedThreadLock);
			return i;
		  }	
	}
	
	pthread_mutex_unlock(&AllocatedThreadLock);
	return -1;

}
