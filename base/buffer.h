//
// Created by frank on 17-2-13.
//

#ifndef FANCY_BUFFER_H
#define FANCY_BUFFER_H

#include "array.h"
#include "fcy_str.h"

#define BUFFER_INIT_SIZE 1024

typedef struct buffer buffer;

struct buffer {
    array *data;
    size_t read_index;
    size_t write_index;
};

buffer *buffer_create(mem_pool *p, size_t size);
void buffer_destroy(buffer *b);

int buffer_empty(buffer *b);

size_t buffer_readable_bytes(buffer *b);
size_t buffer_writable_bytes(buffer *b);

char *buffer_peek(buffer *b);
char *buffer_retrieve(buffer *b, size_t len);
char *buffer_retrieve_until(buffer *b, const char *end);
char *buffer_retrieve_all(buffer *b);
void buffer_transfer(buffer *dst, buffer *src);

void buffer_append(buffer *b, const char *data, size_t len);
void buffer_ensure_writable_bytes(buffer *b, size_t len);
char *buffer_begin_write(buffer *b);
void buffer_has_writen(buffer *b, size_t len);
void buffer_unwrite(buffer *b, size_t len);
size_t buffer_internal_capacity(buffer *b);
ssize_t buffer_read_fd(buffer *b, int fd, int *saved_errno);
ssize_t buffer_write_fd(buffer *b, int fd, int *saved_errno);

#define buffer_append_space(b) \
buffer_append(b, " ", 1)
#define buffer_append_crlf(b) \
buffer_append(b, "\r\n", 2)
#define buffer_append_str(b, s); \
{ typeof(s) s_ = (s); buffer_append(b, s_->data, s_->len); }
#define buffer_append_literal(b, literal) \
buffer_append(b, literal, sizeof(literal) - 1)

#endif //FANCY_BUFFER_H