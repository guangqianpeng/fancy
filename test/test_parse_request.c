//
// Created by frank on 17-2-14.
//

#include <stdio.h>
#include <assert.h>
#include "palloc.h"
#include "request.h"

static void test_parse(request *rqst);
static void reset_request(request *rqst);


int main()
{
    mem_pool    *pool;
    request     rqst;

    pool = mem_pool_create(MEM_POOL_DEFAULT_SIZE);
    if (pool == NULL) {
        err_quit("mem_pool_create error");
    }

    bzero(&rqst, sizeof(rqst));
    rqst.header_in = buffer_create(pool, BUFFER_DEFAULT_SIZE);
    if (rqst.header_in == NULL) {
        err_quit("buffer_create error");
    }

    rqst.parse_state = 0;
    test_parse(&rqst);
    printf("OK");
}

static void test_parse(request *rqst)
{

}

static void test_request_method(request *rqst)
{
    buffer  *header_in = rqst->header_in;
    size_t  n_cases;

    const static char *methods[] = {
            "OPTIONS h", "GET ht", "HEAD http", "POST http:",
            "PUT http:/", "DELETE ", "TRACE ", "CONNECT ",
    };

    const static char *fake[] = {
            "ASDGH",
            "OPTIONS http:/?",
            "PUTS",
            "OPTIONS/index.html",
            " CONNECT",
            "\r\nGET",
            "GES http://www.baidu.com/index.html HTTP/1.1\r\n",
            "GET\r\n",
            "\r\n",
    };


    /* true case */
    n_cases = sizeof(methods) / sizeof(*methods);

    for (size_t i = 0; i < n_cases; ++i) {
        buffer_write(header_in, methods[i], strlen(methods[i]));

        assert(parse_request_line(rqst) == FCY_AGAIN);
        assert(rqst->parse_state == method_sp_);

        reset_request(rqst, method_);
    }

    /* false case */
    n_cases = sizeof(fake) / sizeof(*fake);

    for (size_t i = 0; i < n_cases; ++i) {
        buffer_write(header_in, fake[i], strlen(fake[i]));

        assert(parse_request_line(rqst) == FCY_ERROR);
        assert(rqst->parse_state == error_);

        reset_request(rqst, method_);
    }



    /* 模拟网络传输 */
    n_cases = sizeof(methods) / sizeof(*methods);

    for (size_t i = 0; i < n_cases; ++i) {
        /* 一次写一个字符 */
        const char *c;
        for (c = methods[i]; *(c + 1) ; ++c) {
            buffer_write(header_in, c, 1);

            assert(parse_request_line(rqst) == FCY_AGAIN);
        }

        buffer_write(header_in, c, 1);

        assert(parse_request_line(rqst) == FCY_AGAIN);
        assert(rqst->parse_state == method_sp_);

        reset_request(rqst, method_);
    }

    printf("test_request_method OK\n");
}

static void test_request_host_port(request *rqst)
{
    buffer  *header_in = rqst->header_in;
    size_t  n_cases;

    const static char *host[] = {
            "OPTIONS http://www.baidu.com:",
            "GET http://a.b.c.d:123",
            "HEAD http://.:80",
            "POST http://...a-0-1...:345",
            "PUT http://:",
            "DELETE http://-:",
            "TRACE http://.-.:",
            "CONNECT http://---:456",
    };

    const static char *fake[] = {
            "OPTIONS http://www.baidu.com?:",
            "GET http://a.b.c.d:e",
            "HEAD http://.:80-",
            "POST http://...a-0-1...:#345",
            "PUT http://def>",
            "DELETE http://-+:",
            "TRACE hdfs://.-.:ff",
            "CONNECT file://www.baidu.com:80",
    };

    /* true case */
    n_cases = sizeof(host) / sizeof(*host);

    for (size_t i = 0; i < n_cases; ++i) {
        buffer_write(header_in, host[i], strlen(host[i]));

        assert(parse_request_line(rqst) == FCY_AGAIN);
        assert(rqst->parse_state == port_);

        reset_request(rqst, method_);
    }

    /* false case */
    n_cases = sizeof(fake) / sizeof(*fake);

    for (size_t i = 0; i < n_cases; ++i) {
        buffer_write(header_in, fake[i], strlen(fake[i]));

        assert(parse_request_line(rqst) == FCY_ERROR);
        assert(rqst->parse_state == error_);

        reset_request(rqst, method_);
    }

    /* 模拟网络传输 */

    n_cases = sizeof(host) / sizeof(*host);

    for (size_t i = 0; i < n_cases; ++i) {
        /* 一次写一个字符 */
        const char *c;
        for (c = host[i]; *(c + 1); ++c) {
            buffer_write(header_in, c, 1);

            assert(parse_request_line(rqst) == FCY_AGAIN);
        }

        buffer_write(header_in, c, 1);

        assert(parse_request_line(rqst) == FCY_AGAIN);
        assert(rqst->parse_state == port_);

        reset_request(rqst, method_);
    }

    printf("test_request_host_port OK\n");
}

