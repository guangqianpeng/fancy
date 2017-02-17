//
// Created by frank on 17-2-10.
//

#ifndef FANCY_ARRAY_H
#define FANCY_ARRAY_H

#include "base.h"
#include "palloc.h"

typedef struct array array;

struct array {
    void        *elems;     // 元素指针
    size_t      elem_size;  // 元素大小
    size_t      size;       // 元素个数
    size_t      capacity;   // 容量
    mem_pool    *pool;      // 内存池
};

array *array_create(mem_pool *pool, size_t capacity, size_t elem_size);
/* 销毁数组不是必须的，因为内存池会帮我们统一回收
 * 但是，array是使用最频繁的的数据结构，为提高内存利用率，可以手动destroy
 * */
void array_destroy(array *a);

void *array_alloc(array *a);

#endif //FANCY_ARRAY_H