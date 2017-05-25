//
// Created by frank on 17-2-16.
//

#include <assert.h>
#include "http_parser.h"
#include "connection.h"
#include "request.h"

static size_t index_name_len;
static size_t index_name_suffix_len;
static size_t *location_len;
static size_t root_len;

const static char *suffix_str[] = {
        "html", "txt", "xml", "asp", "css",
        "gif", "ico", "png", "jpg", "js",
        "pdf", NULL,
};
static size_t suffix_str_len[sizeof(suffix_str) / sizeof(*suffix_str)];

const static char *content_type_str[] = {
        "text/html; charset=utf-8",
        "text/plain; charset=utf-8",
        "text/xml",
        "text/asp",
        "text/css",
        "image/gif",
        "image/x-icon",
        "image/png",
        "image/jpeg",
        "application/javascript",
        "application/pdf",
        NULL,
};

static void request_set_cork(connection *conn, int open);
static void request_set_parser(request *r);
static void request_set_conn(request *r, connection *c);

static void request_on_header(void *user, char *name, char *value);
static void request_on_uri(void *user, char *host_uri, char *suffix);
static const char *get_content_type(const char *suffix);

int request_init(mem_pool *pool)
{
    const char *dot;
    size_t     n_location = 0;

    /* index name */
    index_name_len = strlen(index_name);
    dot = strrchr(index_name, '.');
    if (dot == NULL) {
        error_log("index name must have suffix");
        return FCY_ERROR;
    }
    index_name_suffix_len = index_name_len - (dot - index_name) - 1;


    /* locations */
    while (locations[n_location++] != NULL)
        ;
    --n_location;

    location_len = palloc(pool, n_location);
    RETURN_ON(location_len, NULL);

    for (int i = 0; i < n_location; ++i) {
        location_len[i] = strlen(locations[i]);
    }

    /* content type */
    for (int i = 0; suffix_str[i] != NULL; ++i) {
        suffix_str_len[i] = strlen(suffix_str[i]);
    }

    /* root */
    root_len = strlen(root);

    return FCY_OK;
}

request *request_create(connection *c)
{
    request     *r;
    mem_pool    *p;

    p = mem_pool_create(HTTP_POOL_SIZE);
    if(p == NULL) {
        return NULL;
    }

    r = pcalloc(p, sizeof(request));
    if (r == NULL) {
        mem_pool_destroy(p);
        return NULL;
    }

    r->header_in = buffer_create(p, HTTP_REQUEST_SIZE);
    r->header_out = buffer_create(p, HTTP_RESPONSE_SIZE);
    if (r->header_in == NULL ||
        r->header_out == NULL) {
        mem_pool_destroy(p);
        return NULL;
    }

    r->pool = p;

    request_set_cork(c, 1);
    request_set_parser(r);
    request_set_conn(r, c);

    return r;
}

void request_reset(request *r)
{
    request_set_cork(r->conn, 0);
    request_set_cork(r->conn, 1);

    if (r->send_fd > 0) {
        close(r->send_fd);
    }

    buffer *header_in = r->header_in;
    buffer *header_out = r->header_out;
    buffer *body_in = r->body_in;
    buffer *body_out = r->body_out;

    buffer_reset(header_in);
    buffer_reset(header_out);
    if (body_in != NULL) {
        buffer_reset(body_in);
    }
    if (body_out != NULL) {
        buffer_reset(body_out);
    }

    connection *conn = r->conn;
    ++conn->app_count;

    mem_pool *pool = r->pool;

    bzero(r, sizeof(request));

    r->header_in = header_in;
    r->header_out = header_out;
    r->body_in = body_in;
    r->body_out = body_out;
    r->conn = conn;
    r->pool = pool;
    request_set_parser(r);
}

void request_destroy(request *r)
{
    /* 关闭TCP_CORK选项 */
    request_set_cork(r->conn, 0);

    r->conn->app = NULL;
    if (r->send_fd > 0) {
        ABORT_ON(close(r->send_fd), -1);
    }

    mem_pool_destroy(r->pool);
}

