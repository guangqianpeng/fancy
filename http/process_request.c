//
// Created by frank on 17-2-15.
//

#include "assert.h"
#include "request.h"

int process_request_header(request *r)
{
    connection      *conn;
    request_headers *headers;
    long            cnt_len;

    conn = r->conn;
    headers = r->headers;

    if (r->method != HTTP_M_HEAD && r->method != HTTP_M_GET) {
        r->status_code = HTTP_R_NOT_IMPLEMENTED;
        return FCY_ERROR;
    }

    /* HTTP/1.1必须有host字段 */
    if (r->version == HTTP_V11 && r->headers->host_start == NULL) {
        r->status_code = HTTP_R_BAD_REQUEST;
        return FCY_ERROR;
    }

    /* POST请求必须有Content-Length字段, 且字段值>=0 */
    if (r->method == HTTP_M_POST) {
        if (headers->content_length_start == NULL) {
            r->status_code = HTTP_R_LENGTH_REQUIRED;
            return FCY_ERROR;
        }

        cnt_len = strtol(headers->content_length_start, NULL, 0);

        /* TODO:等于0的情况无法判断 */
        if (cnt_len <= 0) {
            r->status_code = HTTP_R_BAD_REQUEST;
            return FCY_ERROR;
        }

        if (cnt_len >= INT_MAX) {
            r->status_code = HTTP_R_PAYLOAD_TOO_LARGE;
            return FCY_ERROR;
        }

        r->cnt_len = (int)cnt_len;
    }

    /* 设置keep_alive字段 */
    if (r->version == HTTP_V11) {
        conn->keep_alive = 1;
    }

    if (headers->connection_start) {
        if (strncmp(headers->connection_start, "keep-alive", 10) == 0) {
            conn->keep_alive = 1;
        }
    }

    r->status_code = HTTP_R_OK;
    return FCY_OK;
}

int process_request_static(request *r)
{
    char        *uri_start, *uri_end;
    char        *relpath;
    struct stat *sbuf;
    int         fd;
    int         err;

    uri_start = r->line->uri_start;
    uri_end = r->line->uri_end;
    relpath = (r->line->uri_static == uri_end ? "index.html" : uri_start + 1);
    sbuf = &r->sbuf;

    assert(*uri_end == ' ');

    /* 测试文件 */
    *uri_end = '\0';
    err = stat(relpath, sbuf);
    *uri_end = ' ';

    if (err == -1) {
        r->status_code = HTTP_R_NOT_FOUND;
        return FCY_ERROR;
    }

    if (!S_ISREG(sbuf->st_mode) || !(S_IRUSR & sbuf->st_mode)) {
        r->status_code = HTTP_R_FORBIDDEN;
        return FCY_ERROR;
    }

    /* 打开文件 */
    *uri_end = '\0';
    fd = open(relpath, O_RDONLY | O_NONBLOCK);
    *uri_end = ' ';

    if (fd == -1) {
        r->status_code = HTTP_R_INTARNAL_SEARVE_ERROR;
        return FCY_ERROR;
    }

    r->send_fd = fd;
    r->status_code = HTTP_R_OK;
    return FCY_OK;
}