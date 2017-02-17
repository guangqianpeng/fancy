//
// Created by frank on 17-2-14.
//

#include <ctype.h>
#include "request.h"

const char   *method_in_str[] = {
        "OPTIONS ", "GET ", "HEAD ", "POST ",
        "PUT ", "DELETE ", "TRACE ", "CONNECT ",
};
const char   *schema_str = "http://";
const char   *version_in_str[] = {
        "HTTP/1.0\r\n", "HTTP/1.1\r\n",
};
const char   *field_in_str[] = {
        /* general headers */
        "Cache-Control: ",
        "Connection: ",
        "Date: ",
        "Pragma: ",
        "Trailer: ",
        "Transfer-Encoding: ",
        "Upgrade: ",
        "Via: ",
        "Warning: ",

        /* entity headers */
        "Allow: ",
        "Content-Encoding: ",
        "Content-Language: ",
        "Content-Length: ",
        "Content-Location: ",
        "Content-MD5: ",
        "Content-Range: ",
        "Content-Type: ",
        "Expires: ",
        "Last-Modified: ",

        /* request headers */
        "Accept: ",
        "Accept-Charset: ",
        "Accept-Encoding: ",
        "Accept-Language: ",
        "Authorization: ",
        "Expect: ",
        "From: ",
        "Host: ",
        "If-Match: ",
        "If-Modified-Since: ",
        "If-None-Match: ",
        "If-Range: ",
        "If-Unmodified-Since: ",
        "Max-Forwards: ",
        "Proxy-Authorization: ",
        "Range: ",
        "Referer: ",
        "TE: ",
        "User-Agent: ",

        "Cookie: "
};

const static int    method_length[] = {
        8, 4, 5, 5, 4, 7, 6, 8
};
const static int    schema_length = 7;
const static int    version_length = 10;
const static int    field_length[] = {
        15, 12, 6, 8, 9, 19, 9, 5, 9,
        7, 18, 18, 16, 18, 13, 15, 14, 9, 15,
        8, 16, 17, 17, 15, 8, 6, 6, 10, 19, 15, 10, 21, 14, 21, 6, 9, 4, 12,
        8,
};

#define N_METHOD    sizeof(method_in_str) / sizeof(*method_in_str)
#define N_VERSION   sizeof(version_in_str) / sizeof(*version_in_str)
#define N_FIELD     sizeof(field_in_str) / sizeof(*field_in_str)

static int strnequal(const char *s1, const char *s2);
static int strtwohex(const char *s);
static char *strrch(char *s, char c);