int request_create_body_in(request *r)
{
    buffer *header_in, *body_in;
    size_t cnt_len, body_read;

    header_in = r->header_in;
    body_in = r->body_in;
    cnt_len = (size_t)r->content_length;
    body_read = buffer_size(header_in);

    assert(cnt_len > 0);

    if (r->body_in == NULL) {
        body_in = r->body_in = buffer_create(r->pool, cnt_len);
        RETURN_ON(body_in, NULL);
    }
    else {
        assert(buffer_empty(body_in));
    }

    memcpy(body_in->start, header_in->data_start, body_read);

    body_in->data_end += body_read;

    return FCY_OK;
}

int request_create_body_out(request *r, size_t cnt_len)
{
    buffer *header_out, *body_out;
    size_t body_read;

    header_out = r->header_out;
    body_out = r->body_out;
    body_read = buffer_size(header_out);

    assert(cnt_len > 0);

    if (body_out == NULL) {
        body_out = r->body_out = buffer_create(r->pool, cnt_len);
        RETURN_ON(body_out, NULL);
    }
    else {
        assert(buffer_empty(body_out));
    }

    memcpy(body_out->start, header_out->data_start, body_read);
    body_out->data_end += body_read;

    return FCY_OK;
}

int request_parse(request *r)
{
    return parser_execute(&r->parser, r->header_in, r->pool);
}

static void request_set_cork(connection *conn, int open)
{
    int err;

    err = setsockopt(conn->sockfd, IPPROTO_TCP, TCP_CORK, &open, sizeof(open));
    if (err == -1) {
        access_log(&conn->addr, "setsockopt error %s", strerror(errno));
    }
}

static void request_set_parser(request *r)
{
    r->parser.type = HTTP_PARSE_REQUEST;
    r->parser.uri_cb = request_on_uri;
    r->parser.header_cb = request_on_header;
    r->parser.user = r;
}

static void request_set_conn(request *r, connection *c)
{
    assert(c->app == NULL);  /* 同一时刻只允许一个app占用connection */
    r->conn = c;
    c->app = r;
    ++c->app_count;
}

static void request_on_header(void *user, char *name, char *value)
{
    request *r = user;
    if (strncasecmp(name, "Host", 4) == 0) {
        r->has_host_header = 1;
    }
    else if (strncasecmp(name, "Connection", 10) == 0) {
        r->has_connection_header = 1;
        if (strncasecmp(value, "keep-alive", 10) == 0) {
            r->should_keep_alive = 1;
            r->keep_alive_value_start = value;
            r->keep_alive_value_end  = value + 10;
        }
        else {
            r->should_keep_alive = 0;
        }
    }
    else if (strncasecmp(name, "Content-Length", 14) == 0) {
        r->has_content_length_header = 1;
        r->content_length = strtol(value, NULL, 0);
    }
}

static void request_on_uri(void *user, char *host_uri, char *suffix)
{
    request *r = user;
    r->host_uri = host_uri;

    for (int i = 0; locations[i] != NULL; ++i) {
        if (strncmp(host_uri + root_len, locations[i], location_len[i]) == 0) {
            r->is_static = 1;
            break;
        }
    }

    if (r->is_static)
    {
        /* 无后缀且访问文件夹，结尾补index_name */
        if (*suffix == '\0' && *(suffix - 1) == '/') {
            strcpy(suffix, index_name);
            suffix += index_name_len - index_name_suffix_len;
        }

        /* 设置content-type */
        r->content_type = get_content_type(suffix);
        if (r->content_type == NULL) {
            r->content_type = content_type_str[0];
        }
    }
}

static const char *get_content_type(const char *suffix)
{
    if (suffix == NULL) {
        return NULL;
    }

    for (int i = 0; suffix_str[i] != NULL ; ++i) {
        if (strncasecmp(suffix, suffix_str[i], suffix_str_len[i]) == 0) {
            return content_type_str[i];
        }
    }

    return NULL;
}