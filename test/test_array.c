//
// Created by frank on 17-2-10.
//

#include <assert.h>
#include <stdio.h>
#include "../base/array.h"

static void print_array(array *a);

int main()
{
    mem_pool    *pool;
    array       *a;
    long        *elem, *temp;

    pool = mem_pool_create(100 * sizeof(long) + sizeof(array) +sizeof(mem_pool));
    palloc(pool, 75 * sizeof(long));

    assert(pool->end - pool->last == 25 * sizeof(long) + sizeof(array));

    /* 初始化数组， 没有再分配 */
    a = array_create(pool, 24, sizeof(long));
    temp = a->elems;

    assert(pool->end - pool->last == 1 * sizeof(long));

    for (int i = 1; i <= 24; ++i) {
        elem = array_alloc(a);
        *elem = i;
    }

    assert(temp == a->elems);
    assert(a->capacity == 24);
    assert(a->size == 24);
    assert((u_char*)a->elems + a->capacity * a->elem_size == a->pool->last);

    elem = a->elems;
    for (size_t i = 0; i < a->size; ++i) {
        assert(elem[i] == i + 1);
    }


    /* 再分配1个元素，由于没有超出内存池边界，因此也不会再分配 */
    temp = a->elems;

    elem = array_alloc(a);
    *elem = 25;

    assert(temp == a->elems);
    assert(a->capacity == 25);
    assert(a->size == 25);
    assert((u_char*)a->elems + a->capacity * a->elem_size == a->pool->last);

    /* 再分配1个元素，触发再分配
     * 此时内存池回收旧的a->elems块，不回收数组头部
     * */
    temp = a->elems;

    elem = array_alloc(a);
    *elem = 26;

    assert(temp != a->elems);
    assert(a->capacity == 50);
    assert(a->size == 26);
    assert((u_char*)a->elems + a->capacity * a->elem_size == a->pool->next->last);

    /* destroy无效 */
    array_destroy(a);
    assert((u_char*)a->elems + a->capacity * a->elem_size == a->pool->next->last);

    print_array(a);

    /* destroy有效
     * 此时会在内存池的第一块分配新数组
     * */
    a = array_create(pool, 1, sizeof(long));
    array_destroy(a);
    assert((u_char*)a->elems + a->capacity * a->elem_size - a->pool->last ==
                   sizeof(array) + sizeof(long));
    printf("OK");
}

static void print_array(array *a)
{
    long *elems;

    printf("sizeof(elem) = %lu\n", a->elem_size);
    printf("size = %lu\n", a->size);
    printf("capacity = %lu\n", a->capacity);

    elems = a->elems;

    for (size_t i = 0; i < a->size; ++i) {
        printf("%ld, ", elems[i]);
    }
    printf("\n");
}