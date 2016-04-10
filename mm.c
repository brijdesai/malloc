/*
Name: Brij Desai   - 201401119
      Pinal Baldha - 201401005
       
WRITEUP for this code is also added at the end of this code.
*/

/* 
 * Simple, 32-bit and 64-bit clean allocator based on an implicit free list,
 * first fit placement, and boundary tag coalescing, as described in the
 * CS:APP2e text.  Blocks are aligned to double-word boundaries.  This
 * yields 8-byte aligned blocks on a 32-bit processor, and 16-byte aligned
 * blocks on a 64-bit processor.  However, 16-byte alignment is stricter
 * than necessary; the assignment only requires 8-byte alignment.  The
 * minimum block size is four words.
 *
 * This allocator uses the size of a pointer, e.g., sizeof(void *), to
 * define the size of a word.  This allocator also uses the standard
 * type uintptr_t to define unsigned integers that are the same size
 * as a pointer, i.e., sizeof(uintptr_t) == sizeof(void *).
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "memlib.h"
#include "mm.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
/* Team name */
"Pacific",
/* First member's full name */
"Brij Desai",
/* First member's email address */
"201401119@daiict.ac.in",
/* Second member's full name (leave blank if none) */
"Pinal Baldha",
/* Second member's email address (leave blank if none) */
"201401005@daiict.ac.in", };

/* Basic constants and macros: */
#define WSIZE      sizeof(void *) /* Word and header/footer size (bytes) */
#define DSIZE      (2 * WSIZE)  /* Doubleword size (bytes) */
#define CHUNKSIZE  (1 << 12)  /* Extend heap by this amount (bytes) */

#define MAX(x, y)  ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word. */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p. */
#define GET(p)       (*(uintptr_t *)(p))
#define PUT(p, val)  (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p. */
#define GET_SIZE(p)   (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p)  (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer. */
#define HDRP(bp)  ((char *)(bp) - WSIZE)
#define FTRP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks. */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Address of next free block will be stored at (address of payload + 4 bytes) */
/* Address of prev free block will be stored at (address of payload) */
#define GET_NEXT_FREEP(bp) (GET(bp + WSIZE))        
#define GET_PREV_FREEP(bp) (GET(bp))                
#define SET_NEXT_FREEP(bp,next) (GET_NEXT_FREEP(bp) = (uintptr_t)next)
#define SET_PREV_FREEP(bp,prev) (GET_PREV_FREEP(bp) = (uintptr_t)prev)

/* Global variables: */
static char *heap_listp; /* Pointer to first block */

/* Here we use explicit free list (sort of doubly linked list) to find free block quickly. 
 freelist_head is a pointer pointing to the first free block of the free list.
 */
static char *freelist_head;

/* Function prototypes for internal helper routines: */
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
//static void *next_fit(size_t asize);
//static void *best_fit(size_t asize);
static void place(void *bp, size_t asize);

/* Function prototypes for heap consistency checker routines: */
static void checkblock(void *bp);
static void checkheap(bool verbose);
static void printblock(void *bp);

/* Inserts a free block in free list */
static void insert_in_freelist(void *bp);
/* Removes a block from free list */
static void remove_from_freelist(void *bp);
/* 
 * Requires:
 *   None.
 *
 * Effects:
 *   Initialize the memory manager.  Returns 0 if the memory manager was
 *   successfully initialized and -1 otherwise.
 */
int mm_init(void)
{

  /* Create the initial empty heap. */
  if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) -1)
    return (-1);
  PUT(heap_listp, 0); /* Alignment padding */
  PUT(heap_listp + (1 * WSIZE), PACK (DSIZE, 1)); /* Prologue header */
  PUT(heap_listp + (2 * WSIZE), PACK (DSIZE, 1)); /* Prologue footer */
  PUT(heap_listp + (3 * WSIZE), PACK (0, 1)); /* Epilogue header */
  heap_listp += (2 * WSIZE);
  freelist_head = heap_listp; /*Initialize free list pointer to heap_listp */
  /* Extend the empty heap with a free block of CHUNKSIZE bytes. */
  if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
    return (-1);
  return (0);
}

