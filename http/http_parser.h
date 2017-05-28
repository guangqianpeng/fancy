//
// Created by frank on 17-5-24.
//

#include "buffer.h"

#ifndef FANCY_PARSE_HEADERS_H
#define FANCY_PARSE_HEADERS_H

#define HTTP_PARSE_REQUEST      0
#define HTTP_PARSE_RESPONSE     1

/* request method */
#define METHOD_GET          0
#define METHOD_HEAD         1
#define METHOD_POST         2
#define METHOD_OPTIONS      3
#define METHOD_DELETE       4
#define METHOD_TRACE        5
#define METHOD_CONNECT      6
extern const char *method_str[];

/* status code */
#define STATUS_OK                               0
#define STATUS_BAD_REQUEST                      1
#define STATUS_FORBIDDEN                        2
#define STATUS_NOT_FOUND                        3
#define STATUS_REQUEST_TIME_OUT                 4
#define STATUS_LENGTH_REQUIRED                  5
#define STATUS_PAYLOAD_TOO_LARGE                6
#define STATUS_URI_TOO_LONG                     7
#define STATUS_REQUEST_HEADER_FIELD_TOO_LARGE   8
#define STATUS_INTARNAL_SEARVE_ERROR            9
#define STATUS_NOT_IMPLEMENTED                  10
extern const char *status_code_out_str[];

#define HTTP_V10                    0
#define HTTP_V11                    1

typedef struct http_parser http_parser;
typedef void(*http_null_callback)(void *user);
typedef void(*http_char_callback)(void *user);
typedef void(*http_pair_callback)(void *user, char *data1, char *data2);

struct http_parser {

    unsigned            type:1;

    unsigned            state:8;
    unsigned            index:8;

    unsigned            method:8;
    unsigned            version:4;

    char                *uri_start;
    char                *uri_end;

    char                *last_header_name_start;
    char                *last_header_value_start;

    http_pair_callback  uri_cb;
    http_pair_callback  header_cb;

    void                *user;
};

int parser_execute(http_parser *ps, buffer *in,
                   /* request only */ mem_pool *pool);

#endif //FANCY_PARSE_HEADERS_H
