//
// Created by frank on 17-2-10.
// simple memory pool
//

#ifndef FANCY_MEM_POOL_H
#define FANCY_MEM_POOL_H

#include <sys/types.h>

#define MEM_POOL_DEFAULT_SIZE   (128 * 1024)

/* memory alignment you can change it before compile
 * (e.g., 64 bytes for cache line alignment) */
#define MEM_POOL_ALIGNMENT      sizeof(unsigned long)

/* align pointer to nearest lower bound */
#define align_ptr(ptr, alignment) \
((typeof(ptr)) (((u_int64_t)ptr + (alignment - 1)) & ~(alignment - 1)))

typedef struct mem_pool mem_pool;

struct mem_pool {
    char        *last;
    char        *end;
    u_int       failed;
    mem_pool    *next;

    /* header node only */
    mem_pool    *current;
};

mem_pool *mem_pool_create(size_t size);
void mem_pool_destroy(mem_pool *pool);

/* return aligned pointer, just like malloc */
void *palloc(mem_pool *pool, size_t size);
void *pcalloc(mem_pool *pool, size_t size);

#endif //FANCY_MEM_POOL_H
