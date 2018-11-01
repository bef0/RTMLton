void umDfsMarkObjectsUnMark(GC_state s, objptr *opp) {
    umDfsMarkObjects(s, opp, UNMARK_MODE);
}

void umDfsMarkObjectsMark(GC_state s, objptr *opp) {
    umDfsMarkObjects(s, opp, MARK_MODE);
}

static
void umShadeObject(GC_state s,objptr *opp){
    
    pointer p = objptrToPointer(*opp, s->heap.start);
    GC_header* headerp = getHeaderp(p);
    GC_header header = *headerp;
    uint16_t bytesNonObjptrs=0;
    uint16_t numObjptrs =0;
    GC_objectTypeTag tag = ERROR_TAG;
    splitHeader(s, header, &tag, NULL, &bytesNonObjptrs, &numObjptrs);
    
    markChunk(p,tag,GREY_MODE,s,numObjptrs);


}

void getObjectType(GC_state s, objptr *opp) {
    pointer p = objptrToPointer(*opp, s->heap.start);
    GC_header* headerp = getHeaderp(p);
    GC_header header = *headerp;
    uint16_t bytesNonObjptrs;
    uint16_t numObjptrs;
    GC_objectTypeTag tag;
    splitHeader(s, header, &tag, NULL, &bytesNonObjptrs, &numObjptrs);

    if (DEBUG_DFS_MARK) {
        switch (tag) {
        case NORMAL_TAG:
            fprintf(stderr, "NORMAL!\n");
            if (p >= s->umheap.start &&
                p < s->umheap.start + s->umheap.size) {
                fprintf(stderr, "  ON UM HEAP!\n");
            } else {
                fprintf(stderr, "  NOT ON UM HEAP\n");
            }
            break;
        case WEAK_TAG:
            fprintf(stderr, "WEAK!\n");
            break;
        case ARRAY_TAG:
            fprintf(stderr, "ARRAY!\n");
            break;
        case STACK_TAG:
            fprintf(stderr, "STACK\n");
            break;
        default:
            die("getObjetctType: swith: Shouldn't be here!\n");
        }
    }
}

static
void markChunk(pointer p, GC_objectTypeTag tag,GC_markMode m,GC_state s,uint16_t numObjptrs)
{
  if (tag == NORMAL_TAG) {

        if (p >= s->umheap.start &&
            p < (s->umheap.start + s->umheap.size)) /*if object is on UM heap */
        {
            GC_UM_Chunk pchunk = (GC_UM_Chunk)(p - GC_NORMAL_HEADER_SIZE); /*Get the chunk holding the mlton object*/
            if (m == MARK_MODE) {
                pchunk->chunk_header |= UM_CHUNK_MARK_MASK;  /*mark chunk header*/
            }
            else if(m == GREY_MODE)
            {
                /*shade chunk only if not shaded*/
                if(!(pchunk->chunk_header & UM_CHUNK_GREY_MASK) && !(pchunk->chunk_header & UM_CHUNK_MARK_MASK)) 
                    pchunk->chunk_header |= UM_CHUNK_GREY_MASK;  /*shade chunk header*/
            }
            else {
                pchunk->chunk_header &= ~UM_CHUNK_MARK_MASK; /*leave chunk header as it is*/
            }

            if (DEBUG_DFS_MARK) {
                fprintf(stderr, "umDfsMarkObjects: chunk: "FMTPTR", sentinel: %d,"
                        " mark_mode: %d, objptrs: %d\n", (uintptr_t)pchunk,
                        pchunk->sentinel,m, numObjptrs);
            }


            /*Mark linked chunk if there is a linked chunk*/
            if (NULL != pchunk->next_chunk) {
                if (m == MARK_MODE) {
                    pchunk->next_chunk->chunk_header |= UM_CHUNK_MARK_MASK;
                }
                else if(m == GREY_MODE)
                {
                    /*shade chunk only if not shaded*/
                    if(!(pchunk->next_chunk->chunk_header & UM_CHUNK_GREY_MASK) && !(pchunk->next_chunk->chunk_header & UM_CHUNK_MARK_MASK)) 
                        pchunk->next_chunk->chunk_header |= UM_CHUNK_GREY_MASK;  /*shade chunk header*/
                }
                else {
                    pchunk->next_chunk->chunk_header &= ~UM_CHUNK_MARK_MASK;
                }
            }
        }
    }

    if (tag == ARRAY_TAG &&
        p >= s->umheap.start &&
        p < s->umheap.start + s->umheap.size) {
        GC_UM_Array_Chunk fst_leaf = (GC_UM_Array_Chunk)
            (p - GC_HEADER_SIZE - GC_HEADER_SIZE);
        if (DEBUG_DFS_MARK) {
            fprintf(stderr, "umDfsMarkObjects: marking array: %p, markmode: %d, "
                    "magic: %d, length: %d\n", (void *)fst_leaf, m,
                    fst_leaf->array_chunk_magic, fst_leaf->array_chunk_length);
        }

        if (fst_leaf->array_num_chunks > 1 &&
            fst_leaf->array_chunk_length > 0) {
            GC_UM_Array_Chunk root = fst_leaf->root;
//            size_t length = root->array_chunk_length;
//
//            size_t i, j;
//            size_t elem_size = bytesNonObjptrs + numObjptrs * OBJPTR_SIZE;
//            for (i=0; i<length; i++) {
//                pointer pobj = UM_Array_offset(s, p, i, elem_size, 0) +
//                    bytesNonObjptrs;
//
//                for (j=0; j<numObjptrs; j++) {
//                    if (m == MARK_MODE)
//                        foreachObjptrInObject(s, pobj, umDfsMarkObjectsMark, true);
//                    else
//                        foreachObjptrInObject(s, pobj, umDfsMarkObjectsUnMark, true);
//                    pobj += OBJPTR_SIZE;
//                }
//            }
            markUMArrayChunks(s, root, m);
        } else
            markUMArrayChunks(s, fst_leaf, m);
    }


}


