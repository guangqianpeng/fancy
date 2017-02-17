//
// Created by frank on 17-2-12.
//

#include "list.h"

void list_init(list *h)
{
    h->next = h->prev = h;
}

int list_empty(list *h)
{
    return h->next == h;
}

void list_insert_head(list *h, list_node *x)
{
    x->next = h->next;
    x->next->prev = x;

    h->next = x;
    x->prev = h;
}

list_node *list_head(list *h)
{
    if (h->next == h) {
        return NULL;
    }
    return h->next;
}

void list_remove(list_node *x)
{
    x->prev->next = x->next;
    x->next->prev = x->prev;
}