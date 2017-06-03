//
// Created by frank on 17-6-3.
//

#ifndef FANCY_CHUNK_READER_H
#define FANCY_CHUNK_READER_H

#include <wchar.h>

typedef struct chunk_reader chunk_reader;
struct chunk_reader {

    unsigned    state:8;

    size_t      where;
    char        *first_hex;

    size_t      expect_chunked_size;
};

int chunk_reader_execute(chunk_reader *cr, char *beg, char *end);

#endif //FANCY_CHUNK_READER_H