static void test_request_uri(request *rqst)
{
    buffer  *header_in = rqst->header_in;
    size_t  n_cases;

    const static char *uri[] = {
            "GET http://www.localhost.com:9877/ ",

            "OPTIONS http://www.baidu.com:/ ",
            "GET http://a.b.c.d:123/a/b/c/d//// ",
            "HEAD http://////////what.html ",
            "POST / ",
            "PUT /asdf/wer ",
            "DELETE /asdf/wer?a=b&c=d ",
            "TRACE http://.-.:/?& ",
            "CONNECT http://---:456/ ",

            "POST http://www.bing.com/fd/ls/lsp.aspx ",
            "GET http://static.blog.csdn.net/css/common.css ",
            "GET http://googleads.g.doubleclick.net/pagead/gen_204?id=wfocus&gqid=ZkekWPDuIIXS2QTfyZOoBQ&qqid=CN6I8pOLktICFUY5lgod80MHaw&fg=1 ",
            "GET http://www.w3.org/Protocols/rfc2616/rfc2616-sec9.html#sec9.6 ",
            "GET http://pan.baidu.com/disk/home?errno=0&errmsg=Auth%20Login%20Sucess&&bduss=&ssnerror=0#list "
    };

    const static char *fake[] = {
            "GET http://static.blog.csdn.net/../logs/error.log ",
            "OPTIONS http://www.baidu.com:/% ",
            "GET http://a.b.c.d:123/a/b/c/d////+ ",
            "HEAD http://////////what.html* ",
            "POST /=",
            "PUT /as+df/wer ",
            "DELETE /asdf/wer?a=b#c=d ",
            "TRACE http://.-.:/?#& ",
            "CONNECT http://---:456/啥 ",
    };

    /* true case */
    n_cases = sizeof(uri) / sizeof(*uri);

    for (size_t i = 0; i < n_cases; ++i) {
        buffer_write(header_in, uri[i], strlen(uri[i]));

        assert(parse_request_line(rqst) == FCY_AGAIN);
        assert(rqst->parse_state == version_);

        reset_request(rqst, method_);
    }

    /* false case */
    n_cases = sizeof(fake) / sizeof(*fake);

    for (size_t i = 0; i < n_cases; ++i) {
        buffer_write(header_in, fake[i], strlen(fake[i]));

        assert(parse_request_line(rqst) == FCY_ERROR);
        assert(rqst->parse_state = error_);

        reset_request(rqst, method_);
    }

    /* 模拟网络传输 */

    n_cases = sizeof(uri) / sizeof(*uri);

    for (size_t i = 0; i < n_cases; ++i) {
        /* 一次写一个字符 */
        const char *c;
        for (c = uri[i]; *(c + 1); ++c) {
            buffer_write(header_in, c, 1);

            assert(parse_request_line(rqst) == FCY_AGAIN);
        }

        buffer_write(header_in, c, 1);

        assert(parse_request_line(rqst) == FCY_AGAIN);
        assert(rqst->parse_state == version_);

        reset_request(rqst, method_);
    }

    printf("test_request_uri OK\n");
}

static void test_request_version(request *rqst)
{
    buffer  *header_in = rqst->header_in;
    size_t  n_cases;

    const static char *uri[] = {
            "OPTIONS http://www.baidu.com:/ HTTP/1.0\r\n",
            "GET http://a.b.c.d:123/a/b/c/d//// HTTP/1.1\r\n",
            "HEAD http://////////what.html HTTP/1.1\r\n",
            "POST / HTTP/1.1\r\n",
            "PUT /asdf/wer HTTP/1.1\r\n",
            "DELETE /asdf/wer?a=b&c=d HTTP/1.1\r\n",
            "TRACE http://.-.:/?& HTTP/1.1\r\n",
            "CONNECT http://---:456/ HTTP/1.1\r\n",
    };

    const static char *fake[] = {
            "OPTIONS http://www.baidu.com:/ HTTP/1.0\n\r",
            "GET http://a.b.c.d:123/a/b/c/d//// http/1.1\r\n",
            "HEAD http://////////what.html HTTP/1.2\r\n",
            "POST / HTTP/1.1?\r\n",
            "PUT /asdf/wer HTTP/1.1xx\r\n",
            "DELETE /asdf/wer?a=b&c=d HttP/1.1\r\n",
            "TRACE http://.-.:/?& ?HTTP/1.0\r\n",
            "CONNECT http://---:456/ HTTP/1.1.987654321\r\n",
    };

    /* true case */
    n_cases = sizeof(uri) / sizeof(*uri);

    for (size_t i = 0; i < n_cases; ++i) {
        buffer_write(header_in, uri[i], strlen(uri[i]));

        assert(parse_request_line(rqst) == FCY_OK);
        assert(rqst->parse_state == line_done_);

        reset_request(rqst, method_);
    }

    /* false case */
    n_cases = sizeof(fake) / sizeof(*fake);

    for (size_t i = 0; i < n_cases; ++i) {
        buffer_write(header_in, fake[i], strlen(fake[i]));

        assert(parse_request_line(rqst) == FCY_ERROR);
        assert(rqst->parse_state = error_);

        reset_request(rqst, method_);
    }

    /* 模拟网络传输 */

    n_cases = sizeof(uri) / sizeof(*uri);

    for (size_t i = 0; i < n_cases; ++i) {
        /* 一次写一个字符 */
        const char *c;
        for (c = uri[i]; *(c + 1); ++c) {
            buffer_write(header_in, c, 1);

            assert(parse_request_line(rqst) == FCY_AGAIN);
        }

        buffer_write(header_in, c, 1);

        assert(parse_request_line(rqst) == FCY_OK);
        assert(rqst->parse_state == line_done_);

        reset_request(rqst, method_);
    }

    printf("test_request_version OK\n");
}

