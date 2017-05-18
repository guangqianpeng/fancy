#include <ctype.h>
#include "request.h"

static int parse_request_line(request *r);
static int parse_request_headers(request *r);
static int parse_uri(request *r);

typedef int(*parse_handler)(request*);

static parse_handler handlers[] = {
    parse_request_line,
    parse_request_headers,
    parse_uri,
    NULL,
};

int parse_request(request *r)
{
    int         err;
    static int  i = 0;

    for (; handlers[i] != NULL; ++i) {
        err = handlers[i](r);

        if (err == FCY_OK) {
            r->parse_state = 0;
            continue;
        }

        i = 0;
        return err;
    }

    i = 0;
    return FCY_OK;
}

static int parse_request_line(request *r)
{
    char            c, *p;
    buffer          *header_in;
    enum {
        start_ = 0,
        method_,
        space_before_uri_,
        uri_,
        space_before_version_,
        version_H_,
        version_HT_,
        version_HTT_,
        version_HTTP_,
        version_HTTP_slash_,
        version_HTTP_slash_1_,
        version_HTTP_slash_1_dot_,
        space_after_version_,
        almost_done_,
        done_,
        error_,
    } state;

    header_in = r->header_in;
    state = r->parse_state;

    for (p = buffer_read(header_in);
         !buffer_empty(header_in);
         p = buffer_seek_start(header_in, 1)) {

        c = *p;
        switch (state) {
            case start_:
                /* method_start_ */
                r->request_start = p;
                if (isupper(c)) {
                    state = method_;
                    break;
                }
                goto error;

            case method_:
                if (isupper(c)) {
                    break;
                }/* method_end */
                if (c == ' ') {
                    if (strncmp(r->request_start, "GET", 3) == 0) {
                        r->method = HTTP_M_GET;
                    }
                    else if (strncmp(r->request_start, "POST", 4) == 0) {
                        r->method = HTTP_M_POST;
                    }
                    else if (strncmp(r->request_start, "HEAD", 4) == 0) {
                        r->method = HTTP_M_HEAD;
                    }
                    else {
                        goto error;
                    }

                    state = space_before_uri_;
                    break;
                }

            case space_before_uri_:
                if (c == ' ') {
                    break;
                }
                if (c == '/') {
                    r->uri_start = p;
                    state = uri_;
                    break;
                }
                goto error;

            case uri_:
                if (c == ' ') {
                    r->uri_end = p;
                    state = space_before_version_;
                    break;
                }
                if (!iscntrl(c)) {
                    break;
                }
                goto error;

            case space_before_version_:
                if (c == ' ') {
                    break;
                }
                if ((c | 0x20) == 'h') {
                    state = version_H_;
                    break;
                }
                goto error;

            case version_H_:
                if ((c | 0x20) == 't') {
                    state = version_HT_;
                    break;
                }
                goto error;

            case version_HT_:
                if ((c | 0x20) == 't') {
                    state = version_HTT_;
                    break;
                }
                goto error;

            case version_HTT_:
                if ((c | 0x20) == 'p') {
                    state = version_HTTP_;
                    break;
                }
                goto error;

            case version_HTTP_:
                if (c == '/') {
                    state = version_HTTP_slash_;
                    break;
                }
                goto error;

            case version_HTTP_slash_:
                if (c == '1') {
                    state = version_HTTP_slash_1_;
                    break;
                }

            case version_HTTP_slash_1_:
                if (c == '.') {
                    state = version_HTTP_slash_1_dot_;
                    break;
                }
                goto error;

            case version_HTTP_slash_1_dot_:
                if (c == '0') {
                    r->version = HTTP_V10;
                    r->keep_alive = 0;
                    state = space_after_version_;
                    break;
                }
                if (c == '1') {
                    r->version = HTTP_V11;
                    r->keep_alive = 1;
                    state = space_after_version_;
                    break;
                }
                goto error;

            case space_after_version_:
                if (c == ' ') {
                    break;
                }
                if (c == '\r') {
                    state = almost_done_;
                    break;
                }
                goto error;

            case almost_done_:
                if (c == '\n') {
                    buffer_seek_start(header_in, 1);
                    state = done_;
                    goto done;
                }
                goto error;

            default:
                assert(0);
        }
    }

    done:
    r->parse_state = state;
    return (state == done_ ? FCY_OK : FCY_AGAIN);

    error:
    r->parse_state = error_;
    return FCY_ERROR;
}

