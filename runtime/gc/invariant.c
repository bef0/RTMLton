
/* Copyright (C) 2011-2012 Matthew Fluet.
 * Copyright (C) 1999-2007 Henry Cejtin, Matthew Fluet, Suresh
 *    Jagannathan, and Stephen Weeks.
 * Copyright (C) 1997-2000 NEC Research Institute.
 *
 * MLton is released under a BSD-style license.
 * See the file MLton-LICENSE for details.
 */

#if ASSERT

#if 0
void assertIsObjptrInFromSpace (GC_state s, objptr *opp) {
    return;
  assert (isObjptrInFromSpace (s, *opp));
  unless (isObjptrInFromSpace (s, *opp))
    die ("gc.c: assertIsObjptrInFromSpace "
         "opp = "FMTPTR"  "
         "*opp = "FMTOBJPTR"\n",
         (uintptr_t)opp, *opp);
  /* The following checks that intergenerational pointers have the
   * appropriate card marked.  Unfortunately, it doesn't work because
   * for stacks, the card containing the beginning of the stack is
   * marked, but any remaining cards aren't.
   */
  if (FALSE and s->mutatorMarksCards 
      and isPointerInOldGen (s, (pointer)opp) 
      and isObjptrInNursery (s, *opp)
      and not isCardMarked (s, (pointer)opp)) {
    displayGCState (s, stderr);
    die ("gc.c: intergenerational pointer from "FMTPTR" to "FMTOBJPTR" with unmarked card.\n",
         (uintptr_t)opp, *opp);
  }
}
#endif

/*Check if all chunks have been unmarked at end of sweep phase*/
bool invariantForRTGC(GC_state s)
{

/*The logic below needs to be re considered. There is no guarantee that during/after the sweep phase, the mutator is not running
 * and hence the invariant failed due to the presence of an unmarked chunk which was allocated AFTER the sweep phase and BEFORE this function is run*/

#if 0
    pointer pchunk;
    size_t step = sizeof(struct GC_UM_Chunk)+sizeof(Word32_t); /*account for 4 bytes of chunktype header*/
    pointer end = s->umheap.start + s->umheap.size - step;

    for (pchunk=s->umheap.start;
         pchunk < end;
         pchunk+=step)
    {
        if(((UM_Mem_Chunk)pchunk)->chunkType == UM_NORMAL_CHUNK)
        {
            GC_UM_Chunk pc = (GC_UM_Chunk)(pchunk+4); /*account for size of chunktype*/
            if (ISINUSE(pc->chunk_header) && ISUNMARKED(pc->chunk_header)){
                fprintf(stderr,"invariantforRTGC failed. Header is "FMTPTR"\n ",(uintptr_t)pc->chunk_header);
                return false;
            }
        }
        else if(((UM_Mem_Chunk)pchunk)->chunkType == UM_ARRAY_CHUNK)
        {

            GC_UM_Array_Chunk pc = (GC_UM_Array_Chunk)(pchunk + 4); /*account for size of chunktype*/
            assert (pc->array_chunk_magic == UM_ARRAY_SENTINEL);
            if (ISINUSE(pc->array_chunk_header) && ISUNMARKED(pc->array_chunk_header)) {
                fprintf(stderr,"invariantforRTGC failed. Header is "FMTPTR"\n ",(uintptr_t)pc->array_chunk_header);

                return false;
            }

        }

    }
#endif
    
    return true;
}

bool invariantForGC (GC_state s) {
  if (DEBUG)
    fprintf (stderr, "%d] invariantForGC\n", PTHREAD_NUM);
  /* Frame layouts */
  for (unsigned int i = 0; i < s->frameLayoutsLength; ++i) {
    GC_frameLayout layout;

    layout = &(s->frameLayouts[i]);
    if (layout->size > 0) {
      GC_frameOffsets offsets;

      assert (layout->size <= s->maxFrameSize);
      offsets = layout->offsets;
      for (unsigned int j = 0; j < offsets[0]; ++j)
        assert (offsets[j + 1] < layout->size);
    }
  }

  /* Adding asserts for global heap
   * TODO: maybe for IntInfheap too?
   */

  assert (isAligned (s->globalHeap.size, s->sysvals.pageSize));
    unless (0 == s->globalHeap.size) {
    assert (s->frontier <= s->limitPlusSlop);
    assert (s->limit == s->limitPlusSlop - GC_HEAP_LIMIT_SLOP);
  }

  /* Current thread. */

  GC_thread thread = getThreadCurrent(s);
  assert (thread->stackDepth <= thread->stackSizeInChunks);

  if (DEBUG)
    fprintf (stderr, "   invariantForGC passed\n");

  return TRUE;
}
#endif

/*Checks if bytes needed is beyond the limit of heap. but has no relevance in RTMLton because we dont do bump pointer allocation*/
bool invariantForMutatorFrontier (GC_state s) {
    return true;
}

/* Do we really need this once stacks are chunked?
 * TODO no, but we should probably think about what invariants would apply here
 */
bool invariantForMutatorStack (GC_state s) {
	return true;
}

#if ASSERT
bool invariantForMutator (GC_state s, bool frontier, bool stack) {
 
/* Obsolete. Check for CLEANUP*/

 return TRUE;
 
 if (DEBUG)
    displayGCState (s, stderr);
  if (frontier)
    assert (invariantForMutatorFrontier(s));
  if (stack)
    assert (invariantForMutatorStack(s));
  assert (invariantForGC (s));
  
   //return TRUE;
}
#endif
