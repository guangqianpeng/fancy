//
// Created by frank on 17-6-2.
//

#include <assert.h>
#include <stdio.h>
#include "buffer.h"

void print(buffer *b) {
    const char *c = buffer_peek(b);
    for (int i = 0; i < buffer_readable_bytes(b); ++i) {
        putchar(c[i]);
    }
    putchar('\n');
}

int main()
{
    mem_pool    *p = mem_pool_create(MEM_POOL_DEFAULT_SIZE);
    buffer      *b = buffer_create(p, 10);

    /* empty buffer */
    assert(buffer_readable_bytes(b) == 0);
    assert(buffer_writable_bytes(b) == 10);
    assert(buffer_internal_capacity(b) == 10);
    print(b);

    buffer_append(b, "0123456789", 10);
    assert(buffer_readable_bytes(b) == 10);
    assert(buffer_writable_bytes(b) == 0);
    assert(buffer_internal_capacity(b) == 10);
    print(b);

    buffer_append(b, "1", 1);
    assert(buffer_readable_bytes(b) == 11);
    assert(buffer_writable_bytes(b) == 0);
    assert(buffer_internal_capacity(b) == 11);
    print(b);

    const char *c = buffer_retrieve(b, 1);
    assert(*c == '0');
    assert(buffer_readable_bytes(b) == 10);
    assert(buffer_writable_bytes(b) == 0);
    assert(buffer_internal_capacity(b) == 11);
    print(b);

    c = buffer_retrieve_all(b);
    assert(*c == '1');
    assert(buffer_readable_bytes(b) == 0);
    assert(buffer_writable_bytes(b) == 11);
    assert(buffer_internal_capacity(b) == 11);
    print(b);

    /* 完全回收 */
    buffer_destroy(b);
    assert((u_char*)b == p->last);

    printf("OK");
}