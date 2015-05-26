void umDfsMarkObjectsUnMark(GC_state s, objptr *opp) {
    umDfsMarkObjects(s, opp, UNMARK_MODE);
}

void umDfsMarkObjectsMark(GC_state s, objptr *opp) {
    umDfsMarkObjects(s, opp, MARK_MODE);
}

void getObjectType(GC_state s, objptr *opp) {
    pointer p = objptrToPointer(*opp, s->heap.start);
    GC_header* headerp = getHeaderp(p);
    GC_header header = *headerp;
    uint16_t bytesNonObjptrs;
    uint16_t numObjptrs;
    GC_objectTypeTag tag;
    splitHeader(s, header, &tag, NULL, &bytesNonObjptrs, &numObjptrs);
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

void umDfsMarkObjects(GC_state s, objptr *opp, GC_markMode m) {
    pointer p = objptrToPointer(*opp, s->heap.start);
    if (DEBUG_MEM)
        fprintf(stderr, "original obj: 0x%x, obj: 0x%x\n",
                (uintptr_t)*opp, (uintptr_t)p);
    GC_header* headerp = getHeaderp(p);
    GC_header header = *headerp;
    uint16_t bytesNonObjptrs;
    uint16_t numObjptrs;
    GC_objectTypeTag tag;
    splitHeader(s, header, &tag, NULL, &bytesNonObjptrs, &numObjptrs);

    if (DEBUG_MEM)
        getObjectType(s, opp);

    /* Using MLton's header to track if it's marked */
    if (isPointerMarkedByMode(p, m)) {
        if (DEBUG_MEM)
            fprintf(stderr, FMTPTR"marked by mark_mode: %d, RETURN\n",
                    (uintptr_t)p,
                    (m == MARK_MODE));
        return;
    }

    if (m == MARK_MODE) {
        if (DEBUG_MEM)
            fprintf(stderr, FMTPTR" mark b pheader: %x, header: %x\n",
                    (uintptr_t)p, *(getHeaderp(p)), header);

        header = header | MARK_MASK;
        *headerp = header;

        if (DEBUG_MEM)
            fprintf(stderr, FMTPTR" mark a pheader: %x, header: %x\n",
                    (uintptr_t)p, *(getHeaderp(p)), header);
    } else {
        if (DEBUG_MEM)
            fprintf(stderr, FMTPTR" unmark b pheader: %x, header: %x\n",
                    (uintptr_t)p, *(getHeaderp(p)), header);

        header = header & ~MARK_MASK;
        (*headerp) = header;

        if (DEBUG_MEM)
            fprintf(stderr, FMTPTR" unmark a pheader: %x, header: %x\n",
                    (uintptr_t)p, *(getHeaderp(p)), header);
    }

    if (tag == NORMAL_TAG) {
        if (p >= s->umheap.start &&
            p < (s->umheap.start + s->umheap.size)) {
            GC_UM_Chunk pchunk = (GC_UM_Chunk)(p - GC_NORMAL_HEADER_SIZE);
            if (m == MARK_MODE) {
                pchunk->chunk_header |= UM_CHUNK_HEADER_MASK;
            } else {
                pchunk->chunk_header &= ~UM_CHUNK_HEADER_MASK;
            }

            if (DEBUG_MEM) {
                fprintf(stderr, "umDfsMarkObjects: chunk: "FMTPTR", sentinel: %d,"
                        " mark_mode: %d\n", (uintptr_t)pchunk, pchunk->sentinel,
                        (m == MARK_MODE));
            }

            if (NULL != pchunk->next_chunk) {
                if (m == MARK_MODE) {
                    pchunk->next_chunk->chunk_header |= UM_CHUNK_HEADER_MASK;
                } else {
                    pchunk->next_chunk->chunk_header &= ~UM_CHUNK_HEADER_MASK;
                }
            }
        }
    }

    if (tag == ARRAY_TAG) {
        if (DEBUG_MEM) {
            fprintf(stderr, "umDfsMarkObjects: marking array markmode: %d\n", m);
        }
        GC_UM_Array_Chunk root = ((GC_UM_Array_Chunk)(p - 8))->next_chunk;
        size_t length = root->array_chunk_length;

        size_t i, j;
        size_t elem_size = bytesNonObjptrs + numObjptrs * OBJPTR_SIZE;
        for (i=0; i<length; i++) {
            pointer pobj = UM_Array_offset(s, p, i, elem_size, 0) + bytesNonObjptrs;

            for (j=0; j<numObjptrs; j++) {
                if (m == MARK_MODE)
                    foreachObjptrInObject(s, pobj, umDfsMarkObjectsMark, true);
                else
                    foreachObjptrInObject(s, pobj, umDfsMarkObjectsUnMark, true);
            }
        }

        markUMArrayChunks(s, root, m);
    } else {
        if (numObjptrs > 0) {
            if (m == MARK_MODE)
                foreachObjptrInObject(s, *p, umDfsMarkObjectsMark, true);
            else
                foreachObjptrInObject(s, *p, umDfsMarkObjectsUnMark, true);
        }
    }
}

void markUMArrayChunks(GC_state s, GC_UM_Array_Chunk p, GC_markMode m) {
    if (DEBUG_MEM) {
        fprintf(stderr, "markUMArrayChunks: marking array markmode: %d\n", m);
    }
    if (m == MARK_MODE)
        p->array_chunk_header |= UM_CHUNK_HEADER_MASK;
    else
        p->array_chunk_header &= ~UM_CHUNK_HEADER_MASK;

    if (p->array_chunk_type == UM_CHUNK_ARRAY_INTERNAL) {
        int i = 0;
        GC_UM_Array_Chunk pcur = p->ml_array_payload.um_array_pointers[i];
        while (pcur) {
            markUMArrayChunks(s, pcur, m);
            i++;
        }
    }
}
