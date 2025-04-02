/*
 * mm.c
 *
 *2100094820 郑斗薰
 *代码实现了一个动态内存分配器，采用了分离空闲链表策略。它根据块的大小将空闲块分为多个链表，并在释放空闲块时合并相邻的空闲块，以减少内存碎片。具体实现方式在在过程中详细注释，谢谢！
 */
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
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
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
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)


/*
 * 初始化：出错时返回 -1，成功时返回 0。
 */

/* 基本常量和宏定义 */
#define ALIGNMENT_SIZE 8
#define WORD_SIZE 4 /* 单字和头部/脚部的大小（字节） */
#define DOUBLE_WORD_SIZE 8 /* 双字的大小（字节） */
#define MIN_BLOCK_SIZE 24
#define INITIAL_CHUNK_SIZE (1 << 12) /* 以此数量扩展堆（字节） */

#define MAXIMUM(x, y) ((x) > (y) ? (x) : (y))

/* 将大小和已分配位打包成一个字 */
#define PACK(size, alloc) ((size) | (alloc))

/* 读取和写入地址 p 的字 */
#define READ(p) (*(unsigned int *)(p))
#define WRITE(p, val) (*(unsigned int *)(p) = val)

/* 从地址 p 读取大小和已分配字段 */
#define GET_SIZE(p) (READ(p) & ~0x7)
#define GET_ALLOC(p) (READ(p) & 0x1)

/* 给定块指针 bp，计算其头部和脚部的地址 */
#define HEADER_PTR(bp) ((char *)(bp) - WORD_SIZE)
#define FOOTER_PTR(bp) ((char *)(bp) + GET_SIZE(HEADER_PTR(bp)) - DOUBLE_WORD_SIZE)

/* 给定块指针 bp，计算下一个和上一个块的地址 */
#define NEXT_BLOCK_PTR(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WORD_SIZE)))
#define PREV_BLOCK_PTR(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DOUBLE_WORD_SIZE)))

/* 在地址 p 写入一个指针 */
#define WRITE_POINTER(p, ptr) (*(unsigned long *)(p) = (unsigned long)(ptr))

/* 给定块指针 bp，读取和写入前驱和后继 */
#define GET_PREVIOUS(bp) ((char *)(*(unsigned long *)(bp)))
#define WRITE_PREVIOUS(bp, ptr) (WRITE_POINTER((char *)(bp), ptr))
#define GET_NEXT(bp) ((char *)(*(unsigned long *)(bp + DOUBLE_WORD_SIZE)))
#define WRITE_NEXT(bp, ptr) (WRITE_POINTER(((char *)(bp) + (DOUBLE_WORD_SIZE)), ptr))

/* 定义大小段常量 */
const size_t segment_sizes[8] = {32, 64, 128, 256, 512, 1024, 2048, 4096};

// 堆的初始块（不包括前面的9+9个数组元素）。
static char *heap_start;

// FreeList_head[i] 指向空闲链表 i 的头部。
static char **free_list_head, **free_list_tail;

static void remove_from_free_list(char *block);
static void insert_into_free_list(char *block, char *p);
static int get_free_list_index(size_t size);
static void *extend_heap(size_t words);
static void *coalesce(void *block);
static void *find_fit(size_t size);
static void place(void *block, size_t size);

int mm_init(void);
void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *oldptr, size_t size);
void *calloc(size_t nmemb, size_t size);
static int in_heap(const void *ptr);
static int aligned(const void *ptr);
void mm_checkheap(int lineno);

/* 从空闲链表中移除指定位置的块 */
static void remove_from_free_list(char *block)
{
    void *prev_block = GET_PREVIOUS(block); // 获取前驱块
    void *next_block = GET_NEXT(block); // 获取后继块
    WRITE_NEXT(prev_block, next_block); // 连接前驱和后继
    WRITE_PREVIOUS(next_block, prev_block); // 更新后继的前驱
}

/* 将块插入空闲链表中 */
static void insert_into_free_list(char *block, char *p)
{
    WRITE_PREVIOUS(p, block); // 设置前驱指针
    WRITE_NEXT(p, GET_NEXT(block)); // 设置后继指针
    WRITE_NEXT(block, p); // 更新链表
    WRITE_PREVIOUS(GET_NEXT(p), p); // 更新后继块的前驱
}

/* 获取适合块大小的空闲链表索引 */
static int get_free_list_index(size_t size)
{
    for (int i = 0; i < 8; i++)
        if (size <= segment_sizes[i])
            return i;
    return 8; // 返回8表示没有找到合适的链表
}

