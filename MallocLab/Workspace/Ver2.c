#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
#define dbg_printf(...) printf(__VA_ARGS__)
#else
#define dbg_printf(...)
#endif

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT - 1)) & ~0x7)

/* Base constants and macros */
#define WSIZE 4 /* Word and header/footer size (bytes) */
#define DSIZE 8 /* Double word size (bytes) */
#define MIN_BLOCK_SIZE 24
#define CHUNKSIZE (1 << 12) /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word*/
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = val)

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

/* Write a pointer at address p */
#define PUT_PTR(p, ptr) (*(unsigned long *)(p) = (unsigned long)(ptr))

/* Given block ptr bp, read and write PRED and SUCC */
#define GET_PRED(bp) ((char *)(*(unsigned long *)(bp)))
#define PUT_PRED(bp, ptr) (PUT_PTR((char *)(bp), ptr))
#define GET_SUCC(bp) ((char *)(*(unsigned long *)(bp + DSIZE)))
#define PUT_SUCC(bp, ptr) (PUT_PTR(((char *)(bp) + (DSIZE)), ptr))

/* Global variables */

// 辅助数组，seg_num[i]输出【空闲链表 i】允许的最大块大小。
const size_t seg_num[8] = {32, 64, 128, 256, 512, 1024, 2048, 4096};

// 堆的第一个块（不包含前面的9+9个数组元素）。
static char *heap_listp;

// FreeList_head[i]输出【空闲链表 i】的头部。
static char **FreeList_head, **FreeList_tail;

static void FreeList_remove(char *position);
static void FreeList_insert(char *position, char *p);
static int get_FreeList_ID(size_t size);
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

static void FreeList_remove(char *position)
{
    void *pred = GET_PRED(position);
    void *succ = GET_SUCC(position);
    PUT_SUCC(pred, succ);
    PUT_PRED(succ, pred);
}

static void FreeList_insert(char *position, char *p)
{
    PUT_PRED(p, position);
    PUT_SUCC(p, GET_SUCC(position));
    PUT_SUCC(position, p);
    PUT_PRED(GET_SUCC(p), p);
}

int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(73 * DSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, PACK(0, 0)); /* Alignment padding */
    heap_listp += WSIZE;

    FreeList_head = (char **)heap_listp; // FreeList_head数组。
    heap_listp += 9 * DSIZE;

    FreeList_tail = (char **)heap_listp; // FreeList_tail数组。
    heap_listp += (9 * DSIZE + WSIZE);

    char *p = heap_listp, *q = heap_listp + 9 * MIN_BLOCK_SIZE; // p指向9个头部块的第一个，q指向9个尾部块的第一个。

    for (int i = 0; i < 9; i++)
    {
        PUT(HDRP(p), PACK(3 * DSIZE, 1));
        PUT(FTRP(p), PACK(3 * DSIZE, 1));
        PUT(HDRP(q), PACK(3 * DSIZE, 1));
        PUT(FTRP(q), PACK(3 * DSIZE, 1));

        PUT_PRED(p, NULL);
        PUT_SUCC(p, q);
        PUT_PRED(q, p);
        PUT_SUCC(q, NULL);

        FreeList_head[i] = p;
        FreeList_tail[i] = q;

        p = NEXT_BLKP(p);
        q = NEXT_BLKP(q);
    }

    PUT(HDRP(heap_listp + 18 * MIN_BLOCK_SIZE), PACK(0, 1)); /* Epilogue Header */

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    char *listp = FreeList_head[get_FreeList_ID(size)];
    FreeList_insert(listp, bp);

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

// 输入size，获取链表类的ID。
static int get_FreeList_ID(size_t size)
{
    for (int i = 0; i < 8; i++)
        if (size <= seg_num[i])
            return i;
    return 8;
}

