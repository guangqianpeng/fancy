//
// Created by frank on 17-2-10.
// dynamic array
//

#ifndef FANCY_ARRAY_H
#define FANCY_ARRAY_H

#include "palloc.h"

typedef struct array array;

struct array {
    char        *elems;     // element pointer
    size_t      elem_size;  // size of each elements
    size_t      size;       // number of elements
    size_t      capacity;   // capacity of elements
    mem_pool    *pool;      // associated memory pool
};

array *array_create(mem_pool *pool, size_t capacity, size_t elem_size);

/* destroy a array is ok */
void array_destroy(array *a);

void *array_alloc(array *a);

void *array_at(array *a, size_t i);

void array_resize(array *a, size_t size);

#endif //FANCY_ARRAY_H