/* 
 * Requires:
 *   None.
 *
 * Effects:
 *   Allocate a block with at least "size" bytes of payload, unless "size" is
 *   zero.  Returns the address of this block if the allocation was successful
 *   and NULL otherwise.
 */
void *
mm_malloc(size_t size)
{
  size_t asize; /* Adjusted block size */
  size_t extendsize; /* Amount to extend heap if no fit */
  void *bp;
  /* Ignore spurious requests. */
  if (size == 0)
    return (NULL);

  /* Adjust block size to include overhead and alignment reqs. */
  if (size <= DSIZE)
    asize = 2 * DSIZE;
  else
    asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);

  /* Search the free list for a fit. */
  if ((bp = find_fit(asize)) != NULL)
  {
    place(bp, asize);
    return (bp);
  }

  /* No fit found.  Get more memory and place the block. */
  extendsize = MAX(asize, CHUNKSIZE);
  if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
    return (NULL);
  place(bp, asize);
  return (bp);
}

/* 
 * Requires:
 *   "bp" is either the address of an allocated block or NULL.
 *
 * Effects:
 *   Free a block.
 */
void mm_free(void *bp)
{
  size_t size;

  /* Ignore spurious requests. */
  if (bp == NULL)
    return;

  /* Free and coalesce the block. */
  size = GET_SIZE(HDRP (bp));
  PUT(HDRP (bp), PACK (size, 0));
  PUT(FTRP (bp), PACK (size, 0));
  coalesce(bp);
}

/*
 * Requires:
 *   "ptr" is either the address of an allocated block or NULL.
 *
 * Effects:
 *   Reallocates the block "ptr" to a block with at least "size" bytes of
 *   payload, unless "size" is zero.  If "size" is zero, frees the block
 *   "ptr" and returns NULL.  If the block "ptr" is already a block with at
 *   least "size" bytes of payload, then "ptr" may optionally be returned.
 *   Otherwise, a new block is allocated and the contents of the old block
 *   "ptr" are copied to that new block.  Returns the address of this new
 *   block if the allocation was successful and NULL otherwise.
 */
void *mm_realloc(void *ptr, size_t size)
{
  /* If size == 0 then this is just free, and we return NULL. */
  if (size == 0)
  {
    mm_free(ptr);
    return (NULL);
  }

  /* If oldptr is NULL, then this is just malloc. */
  if (ptr == NULL)
    return (mm_malloc(size));

  size_t expectedNewSize = size + 2 * WSIZE;
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP (ptr)));
  size_t oldsize = GET_SIZE(HDRP (ptr));
  size_t expandableSize = oldsize + GET_SIZE(HDRP (NEXT_BLKP (ptr)));
  /* If expected size of new block is less than or equal to current block then no need to allocate a new block and just return a old block pointer.*/
  if (expectedNewSize <= oldsize)
    return ptr;

  if (!next_alloc && (expandableSize >= expectedNewSize))
  {
    remove_from_freelist(NEXT_BLKP(ptr)); /* Removes next block from free list. */
    PUT(HDRP (ptr), PACK (expandableSize, 1)); /*Update header and footer to the sum of current and next block sizes. */
    PUT(FTRP (ptr), PACK (expandableSize, 1));
    return ptr;
  }
  else
  {
    void *newptr = mm_malloc(expectedNewSize);
    /* Requests a new block with (requested size + header size + footer size)) */
    memcpy(newptr, ptr, expectedNewSize);	 /* Copies old block's data (data size = size of new block) into new block */
    mm_free(ptr);                			 /* Frees the old block */
    return (newptr);
  }
}

/*
 * The following routines are internal helper routines.
 */

/*
 * Requires:
 *   "bp" is the address of a newly freed block.
 *
 * Effects:
 *   Perform boundary tag coalescing.  Returns the address of the coalesced
 *   block.
 */
