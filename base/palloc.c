//
// Created by frank on 17-2-10.
//

#include <assert.h>
#include "palloc.h"

static void *align_ptr(void *ptr, size_t alignment);

/* 追加一个新的空闲内存块 */
static mem_pool *mem_pool_append(mem_pool *pool);

mem_pool *mem_pool_create(size_t size)
{
    mem_pool *pool = NULL;
    int      val;

    val = posix_memalign((void**)&pool, MEM_POOL_ALIGNMENT, size);
    if (val == -1 || pool == NULL) {
        return NULL;
    }

    pool->last = (u_char*) pool + sizeof(mem_pool);
    pool->end = (u_char*) pool + size;
    pool->failed = 0;
    pool->next = NULL;
    pool->current = pool;

    return pool;
}

void mem_pool_destroy(mem_pool *pool)
{
    mem_pool *temp;

    while (pool != NULL) {
        temp = pool;
        pool = pool->next;
        free(temp);
    }
}

void *palloc(mem_pool *pool, size_t size)
{
    u_char      *last;
    mem_pool    *p;

    for (p = pool->current; p; p = p->next) {

        /* 对齐指针 */
        last = align_ptr(p->last, MEM_POOL_ALIGNMENT);

        if (p->end - last >= size) {
            p->last = last + size;
            return last;
        }
    }

    assert(size <= pool->end - (u_char*)pool - offsetof(mem_pool, current));

    p = mem_pool_append(pool);
    if (p == NULL) {
        return NULL;
    }

    last = p->last;
    p->last += size;

    return last;
}

void *pcalloc(mem_pool *pool, size_t size)
{
    u_char *last = palloc(pool, size);
    if (last == NULL) {
        return NULL;
    }

    bzero(last, size);
    return last;
}

static void *align_ptr(void *ptr, size_t alignment)
{
    return (void*) (((u_int64_t)ptr + (alignment - 1)) & ~(alignment - 1));
}

static mem_pool *mem_pool_append(mem_pool *pool)
{
    size_t   size;
    mem_pool *new, *p;

    size = pool->end - (u_char*)pool;
    new = malloc(size);
    if (new == NULL) {
        return NULL;
    }

    new->last = (u_char*)new + offsetof(mem_pool, current);
    new->last = align_ptr(new->last, MEM_POOL_ALIGNMENT);

    new->end = (u_char*)new + size;
    new->failed = 0;
    new->next = NULL;

    /* 若内存空闲不足发生5次，则不再使用它分配
     * failed最大值为6
     * */
    for (p = pool->current; p->next; p = p->next) {
        if (p->failed++ > 4) {
            pool->current = p->next;
        }
    }

    p->next = new;

    return new;
}