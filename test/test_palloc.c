//
// Created by frank on 17-2-10.
//

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "../base/palloc.h"

#define LOOP_SIZE 1000

static void test_once();
static int check_pool(mem_pool *pool, size_t alloc);
static void print_pool(mem_pool *pool);

int main()
{
    for (int i = 0; i < LOOP_SIZE; ++i) {
        test_once();
    }
    printf("OK");
}

static void test_once()
{
    mem_pool    *pool;
    void        *data;
    size_t      bytes, alloc;

    pool = mem_pool_create(MEM_POOL_DEFAULT_SIZE);
    if (pool == NULL) {
        exit(1);
    }

    alloc = 0;

    for (int i = 0; i < LOOP_SIZE; ++i) {
        bytes = (size_t)rand() % (pool->end - (u_char*)pool - sizeof(mem_pool)) + 1;

        /* palloc的对齐操作会造成内部碎片，难以验证内存池的正确性
         * 因此总是分配对齐的整数倍字节
         * */
        bytes = bytes / MEM_POOL_ALIGNMENT * MEM_POOL_ALIGNMENT;

        /* 返回的指针应该总是对齐的 */
        data = palloc(pool, bytes);
        if (data == NULL){
            exit(1);
        }
        assert(((u_int64_t)data & ~(MEM_POOL_ALIGNMENT - 1)) == (u_int64_t)data);

        alloc += bytes;
        assert(check_pool(pool, alloc));
    }

    print_pool(pool);

    mem_pool_destroy(pool);
}

static int check_pool(mem_pool *pool, size_t alloc)
{
    size_t      size;
    mem_pool    *p;

    size = 0;
    for (p = pool; p != NULL; p = p->next) {
        if (p == pool) {
            size += p->last - ((u_char*)p + sizeof(mem_pool));
        }
        else {
            size += p->last - ((u_char*)p + offsetof(mem_pool, current));
        }
    }
    // printf("real alloc = %lu, calcu alloc = %lu\n", size, alloc);

    return size == alloc;
}

static void print_pool(mem_pool *pool)
{
    u_char      *first;
    mem_pool    *p;
    size_t      alloc, free;

    for (p = pool; p; p = p->next) {
        if (p == pool) {
            first = (u_char *) p + sizeof(mem_pool);
        }
        else {
            first = (u_char *) p + offsetof(mem_pool, current);
        }

        alloc = p->last - first;
        free = p->end - p->last;
        printf("alloc = %lu,\tfree = %lu,\tfailed = %u\n", alloc, free, p->failed);
    }
}