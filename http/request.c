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

static void request_on_header(void *user, char *name, char *value);
static void request_on_uri(void *user, char *uri, size_t uri_len, char *suffix);
static const char *get_content_type(const char *suffix);


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
        CHECK(close(r->send_fd));
    }

    mem_pool_destroy(r->pool);
}

int request_create_body_in(request *r)
{
    buffer *header_in, *body_in;
    size_t size, body_read;

    header_in = r->header_in;
    body_in = r->body_in;
    size = r->is_chunked ? HTTP_CHUNK_SIZE : (size_t)r->content_length;
    body_read = buffer_size(header_in);

    assert(size > 0);

    if (r->body_in == NULL) {
        body_in = r->body_in = buffer_create(r->pool, size);
        if (body_in == NULL) {
            return FCY_ERROR;
        }
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
        if (body_out == NULL) {
            return FCY_ERROR;
        }
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
    return parser_execute(&r->parser, r->header_in);
}

int check_request_header(request *r)
{
    http_parser *p= &r->parser;

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
    char            *path = r->host_uri;
    struct stat     *sbuf = &r->sbuf;
    int             err;

    assert(r->is_static);

    if (loc == NULL) {
        r->status_code = STATUS_NOT_FOUND;
        return FCY_ERROR;
    }

    /* 测试文件 */
    char *path_end = path + r->host_uri_len;
    if (r->suffix == NULL && path[r->host_uri_len - 1] == '/') {

        int found = 0;
        for (int i = 0; loc->index[i] != NULL; ++i) {
            strcpy(path_end, loc->index[i]);
            err = fstatat(loc->root_dirfd, path + 1, sbuf, 0);
            if (err != -1) {
                found = 1;
                r->suffix = strchr(loc->index[i], '.');
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
        r->status_code = STATUS_INTARNAL_SEARVE_ERROR;
        return FCY_ERROR;
    }

    assert(r->content_type == NULL);
    r->content_type = get_content_type(r->suffix);

    r->send_fd = fd;
    r->status_code = STATUS_OK;
    return FCY_OK;
}

void set_conn_header_closed(request *r)
{
    /* 动态类型请求不支持keep-alive */
    buffer  *in;
    char    *beg, *end;

    in = r->header_in;
    beg = r->keep_alive_value_start;
    end = r->keep_alive_value_end;

    /* in has complete headers */
    assert(strcmp_stop((char*)in->data_end - 4, "\r\n\r\n") == 0);

    if (!r->has_connection_header) {
        in->data_end -= 2;
        buffer_write(in, "Connection:close\r\n\r\n", 20);
    }
    else if (beg != NULL) {
        assert(end - beg == 10); // keep_alive
        strncpy(beg, "close     ", end - beg);
    }
}

void set_proxy_pass_host(request *r)
{
    buffer      *in;
    char        *beg, *end;
    location    *loc;

    in = r->header_in;
    beg = r->host_value_start;
    end = r->host_value_end;
    loc = r->loc;

    assert(strcmp_stop((char*)in->data_end - 4, "\r\n\r\n") == 0);
    assert(loc->use_proxy);

    if (r->has_host_header) {
        assert(beg!= NULL && end != NULL);
        if (end - beg >= loc->proxy_pass_len) {
            strncpy(beg, loc->proxy_pass_str, loc->proxy_pass_len);
            beg += loc->proxy_pass_len;
            memset(beg, ' ', end - beg);
            return;
        } else {
            /* ...\r\n
             * Host: abcde\r\n
             * */
            memset(r->host_header_start, ' ', end + 2 - r->host_header_start);
        }
    }

    in->data_end -= 2;
    buffer_write(in, "Host:", 5);
    buffer_write(in, loc->proxy_pass_str, loc->proxy_pass_len);
    buffer_write(in, "\r\n\r\n", 4);
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

static void request_on_header(void *user, char *name, char *value)
{
    request *r = user;
    if (strncasecmp(name, "Host", 4) == 0) {
        r->has_host_header = 1;
        r->host_value_start = value;
        while (!isspace(*value) && *value != '\r')
            ++value;
        r->host_value_end = value;
        r->host_header_start = name;
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
    else if (strncasecmp(name, "Transfer-Encoding", 17) == 0) {
        if (strncasecmp(value, "chunked", 7) == 0) {
            r->is_chunked = 1;
        }
    }
}

static void request_on_uri(void *user, char *uri, size_t uri_len, char *suffix)
{
    request *r = user;
    location *loc = NULL;

    for (size_t i = 0; i < locations->size; ++i) {
        loc = array_at(locations, i);
        if (strcmp_stop(uri, loc->prefix) == 0) {
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

    if (r->is_static) {
        r->host_uri_len = uri_len;
        r->host_uri = palloc(r->pool, uri_len + 32);
        strcpy(r->host_uri, uri);
        if (suffix != NULL) {
            r->suffix = r->host_uri + (suffix - uri);
        }
    }
}

static const char *get_content_type(const char *suffix)
{
    if (suffix != NULL) {
        assert(*suffix == '.');
        ++suffix;
        for (int i = 0; suffix_str[i] != NULL; ++i) {
            if (strcmp_stop(suffix, suffix_str[i]) == 0) {
                return content_type_str[i];
            }
        }
    }
    return content_type_str[0];
}

int strcmp_stop(const char *data, const char *stop)
{
    while (*data == *stop) {
        ++data;
        ++stop;

        if (*stop == '\0') {
            return 0;
        }
    }
    return *data - *stop;
}