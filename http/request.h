//
// Created by frank on 17_2_14.
//

#ifndef FANCY_REQUEST_H
#define FANCY_REQUEST_H

#include "base.h"
#include "event.h"

/* request method */
#define HTTP_M_OPTIONS      0
#define HTTP_M_GET          1
#define HTTP_M_HEAD         2
#define HTTP_M_POST         3
#define HTTP_M_PUT          4
#define HTTP_M_DELETE       5
#define HTTP_M_TRACE        6
#define HTTP_M_CONNECT      7

/* status code */
#define HTTP_R_OK                               0
#define HTTP_R_BAD_REQUEST                      1
#define HTTP_R_FORBIDDEN                        2
#define HTTP_R_NOT_FOUND                        3
#define HTTP_R_REQUEST_TIME_OUT                 4
#define HTTP_R_LENGTH_REQUIRED                  5
#define HTTP_R_PAYLOAD_TOO_LARGE                6
#define HTTP_R_URI_TOO_LONG                     7
#define HTTP_R_REQUEST_HEADER_FIELD_TOO_LARGE   8
#define HTTP_R_INTARNAL_SEARVE_ERROR            9
#define HTTP_R_NOT_IMPLEMENTED                  10

#define HTTP_V10            0
#define HTTP_V11            1

#define HTTP_POOL_SIZE      MEM_POOL_DEFAULT_SIZE
#define HTTP_REQUEST_SIZE   BUFFER_DEFAULT_SIZE
#define HTTP_RESPONSE_SIZE  BUFFER_DEFAULT_SIZE

extern const char *status_code_out_str[];

typedef struct request          request;

struct request {

    int             method;
    int             version;
    long            cnt_len;

    unsigned        keep_alive:1;
    unsigned        has_args:1;
    unsigned        has_host_header:1;
    unsigned        has_content_length_header:1;
    unsigned        is_static:1;

    char            *request_start;
    char            *uri_start;
    char            *uri_end;
    char            *last_header_name_start;
    char            *last_header_value_start;

    char            *uri;
    char            *suffix;

    connection      *conn;

    mem_pool        *pool;
    buffer          *header_in;
    buffer          *header_out;

    int             send_fd;
    struct stat     sbuf;

    int             status_code;
    const char      *content_type;

    int             parse_state;
};

request *request_create(connection *c);
void request_destroy(request *r);
void request_reset(request *r); /* for keep_alive, avoid destroy */
void request_print(request *r); /* debug */

int parse_request(request *r);

int check_request_header_filed(request *r);

int process_request_static(request *r);

#endif //FANCY_REQUEST_H