static void test_request_header(request *rqst)
{
    buffer  *header_in = rqst->header_in;
    size_t  n_cases;

    const static char *header[] = {
            "User-Agent: X\r\n\r\n",
            "\r\n",
            "Date: 1994-12-23\r\n\r\n",
            "Date: 1994-12-23\r\nContent-Length: 123456\r\n\r\n",
            "Accept: ???\r\n\r\n",
            "Host: www.bing.com\r\n"
                    "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:51.0) Gecko/20100101 Firefox/51.0\r\n"
                    "Accept: */*\r\n"
                    "Accept-Language: en-US,en;q=0.5\r\n"
                    "Accept-Encoding: gzip, deflate, br\r\n"
                    "Referer: https://www.bing.com/\r\n"
                    "Content-Type: text/xml\r\n"
                    "Content-Length: 804\r\n"
                    "Cookie: MUID=235E5F06DD3C658020A85698D93C660B; SRCHD=AF=MOZLBR; SRCHUID=V=2&GUID=4CE45B5F266540CFA5DA41555A189A82; SRCHUSR=DOB=20161203; MUIDB=235E5F06DD3C658020A85698D93C660B; SRCHHPGUSR=CW=1908&CH=408&DPR=1&UTC=480; _RwBf=s=70&o=16; _SS=SID=39FD82DA02066B342A4688F003A76AEA&bIm=493&PC=MOZI&HV=1487159554&R=100; _EDGE_S=mkt=zh-cn&ui=en-us&SID=39FD82DA02066B342A4688F003A76AEA; WLS=TS=63622756352; SRCHS=PC=MOZI; _FP=hta=on; ipv6=hit=1; SNRHOP=I=&TS=\r\n"
                    "Connection: keep-alive\r\n\r\n"
    };

    const static char *fake[] = {
            "d?ate: 1994-12-23\r\nContent-Length: 123456\r\nUser-Agent: X\r\n",
            "Accept: ???\n\r",
            "Accept: \t\r\n",
            "Accept: \r\n\n",
            "Accept: \r\n??",
            "Accept: \r\nContent-Length: \n\r",
    };

    /* true case */
    n_cases = sizeof(header) / sizeof(*header);

    for (size_t i = 0; i < n_cases; ++i) {
        buffer_write(header_in, header[i], strlen(header[i]));

        assert(parse_request_headers(rqst) == FCY_OK);
        assert(rqst->parse_state == method_);

        reset_request(rqst, line_done_);
    }

    /* false case */
    n_cases = sizeof(fake) / sizeof(*fake);

    for (size_t i = 0; i < n_cases; ++i) {
        buffer_write(header_in, fake[i], strlen(fake[i]));

        assert(parse_request_headers(rqst) == FCY_ERROR);
        assert(rqst->parse_state = error_);

        reset_request(rqst, line_done_);
    }

    /* 模拟网络传输 */

    n_cases = sizeof(header) / sizeof(*header);

    for (size_t i = 0; i < n_cases; ++i) {
        /* 一次写一个字符 */
        const char *c;
        for (c = header[i]; *(c + 1); ++c) {
            buffer_write(header_in, c, 1);

            assert(parse_request_headers(rqst) == FCY_AGAIN);
        }

        buffer_write(header_in, c, 1);

        assert(parse_request_headers(rqst) == FCY_OK);
        assert(rqst->parse_state == method_);

        reset_request(rqst, line_done_);
    }

    printf("test_request_header OK\n");
}

static void reset_request(request *rqst)
{
    buffer          *buf = rqst->header_in;
    request_line    *line = rqst->line;
    request_headers *header = rqst->headers;

    bzero(line, sizeof(*line));
    bzero(header, sizeof(*header));
    bzero(rqst, sizeof(*rqst));

    rqst->header_in = buf;
    rqst->line = line;
    rqst->headers = header;
    rqst->parse_state = state;

    buffer_reset(buf);
}