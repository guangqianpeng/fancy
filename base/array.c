//
// Created by frank on 17-2-10.
//

#include <stddef.h>
#include <string.h>
#include <assert.h>
#include "array.h"

array *array_create(mem_pool *pool, size_t capacity, size_t elem_size)
{
    array *a = palloc(pool, sizeof(array));
    if (a == NULL) {
        return NULL;
    }

    a->elems = palloc(pool, capacity * elem_size);
    if (a->elems == NULL) {
        return NULL;
    }

    a->elem_size = elem_size;
    a->size = 0;
    a->capacity = capacity;
    a->pool = pool;

    return a;
}

void array_destroy(array *a)
{
    if ((u_char*)a->elems + a->capacity * a->elem_size == a->pool->last) {
        a->pool->last -= a->capacity * a->elem_size;
    }

    if ((u_char*)a + sizeof(array) == a->pool->last) {
        a->pool->last -= sizeof(array);
    }
}

void *array_alloc(array *a)
{
    void    *new_elems;
    u_char  *next;

    next = a->elems + a->size * a->elem_size;

    // capacity足够
    if (a->capacity > a->size) {
        ++a->size;
    }
        // capacity不够，但内存池足够
    else if (next == a->pool->last
            && a->pool->last + a->elem_size <= a->pool->end) {
        a->pool->last += a->elem_size;
        ++a->size;
        ++a->capacity;
    }
        // 重新分配
    else {
        new_elems = palloc(a->pool, 2 * a->capacity * a->elem_size);
        if (new_elems == NULL) {
            return NULL;
        }

        memcpy(new_elems, a->elems, a->size * a->elem_size);

        /* 回收旧数组的elems */
        if ((u_char*)a->elems + a->capacity * a->elem_size == a->pool->last) {
            a->pool->last -= a->capacity * a->elem_size;
        }

        a->elems = new_elems;

        next = a->elems + a->size * a->elem_size;

        ++a->size;
        a->capacity *= 2;
    }

    return next;
}

void *array_at(array *a, size_t i)
{
    assert(i < a->size);
    return (u_char*)a->elems + i * a->elem_size;
}