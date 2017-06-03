//
// Created by frank on 17-2-13.
//

#include <assert.h>
#include <string.h>
#include <sys/uio.h>
#include "base.h"
#include "buffer.h"

static void buffer_make_space(buffer *b, size_t len);

buffer *buffer_create(mem_pool *p, size_t size)
{
    buffer *b = palloc(p, sizeof(buffer));
    if (b == NULL) {
        return NULL;
    }

    b->data = array_create(p, size, sizeof(char));
    if (b->data == NULL) {
        return NULL;
    }

    b->data->size = size;

    b->read_index = b->write_index = 0;

    return b;
}

void buffer_destroy(buffer *b)
{
    array_destroy(b->data);
    if ((char*)b + sizeof(buffer) == b->data->pool->last) {
        b->data->pool->last -= sizeof(buffer);
    }
}

int buffer_empty(buffer *b)
{
    return buffer_readable_bytes(b) == 0;
}

size_t buffer_readable_bytes(buffer *b)
{
    assert(b->write_index >= b->read_index);
    return b->write_index - b->read_index;
}

size_t buffer_writable_bytes(buffer *b)
{
    assert(b->data->size >= b->write_index);
    return b->data->size - b->write_index;
}

char *buffer_peek(buffer *b)
{
    return array_at(b->data, b->read_index);
}

char *buffer_retrieve(buffer *b, size_t len)
{
    assert(len <= buffer_readable_bytes(b));
    char *ret = buffer_peek(b);
    if (len < buffer_readable_bytes(b)) {
        b->read_index += len;
    }
    else {
        buffer_retrieve_all(b);
    }
    return ret;
}

char *buffer_retrieve_until(buffer *b, const char *end)
{
    assert(buffer_peek(b) <= end);
    assert(end <= buffer_begin_write(b));
    return buffer_retrieve(b, end - buffer_peek(b));
}

char *buffer_retrieve_all(buffer *b)
{
    char *ret = buffer_peek(b);
    b->read_index = b->write_index = 0;
    return ret;
}

void buffer_transfer(buffer *dst, buffer *src)
{
    size_t readable = buffer_readable_bytes(src);

    assert(readable > 0);

    char *data = buffer_retrieve_all(src);
    buffer_append(dst, data, readable);
}

void buffer_append(buffer *b, const char *data, size_t len)
{
    buffer_ensure_writable_bytes(b, len);
    memcpy(buffer_begin_write(b), data, len);
    buffer_has_writen(b, len);
}

void buffer_ensure_writable_bytes(buffer *b, size_t len)
{
    if (buffer_writable_bytes(b) < len) {
        buffer_make_space(b, len);
    }
    assert(buffer_writable_bytes(b) >= len);
}

char *buffer_begin_write(buffer *b)
{
    assert(b->write_index <= b->data->size);
    return b->data->elems + b->write_index;
}

void buffer_has_writen(buffer *b, size_t len)
{
    assert(len <= buffer_writable_bytes(b));
    b->write_index += len;
}

void buffer_unwrite(buffer *b, size_t len)
{
    assert(len <= buffer_readable_bytes(b));
    b->write_index -= len;
}

size_t buffer_internal_capacity(buffer *b)
{
    return b->data->capacity;
}

ssize_t buffer_read_fd(buffer *b, int fd, int *saved_errno)
{
    char    extra_buf[65536];
    size_t  writable = buffer_writable_bytes(b);

    struct iovec vec[2] = {{
                    .iov_base = b->data->elems + b->write_index,
                    .iov_len = writable, }, {
                    .iov_base = extra_buf,
                    .iov_len = sizeof(extra_buf),
            },};

    int     iovcnt = (writable < sizeof(extra_buf)) ? 2 : 1;
    ssize_t n = readv(fd, vec, iovcnt);

    if (n == -1) {
       *saved_errno = errno;
    }
    else if (n <= writable) {
        b->write_index += n;
    }
    else {
        b->write_index = b->data->size;
        assert(buffer_writable_bytes(b) == 0);
        buffer_append(b, extra_buf, n - writable);
    }

    return n;
}

ssize_t buffer_write_fd(buffer *b, int fd, int *saved_errno)
{
    size_t  readable = buffer_readable_bytes(b);
    ssize_t n = write(fd, buffer_peek(b), readable);
    if (n == -1) {
        *saved_errno = errno;
    }
    else {
        buffer_retrieve(b, (size_t)n);
    }
    return n;
}

static void buffer_make_space(buffer *b, size_t len)
{
    if (b->read_index + buffer_writable_bytes(b) < len) {
        array_resize(b->data, b->write_index + len);
    }
    else {
        size_t readable = buffer_readable_bytes(b);
        memmove(b->data->elems,
                array_at(b->data, b->read_index),
                readable);
        b->read_index = 0;
        b->read_index = readable;
        assert(readable == buffer_readable_bytes(b));
    }
}