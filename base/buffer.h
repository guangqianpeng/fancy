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

int buffer_empty(buffer *b);
int buffer_full(buffer *b);
void buffer_reset(buffer *b);
int buffer_write(buffer *b, const void *src, size_t size);
size_t buffer_size(buffer* b);
size_t buffer_space(buffer* b);
void *buffer_read(buffer *b);
void *buffer_seek_start(buffer *b, int offset);
void *buffer_seek_end(buffer *b, int offset);

#endif //FANCY_BUFFER_H