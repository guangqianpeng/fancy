//
// Created by frank on 17-2-9.
//

#include <stddef.h>
#include "rbtree.h"

#define RED     0
#define BLACK   1

static void rotate_left(rbtree *tree, rbtree_node *node);
static void rotate_right(rbtree *tree, rbtree_node *node);

static void insert(rbtree *tree, rbtree_node *node);
static void transplant(rbtree *tree, rbtree_node *p, rbtree_node *c); /* remove p, imporve c */
static void delete_fix(rbtree *tree, rbtree_node *node);

static rbtree_node* min(rbtree_node *node, rbtree_node *sentinel);

static int is_red(rbtree_node *node);
static int is_black(rbtree_node *node);
static void set_red(rbtree_node *node);
static void set_black(rbtree_node *node);

/* for rbtree_is_regular */
static int balck_height(rbtree_node *node, rbtree_node *sentinel);
static int is_regular(rbtree_node *node, rbtree_node *sentinel);

void rbtree_init(rbtree *tree, rbtree_node *sentinel)
{
    rbtree_node *root;

    root = tree->root = sentinel;
    tree->sentinel = sentinel;

    set_black(root);

    root->left = NULL;
    root->right = NULL;

    /* 该字段在删除节点时有用 */
    root->parent = NULL;
}

int rbtree_empty(rbtree *tree)
{
    return tree->root == tree->sentinel;
}

void rbtree_insert(rbtree *tree, rbtree_node *node)
{
    rbtree_node *temp;

    /* 空树 */
    if (tree->root == tree->sentinel) {
        set_black(node);
        node->left = tree->sentinel;
        node->right = tree->sentinel;
        node->parent = tree->sentinel;
        tree->root = node;
        return;
    }

    insert(tree, node);

    /* 因为is_black(sentinel) && root.parent==sentinel，所以node==root时也会跳出循环 */
    while (is_red(node->parent)) {
        if (node->parent == node->parent->parent->left) {
            temp = node->parent->parent->right;
            if (is_red(temp)) {
                set_black(node->parent);
                set_black(temp);
                set_red(temp->parent);
                node = node->parent->parent;
            }
            else {
                if (node->parent->right == node) {
                    rotate_left(tree, node->parent);
                    node = node->left;
                }

                set_black(node->parent);
                set_red(node->parent->parent);
                rotate_right(tree, node->parent->parent);
                break;  /* 此时is_black(node.parent), 用break避免多余测试 */
            }
        }
        else {
            temp = node->parent->parent->left;
            if (is_red(temp)) {
                set_black(node->parent);
                set_black(temp);
                set_red(temp->parent);
                node = node->parent->parent;
            }
            else {
                if (node->parent->left == node) {
                    rotate_right(tree, node->parent);
                    node = node->right;
                }

                set_black(node->parent);
                set_red(node->parent->parent);
                rotate_left(tree, node->parent->parent);
                break;
            }
        }
    }

    set_black(tree->root);
}

void rbtree_delete(rbtree *tree, rbtree_node *node)
{
    /* subt是需要移动的节点， temp是subt的子树
     * color==subt.color
     * */
    rbtree_node *sentinel, *temp, *subt;
    u_char      color;

    sentinel = tree->sentinel;
    subt = node;
    color = subt->color;

    /* delete */
    if (node->left == sentinel) {
        temp = subt->right;
        transplant(tree, subt, temp);
    }
    else if (node->right == sentinel) {
        temp = subt->left;
        transplant(tree, subt, temp);
    }
    else {
        subt = min(node->right, tree->sentinel);
        color = subt->color;
        temp = subt->right;

        if (subt->parent != node) {
            transplant(tree, subt, temp);
            subt->right = node->right;
            subt->right->parent = subt;
        }
        else {
            /* 此处操作是为了将可能的sentinel.parent字段指向subt, 保证删除操作的正确性 */
            temp->parent = subt;
        }

        transplant(tree, node, subt);
        subt->left = node->left;
        subt->left->parent = subt;
        subt->color = node->color;
    }

    /* fix */
    if (color == BLACK) {
        delete_fix(tree, temp);
    }
}

rbtree_node* rbtree_min(rbtree *tree)
{
    return min(tree->root, tree->sentinel);
}

int rbtree_is_regular(rbtree *tree)
{
    if (!is_black(tree->root) || !is_black(tree->sentinel))
        return 0;

    return is_regular(tree->root, tree->sentinel);
}

