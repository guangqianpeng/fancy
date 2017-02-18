//
// Created by frank on 17-2-16.
//

#include <assert.h>
#include "request.h"

const char *status_code_out_str[] = {
        "200 OK",
        "400 Bad Request",
        "403 Forbidden",
        "404 Not Found",
        "408 Request Time-out",
        "411 Length Required",
        "413 Payload Too Large",
        "414 URI Too Long",
        "431 Request Header Fields Too Large",
        "500 Internal Server Error",
        "501 Not Implemented",
};

const char *content_type_out_str[] = {
        "text/html",
        "text/plain",
        "text/xml",
        "text/asp",
        "text/css",
        "image/gif",
        "image/x-icon",
        "image/png",
        "image/jpeg",
};

const char *file_suffix_str[] = {
        "html", "txt", "xml", "asp", "css",
        "gif", "ico", "png", "jpg", NULL,
};

static void print_str(const char *start, const char *end)
{
    for (; start != end; ++start) {
        putchar(*start);
    }
    fflush(stdout);
}

request *request_create(connection *c)
{
    request     *r;
    mem_pool    *p;

    p = mem_pool_create(HTTP_POOL_SIZE);
    if (p == NULL) {
        return NULL;
    }

    r = pcalloc(p, sizeof(*r));
    if (r == NULL) {
        mem_pool_destroy(p);
        return NULL;
    }

    r->line = pcalloc(p, sizeof(*r->line));
    r->headers = pcalloc(p, sizeof(*r->headers));
    r->header_in = buffer_create(p, HTTP_HEADER_SIZE);
    r->header_out = buffer_create(p, HTTP_HEADER_SIZE);
    if (r->line == NULL ||
        r->headers == NULL ||
        r->header_in == NULL ||
        r->header_out == NULL) {
        mem_pool_destroy(p);
        return NULL;
    }

    r->conn = c;
    r->pool = p;

    assert(c->app == NULL);  /* 同一时刻只允许一个app占用connection */
    c->app = r;

    return r;
}

void request_destroy(request *r)
{
    r->conn->app = NULL;

    if (r->send_fd > 0) {
        close(r->send_fd);
    }

    mem_pool_destroy(r->pool);
}

void request_print(request *r)
{
    request_line *line;
    request_headers *headers;
    char **start;

    line = r->line;
    headers = r->headers;
    start = (char **) headers;

    assert(line->method_start);
    print_str(line->method_start, line->method_end);
    putchar(' ');


    if (line->schema_start) {
        print_str(line->schema_start, line->schema_end);
    }

    if (line->host_start) {
        print_str(line->host_start, line->host_end);
    }

    if (line->port_start) {
        print_str(line->port_start, line->port_end);
    }

    assert(line->uri_start);
    print_str(line->uri_start, line->uri_end);
    printf(" ");

    assert(line->version_start);
    print_str(line->version_start, line->version_end);
    printf("\n");

    if (line->uri_static) {
        printf("\tstatic service: ");
        print_str(line->uri_static, line->uri_end);
        printf("\n");
    }

    if (line->uri_suffix_start) {
        printf("\tsuffix: ");
        print_str(line->uri_suffix_start, line->uri_suffix_end);
        printf("\n");
    }

    if (line->uri_dynamic) {
        printf("\tdynamic service: ");
        print_str(line->uri_dynamic, line->uri_end);
        printf("\n");
    }

    if (line->uri_sharp) {
        printf("\tfragment: ");
        print_str(line->uri_sharp, line->uri_end);
        printf("\n");
    }

    for (int i = 0; i < sizeof(*headers) / sizeof(char *) / 2; ++i) {
        if (start[i * 2]) {
            printf("%s", field_in_str[i]);
            print_str(start[i * 2], start[i * 2 + 1]);
            printf("\n");
        }
    }

    printf("%s\n\n", status_code_out_str[r->status_code]);
}