/* 扩展堆的功能 */
static void *extend_heap(size_t words)
{
    char *block_ptr;
    //size_t size=(words % 2) ? (words + 1) * WORD_SIZE : words * WORD_SIZE;
    size_t size;
    if (words % 2) {
        size = (words + 1) * WORD_SIZE;
    } else {
        size = words * WORD_SIZE;
    }
    /* 为了保持对齐，分配偶数个字 */
    if ((long)(block_ptr = mem_sbrk(size)) == -1)
        return NULL; // 失败则返回NULL

    /* 初始化空闲块的头部/脚部和结束标志头部 */
    WRITE(HEADER_PTR(block_ptr), PACK(size, 0));
    WRITE(FOOTER_PTR(block_ptr), PACK(size, 0));
    WRITE(HEADER_PTR(NEXT_BLOCK_PTR(block_ptr)), PACK(0, 1)); /* 新的结束标志头部 */

    char *list_head = free_list_head[get_free_list_index(size)];
    insert_into_free_list(list_head, block_ptr); // 将新块插入空闲链表

    /* 如果前一个块是空闲的，则合并 */
    return coalesce(block_ptr);
}

/* 合并相邻的空闲块 */
static void *coalesce(void *block_ptr)
{
    void *prev_block_ptr = PREV_BLOCK_PTR(block_ptr); // 获取前一个块的指针
    void *next_block_ptr = NEXT_BLOCK_PTR(block_ptr); // 获取下一个块的指针

    size_t prev_alloc = GET_ALLOC(FOOTER_PTR(prev_block_ptr)); // 检查前一个块是否已分配
    size_t next_alloc = GET_ALLOC(HEADER_PTR(next_block_ptr)); // 检查下一个块是否已分配
    size_t size = GET_SIZE(HEADER_PTR(block_ptr)); // 当前块的大小

    // 根据前后块的分配状态进行合并
    if (prev_alloc && next_alloc) // 情况1: 前后块都已分配
    {
        return block_ptr; // 无需合并
    }
    else if (prev_alloc && !next_alloc) // 情况2: 前块已分配，后块未分配
    {
        size += GET_SIZE(HEADER_PTR(next_block_ptr)); // 合并后块
        remove_from_free_list(block_ptr); // 从空闲链表中移除当前块
        remove_from_free_list(next_block_ptr); // 从空闲链表中移除后块
    }
    else if (!prev_alloc && next_alloc) // 情况3: 前块未分配，后块已分配
    {
        size += GET_SIZE(FOOTER_PTR(prev_block_ptr)); // 合并前块
        remove_from_free_list(prev_block_ptr); // 从空闲链表中移除前块
        remove_from_free_list(block_ptr); // 从空闲链表中移除当前块
        block_ptr = prev_block_ptr; // 更新当前块为合并后的块
    }
    else if (!prev_alloc && !next_alloc) // 情况4: 前后块都未分配
    {
        size += GET_SIZE(HEADER_PTR(prev_block_ptr)) + GET_SIZE(FOOTER_PTR(next_block_ptr)); // 合并前后块
        remove_from_free_list(prev_block_ptr); // 从空闲链表中移除前块
        remove_from_free_list(block_ptr); // 从空闲链表中移除当前块
        remove_from_free_list(next_block_ptr); // 从空闲链表中移除后块
        block_ptr = prev_block_ptr; // 更新当前块为合并后的块
    }

    // 更新头部和脚部
    WRITE(HEADER_PTR(block_ptr), PACK(size, 0));
    WRITE(FOOTER_PTR(block_ptr), PACK(size, 0));

    char *list_head = free_list_head[get_free_list_index(size)];
    insert_into_free_list(list_head, block_ptr); // 将合并后的块插入空闲链表

    return block_ptr; // 返回合并后的块指针
}

/* 查找适合的空闲块 */
static void *find_fit(size_t size)
{
    /* 首次适配搜索 */
    for (int i = get_free_list_index(size); i < 9; i++)
    {
        char *list_head = free_list_head[i];
        char *list_tail = free_list_tail[i];
        for (void *block_ptr = GET_NEXT(list_head); block_ptr != list_tail; block_ptr = GET_NEXT(block_ptr))
            if (size <= GET_SIZE(HEADER_PTR(block_ptr))) // 如果找到合适的块
                return block_ptr;
    }

    return NULL; /* 未找到合适的块 */
}

