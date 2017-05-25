//
// Created by frank on 17-2-15.
//

#include "assert.h"
#include "request.h"

const char *get_content_type(const char *suffix);

int check_request_header(request *r)
{
    http_parser *p= &r->parser;

    if (p->method != HTTP_METHOD_GET && p->method != HTTP_METHOD_POST) {
        r->status_code = HTTP_STATUS_NOT_IMPLEMENTED;
        return FCY_ERROR;
    }

    /* HTTP/1.1必须有host字段 */
    if (p->version == HTTP_V11 && !r->has_host_header) {
        r->status_code = HTTP_STATUS_BAD_REQUEST;
        return FCY_ERROR;
    }

    /* HTTP/1.1 默认开启keep alive*/
    if (p->version == HTTP_V11 && !r->has_connection_header) {
        r->should_keep_alive = 1;
    }


    /* POST请求必须有Content-Length字段, 且字段值>=0
     * */
    if (p->method == HTTP_METHOD_POST) {
        if (!r->has_content_length_header) {
            r->status_code = HTTP_STATUS_LENGTH_REQUIRED;
            return FCY_ERROR;
        }
        if (r->content_length <= 0) {
            r->status_code = HTTP_STATUS_BAD_REQUEST;
            return FCY_ERROR;
        }
        if (r->content_length >= INT_MAX) {
            r->status_code = HTTP_STATUS_PAYLOAD_TOO_LARGE;
            return FCY_ERROR;
        }
    }

    /* status code未知 */
    return FCY_OK;
}

int open_static_file(request *r)
{
    char            *path;
    struct stat     *sbuf;
    int             err, f_flag;

    assert(r->is_static);

    path = r->host_uri;
    sbuf = &r->sbuf;

    /* 测试文件 */
    err = stat(path, sbuf);
    if (err == -1) {
        r->status_code = HTTP_STATUS_NOT_FOUND;
        return FCY_ERROR;
    }

    if (!S_ISREG(sbuf->st_mode) || !(S_IRUSR & sbuf->st_mode)) {
        r->status_code = HTTP_STATUS_FORBIDDEN;
        return FCY_ERROR;
    }

    /* 打开文件会阻塞！！ */
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        r->status_code = HTTP_STATUS_INTARNAL_SEARVE_ERROR;
        return FCY_ERROR;
    }

    f_flag = fcntl(fd, F_GETFL, 0);
    if (f_flag == -1) {
        err_sys("fcntl error");
    }

    err = fcntl(fd, F_SETFL, f_flag | O_NONBLOCK);
    if (err == -1) {
        err_sys("fcntl error");
    }

    r->send_fd = fd;
    r->status_code = HTTP_STATUS_OK;
    return FCY_OK;
}

/* 动态类型请求不支持keep-alive */
void set_conn_header_closed(request *r)
{
    buffer  *in;
    char    *beg, *end;

    in = r->header_in;
    beg = r->keep_alive_value_start;
    end = r->keep_alive_value_end;

    assert(strncmp((char*)in->data_end - 4, "\r\n\r\n", 4) == 0);

    if (!r->has_connection_header) {
        in->data_end -= 2;
        buffer_write(in, "Connection:close\r\n\r\n", 20);
        r->has_connection_header = 1;
    }
    else if (beg != NULL) {
        assert(end - beg == 10); // keep_alive
        strncpy(beg, "close     ", end - beg);
    }
}