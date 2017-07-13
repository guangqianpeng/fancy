//
// Created by frank on 17-2-9.
//

#ifndef FANCY_RBTREE_H
#define FANCY_RBTREE_H

#include <sys/types.h>

typedef struct rbtree       rbtree;
typedef struct rbtree_node  rbtree_node;
typedef unsigned long       rbtree_key;

struct rbtree {
    rbtree_node     *root;
    rbtree_node     *sentinel;
};

struct rbtree_node {
    rbtree_key      key;
    rbtree_node     *left;
    rbtree_node     *right;
    rbtree_node     *parent;
    u_char          color;
};

void rbtree_init(rbtree *tree, rbtree_node *sentinel);
int rbtree_empty(rbtree *tree);
void rbtree_insert(rbtree *tree, rbtree_node *node);
void rbtree_delete(rbtree *tree, rbtree_node *node);
rbtree_node* rbtree_min(rbtree *tree);

/* debug */
int rbtree_is_regular(rbtree *tree);

#endif //FANCY_RBTREE_H
