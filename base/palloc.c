//
// Created by frank on 17-2-10.
//

#include <assert.h>
#include "base.h"
#include "palloc.h"

/* append a new memory block */
static mem_pool *mem_pool_append(mem_pool *pool);

mem_pool *mem_pool_create(size_t size)
{
    mem_pool *pool = NULL;
    int      val;

    val = posix_memalign((void**)&pool, MEM_POOL_ALIGNMENT, size);
    if (val == -1 || pool == NULL) {
        assert(0);
        return NULL;
    }

    pool->last = (char *) pool + sizeof(mem_pool);
    pool->end = (char *) pool + size;
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
    char      *last;
    mem_pool  *p;

    for (p = pool->current; p; p = p->next) {

        last = align_ptr(p->last, MEM_POOL_ALIGNMENT);

        if (p->end - last >= size) {
            p->last = last + size;
            return last;
        }
    }

    assert(size <= pool->end - (char *)pool - offsetof(mem_pool, current));

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
    char *last = palloc(pool, size);
    if (last == NULL) {
        return NULL;
    }

    bzero(last, size);
    return last;
}

static mem_pool *mem_pool_append(mem_pool *pool)
{
    size_t      size;
    mem_pool    *new = NULL, *p;
    int         val;

    size = pool->end - (char *)pool;
    val = posix_memalign((void**)&new, MEM_POOL_ALIGNMENT, size);
    if (val == -1 || new == NULL) {
        assert(0);
        return NULL;
    }

    new->last = (char *)new + offsetof(mem_pool, current);
    new->last = align_ptr(new->last, MEM_POOL_ALIGNMENT);

    new->end = (char *)new + size;
    new->failed = 0;
    new->next = NULL;

    /* for memory block failed more than 5 times,
     * jump to the next block
     * */
    for (p = pool->current; p->next; p = p->next) {
        if (++p->failed >= 5) {
            pool->current = p->next;
        }
    }

    p->next = new;

    return new;
}