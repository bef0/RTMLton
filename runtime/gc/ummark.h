#if (defined (MLTON_GC_INTERNAL_FUNCS))
GC_objectTypeTag getObjectType(GC_state s, pointer p);
void umDfsMarkObjects(GC_state s, objptr *opp, GC_markMode m);
void umDfsMarkObjectsToWorklist(GC_state s, objptr *opp, GC_markMode m);
void umDfsMarkObjectsUnMark(GC_state s, objptr *opp);
void umDfsMarkObjectsMark(GC_state s, objptr *opp);
void umDfsMarkObjectsMarkToWL(GC_state s, objptr *opp);
void markUMArrayChunks(GC_state s, GC_UM_Array_Chunk p, GC_markMode m);

void addToWorklist(GC_state s,objptr *opp);

static void markWorklist(GC_state s);

static inline bool isEmptyWorklist(GC_state s);
static inline bool isObjectShaded(GC_state s, objptr *opp);
static inline bool isObjectMarked(GC_state s, objptr *opp);
static inline bool isWorklistShaded(GC_state s);

static void markChunk(pointer p, GC_objectTypeTag tag,GC_markMode m, GC_state s,uint16_t numObjptrs);
static void umShadeObject(GC_state s,objptr *opp);
static bool isChunkMarked(pointer p, GC_objectTypeTag tag);
static bool isContainerChunkMarkedByMode (pointer p, GC_markMode m,GC_objectTypeTag tag);


#endif /* (defined (MLTON_GC_INTERNAL_FUNCS)) */