static void rotate_left(rbtree *tree, rbtree_node *node)
{
    rbtree_node *temp;

    temp = node->right;

    node->right = temp->left;
    /* 此处的检查是必须的，因为sentinel.parent字段在删除节点时有特殊用途
     * rotate_right也一样
     * */
    if (node->right != tree->sentinel) {
        node->right->parent = node;
    }

    temp->parent = node->parent;

    if (node == tree->root) {
        tree->root = temp;
    }
    else if (node == node->parent->left) {
        node->parent->left = temp;
    }
    else {
        node->parent->right = temp;
    }

    temp->left = node;
    node->parent = temp;
}

static void rotate_right(rbtree *tree, rbtree_node *node)
{
    rbtree_node *temp;

    temp = node->left;

    node->left = temp->right;
    if (node->left != tree->sentinel) {
        node->left->parent = node;
    }

    temp->parent = node->parent;

    if (tree->root == node) {
        tree->root = temp;
    }
    else if (node == node->parent->left) {
        node->parent->left = temp;
    }
    else {
        node->parent->right = temp;
    }

    temp->right = node;
    node->parent = temp;
}

static void insert(rbtree *tree, rbtree_node *node)
{
    rbtree_node **p, *temp;

    temp = tree->root;
    while (1) {
        if (node->key < temp->key) {
            p = &temp->left;
        }
        else {
            p = &temp->right;
        }

        if (*p == tree->sentinel)
            break;

        temp = *p;
    }

    set_red(node);
    node->left = tree->sentinel;
    node->right = tree->sentinel;
    node->parent = temp;
    *p = node;
}

static void transplant(rbtree *tree, rbtree_node *p, rbtree_node *c)
{
    if (p == tree->root) {
        tree->root = c;
    }
    else if (p == p->parent->left) {
        p->parent->left = c;
    }
    else {
        p->parent->right = c;
    }
    c->parent = p->parent;
}

static void delete_fix(rbtree *tree, rbtree_node *node)
{
    rbtree_node *brother;

    while (node != tree->root && is_black(node)) {
        if (node == node->parent->left) {
            brother = node->parent->right;

            if (is_red(brother)) {
                set_red(brother->parent);
                set_black(brother);
                rotate_left(tree, brother->parent);
                brother = node->parent->right;
            }

            if (is_black(brother->left) && is_black(brother->right)) {
                set_red(brother);
                node = node->parent;
            }
            else {
                if (is_black(brother->right)) {
                    set_red(brother);
                    set_black(brother->left);
                    rotate_right(tree, brother);
                    brother = node->parent->right;
                }

                brother->color = brother->parent->color;
                set_black(brother->parent);
                set_black(brother->right);
                rotate_left(tree, brother->parent);
                break;
            }
        }
        else {
            brother = node->parent->left;

            if (is_red(brother)) {
                set_black(brother);
                set_red(brother->parent);
                rotate_right(tree, brother->parent);
                brother = node->parent->left;
            }

            if (is_black(brother->left) && is_black(brother->right)) {
                set_red(brother);
                node = node->parent;
            }
            else {
                if (is_black(brother->left)) {
                    set_red(brother);
                    set_black(brother->right);
                    rotate_left(tree, brother);
                    brother = node->parent->left;
                }

                brother->color = brother->parent->color;
                set_black(brother->parent);
                set_black(brother->left);
                rotate_right(tree, brother->parent);
                break;
            }
        }
    }
    set_black(node);
}

static rbtree_node* min(rbtree_node *node, rbtree_node *sentinel)
{
    while (node->left != sentinel) {
        node = node->left;
    }
    return node;
}

static int  is_red(rbtree_node *node)
{
    return node->color == RED;
}

static int  is_black(rbtree_node *node)
{
    return node->color == BLACK;
}

static void set_red(rbtree_node *node)
{
    node->color = RED;
}

static void set_black(rbtree_node *node)
{
    node->color = BLACK;
}

static int balck_height(rbtree_node *node, rbtree_node *sentinel)
{
    if (node == sentinel)
        return 0;

    return balck_height(node->left, sentinel) + is_black(node);
}

static int is_regular(rbtree_node *node, rbtree_node *sentinel)
{
    if (node == sentinel)
        return 1;

    /* 不能有相连的红色节点 */
    if (is_red(node) && is_red(node->parent))
        return 0;

    /* 黑高完美平衡 */
    if (balck_height(node->left, sentinel) != balck_height(node->right, sentinel))
        return 0;

    /* 二叉树基本性质 */
    if ( (node->parent->left == node && node->key > node->parent->key)
        || (node->parent->right == node && node->key < node->parent->key))
        return 0;

    if (!is_regular(node->left, sentinel) || !is_regular(node->right, sentinel))
        return 0;

    return 1;
}