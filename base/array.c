//
// Created by frank on 17-2-10.
//

#include <string.h>
#include <assert.h>
#include "log.h"
#include "array.h"

#define min(a,b) \
  ({ typeof (a) _a = (a); \
      typeof (b) _b = (b); \
    _a < _b ? _a : _b; })
#define max(a,b) \
  ({ typeof (a) _a = (a); \
      typeof (b) _b = (b); \
    _a > _b ? _a : _b; })

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
    if (a->elems + a->capacity * a->elem_size == a->pool->last) {
        a->pool->last -= a->capacity * a->elem_size;
    }

    if ((char*)a + sizeof(array) == a->pool->last) {
        a->pool->last -= sizeof(array);
    }
}

void *array_alloc(array *a)
{
    void    *new_elems;
    char    *size_next;
    char    *capacity_next;

    size_next = a->elems + a->size * a->elem_size;
    capacity_next = a->elems + a->capacity * a->elem_size;

    if (a->capacity > a->size) {
        // capacity is enough
        ++a->size;
    }
    else if (capacity_next == a->pool->last
            && a->pool->last + a->elem_size <= a->pool->end) {
        // capacity is't enough, but memory pool has more space
        a->pool->last += a->elem_size;
        ++a->size;
        ++a->capacity;
    }
    else {
        // new memory block is needed
        new_elems = palloc(a->pool, 2 * a->capacity * a->elem_size);
        if (new_elems == NULL) {
            return NULL;
        }

        memcpy(new_elems, a->elems, a->size * a->elem_size);

        /* free old elements */
        if (a->elems + a->capacity * a->elem_size == a->pool->last) {
            a->pool->last -= a->capacity * a->elem_size;
        }

        a->elems = new_elems;

        size_next = a->elems + a->size * a->elem_size;

        ++a->size;
        a->capacity *= 2;
    }

    return size_next;
}

void *array_n_alloc(array *a, size_t n)
{
    void    *new_elems;
    char    *size_next;
    char    *capacity_next;
    size_t  new_size;

    size_next = a->elems + a->size * a->elem_size;
    capacity_next = a->elems + a->capacity * a->elem_size;
    new_size = a->size + n;

    if (a->capacity >= a->size + n) {
        // capacity is enough
        a->size = new_size;
    }
    else if (capacity_next == a->pool->last
             && a->pool->last + n * a->elem_size <= a->pool->end) {
        // new memory block is needed
        a->pool->last += n * a->elem_size;
        a->size += n;
        a->capacity = a->size;
    }
    else {
        // new memory block is needed
        size_t s = max(2 * a->capacity, new_size);

        new_elems = palloc(a->pool, s * a->elem_size);
        if (new_elems == NULL) {
            return NULL;
        }

        memcpy(new_elems, a->elems, a->size * a->elem_size);

        if ((char*)a->elems + a->capacity * a->elem_size == a->pool->last) {
            /* free old elements */
            a->pool->last -= a->capacity * a->elem_size;
        }

        a->elems = new_elems;

        size_next = a->elems + a->size * a->elem_size;

        a->size = new_size;
        a->capacity = s;
    }
    return size_next;
}

void *array_at(array *a, size_t i)
{
    assert(i < a->size);
    return a->elems + i * a->elem_size;
}

void array_resize(array *a, size_t size)
{
    if (size <= a->capacity) {
        a->size = size;
    }
    else {
        char *ret = array_n_alloc(a, size - a->size);
        if (ret == NULL) {
            LOG_FATAL("resize failed");
        }
    }
}