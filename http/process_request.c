//
// Created by frank on 17-2-15.
//

#include "assert.h"
#include "request.h"

const char *get_content_type(char *suffix);

int check_request_header_filed(request *r)
{
    if (r->method != HTTP_M_GET) {
        r->status_code = HTTP_R_NOT_IMPLEMENTED;
        return FCY_ERROR;
    }

    /* HTTP/1.1必须有host字段 */
    if (r->version == HTTP_V11 && !r->has_host_header) {
        r->status_code = HTTP_R_BAD_REQUEST;
        return FCY_ERROR;
    }

    /* POST请求必须有Content-Length字段, 且字段值>=0
     * 然而post请求尚未实现，这段代码不会执行
     * */
    if (r->method == HTTP_M_POST) {
        if (!r->has_content_length_header) {
            r->status_code = HTTP_R_LENGTH_REQUIRED;
            return FCY_ERROR;
        }
        if (r->cnt_len <= 0) {
            r->status_code = HTTP_R_BAD_REQUEST;
            return FCY_ERROR;
        }
        if (r->cnt_len >= INT_MAX) {
            r->status_code = HTTP_R_PAYLOAD_TOO_LARGE;
            return FCY_ERROR;
        }
    }

    /* 设置content-type, is_static字段
     * */
    if (r->suffix) {
        r->content_type = get_content_type(r->suffix);
        if (r->content_type != NULL) {
            r->is_static = 1;
        }
    }

    /* status code未知 */
    return FCY_OK;
}

int process_request_static(request *r)
{
    char            *relpath;
    struct stat     *sbuf;
    int             fd, err, f_flag;

    relpath = r->uri + 1;
    sbuf = &r->sbuf;

    /* 测试文件 */
    err = stat(relpath, sbuf);
    if (err == -1) {
        r->status_code = HTTP_R_NOT_FOUND;
        return FCY_ERROR;
    }

    if (!S_ISREG(sbuf->st_mode) || !(S_IRUSR & sbuf->st_mode)) {
        r->status_code = HTTP_R_FORBIDDEN;
        return FCY_ERROR;
    }

    /* 打开文件会阻塞！！ */
    fd = open(relpath, O_RDONLY);
    if (fd == -1) {
        r->status_code = HTTP_R_INTARNAL_SEARVE_ERROR;
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
    r->status_code = HTTP_R_OK;
    return FCY_OK;
}

const char *get_content_type(char *suffix)
{
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

    for (int i = 0; suffix_str[i] != NULL ; ++i) {
        if (strncasecmp(suffix, suffix_str[i], strlen(suffix_str[i])) == 0) {
            return content_type_str[i];
        }
    }

    return NULL;
}