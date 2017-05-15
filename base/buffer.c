//
// Created by frank on 17-2-13.
//

#include <assert.h>
#include "buffer.h"

buffer *buffer_create(mem_pool *p, size_t size)
{
    buffer *b;

    b = palloc(p, sizeof(buffer));
    if (b == NULL) {
        return NULL;
    }

    b->start = palloc(p, size + 1);
    if (b->start == NULL) {
        return NULL;
    }

    b->end = b->start + size;
    b->data_start = b->data_end = b->start;
    *b->data_end = '\0';

    return b;
}

