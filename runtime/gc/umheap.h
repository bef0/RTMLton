#if (defined (MLTON_GC_INTERNAL_TYPES))

#define UM_HEADER_TYPE__(z) uint ## z ## _t
#define UM_HEADER_TYPE_(z) UM_HEADER_TYPE__(z)
#define UM_HEADER_TYPE UM_HEADER_TYPE_(GC_MODEL_HEADER_SIZE)

typedef UM_HEADER_TYPE UM_header;

typedef union GC_UM_Chunktype {
   struct GC_UM_Chunk* umChunk;
   struct GC_UM_Array_Chunk* umArrayChunk;
} GC_UM_Chunktype;


typedef objptr** GC_worklist;

typedef struct UM_Mem_Chunk {
   UM_header chunkType;
   struct UM_Mem_Chunk* next_chunk;
} *UM_Mem_Chunk;


typedef struct GC_UM_Chunk {
    unsigned char ml_object[UM_CHUNK_PAYLOAD_SIZE + UM_CHUNK_PAYLOAD_SAFE_REGION]; /* 154 + 16 */
    UM_header chunk_header;          /* +0 */
    size_t sentinel;                 /* +4 */
    GC_returnAddress ra;             /* +8 */
    GC_returnAddress handler;        /* +12 */
    GC_returnAddress link;           /* +16 */
    struct GC_UM_Chunk* next_chunk;  /* +20 */
    struct GC_UM_Chunk* prev_chunk;
} *GC_UM_Chunk;

typedef struct GC_UM_heap {
    pointer start;
    pointer end;
    pointer limit;
    size_t size;
    UM_Mem_Chunk fl_head;
    UM_Mem_Chunk fl_tail;
    UM_Mem_Chunk sl_head;
    UM_Mem_Chunk sl_tail;
} *GC_UM_heap;

struct GC_UM_Array_Chunk;

typedef union GC_UM_Array_Payload {
    pointer um_array_pointers[UM_CHUNK_ARRAY_INTERNAL_POINTERS]; //  38 - um_constants.h
    unsigned char ml_object[UM_CHUNK_ARRAY_PAYLOAD_SIZE];                          // 152 - um_constants.h
} GC_UM_Array_Payload;

/* The array chunk. It still has a portion of ml header... for object type
   recognition. GC_arrayAllocate now returns p = &ml_array_payload, so that
   p - 4 returns the header of the array
*/
typedef struct GC_UM_Array_Chunk {
    Word32_t array_chunk_length;            /* Array Length                     */
    Word32_t array_chunk_ml_header;         /* MLton's array header             */
    GC_UM_Array_Payload ml_array_payload;   /* Payload or internal pointer      */
    UM_header array_chunk_header;           /* For Mark / Sweep                 */
    Word32_t array_chunk_type;              /* Internal or Leaf                 */
    size_t array_height;                    /* Height of the tree (subtree)     */
    size_t num_els_per_chunk;               /* Number of objects in leaf        */
    size_t num_els;                         /* Array length                     */
    size_t el_size;                         /* Object size of each element      */
    Word32_t array_chunk_magic;             /* A magic value for debug          */
    struct GC_UM_Array_Chunk* parent;       /* Pointer to parent chunk for iter */
    struct GC_UM_Array_Chunk* next_chunk;   /* For free list maintainance       */
    struct GC_UM_Array_Chunk* root;         /* For header to connect root       */
} *GC_UM_Array_Chunk;

/*typedef struct GC_UM_Array_heap {
    pointer start;
    pointer end;
    size_t size;
    UM_Mem_Chunk fl_head;
} *GC_UM_Array_heap;*/
#endif /* MLTON_GC_INTERNAL_TYPES */

#if (defined (MLTON_GC_INTERNAL_FUNCS))
static void insertChunkToFL(GC_state s, GC_UM_heap h, pointer c);
static void addSweepListToFL(GC_state s, GC_UM_heap h);
static inline void insertChunktoSubList(GC_state s, GC_UM_heap h, pointer c);
static void initUMHeap(GC_state s, GC_UM_heap h);
//static void initUMArrayHeap(GC_state s, GC_UM_heap h);
GC_UM_Array_Chunk insertArrayFreeChunk(GC_state s, GC_UM_heap h, pointer c);
GC_UM_Chunk allocateChunks(GC_state s, GC_UM_heap h,size_t numChunks,UM_header chunkType);
bool createUMHeap(GC_state s, GC_UM_heap h, size_t diredSize, size_t minSize);
static GC_UM_Chunk allocNextChunk(GC_state s, GC_UM_heap h, UM_header chunkType);

GC_UM_Array_Chunk allocNextArrayChunk(GC_state s, GC_UM_heap h);
GC_UM_Array_Chunk allocateArrayChunks(GC_state s, GC_UM_heap h, size_t numChunks);

#endif