/* 将块放置到适当的位置 */
static void place(void *block_ptr, size_t size)
{
    size_t current_size = GET_SIZE(HEADER_PTR(block_ptr)); // 当前块的大小

    // 如果块的剩余空间足够，则进行分割
    if ((current_size - size) >= (3 * DOUBLE_WORD_SIZE)) 
    {
        WRITE(HEADER_PTR(block_ptr), PACK(size, 1)); // 更新头部为已分配
        WRITE(FOOTER_PTR(block_ptr), PACK(size, 1)); // 更新脚部为已分配
        remove_from_free_list(block_ptr); // 从空闲链表中移除当前块

        void *remaining_block_ptr = NEXT_BLOCK_PTR(block_ptr); // 获取剩余块
        WRITE(HEADER_PTR(remaining_block_ptr), PACK(current_size - size, 0)); // 更新剩余块的头部
        WRITE(FOOTER_PTR(remaining_block_ptr), PACK(current_size - size, 0)); // 更新剩余块的脚部
        char *list_head = free_list_head[get_free_list_index(current_size - size)];
        insert_into_free_list(list_head, remaining_block_ptr); // 将剩余块插入空闲链表
    }
    else
    {
        WRITE(HEADER_PTR(block_ptr), PACK(current_size, 1)); // 更新头部为已分配
        WRITE(FOOTER_PTR(block_ptr), PACK(current_size, 1)); // 更新脚部为已分配
        remove_from_free_list(block_ptr); // 从空闲链表中移除当前块
    }
}

/* 打印块的信息 */
static void print_block(void *block_ptr)
{
    printf("Address:%p, Size:%d, Alloc:%c.\n", block_ptr, GET_SIZE(HEADER_PTR(block_ptr)), GET_ALLOC(HEADER_PTR(block_ptr)));
}

/* 检查块的状态 */
static void check_block(void *block_ptr)
{
    if (!aligned(block_ptr)) // 检查对齐
        printf("Error: The block at address %p is not double word aligned!\n", block_ptr);
    if (READ(HEADER_PTR(block_ptr)) != READ(FOOTER_PTR(block_ptr))) // 检查头部和脚部是否匹配
        printf("Error: The block at address %p, Header does not match footer!\n", block_ptr);
}

/* 初始化内存管理模块 */
int mm_init(void) {
    /* 创建初始空堆 */
    if ((heap_start = mem_sbrk(73 * DOUBLE_WORD_SIZE)) == (void *)-1)
        return -1; // 初始化失败

    WRITE(heap_start, PACK(0, 0)); /* 对齐填充 */
    heap_start += WORD_SIZE; // 更新堆起始位置

    free_list_head = (char **)heap_start; 
    heap_start += 9 * DOUBLE_WORD_SIZE; // 更新堆位置

    free_list_tail = (char **)heap_start; 
    heap_start += (9 * DOUBLE_WORD_SIZE + WORD_SIZE); // 更新堆位置

    char *p = heap_start, *q = heap_start + 9 * MIN_BLOCK_SIZE; 

    // 初始化空闲块
    for (int i = 0; i < 9; i++)
    {
        WRITE(HEADER_PTR(p), PACK(3 * DOUBLE_WORD_SIZE, 1)); // 设置头部
        WRITE(FOOTER_PTR(p), PACK(3 * DOUBLE_WORD_SIZE, 1)); // 设置脚部
        WRITE(HEADER_PTR(q), PACK(3 * DOUBLE_WORD_SIZE, 1)); // 设置头部
        WRITE(FOOTER_PTR(q), PACK(3 * DOUBLE_WORD_SIZE, 1)); // 设置脚部

        WRITE_PREVIOUS(p, NULL); // 初始化前驱为空
        WRITE_NEXT(p, q); // 设置后继
        WRITE_PREVIOUS(q, p); // 设置前驱
        WRITE_NEXT(q, NULL); // 初始化后继为空

        free_list_head[i] = p; // 设置空闲链表头
        free_list_tail[i] = q; // 设置空闲链表尾

        p = NEXT_BLOCK_PTR(p); // 更新块指针
        q = NEXT_BLOCK_PTR(q); // 更新块指针
    }

    WRITE(HEADER_PTR(heap_start + 18 * MIN_BLOCK_SIZE), PACK(0, 1)); /* 设置结束标志头部 */

    /* 以 INITIAL_CHUNK_SIZE 字节扩展空堆 */
    if (extend_heap(INITIAL_CHUNK_SIZE / WORD_SIZE) == NULL)
        return -1; // 扩展失败
    return 0; // 初始化成功
}

/*
 * malloc
 */
void *malloc(size_t size) {
    size_t adjusted_size;      /* 调整后的块大小 */
    size_t extend_size; /* 如果没有合适的块，则扩展堆的大小 */
    char *block_ptr;

    if (heap_start == 0) // 如果堆未初始化
    {
        mm_init();
    }
    /* 忽略无效请求 */
    if (size == 0)
        return NULL;

    /* 调整块大小以包括开销和对齐要求 */
    if (size <= 2 * DOUBLE_WORD_SIZE)
        adjusted_size = 3 * DOUBLE_WORD_SIZE; // 小块的最小大小
    else
        adjusted_size = DOUBLE_WORD_SIZE * ((size + (DOUBLE_WORD_SIZE) + (DOUBLE_WORD_SIZE - 1)) / DOUBLE_WORD_SIZE);

    /* 在空闲链表中搜索合适的块 */
    if ((block_ptr = find_fit(adjusted_size)) != NULL)
    {
        place(block_ptr, adjusted_size); // 放置块
        return block_ptr; // 返回块指针
    }

    /* 未找到合适的块，获取更多内存并放置块 */
    extend_size = MAXIMUM(adjusted_size, INITIAL_CHUNK_SIZE);
    if ((block_ptr = extend_heap(extend_size / WORD_SIZE)) == NULL)
        return NULL; // 扩展失败
    place(block_ptr, adjusted_size); // 放置块
    return block_ptr; // 返回块指针
}