static void *coalesce(void *bp)
{
  bool prev_alloc = GET_ALLOC (FTRP (PREV_BLKP (bp)));
  bool next_alloc = GET_ALLOC(HDRP (NEXT_BLKP (bp)));
  size_t size = GET_SIZE(HDRP (bp));

  if (prev_alloc && next_alloc)
  { /* Case 1 */
    insert_in_freelist(bp); /* Inserts bp to free list. */
    return (bp);
  }
  else if (prev_alloc && !next_alloc)
  { /* Case 2 */
    size += GET_SIZE(HDRP (NEXT_BLKP (bp)));
    remove_from_freelist(NEXT_BLKP(bp)); /* Next block will be removed from free list. */
    PUT(HDRP (bp), PACK (size, 0));
    PUT(FTRP (bp), PACK (size, 0));
  }
  else if (!prev_alloc && next_alloc)
  { /* Case 3 */
    size += GET_SIZE(HDRP (PREV_BLKP (bp)));
    remove_from_freelist(PREV_BLKP(bp)); /* Previous block will be removed from free list. */
    PUT(FTRP (bp), PACK (size, 0));
    PUT(HDRP (PREV_BLKP (bp)), PACK (size, 0));
    bp = PREV_BLKP(bp);
  }
  else
  { /* Case 4 */
    size += GET_SIZE(HDRP (PREV_BLKP (bp))) +
    GET_SIZE (FTRP (NEXT_BLKP (bp)));
    remove_from_freelist(PREV_BLKP(bp)); /* Previous and Next block will be removed from free list. */
    remove_from_freelist(NEXT_BLKP(bp));
    PUT(HDRP (PREV_BLKP (bp)), PACK (size, 0));
    PUT(FTRP (NEXT_BLKP (bp)), PACK (size, 0));
    bp = PREV_BLKP(bp);
  }
  insert_in_freelist(bp); /* Inserts a merged block in to the free list */
  return (bp);
}

/* 
 * Requires:
 *   None.
 *
 * Effects:
 *   Extend the heap with a free block and return that block's address.
 */
static void *extend_heap(size_t words)
{
  void *bp;
  size_t size;

  /* Allocate an even number of words to maintain alignment. */
  size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

  if ((bp = mem_sbrk(size)) == (void *) -1)
    return (NULL);

  /* Initialize free block header/footer and the epilogue header. */
  PUT(HDRP (bp), PACK (size, 0)); /* Free block header */
  PUT(FTRP (bp), PACK (size, 0)); /* Free block footer */
  PUT(HDRP (NEXT_BLKP (bp)), PACK (0, 1)); /* New epilogue header */

  /* Coalesce if the previous block was free. */
  return (coalesce(bp));
}

/*
 * Requires:
 *   None.
 *
 * Effects:
 *   Find a fit for a block with "asize" bytes.  Returns that block's address
 *   or NULL if no suitable block was found. 
 */

static void *find_fit(size_t asize)
{
  void *bp;
  /* Traverses only through the free list intead of heap */
  for (bp = (void *) freelist_head; GET_ALLOC (HDRP (bp)) == 0; bp =(void *) GET_NEXT_FREEP(bp))
  {
    if (asize <= GET_SIZE(HDRP (bp)))
      return (bp);
  }
  //No fit was found. 
  return (NULL);
}

/*
static void *next_fit(size_t asize)
{
  void *bp = freelist_head;
  void *initial = freelist_head;
// Search for the next fit.
  for (bp = freelist_head; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
  {
    if (!GET_ALLOC(HDRP(bp)) && asize <= GET_SIZE(HDRP(bp)))
    {
      freelist_head = bp;
      return (bp);
    }
  }
  for (bp = heap_listp; HDRP(bp) != HDRP(initial); bp = NEXT_BLKP(bp))
    if (!GET_ALLOC(HDRP(bp)) && asize <= GET_SIZE(HDRP(bp)))
    {
      freelist_head = bp;
      return (bp);
    }
//No fit was found. 
  return (NULL);
}
*/


/*
static void *best_fit(size_t asize)
{
  void *bp = heap_listp, *best = NULL;
  size_t min = 2147483647;
  for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
  {
    if (GET_SIZE(HDRP(bp))
        >= asize&& GET_SIZE(HDRP(bp))<min && !GET_ALLOC(HDRP(bp)))
    {
      min = GET_SIZE(HDRP(bp));
      best = bp;
    }
  }
  if (best == NULL)
    return (NULL);
  return (best);
}
*/

