//
// Created by frank on 17-2-16.
//

#include <assert.h>

#include "log.h"
#include "base.h"
#include "http_parser.h"
#include "connection.h"
#include "request.h"

const static char *suffix_str[] = {
        "html", "txt", "xml", "asp", "css",
        "gif", "ico", "png", "jpg", "js",
        "pdf", NULL,
};

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

static void request_on_header(void *user, string *name, string *value);
static void request_on_uri(void *user, string *uri, string *suffix);
static const char *get_content_type(string *suffix);


int request_init(mem_pool *pool)
{
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

    r->header_in = buffer_create(p, HTTP_BUFFER_SIZE);
    r->header_out = buffer_create(p, HTTP_BUFFER_SIZE);
    r->body_in = buffer_create(p, HTTP_BUFFER_SIZE);
    r->body_out = buffer_create(p, HTTP_BUFFER_SIZE);
    if (r->header_in == NULL
        || r->header_out == NULL
        || r->body_in == NULL
        || r->body_out == NULL) {
        mem_pool_destroy(p);
        return NULL;
    }

    r->headers = array_create(p, 10, sizeof(keyval));
    if (r->headers == NULL) {
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
    array  *headers = r->headers;

    assert(buffer_empty(header_in));
    assert(buffer_empty(header_out));
    assert(buffer_empty(body_in));
    assert(buffer_empty(body_out));


    headers->size = 0;

    connection *conn = r->conn;
    ++conn->app_count;

    mem_pool *pool = r->pool;

    bzero(r, sizeof(request));

    r->header_in = header_in;
    r->header_out = header_out;
    r->body_in = body_in;
    r->body_out = body_out;
    r->headers = headers;
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
        CHECK(close(r->send_fd));
    }

    mem_pool_destroy(r->pool);
}

int request_parse(request *r)
{
    buffer  *b = r->header_in;
    char *beg = buffer_peek(b);
    char *end = buffer_begin_write(b);
    return parser_execute(&r->parser, beg, end);
}

void request_headers_htop(request *r, buffer *b)
{
    assert(buffer_empty(b));

    http_parser *p = &r->parser;
    location    *loc = r->loc;

    /* line */
    buffer_append_str(b, &method_str[p->method]);
    buffer_append_space(b);
    buffer_append_str(b, &r->uri);
    buffer_append_space(b);
    buffer_append_literal(b, "HTTP/1.1\r\n");

    /* host */
    buffer_append_literal(b, "Host: ");
    if (loc->use_proxy) {
        buffer_append_str(b, &loc->proxy_pass_str);
    }
    else {
        buffer_append_str(b, &r->host);
    }

    /* connection */
    buffer_append_literal(b, "\r\nConnection: ");
    if (r->should_keep_alive) {
        buffer_append_literal(b, "keep-alive\r\n");
    }
    else {
        buffer_append_literal(b, "close\r\n");
    }

    /* other headers */
    for (size_t i = 0; i < r->headers->size; ++i) {
        keyval *kv = array_at(r->headers, i);
        buffer_append_str(b, &kv->key);
        buffer_append_literal(b, ": ");
        buffer_append_str(b, &kv->value);
        buffer_append_crlf(b);
    }
    buffer_append_crlf(b);
}

int request_read_chunked(request *r)
{
    // TODO
    return FCY_ERROR;
}

int check_request_header(request *r)
{
    http_parser *p= &r->parser;

    if (r->loc == NULL) {
        r->status_code = STATUS_NOT_FOUND;
        return FCY_ERROR;
    }

    if (p->method != METHOD_GET && p->method != METHOD_POST) {
        r->status_code = STATUS_NOT_IMPLEMENTED;
        return FCY_ERROR;
    }

    /* HTTP/1.1必须有host字段 */
    if (p->version == HTTP_V11 && !r->has_host_header) {
        r->status_code = STATUS_BAD_REQUEST;
        return FCY_ERROR;
    }

    /* HTTP/1.1 默认开启keep alive*/
    if (p->version == HTTP_V11 && !r->has_connection_header) {
        r->should_keep_alive = 1;
    }


    /* POST请求必须有Content-Length字段, 且字段值>=0
     * */
    if (p->method == METHOD_POST) {
        if (!r->has_content_length_header) {
            r->status_code = STATUS_LENGTH_REQUIRED;
            return FCY_ERROR;
        }
        if (r->content_length <= 0) {
            r->status_code = STATUS_BAD_REQUEST;
            return FCY_ERROR;
        }
        if (r->content_length >= INT_MAX) {
            r->status_code = STATUS_PAYLOAD_TOO_LARGE;
            return FCY_ERROR;
        }
    }

    /* status code未知 */
    return FCY_OK;
}

