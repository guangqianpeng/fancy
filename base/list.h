//
// Created by frank on 17-2-12.
//

#ifndef FANCY_LIST_H
#define FANCY_LIST_H

#include "base.h"

typedef struct list list;
typedef struct list list_node;

struct list {
    list_node *prev;
    list_node *next;
};

void list_init(list *h);
int list_empty(list *h);
void list_insert_head(list *h, list_node *x);
list_node *list_head(list *h);
void list_remove(list_node *x);

#endif //FANCY_LIST_H
