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

int buffer_empty(buffer *b)
{
    return b->data_start == b->data_end;
}

int buffer_full(buffer *b)
{
    return b->data_end == b->end;
}

void buffer_reset(buffer *b) {
    b->data_start = b->data_end = b->start;
    *b->data_end = '\0';
}

int buffer_write(buffer *b, const void *src, size_t size)
{
    if (b->end - b->data_end < size) {
        return -1;
    }

    memcpy(b->data_end, src, size);
    b->data_end += size;

    *b->data_end = '\0';

    return 0;
}

size_t buffer_size(buffer* b)
{
    return b->data_end - b->data_start;
}

void *buffer_read(buffer *b)
{
    return b->data_start;
}

void *buffer_seek_start(buffer *b, int offset)
{
    b->data_start += offset;

    assert(b->data_start <= b->data_end);
    assert(b->data_start >= b->start);

    if (b->data_start == b->data_end) {
        b->data_start = b->data_end = b->start;
        *b->data_end = '\0';
    }

    return b->data_start;
}

void *buffer_seek_end(buffer *b, int offset)
{
    b->data_end += offset;

    assert(b->data_end >= b->data_start);
    assert(b->data_end <= b->end);

    *b->data_end = '\0';

    return b->data_start;
}