void free(void *bp)
{
    if (!bp)
        return;

    size_t size = GET_SIZE(HDRP(bp));
    if (heap_listp == 0)
    {
        mm_init();
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    char *listp = FreeList_head[get_FreeList_ID(size)];
    FreeList_insert(listp, bp);

    coalesce(bp);
}

static void *coalesce(void *bp)
{
    void *prev_ptr = PREV_BLKP(bp);
    void *next_ptr = NEXT_BLKP(bp);

    size_t prev_alloc = GET_ALLOC(FTRP(prev_ptr));
    size_t next_alloc = GET_ALLOC(HDRP(next_ptr));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)
    { /* Case 1 */
        return bp;
    }
    else if (prev_alloc && !next_alloc)
    { /* Case 2 */
        size += GET_SIZE(HDRP(next_ptr));
        FreeList_remove(bp);
        FreeList_remove(next_ptr);
    }
    else if (!prev_alloc && next_alloc)
    { /* Case 3 */
        size += GET_SIZE(FTRP(prev_ptr));
        FreeList_remove(prev_ptr);
        FreeList_remove(bp);
        bp = prev_ptr;
    }
    else if (!prev_alloc && !next_alloc)
    { /* Case 4 */
        size += GET_SIZE(HDRP(prev_ptr)) +
                GET_SIZE(FTRP(next_ptr));
        FreeList_remove(prev_ptr);
        FreeList_remove(bp);
        FreeList_remove(next_ptr);
        bp = prev_ptr;
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    char *listp = FreeList_head[get_FreeList_ID(size)];
    FreeList_insert(listp, bp);

    return bp;
}

void *malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;

    if (heap_listp == 0)
    {
        mm_init();
    }
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= 2 * DSIZE)
        asize = 3 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

static void *find_fit(size_t asize)
{
    /* First-fit search */
    for (int i = get_FreeList_ID(asize); i < 9; i++)
    {
        char *listp = FreeList_head[i];
        char *tailp = FreeList_tail[i];
        for (void *bp = GET_SUCC(listp); bp != tailp; bp = GET_SUCC(bp))
            if (asize <= GET_SIZE(HDRP(bp)))
                return bp;
    }

    return NULL; /* No fit */
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (3 * DSIZE)) // 剩余空间足以构成空闲块。
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        FreeList_remove(bp);

        void *remainp = NEXT_BLKP(bp);
        PUT(HDRP(remainp), PACK(csize - asize, 0));
        PUT(FTRP(remainp), PACK(csize - asize, 0));
        char *listp = FreeList_head[get_FreeList_ID(csize - asize)];
        FreeList_insert(listp, remainp);
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        FreeList_remove(bp);
    }
}

void *realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if (size == 0)
    {
        free(ptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL)
    {
        return malloc(size);
    }

    newptr = malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if (!newptr)
    {
        return 0;
    }

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(ptr));
    if (size < oldsize)
        oldsize = size;
    memcpy(newptr, ptr, oldsize);

    /* Free the old block. */
    free(ptr);

    return newptr;
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *calloc(size_t nmemb, size_t size)
{
    size_t bytes = nmemb * size;
    void *newptr;

    newptr = malloc(bytes);
    memset(newptr, 0, bytes);

    return newptr;
}

/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void *p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static int aligned(const void *p)
{
    return (size_t)ALIGN(p) == (size_t)p;
}

static void check_block(void *bp)
{
    if (!aligned(bp))
        printf("Error: The block at address %p is not double word aligned ! \n", bp);
    if (GET(HDRP(bp)) != GET(FTRP(bp)))
        printf("Error: The block at address %p , Header does not match footer ! \n", bp);
}

static void print_block(void *bp)
{
    printf("Address:%p,Size:%d,Alloc:%c.\n", bp, GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)));
}

/*
 * mm_checkheap
 */
void mm_checkheap(int lineno)
{
    char *bp = heap_listp;

    for (int i = 0; i < 9; i++)
    {
        if ((GET_SIZE(HDRP(bp)) != MIN_BLOCK_SIZE) || (!GET_ALLOC(HDRP(bp))))
            printf("Error: Bad FreeList Head %d! \n", i);
        bp = NEXT_BLKP(bp);
    }

    for (int i = 0; i < 9; i++)
    {
        if ((GET_SIZE(HDRP(bp)) != MIN_BLOCK_SIZE) || (!GET_ALLOC(HDRP(bp))))
            printf("Error: Bad FreeList Tail %d! \n", i);
        bp = NEXT_BLKP(bp);
    }

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (lineno)
            print_block(bp);
        check_block(bp);
    }

    if (lineno)
        print_block(bp);
    if ((GET_SIZE(HDRP(bp)) != 0) || (!GET_ALLOC(HDRP(bp))))
        printf("Error: Bad Epilogue Block ! \n");
}