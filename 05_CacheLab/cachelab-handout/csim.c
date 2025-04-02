// 郑斗薰 2100094820

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "cachelab.h"

// 定义缓存行
typedef struct {
    int valid_bit; // 是否有效
    int tag_value; // 标签位
    int last_used; // 使用时间戳
} CacheLine;

// 定义缓存组
typedef struct {
    CacheLine* lines; // 一个组包含多行
} CacheSet;

// 定义缓存
typedef struct {
    CacheSet* sets; // 缓存由多个组组成
    int num_sets;   // 组数
    int num_lines;  // 每组行数
} Cache;

// 全局变量
Cache cache;
int s_bits, E_lines, b_bits, verbose_flag;
unsigned int total_hits = 0, total_misses = 0, total_evictions = 0;
int current_time = 0;

// 初始化缓存
void initCache(int num_sets, int num_lines) {
    cache.num_sets = num_sets;
    cache.num_lines = num_lines;
    cache.sets = (CacheSet*)malloc(num_sets * sizeof(CacheSet));

    for (int i = 0; i < num_sets; i++) {
        cache.sets[i].lines = (CacheLine*)malloc(num_lines * sizeof(CacheLine));
        for (int j = 0; j < num_lines; j++) {
            cache.sets[i].lines[j].valid_bit = 0;
            cache.sets[i].lines[j].tag_value = -1;
            cache.sets[i].lines[j].last_used = 0;
        }
    }
}

// 打印帮助信息
void displayUsage() {
    printf("Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("  linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

// 模拟缓存访问
void accessCache(size_t address, int is_modify) {
    int set_index = (address >> b_bits) & ((1 << s_bits) - 1); // 计算组索引
    int tag = address >> (b_bits + s_bits); // 计算标签
    CacheSet* set = &cache.sets[set_index];
    int lru_index = 0;
    int lru_time = set->lines[0].last_used;

    // 遍历当前组，检查命中或找到LRU
    for (int i = 0; i < E_lines; i++) {
        if (set->lines[i].valid_bit && set->lines[i].tag_value == tag) {
            total_hits++;
            set->lines[i].last_used = current_time;
            if (is_modify) total_hits++; // 修改操作等于两次命中
            return;
        }
        if (set->lines[i].last_used < lru_time) {
            lru_time = set->lines[i].last_used;
            lru_index = i;
        }
    }

    // 缓存未命中
    total_misses++;
    if (set->lines[lru_index].valid_bit) total_evictions++;

    set->lines[lru_index].valid_bit = 1;
    set->lines[lru_index].tag_value = tag;
    set->lines[lru_index].last_used = current_time;
    if (is_modify) total_hits++;
}

// 处理指令
void handleInstruction(char type, size_t addr, int size) {
    if (verbose_flag) printf("%c %lx,%d ", type, addr, size);

    switch (type) {
        case 'L': // Load
        case 'S': // Store
            accessCache(addr, 0);
            break;
        case 'M': // Modify
            accessCache(addr, 1);
            break;
        default:
            break;
    }
    if (verbose_flag) printf("\n");
}

// 释放缓存
void freeCache() {
    for (int i = 0; i < cache.num_sets; i++) {
        free(cache.sets[i].lines);
    }
    free(cache.sets);
}

int main(int argc, char* argv[]) {
    int option;
    FILE* trace_file = NULL;

    while ((option = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (option) {
            case 'h':
                displayUsage();
                return 0;
            case 'v':
                verbose_flag = 1;
                break;
            case 's':
                s_bits = atoi(optarg);
                break;
            case 'E':
                E_lines = atoi(optarg);
                break;
            case 'b':
                b_bits = atoi(optarg);
                break;
            case 't':
                trace_file = fopen(optarg, "r");
                break;
            default:
                displayUsage();
                return 1;
        }
    }

    if (s_bits <= 0 || E_lines <= 0 || b_bits <= 0 || !trace_file) {
        displayUsage();
        return 1;
    }

    int num_sets = 1 << s_bits; // 计算组数
    initCache(num_sets, E_lines);

    char operation;
    size_t address;
    int size;

    while (fscanf(trace_file, " %c %lx,%d", &operation, &address, &size) == 3) {
        current_time++;
        handleInstruction(operation, address, size);
    }

    fclose(trace_file);
    freeCache();
    printSummary(total_hits, total_misses, total_evictions);
    return 0;
}