static
bool isChunkMarked(pointer p, GC_objectTypeTag tag)
{
    /*Treat shaded objects as unmarked*/
    if(tag == NORMAL_TAG)
    {
            GC_UM_Chunk pc = (GC_UM_Chunk)(p - GC_NORMAL_HEADER_SIZE); /*Get the chunk holding the mlton object*/
            if ((pc->chunk_header & UM_CHUNK_IN_USE) && (pc->chunk_header & UM_CHUNK_MARK_MASK))
            {
                return true;
            }
            else
                return false;
    }
    else if(tag == ARRAY_TAG)
    {
            GC_UM_Array_Chunk pc = (GC_UM_Array_Chunk) (p - GC_HEADER_SIZE - GC_HEADER_SIZE);
            if ((pc->array_chunk_header & UM_CHUNK_IN_USE) && (pc->array_chunk_header & UM_CHUNK_MARK_MASK))
                return true;
            else
                return false;
    }
    else
    {
        return false;
        //die("Why are you checking a %s object chunk??\n",(tag == STACK_TAG)?"Stack":"Weak");
    }

}

static
bool isContainerChunkMarkedByMode (pointer p, GC_markMode m,GC_objectTypeTag tag) {
  switch (m) {
  case MARK_MODE:
    return isChunkMarked (p,tag);
  case UNMARK_MODE:
    return not isChunkMarked (p,tag);
  default:
    die ("bad mark mode %u", m);
  }
}


//TODO: handle marking the mlton objects if packing more than one object in a chunk
/* Tricolor abstraction at the chunk level. Binary marking for the MLton objects remain same. 
 * Implementation: 
 * 1. If function is in marking mode, mark current chunk grey.
 * 2. mark children grey
 * 3. Mark Chunk Black
 * 4. Continue marking in dfs
 * NOTE: Function shades object grey only if it isn't already marked grey / black. This makes sure that the shading is to a darker shade and never to lighter shade (unless you are in unmark mode) 
 * */