int parse_request_line(request *r)
{
    buffer          *buf;
    request_line    *line;
    char            *ch;
    int             i, n = -1;

    buf = r->header_in;
    line = r->line;
    ch = buffer_read(buf);

    while (*ch) {
        switch (r->parse_state) {
            case method_:
                for (i = 0; i < N_METHOD; ++i) {
                    n = strnequal(ch, method_in_str[i]);
                    if (n >= 0) {
                        break;
                    }
                }

                if (n == method_length[i]) {
                    r->method = i;
                    r->parse_state = method_sp_;

                    line->method_start = ch;
                    line->method_end = ch + n - 1; /* 指向空格 */

                    ch = buffer_seek_start(buf, n);
                }
                else if (n >= 0) {
                    return FCY_AGAIN;
                }
                else{
                    r->parse_state = error_;
                    return FCY_ERROR;
                }

                break;

            case method_sp_:
                if (*ch == '/') {
                    r->parse_state = uri_;
                    line->uri_start = ch;
                }
                else {
                    n = strnequal(ch, schema_str);

                    if (n == schema_length) {
                        r->parse_state = host_;
                        line->schema_start = ch;
                        line->schema_end = ch + n;
                        line->host_start = ch + n;

                        ch = buffer_seek_start(buf, n);
                    }
                    else if (n >= 0) {
                        return FCY_AGAIN;
                    }
                    else{
                        r->parse_state = error_;
                        return FCY_ERROR;
                    }
                }

                break;

            case host_:
                if (isdigit(*ch) || islower(*ch)
                    || *ch == '.' || *ch == '-') {
                    ch = buffer_seek_start(buf, 1);
                    break;
                }
                else if (*ch == ':') {
                    r->parse_state = port_;
                    line->host_end = ch;
                    line->port_start = ch;

                    ch = buffer_seek_start(buf, 1);
                }
                else if (*ch == '/') {
                    r->parse_state = uri_;
                    line->host_end = ch;
                    line->uri_start = ch;

                    ch = buffer_seek_start(buf, 1);
                }
                else {
                    r->parse_state = error_;
                    return FCY_ERROR;
                }

                break;

            case port_:
                if (*ch >= '0' && *ch <= '9') {
                    ch = buffer_seek_start(buf, 1);
                    break;
                }
                else if (*ch == '/') {
                    r->parse_state = uri_;
                    line->port_end = ch;
                    line->uri_start = ch;

                    ch = buffer_seek_start(buf, 1);
                }
                else {
                    r->parse_state = error_;
                    return FCY_ERROR;
                }

                break;

            case uri_:
                if (*ch == ' ') {
                    r->parse_state = version_;

                    line->uri_static = strrch(ch, '/') + 1;
                    line->uri_end = ch;
                    line->version_start = ch + 1;

                    ch = buffer_seek_start(buf, 1);
                }
                else if (*ch == '#') {
                    r->parse_state = uri_sharp_;
                    line->uri_sharp = ch + 1;

                    ch = buffer_seek_start(buf, 1);
                }
                else if (*ch == '?') {
                    r->parse_state = uri_dynamic_;
                    line->uri_dynamic = strrch(ch, '/') + 1;

                    ch = buffer_seek_start(buf, 1);
                }
                else if (*ch == '.') {
                    r->parse_state = uri_static_;
                    line->uri_static = strrch(ch, '/') + 1;

                    ch = buffer_seek_start(buf, 1);
                }
                else if (isalnum(*ch) || *ch == '-' || *ch == '.' || *ch == '_' || *ch == '/' || *ch == '~') {
                    ch = buffer_seek_start(buf, 1);
                }
                else if (*ch == '%') {
                    // 匹配后两个HEX
                    n = strtwohex(ch + 1);

                    if (n == 2) {
                        ch = buffer_seek_start(buf, 3);
                    }
                    else if (n >= 0) {
                        return FCY_AGAIN;
                    }
                    else {
                        r->parse_state = error_;
                        return FCY_ERROR;
                    }
                }
                else {
                    r->parse_state = error_;
                    return FCY_ERROR;
                }

                break;

            case uri_dynamic_:
                if (*ch == ' ') {
                    r->parse_state = version_;
                    line->uri_end = ch;
                    line->version_start = ch + 1;

                    ch = buffer_seek_start(buf, 1);
                }
                else if (isalnum(*ch) || *ch == '_' || *ch == '&' || *ch == '=' || *ch == '-' || *ch == '.' || *ch == '/' || *ch == '~') {
                    ch = buffer_seek_start(buf, 1);
                }
                else if (*ch == '%') {
                    // 匹配后两个HEX
                    n = strtwohex(ch + 1);

                    if (n == 2) {
                        ch = buffer_seek_start(buf, 3);
                    }
                    else if (n >= 0) {
                        return FCY_AGAIN;
                    }
                    else {
                        r->parse_state = error_;
                        return FCY_ERROR;
                    }
                }
                else if (*ch == '#') {
                    r->parse_state = uri_sharp_;
                    line->uri_sharp = ch + 1;

                    ch = buffer_seek_start(buf, 1);
                }
                else {
                    r->parse_state = error_;
                    return FCY_ERROR;
                }

                break;

            case uri_static_:
                if (*ch == ' ') {
                    r->parse_state = version_;
                    line->uri_end = ch;
                    line->version_start = ch + 1;

                    ch = buffer_seek_start(buf, 1);
                }
                else if (isalnum(*ch) || *ch == '_' || *ch == '.' || *ch == '-' || *ch == '~') {
                    ch = buffer_seek_start(buf, 1);
                }
                else if (*ch == '%') {
                    // 匹配后两个HEX
                    n = strtwohex(ch + 1);

                    if (n == 2) {
                        ch = buffer_seek_start(buf, 3);
                    }
                    else if (n >= 0) {
                        return FCY_AGAIN;
                    }
                    else {
                        r->parse_state = error_;
                        return FCY_ERROR;
                    }
                }
                else if (*ch == '#') {
                    r->parse_state = uri_sharp_;
                    line->uri_sharp = ch + 1;

                    ch = buffer_seek_start(buf, 1);
                }
                else {
                    r->parse_state = error_;
                    return FCY_ERROR;
                }

                break;

            case uri_sharp_:
                if (*ch == ' ') {
                    r->parse_state = version_;
                    line->uri_end = ch;
                    line->version_start = ch + 1;

                    ch = buffer_seek_start(buf, 1);
                }
                else if (isalnum(*ch) || *ch == '_' || *ch == '.' || *ch == '-' || *ch == '~') {
                    ch = buffer_seek_start(buf, 1);
                }
                else if (*ch == '%') {
                    // 匹配后两个HEX
                    n = strtwohex(ch + 1);

                    if (n == 2) {
                        ch = buffer_seek_start(buf, 3);
                    }
                    else if (n >= 0) {
                        return FCY_AGAIN;
                    }
                    else {
                        r->parse_state = error_;
                        return FCY_ERROR;
                    }
                }
                else {
                    r->parse_state = error_;
                    return FCY_ERROR;
                }

                break;

            case version_:
                for (i = 0; i < N_VERSION; ++i) {
                    n = strnequal(ch, version_in_str[i]);
                    if (n > 0) {
                        break;
                    }
                }

                if (n == version_length) {
                    r->parse_state = line_done_;
                    r->version = i;
                    line->version_end = ch + n - 2;

                    ch = buffer_seek_start(buf, n);

                    return FCY_OK;
                }
                else if (n >= 0) {
                    return FCY_AGAIN;
                }
                else {
                    r->parse_state = error_;
                    return FCY_ERROR;
                }
            default:
                r->parse_state = error_;
                return FCY_ERROR;
        }
    }

    return FCY_AGAIN;
}