int open_static_file(request *r)
{
    location        *loc = r->loc;
    string         *uri = &r->uri;
    struct stat     *sbuf = &r->sbuf;
    int             err;

    assert(r->is_static);

    if (loc == NULL) {
        r->status_code = STATUS_NOT_FOUND;
        return FCY_ERROR;
    }

    /* 测试文件 */
    char path[uri->len + 64];
    char *path_base = path + uri->len;
    strcpy(path, uri->data);
    if (r->suffix.data == NULL && path[uri->len - 1] == '/') {

        int found = 0;
        for (int i = 0; loc->index[i].data != NULL; ++i) {
            strcpy(path_base, loc->index[i].data);
            err = fstatat(loc->root_dirfd, path + 1, sbuf, 0);
            if (err != -1) {
                found = 1;
                r->suffix.data = strchr(loc->index[i].data, '.');
                r->suffix.len = strlen(r->suffix.data);
                break;
            }
        }

        if (!found) {
            r->status_code = STATUS_NOT_FOUND;
            return FCY_ERROR;
        }
    }
    else {
        err = fstatat(loc->root_dirfd, path + 1, sbuf, 0);
        if (err == -1) {
            LOG_SYSERR("fstatat error");
            r->status_code = STATUS_NOT_FOUND;
            return FCY_ERROR;
        }
    }

    if (!S_ISREG(sbuf->st_mode) || !(S_IRUSR & sbuf->st_mode)) {
        r->status_code = STATUS_FORBIDDEN;
        return FCY_ERROR;
    }

    /* 打开文件会阻塞！！ */
    int fd = openat(loc->root_dirfd, path + 1, O_RDONLY);
    if (fd == -1) {
        LOG_SYSERR("fstatat error");
        r->status_code = STATUS_INTARNAL_SEARVE_ERROR;
        return FCY_ERROR;
    }

    assert(r->content_type == NULL);
    r->content_type = get_content_type(&r->suffix);

    r->send_fd = fd;
    r->status_code = STATUS_OK;
    return FCY_OK;
}

static void request_set_cork(connection *conn, int open)
{
    CHECK(setsockopt(conn->sockfd, IPPROTO_TCP, TCP_CORK, &open, sizeof(open)));
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

static void request_on_header(void *user, string *name, string *value)
{
    request *r = user;
    if (strcasecmp(name->data, "Host") == 0) {
        r->has_host_header = 1;
        r->host = *value;
        return;
    }
    else if (strcasecmp(name->data, "Connection") == 0) {
        r->has_connection_header = 1;
        r->connection = *value;
        if (strcasecmp(value->data, "Keep-alive") == 0) {
            r->should_keep_alive = 1;
        }
        else {
            r->should_keep_alive = 0;
        }
        return;
    }
    else if (strcasecmp(name->data, "Content-Length") == 0) {
        r->has_content_length_header = 1;
        r->content_length = atoi(value->data);
    }
    else if (strcasecmp(name->data, "Transfer-Encoding") == 0) {
        if (strcasecmp(value->data, "chunked") == 0) {
            r->is_chunked = 1;
        }
    }

    keyval *kv = array_alloc(r->headers);
    if (kv == NULL) {
        LOG_ERROR("array alloc failed");
        exit(EXIT_FAILURE);
    }
    kv->key = *name;
    kv->value = *value;
}

static void request_on_uri(void *user, string *uri, string *suffix)
{
    request *r = user;
    location *loc = NULL;

    r->uri = *uri;
    r->suffix = *suffix;

    for (size_t i = 0; i < locations->size; ++i) {
        loc = array_at(locations, i);
        if (strncmp(uri->data, loc->prefix.data, loc->prefix.len) == 0) {
            r->loc = loc;
            if (!loc->use_proxy) {
                r->is_static = 1;
            }
            else {
                r->conn->peer->addr = loc->proxy_pass;
            }
            break;
        }
    }

    if (loc == NULL) {
        r->status_code = STATUS_NOT_FOUND;
        return;
    }
}

static const char *get_content_type(string *suffix)
{
    if (suffix != NULL) {
        assert(*suffix->data == '.');
        for (int i = 0; suffix_str[i] != NULL; ++i) {
            if (strcmp(suffix->data + 1, suffix_str[i]) == 0) {
                return content_type_str[i];
            }
        }
    }
    return content_type_str[0];
}