/* 
 * Requires:
 *   "bp" is the address of a free block that is at least "asize" bytes.
 *
 * Effects:
 *   Place a block of "asize" bytes at the start of the free block "bp" and
 *   split that block if the remainder would be at least the minimum block
 *   size. 
 */
static void place(void *bp, size_t asize)
{
  size_t csize = GET_SIZE(HDRP (bp));
  /* Removes a block from free list as it is now allocted. */
  remove_from_freelist(bp);
  if ((csize - asize) >= (2 * DSIZE))
  {
    PUT(HDRP (bp), PACK (asize, 1));
    PUT(FTRP (bp), PACK (asize, 1));
    bp = NEXT_BLKP(bp);
    PUT(HDRP (bp), PACK (csize - asize, 0));
    PUT(FTRP (bp), PACK (csize - asize, 0));
    insert_in_freelist(bp); /* Insert a free block (after splitting) to free list. */
  }
  else
  {
    PUT(HDRP (bp), PACK (csize, 1));
    PUT(FTRP (bp), PACK (csize, 1));
  }

}

/* 
 * The remaining routines are heap consistency checker routines. 
 */

/*
 * Requires:
 *   "bp" is the address of a block.
 *
 * Effects:
 *   Perform a minimal check on the block "bp".
 */
static void checkblock(void *bp)
{

  if ((uintptr_t) bp % DSIZE)
    printf("Error: %p is not doubleword aligned\n", bp);
  if (GET(HDRP (bp)) != GET(FTRP (bp)))
    printf("Error: header does not match footer\n");
}

/* 
 * Requires:
 *   None.
 *
 * Effects:
 *   Perform a minimal check of the heap for consistency. 
 */
static void checkheap(bool verbose)
{
  void *bp;

  if (verbose)
    printf("Heap (%p):\n", heap_listp);

  if (GET_SIZE (HDRP (heap_listp)) != DSIZE || !GET_ALLOC(HDRP (heap_listp)))
    printf("Bad prologue header\n");
  checkblock(heap_listp);
  /*Traverse through free list instead of all the blocks*/
for (bp = (void *) freelist_head; GET_ALLOC (HDRP (bp)) == 0; bp =(void *) GET_NEXT_FREEP(bp))    
  {
    if (verbose)
      printblock(bp);
    checkblock(bp);
  }
for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = (void *)NEXT_BLKP(bp));     /* Set bp to epilogue. */
  if (verbose)
    printblock(bp);
  if (GET_SIZE (HDRP (bp)) != 0 || !GET_ALLOC(HDRP (bp)))
    printf("Bad epilogue header\n");
}

/*
 * Requires:
 *   "bp" is the address of a block.
 *
 * Effects:
 *   Print the block "bp".
 */
static void printblock(void *bp)
{
  bool halloc, falloc;
  size_t hsize, fsize;

  checkheap(false);
  hsize = GET_SIZE(HDRP (bp));
  halloc = GET_ALLOC(HDRP (bp));
  fsize = GET_SIZE(FTRP (bp));
  falloc = GET_ALLOC(FTRP (bp));

  if (hsize == 0)
  {
    printf("%p: end of heap\n", bp);
    return;
  }

  printf("%p: header: [%zu:%c] footer: [%zu:%c]\n", bp, hsize,
      (halloc ? 'a' : 'f'), fsize, (falloc ? 'a' : 'f'));
}

/* Functions to maintain freelist */
/* It insert a free block at the front of freelist and set next and previous block links. */
static void insert_in_freelist(void *bp)
{
  SET_NEXT_FREEP(bp, freelist_head);
  SET_PREV_FREEP(bp, NULL);
  /* Prevents overwrite to prologue footer when this function called for the first time. */
  if(freelist_head!=heap_listp)
    SET_PREV_FREEP(freelist_head, bp);
  freelist_head = bp; /* Updates freelist_head to bp */
}