int parse_request_headers(request *r)
{
    buffer          *buf;
    request_headers *headers;
    char            *ch;
    int             i, n = -1;

    buf = r->header_in;
    headers = r->headers;

    ch = buffer_read(buf);

    i = r->parse_header_index;

    while (*ch) {
        switch (r->parse_state) {
            case line_done_:
                n = strnequal(ch, "\r\n");

                if (n == 2) {
                    r->parse_state = method_;
                    ch = buffer_seek_start(buf, 2);
                    return FCY_OK;
                }
                else if (n >= 0) {
                    return FCY_AGAIN;
                }
                else {
                    r->parse_state = field_name_;
                }

                break;

            case field_name_:
                for (i = 0; i < N_FIELD; ++i) {
                    n = strnequal(ch, field_in_str[i]);
                    if (n >= 0) {
                        break;
                    }
                }

                if (n == field_length[i]) {
                    r->parse_state = field_value_;
                    r->parse_header_index = i;
                    /* 设置field_value_start */
                    *((char **) headers + 2 * i) = ch + n;

                    ch = buffer_seek_start(buf, n);
                }
                else if (n >= 0) {
                    return FCY_AGAIN;
                }
                else {  // 返回错误可能是遇到了不认识的header
                    r->parse_header_index = -1;
                    r->parse_state = unknown_filed_name_;
                }

                break;

            case unknown_filed_name_:
                if (isalpha(*ch) || *ch == '-') {
                    ch = buffer_seek_start(buf, 1);
                }
                else {
                    n = strnequal(ch, ": ");

                    if (n == 2) {
                        r->parse_state = field_value_;
                        ch = buffer_seek_start(buf, 2);
                    } else if (n >= 0) {
                        return FCY_AGAIN;
                    } else {
                        r->parse_state = FCY_ERROR;
                        return FCY_ERROR;
                    }
                }

                break;
            case field_value_:
                n = strnequal(ch, "\r\n");
                if (n == 2) {
                    r->parse_state = field_value_sp_;

                    /* 设置field_value_end */
                    if (i != -1) {
                        *((char **) headers + 2 * i + 1) = ch;
                    }
                    ch = buffer_seek_start(buf, 2);
                }
                else if(n >= 0) {
                    return FCY_AGAIN;
                }
                else if (!iscntrl(*ch)) {
                    ch = buffer_seek_start(buf, 1);
                }
                else {
                    r->parse_state = error_;
                    return FCY_ERROR;
                }

                break;

            case field_value_sp_:
                n = strnequal(ch, "\r\n");
                if (n == 2) {
                    r->parse_state = method_;
                    ch = buffer_seek_start(buf, 2);
                    return FCY_OK;
                }
                else if (n >= 0) {
                    return FCY_AGAIN;
                }
                else {
                    r->parse_state = field_name_;
                }

                break;

            default:
                r->parse_state = error_;
                return FCY_ERROR;
        }
    }

    return FCY_AGAIN;
}

static int strnequal(const char *s1, const char *s2)
{
    int count = 0;
    while (*s1 && *s1 == *s2) {
        ++s1;
        ++s2;
        ++count;
    }
    return (*s1 && *s2) ? -1 : count;
}

static int strtwohex(const char *s) {
    char c1, c2;

    c1 = *s;
    if (c1 == '\0') {
        return 0;
    }

    if (isdigit(c1) || (c1 >= 'A' && c1 <='F')) {
        c2 = *(s + 1);
        if (c2 == '\0') {
            return 1;
        }

        if (isdigit(c2) || (c2 >= 'A' && c2 <='F')) {
            return 2;
        }
    }
    return -1;
}

static char* strrch(char *s, char c)
{
    while (*s != c) {
        --s;
    }
    return s;
}