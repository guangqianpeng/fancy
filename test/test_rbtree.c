//
// Created by frank on 17-2-9.
//

#include <assert.h>
#include <stdio.h>
#include "../base/rbtree.h"

#define TREE_SIZE 50

static int rbtree_size(rbtree_node *node, rbtree_node *sentinel);

int main() {
    rbtree      tree;
    rbtree_node sentinel;
    rbtree_node nodes[TREE_SIZE];
    rbtree_key  keys[TREE_SIZE] = {
            1 ,10,12,18,19,21,26,24,6 ,8 ,
            11,13,15,17,22,25,27,28,30,68,
            35,36,37,38,39,40,41,42,43,44,
            67,66,65,64,63,62,61,60,59,58,
            51,52,53,54,55,56,57,69,70,70,
    };

    /* 空树 */
    rbtree_init(&tree, &sentinel);
    assert(rbtree_is_regular(&tree));
    assert(rbtree_size(tree.root, &sentinel) == 0);

    /* 逐个node添加 */
    for (int i = 0; i < TREE_SIZE; ++i) {
        nodes[i].key = keys[i];
        rbtree_insert(&tree, &nodes[i]);
        assert(rbtree_is_regular(&tree));
        assert(rbtree_size(tree.root, &sentinel) == i + 1);
        assert(rbtree_min(&tree)->key == 1);
    }

    /* 翻转一个node的颜色 */
    nodes[TREE_SIZE - 1].color = !nodes[TREE_SIZE - 1].color;
    assert(!rbtree_is_regular(&tree));
    nodes[TREE_SIZE - 1].color = !nodes[TREE_SIZE - 1].color;
    assert(rbtree_is_regular(&tree));

    /* 逐个node删除 */
    for (int i = 0; i < TREE_SIZE; ++i) {
        rbtree_delete(&tree, &nodes[i]);
        assert(rbtree_is_regular(&tree));
        assert(rbtree_size(tree.root, &sentinel) == TREE_SIZE - i - 1);
    }

    printf("OK");
}

static int rbtree_size(rbtree_node *node, rbtree_node *sentinel) {
    if (node == sentinel)
        return 0;
    return rbtree_size(node->left, sentinel) +
            rbtree_size(node->right, sentinel) + 1;
}