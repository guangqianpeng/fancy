//
// Created by frank on 17-6-2.
//

#ifndef FANCY_STR_H
#define FANCY_STR_H

#include <stddef.h>

typedef struct fcy_str  fcy_str;
typedef struct keyval   keyval;

struct fcy_str {
    size_t  len;
    char    *data;
};

struct keyval {
    fcy_str   key;
    fcy_str   value;
};

#define string(str) { sizeof(str) - 1, (char*) str }
#define null_string {0, NULL}
#define fcy_str_set(str, text)  \
    (str)->len = sizeof(text) - 1; (str)->data = (char*) text
#define fcy_str_null(str) (str)->len = 0; (str)->data = NULL

#endif //FANCY_STR_H
