//
// Created by frank on 17_2_14.
//

#ifndef FANCY_REQUEST_H
#define FANCY_REQUEST_H

#include "base.h"
#include "buffer.h"
#include "event.h"
#include "http_parser.h"
#include "chunk_reader.h"

#define HTTP_POOL_SIZE              (4096 * 1024)
#define HTTP_BUFFER_SIZE            BUFFER_INIT_SIZE
#define HTTP_MAX_CONTENT_LENGTH     (4000 * 1000)


typedef struct request  request;
typedef struct location location;

struct request {

    unsigned        should_keep_alive:1;

    unsigned        has_connection_header:1;
    unsigned        has_host_header:1;
    unsigned        has_content_length_header:1;
    unsigned        is_static:1;
    unsigned        is_chunked:1;

    fcy_str         uri;
    fcy_str         suffix;
    fcy_str         host;
    fcy_str         connection;
    array           *headers;   /* 其余的header */

    location        *loc;

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

    fcy_str     prefix;
    unsigned    use_proxy:1;

    union
    {
        struct {
            int     root_dirfd;
            fcy_str root;
#define MAX_INDEX 10
            fcy_str index[MAX_INDEX];
        };
        struct {
            fcy_str proxy_pass_str;
            struct sockaddr_in proxy_pass;
        };
    };
};

/* event loop 开始前必须调用 */
int request_init(mem_pool *pool);

request *request_create(connection *c);
void request_destroy(request *r);
void request_reset(request *r); /* for keep_alive, avoid destroy */
int request_parse(request *r);
void request_headers_htop(request *, buffer *);

int request_read_chunked(request *r);

/* process function */
int check_request_header(request *r);
int open_static_file(request *r);

#endif //FANCY_REQUEST_H
