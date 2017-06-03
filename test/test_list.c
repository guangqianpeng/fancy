//
// Created by frank on 17-2-12.
//

#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include "list.h"

typedef struct Node {
    int         data;
    list_node   node;
} Node;

int main()
{
    list h;
    Node nodes[10];
    Node *head;

    for (int i = 0; i < 10; ++i) {
        nodes[i].data = i;
    }

    list_init(&h);
    assert(list_empty(&h));

    /* 插入10个元素 */
    for (int i = 0; i < 10; ++i) {
        list_insert_head(&h, &nodes[i].node);
        head = link_data(&nodes[i].node, Node, node);
        assert(head->data == i);
    }

    /* 删除尾部元素0 */
    head = link_data(h.prev, Node, node);
    assert(head->data == 0);
    list_remove(h.prev);

    /* 顺序删除 */
    for (int i = 0; i < 9; ++i) {
        list_node *x = list_head(&h);
        head = link_data(x, Node, node);
        assert(head->data == 9 - i);
        list_remove(x);
    }

    assert(list_empty(&h));
    printf("OK");
}