void umDfsMarkObjects(GC_state s, objptr *opp, GC_markMode m) {
    pointer p = objptrToPointer(*opp, s->heap.start);
    if (DEBUG_DFS_MARK)
        fprintf(stderr, "original obj: 0x%x, obj: 0x%x\n",
                (uintptr_t)*opp, (uintptr_t)p);
    GC_header* headerp = getHeaderp(p);
    GC_header header = *headerp;
    uint16_t bytesNonObjptrs = 0;
    uint16_t numObjptrs =0;
    GC_objectTypeTag tag = ERROR_TAG;
    splitHeader(s, header, &tag, NULL, &bytesNonObjptrs, &numObjptrs);

//    if (DEBUG_DFS_MARK)
    getObjectType(s, opp);



    /* Using MLton object to track if containing chunk marked */
    if (isContainerChunkMarkedByMode(p, m,tag)) {
        if (DEBUG_DFS_MARK)
            fprintf(stderr, FMTPTR"marked by mark_mode: %d, RETURN\n",
                    (uintptr_t)p,
                    (m == MARK_MODE));
        return;
    }

    /*ensure the MLton object isn't marked. It should be unmarked always*/
    assert(!isPointerMarkedByMode(p,MARK_MODE));
   
    /*mark children*/
   if(m == MARK_MODE)
   {
       markChunk(p,tag,GREY_MODE,s,numObjptrs);
       if(numObjptrs > 0)
            foreachObjptrInObject(s, p, umShadeObject, false);
   } 
    

    /*mark object*/
   /*
    if (m == MARK_MODE) {
        if (DEBUG_DFS_MARK)
            fprintf(stderr, FMTPTR" mark b pheader: %x, header: %x\n",
                    (uintptr_t)p, *(getHeaderp(p)), header);

        header = header | MARK_MASK;
        *headerp = header;

        if (DEBUG_DFS_MARK)
            fprintf(stderr, FMTPTR" mark a pheader: %x, header: %x\n",
                    (uintptr_t)p, *(getHeaderp(p)), header);
    } else {
        if (DEBUG_DFS_MARK)
            fprintf(stderr, FMTPTR" unmark b pheader: %x, header: %x\n",
                    (uintptr_t)p, *(getHeaderp(p)), header);

        header = header & ~MARK_MASK;
        (*headerp) = header;

        if (DEBUG_DFS_MARK)
            fprintf(stderr, FMTPTR" unmark a pheader: %x, header: %x\n",
                    (uintptr_t)p, *(getHeaderp(p)), header);
    }
    */

    /*Mark chunk*/
   
    markChunk(p,tag,m,s,numObjptrs);


    /*if there are references in the object fields, mark them*/
    if (numObjptrs > 0) {
        if (m == MARK_MODE)
            foreachObjptrInObject(s, p, umDfsMarkObjectsMark, false);
        else
            foreachObjptrInObject(s, p, umDfsMarkObjectsUnMark, false);
    }
}



void markUMArrayChunks(GC_state s, GC_UM_Array_Chunk p, GC_markMode m) {
    if (DEBUG_DFS_MARK)
        fprintf(stderr, "markUMArrayChunks: %p: marking array markmode: %d, "
                "type: %d\n", (void *)p, m,
                p->array_chunk_type);

    if (m == MARK_MODE)
        p->array_chunk_header |= UM_CHUNK_MARK_MASK;
    else if (m == GREY_MODE)
    {       /*shade chunk only if not shaded*/
        if(!(p->array_chunk_header & UM_CHUNK_GREY_MASK) && !(p->array_chunk_header & UM_CHUNK_MARK_MASK))
            p->array_chunk_header |= UM_CHUNK_GREY_MASK;
    }
    else
        p->array_chunk_header &= ~UM_CHUNK_MARK_MASK;

    if (p->array_chunk_type == UM_CHUNK_ARRAY_INTERNAL) {
        int i = 0;
        for (i=0; i<UM_CHUNK_ARRAY_INTERNAL_POINTERS; i++) {
            GC_UM_Array_Chunk pcur = p->ml_array_payload.um_array_pointers[i];
            if (!pcur)
                break;
            markUMArrayChunks(s, pcur, m);
        }
    }
}