//
// Created by frank on 17_2_14.
//

#ifndef FANCY_REQUEST_H
#define FANCY_REQUEST_H

#include "base.h"
#include "event.h"
#include "http_parser.h"

#define HTTP_POOL_SIZE              MEM_POOL_DEFAULT_SIZE
#define HTTP_REQUEST_SIZE           BUFFER_DEFAULT_SIZE
#define HTTP_RESPONSE_SIZE          BUFFER_DEFAULT_SIZE
#define HTTP_MAX_CONTENT_LENGTH     4000 * 1000


typedef struct request  request;
typedef struct location location;

struct request {

    unsigned        should_keep_alive:1;

    unsigned        has_connection_header:1;
    unsigned        has_host_header:1;
    unsigned        has_content_length_header:1;
    unsigned        is_static:1;

    char            *keep_alive_value_start;
    char            *keep_alive_value_end;

    /* 处理过的uri， 仅对静态请求有意义 */
    location        *loc;
    char            *suffix;
    char            *host_uri;
    size_t          host_uri_len;

    connection      *conn;

    mem_pool        *pool;
    buffer          *header_in;
    buffer          *header_out;
    buffer          *body_in;
    buffer          *body_out;

    int             send_fd;
    struct stat     sbuf;

    int             status_code;
    long            content_length;
    const char      *content_type;

    http_parser     parser;
};

struct location {

    const char  *prefix;
    unsigned    use_proxy:1;

    union
    {
        struct {
            int     root_dirfd;
            char    *root;
#define MAX_INDEX 10
            char    *index[MAX_INDEX];
        } s;

        struct sockaddr_in  proxy_pass;
    };
};

/* event loop 开始前必须调用 */
int request_init(mem_pool *pool);

request *request_create(connection *c);
int request_create_body_in(request *r);
int request_create_body_out(request *r, size_t cnt_len);
void request_destroy(request *r);
void request_reset(request *r); /* for keep_alive, avoid destroy */
int request_parse(request *r);

/* process function */
int check_request_header(request *r);
int open_static_file(request *r);
void set_conn_header_closed(request *r);

int strcmp_stop(const char *data, const char *stop);

#endif //FANCY_REQUEST_H
