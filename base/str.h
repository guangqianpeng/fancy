//
// Created by frank on 17-6-2.
//

#ifndef FANCY_STR_H
#define FANCY_STR_H

#include <stddef.h>

typedef struct string   string;
typedef struct keyval   keyval;

struct string {
    size_t  len;
    char    *data;
};

struct keyval {
    string   key;
    string   value;
};

#define string(str) { sizeof(str) - 1, (char*) str }
#define null_str    {0, NULL}
#define str_set(str, text)  \
    (str)->len = sizeof(text) - 1; (str)->data = (char*) text
#define str_null(str) { (str)->len = 0; (str)->data = NULL; }

#endif //FANCY_STR_H