/* It removes a block from free list and update next and previous block links */
static void remove_from_freelist(void *bp)
{
  if (GET_PREV_FREEP (bp) == (uintptr_t) NULL) /* If it is the first block of free list then just update freelist_head to next block. */
    freelist_head = (char *) GET_NEXT_FREEP(bp);
  else
    SET_NEXT_FREEP(GET_PREV_FREEP (bp), GET_NEXT_FREEP (bp));
  /* Prevent overwrite to prologue footer when removing the last block of free list (which has prologue footer as next block pointer) */
  if(GET_NEXT_FREEP(bp)!=(uintptr_t)heap_listp)
    SET_PREV_FREEP(GET_NEXT_FREEP (bp), GET_PREV_FREEP (bp));
}

/*
 * The last lines of this file configures the behavior of the "Tab" key in
 * emacs.  Emacs has a rudimentary understanding of C syntax and style.  In
 * particular, depressing the "Tab" key once at the start of a new line will
 * insert as many tabs and/or spaces as are needed for proper indentation.
 */

/* Local Variables: */
/* mode: c */
/* c-default-style: "bsd" */
/* c-basic-offset: 8 */
/* c-continued-statement-offset: 4 */
/* indent-tabs-mode: t */
/* End: */

/*

*******************************************************
                         WRITEUP
*******************************************************
  

-->   Description about Explicit Free list
 Here we use explicit free list instead of implicit free list to improve performance of the memory allocator. 
  We can utilize the space of payload of free block to store next and previous free block pointers. 
  We 4 macros and 2 function to maintain explicit free list. 
            #define GET_NEXT_FREEP(bp) (GET(bp + WSIZE))        
            #define GET_PREV_FREEP(bp) (GET(bp))                
            #define SET_NEXT_FREEP(bp,next) (GET_NEXT_FREEP(bp) = next)
            #define SET_PREV_FREEP(bp,prev) (GET_PREV_FREEP(bp) = prev)
  Above macros are subject to get and set the links of next and previous free blocks.
            static void insert_in_freelist (void *bp);
            static void remove_from_freelist (void *bp);
  Above functions are subject to insert and remove free block from free list. They are called from other functions whenever needed.

  Below is the overview of the original heap.

          Padding       Prologue                                                                  Epilogue
          ---------   -------------------                                                         ---------
          | WSIZE |   | |PH 8,1| |PF 8,1|              blocks (free and allocated)                |EH 0,1 |
          ---------   -------------------  .................................................      ---------
                            ^
                            heap_listp
                            freelist_head (Initially)


  Below is the overview of the explicit free list. It is just list of free blocks among above heap itself.

          -----------------------     -----------------------                                 ---------------------------------------------    
          | |H| NULL | next |F| |     | |H| prev | next |F| |                                 | |H| prev | Address of Prologue Footer |F| |
          -----------------------     ----------------------- ..............................  ---------------------------------------------
                ^
                freelist_head
                (Pointing to the start of the free list)

-->    Changes in find_fit() 
  We use this free list to find a block with requested size by traversing explicit free list. It improves the performance because 
we don't need to track all the blocks now.

-->    Changes in mm_realloc()  
  It may happen that reallocation of the block is requested and next block
is free then we should check the combined size instead of directly allocating new block. If check succeed then we merge 
that two blocks and return the same block pointer otherwise allocate a new block and copy the old data into new block, free 
the old block and return the new block pointer.

-->    Changes in checkheap(), the heap consistency checker.    
  We made checkheap() to use explict free list instead of implicit free list. It does not improve performance. There is no need to traverse
through all the blocks for checking heap consistency. So we changed traversal path of the checkheap() function.

NOTE:
  When we insert the free block in the free list for the first time, freelist_head was pointing to the prologue's footer, so if we set previous link of 
freelist_head to the block(which we are inserting) then prolouge's footer will change that is undesirable. So we take care of this case by adding condition in insert_in_freelist() function.
  When we remove the last block from the free list, the previous link of prologue footer(which actually does not exists) will be set to previous block of
the last block that is also undesirable.  So we take care of this case by adding condition in remove_from_freelist() function.




*/