static int parse_request_headers(request *r)
{
    char c, *p;
    buffer *header_in;
    enum {
        start_ = 0,
        name_,
        space_before_value_,
        value_,
        space_after_value_,
        almost_done_,
        all_headers_almost_done,
        all_done_,
        error_,
    } state;

    header_in = r->header_in;
    state = r->parse_state;

    for (p = buffer_read(header_in);
         !buffer_empty(header_in);
         p = buffer_seek_start(header_in, 1)) {

        c = *p;
        switch (state) {
            case start_:
                if (c == '\r') {
                    state = all_headers_almost_done;
                    break;
                }
                if (isalpha(c) || c == '-') {
                    state = name_;
                    r->last_header_name_start = p;
                    break;
                }
                goto error;

            case name_:
                if (isalpha(c) || c == '-') {
                    break;
                }
                if (c == ':') {
                    state = space_before_value_;
                    break;
                }
                goto error;

            case space_before_value_:
                if (c == ' ') {
                    break;
                }
                if (!iscntrl(c)) {
                    r->last_header_value_start = p;
                    state = value_;
                    break;
                }
                goto error;

            case value_:
                if (c == '\r' || c == ' ') {
                    if (strncasecmp(r->last_header_name_start, "Host", 4) == 0) {
                        r->has_host_header = 1;
                    }
                    else if (strncasecmp(r->last_header_name_start, "Connection", 10) == 0) {
                        if (strncasecmp(r->last_header_value_start, "keep-alive", 10) == 0) {
                            r->keep_alive = 1;
                        }
                        else {
                            r->keep_alive = 0;
                        }
                    }
                    else if (strncasecmp(r->last_header_name_start, "Content-Length", 14) == 0) {
                        r->has_content_length_header = 1;
                        r->content_length = strtol(r->last_header_value_start, NULL, 0);
                    }

                    state = (c == '\r' ? almost_done_ : space_before_value_);
                    break;
                }
                if (!iscntrl(c)) {
                    break;
                }
                goto error;

            case space_after_value_:
                if (c == ' ') {
                    break;
                }
                if (c == '\r') {
                    state = almost_done_;
                    break;
                }
                goto error;

            case almost_done_:
                if (c == '\n') {
                    state = start_;
                    break;
                }
                goto error;

            case all_headers_almost_done:
                if (c == '\n') {
                    buffer_seek_start(header_in, 1);
                    state = all_done_;
                    goto done;
                }

            default:
                assert(0);
        }
    }

    done:
    r->parse_state = state;
    return (state == all_done_ ? FCY_OK : FCY_AGAIN);

    error:
    r->parse_state = error_;
    return FCY_ERROR;
}

static int parse_uri(request *r)
{
    int     hex1, hex2;
    char    c, *p, *u, *last_dot = NULL;
    enum {
        start_ = 0,
        after_slash_,
        quote_,
        args_,
        error_,
    } state;

    state = r->parse_state;
    u = r->uri;

    /* 注意，此时uri已经读完了，不需要考虑FCY_AGAIN的情况 */
    for (p = r->uri_start; p < r->uri_end; ++p) {

        c = *p;
        switch (state) {
            case start_:
                if (c == '/') {
                    u = r->uri = palloc(r->pool, r->uri_end - r->uri_start + 11);
                    if (r->uri == NULL) {
                        goto error;
                    }
                    *u++ = '/';
                    state = after_slash_;
                    break;
                }
                goto error;

            case after_slash_:
                switch (c) {
                    case '/':
                        *u++ = '/';
                        last_dot = NULL;
                        break;
                    case '#':
                        goto done;
                    case '?':
                        r->has_args = 1;
                        *u++ = '?';
                        state = args_;
                        break;
                    case '%':
                        state = quote_;
                        break;
                    case '.':
                        last_dot = u;
                        *u++ = '.';
                        break;
                    default:
                        *u++ = c;
                        break;
                }
                break;

            case quote_:
                if (isdigit(c)) {
                    hex1 = c - '0';
                }
                else {
                    hex1 = (c | 0x20);
                    if (hex1 >= 'a' && hex1 <= 'f') {
                        hex1 = hex1 - 'a' + 10;
                    }
                    else {
                        goto error;
                    }
                }

                c = *++p;

                if (isdigit(c)) {
                    hex2 = c - '0';
                }
                else {
                    hex2 = (c | 0x20);
                    if (hex2 >= 'a' && hex2 <= 'f') {
                        hex2 = hex2 - 'a' + 10;
                    }
                    else {
                        goto error;
                    }
                }

                *u++ = (char)((hex1 << 4) + hex2);
                state = after_slash_;
                break;

            case args_:
                switch (c) {
                    case '#':
                        goto done;
                    case '/':
                        goto error;
                    default:
                        *u++ = c;
                        break;
                }
                break;

            default:
                assert(0);
        }
    }

    done:
    // 文件后缀
    if (last_dot) {
        r->suffix = last_dot + 1;
    }

        // 访问文件夹, 结尾无'/'
    if (!last_dot && !r->has_args && *(u - 1) != '/') {
        strcpy(u, "/index.html");
        u += 11;
        r->suffix = u - 4;
    }
        // 访问的文件夹但结尾没有'/'
    else if (*(u - 1) == '/') {
        strcpy(u, "index.html");
        u += 10;
        r->suffix = u - 4;
    }

    *u = '\0';
    return FCY_OK;

    error:
    r->parse_state = error_;
    return FCY_ERROR;
}