//
// Created by frank on 17-2-13.
//

#ifndef FANCY_BUFFER_H
#define FANCY_BUFFER_H

#include "base.h"
#include "palloc.h"

#define BUFFER_DEFAULT_SIZE (1024 - 1)

typedef struct buffer buffer;

struct buffer {
    u_char *start;
    u_char *end;

    u_char *data_start;
    u_char *data_end;
};

buffer *buffer_create(mem_pool *p, size_t size);

inline int buffer_empty(buffer *b)
{
    return b->data_start == b->data_end;
}

inline int buffer_full(buffer *b)
{
    return b->data_end == b->end;
}

inline void buffer_reset(buffer *b) {
    b->data_start = b->data_end = b->start;
    *b->data_end = '\0';
}

inline int buffer_write(buffer *b, const void *src, size_t size)
{
    if (b->end - b->data_end < size) {
        return -1;
    }

    memcpy(b->data_end, src, size);
    b->data_end += size;

    *b->data_end = '\0';

    return 0;
}

inline size_t buffer_size(buffer* b)
{
    return b->data_end - b->data_start;
}

inline void *buffer_read(buffer *b)
{
    return b->data_start;
}

inline void *buffer_seek_start(buffer *b, int offset)
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

inline void *buffer_seek_end(buffer *b, int offset)
{
    b->data_end += offset;

    assert(b->data_end >= b->data_start);
    assert(b->data_end <= b->end);

    *b->data_end = '\0';

    return b->data_start;
}

#endif //FANCY_BUFFER_H