/*
 * free
 */
void free(void *ptr) {
    if (!ptr) // 如果指针为空，返回
        return;

    size_t size = GET_SIZE(HEADER_PTR(ptr)); // 获取块大小
    if (heap_start == 0) // 如果堆未初始化
    {
        mm_init();
    }

    WRITE(HEADER_PTR(ptr), PACK(size, 0)); // 将块标记为空闲
    WRITE(FOOTER_PTR(ptr), PACK(size, 0)); // 更新脚部

    char *list_head = free_list_head[get_free_list_index(size)];
    insert_into_free_list(list_head, ptr); // 将块插入空闲链表

    coalesce(ptr); // 合并相邻的空闲块
}

/*
 * realloc
 */
void *realloc(void *oldptr, size_t size) {
    size_t old_size;
    void *new_ptr;

    /* 如果 size 为 0，则这只是释放，返回 NULL。 */
    if (size == 0)
    {
        free(oldptr);
        return 0;
    }

    /* 如果 oldptr 为空，则这是分配新内存。 */
    if (oldptr == NULL)
    {
        return malloc(size);
    }

    new_ptr = malloc(size); // 分配新内存

    /* 如果 realloc() 失败，则原始块保持不变 */
    if (!new_ptr)
    {
        return 0; // 分配失败
    }

    /* 复制旧数据 */
    old_size = GET_SIZE(HEADER_PTR(oldptr)); // 获取旧块大小
    if (size < old_size) // 如果新大小小于旧大小
        old_size = size; // 只复制新大小
    memcpy(new_ptr, oldptr, old_size); // 复制数据

    /* 释放旧块 */
    free(oldptr);

    return new_ptr; // 返回新指针
}

/*
 * calloc
 */
void *calloc(size_t nmemb, size_t size) {
    size_t total_bytes = nmemb * size; // 计算总字节数
    void *new_ptr;

    new_ptr = malloc(total_bytes); // 分配内存
    memset(new_ptr, 0, total_bytes); // 将内存清零
    return new_ptr; // 返回新指针
}

/*
 * 返回指针是否在堆中。
 * 可能对调试有用。
 */
static int in_heap(const void *ptr) {
    return ptr <= mem_heap_hi() && ptr >= mem_heap_lo();
}

/*
 * 返回指针是否对齐。
 * 可能对调试有用。
 */
static int aligned(const void *ptr) {
    return (size_t)(ptr) % ALIGNMENT_SIZE == 0;
}

/*
 * mm_checkheap
 */
void mm_checkheap(int lineno) {
    char *block_ptr = heap_start;

    // 检查空闲链表的头部
    for (int i = 0; i < 8; i++) {
        if ((GET_SIZE(HEADER_PTR(block_ptr)) != MIN_BLOCK_SIZE) || (!GET_ALLOC(HEADER_PTR(block_ptr))))
            printf("Error: Bad FreeList Head %d!\n", i);
        block_ptr = NEXT_BLOCK_PTR(block_ptr); // 移动到下一个块
    }

    // 检查空闲链表的尾部
    for (int i = 0; i < 8; i++) {
        if ((GET_SIZE(HEADER_PTR(block_ptr)) != MIN_BLOCK_SIZE) || (!GET_ALLOC(HEADER_PTR(block_ptr))))
            printf("Error: Bad FreeList Tail %d!\n", i);
        block_ptr = NEXT_BLOCK_PTR(block_ptr); // 移动到下一个块
    }

    // 遍历堆中的每个块，检查其状态
    for (block_ptr = heap_start; GET_SIZE(HEADER_PTR(block_ptr)) > 0; block_ptr = NEXT_BLOCK_PTR(block_ptr)) {
        if (lineno)
            print_block(block_ptr); // 打印块的信息
        check_block(block_ptr); // 检查块的状态
    }

    // 检查结束块的状态
    if (lineno)
        print_block(block_ptr);
    if ((GET_SIZE(HEADER_PTR(block_ptr)) != 0) || (!GET_ALLOC(HEADER_PTR(block_ptr))))
        printf("Error: Bad Epilogue Block!\n"); // 检查结束块是否正确
}