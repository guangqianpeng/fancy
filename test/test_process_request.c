//
// Created by frank on 17-2-15.
//

#include <assert.h>
#include <stdio.h>
#include "request.h"

const static char *dataset[] = {
        "POST http://www.bing.com/fd/ls/lsp.aspx HTTP/1.0\r\n"
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
                "<ClientInstRequest><Events><E><T>Event.ClientInst</T><IG>A5D081DE892A44AF9E65916FA16AD054</IG><TS>1487160768092</TS><D><![CDATA[[{\"T\":\"CI.BoxModel\",\"FID\":\"CI\",\"Name\":\"v2.8\",\"SV\":\"4\",\"P\":{\"C\":17,\"N\":8,\"I\":\"2jm\",\"S\":\"C+V\",\"M\":\"V+L+M+MT+E+N+C+K+BD\",\"T\":1215440,\"K\":\"mh8z+mh9v\",\"F\":0},\"V\":\"q119//1////////scroll+q12l//13////////+q133//21////////+q13j//38////////+q140//4f////////+q14g//5o////////+q14y//6t////////+q15e//7x////////+q15u//8w////////+q16b//9u////////+q16r//ao////////+q179//bb////////+q186//c9////////+q193//ci////////\",\"N\":\"@11/p//@5/ls%2Flsp.aspx/xmlhttprequest/v/@6/@11/0/@11/mh90/@12/@12\",\"C\":\"q0io/j////ss/b7/+q0js/////sq/9k/+q0kw/////si/82/+q0pm//mousedown///sd/6t/1+q0s2//mouseup/////0+q0s6//click/////+q0y0//@m///ry/6x/\"}]]]></D></E></Events><STS>1487160768092</STS></ClientInstRequest>",

        "GET http://static.blog.csdn.net/css/common.css HTTP/1.1\r\n"
                "Host: static.blog.csdn.net\r\n"
                "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:51.0) Gecko/20100101 Firefox/51.0\r\n"
                "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                "Accept-Language: en-US,en;q=0.5\r\n"
                "Accept-Encoding: gzip, deflate\r\n"
                "Cookie: uuid_tt_dd=3973608807328730451_20160928; __message_sys_msg_id=0; __message_gu_msg_id=0; __message_cnel_msg_id=0; Hm_lvt_6bcd52f51e9b3dce32bec4a3997715ac=1486955517,1486991159,1487159557,1487161187; UN=qq_19450531; UE=\"\"; BT=1487142258000; _ga=GA1.2.1285523080.1484371756; __message_in_school=0; UserName=qq_19450531; UserInfo=AYfNBRq2h3SFG2ys%2ByVm2FcC6%2FJPbJ65COXS6qqcUs%2BKPAqcAzPzkrBTpziPT8nEFN4B4m4BjuKYQAxpSYK3ZQ%3D%3D; UserNick=qq_19450531; AU=450; access-token=09b5f29b-3fe9-4412-94b1-12b60eb95398; __message_district_code=000000; Hm_lpvt_6bcd52f51e9b3dce32bec4a3997715ac=1487161187; dc_tos=olf0wz; dc_session_id=1487161187989\r\n"
                "Connection: keep-alive\r\n\r\n",

        "GET http://googleads.g.doubleclick.net/pagead/gen_204?id=wfocus&gqid=ZkekWPDuIIXS2QTfyZOoBQ&qqid=CN6I8pOLktICFUY5lgod80MHaw&fg=1 HTTP/1.1\r\n"
                "Host: googleads.g.doubleclick.net\r\n"
                "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:51.0) Gecko/20100101 Firefox/51.0\r\n"
                "Accept: */*\r\n"
                "Accept-Language: en-US,en;q=0.5\r\n"
                "Accept-Encoding: gzip, deflate\r\n"
                "Referer: http://googleads.g.doubleclick.net/pagead/ads?client=ca-pub-1076724771190722&format=960x90&output=html&h=90&slotname=9757026646&adk=1004154756&adf=3407270574&w=960&afmt=auto&rafmt=1&ea=0&flash=11.2.202&url=http%3A%2F%2Fwww.csdn.net%2F&resp_fmts=3&wgl=1&dt=1487161190359&bpp=18&bdt=33&fdt=25&idt=90&shv=r20170208&cbv=r20170110&saldr=aa&correlator=8280351656302&frm=8&ga_vid=1285523080.1484371756&ga_sid=1487161190&ga_hid=540748168&ga_fc=0&pv=2&icsg=2&nhd=3&dssz=2&mdo=0&mso=0&u_tz=480&u_his=1&u_java=0&u_h=990&u_w=1760&u_ah=925&u_aw=1760&u_cd=24&u_nplug=4&u_nmime=7&dff=sans-serif&dfs=16&adx=0&ady=0&biw=-12245933&bih=-12245933&isw=960&ish=90&ifk=2888106221&eid=33509847&oid=3&loc=http%3A%2F%2Fblog.csdn.net%2Fz69183787%2Farticle%2Fdetails%2F19153405&rx=0&eae=2&brdim=%2C%2C0%2C22%2C1760%2C22%2C1760%2C925%2C960%2C90&vis=2&rsz=%7C%7CceE%7C&abl=NS&ppjl=f&pfx=0&fu=136&bc=1&ifi=1&dtd=132\r\n"
                "Cookie: id=223919fead0b004d||t=1474440980|et=730|cs=002213fd48c3665071787ac7ac\r\n"
                "Connection: keep-alive\r\n\r\n",

        "GET http://www.w3.org/Protocols/rfc2616/rfc2616-sec9.html#sec9.6 HTTP/1.1\r\n"
                "Host: www.w3.org\r\n"
                "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:51.0) Gecko/20100101 Firefox/51.0\r\n"
                "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                "Accept-Language: en-US,en;q=0.5\r\n"
                "Accept-Encoding: gzip, deflate, br\r\n"
                "Referer: https://www.w3.org/Protocols/rfc2616/rfc2616-sec5.html\r\n"
                "Connection: keep-alive\r\n"
                "Upgrade-Insecure-Requests: 1\r\n"
                "If-Modified-Since: Wed, 01 Sep 2004 13:24:52 GMT\r\n"
                "If-None-Match: \"40d7-3e3073913b100\"\r\n"
                "Cache-Control: max-age=0\r\n\r\n",

        "GET http://pan.baidu.com/disk/home?errno=0&errmsg=Auth%20Login%20Sucess&bduss=&ssnerror=0#listpath HTTP/1.1\r\n"
                "Host: pan.baidu.com\r\n"
                "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:51.0) Gecko/20100101 Firefox/51.0\r\n"
                "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                "Accept-Language: en-US,en;q=0.5\r\n"
                "Accept-Encoding: gzip, deflate, br\r\n"
                "Cookie: PANWEB=1; bdshare_firstime=1484836245135; Hm_lvt_7a3960b6f067eb0085b7f96ff5e660b0=1484836245,1487165219; secu=1; BAIDUID=22069CADECC2055FD947A2929E94FC78:FG=1; panlogin_animate_showed=1; BDUSS=lowSVFiQmhWbjhxbE5KU1FSSFlOdTY1MWRvaWtMbjJ0WXZ2UksyfjJNSWE1TXRZSVFBQUFBJCQAAAAAAAAAAAEAAADwn9oiRnJhbmtfxe0AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABpXpFgaV6RYR; pan_login_way=1; STOKEN=74c10f51522fd42651855376a5f8ec0a664a3729f52dc6e9ac2a1a445ada78f7; SCRC=f2634c2781c09410946c71924832ed83; PANPSC=8509914216082675368%3ASzpdS1fQcpuNVcD8Vg%2F2Ce%2FVvwJ0ndn1%2FjHwFzqIEtWEqKzs90OCRROAD5r1J1nb24QLyane47qoWuyTokTY1Er%2FP%2FllHCWljRT8o6c2oTqP8XxTJKWpuFIA%2FsYpD6GsYfpwQsTukZ8rWOqA3q3PCcDuiFJchHVyObuDsIux8jhE%2BhLVkDMtirBxzfcLsdyQ; Hm_lpvt_7a3960b6f067eb0085b7f96ff5e660b0=1487165219; cflag=15%3A3\r\n"
                "Connection: keep-alive\r\n"
                "Upgrade-Insecure-Requests: 1\r\n"  /* 忽略不支持的header */
                "Cache-Control: max-age=0\r\n\r\n",

        /* HTTP1.1必须提供Host */
        "GET http://pan.baidu.com/disk/home?errno=0&errmsg=Auth%20Login%20Sucess&bduss=&ssnerror=0#listpath HTTP/1.1\r\n"
                "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:51.0) Gecko/20100101 Firefox/51.0\r\n"
                "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                "Accept-Language: en-US,en;q=0.5\r\n"
                "Accept-Encoding: gzip, deflate, br\r\n\r\n",

        /* POST必须提供Content-Length */
        "POST http://pan.baidu.com/disk/home?errno=0&errmsg=Auth%20Login%20Sucess&bduss=&ssnerror=0#listpath HTTP/1.1\r\n"
                "Host: pan.baidu.com\r\n"
                "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:51.0) Gecko/20100101 Firefox/51.0\r\n"
                "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                "Accept-Language: en-US,en;q=0.5\r\n"
                "Accept-Encoding: gzip, deflate, br\r\n\r\n",

        /* 目前只实现 HEAD GET POST */
        "PUT http://pan.baidu.com/disk/home?errno=0&errmsg=Auth%20Login%20Sucess&bduss=&ssnerror=0#listpath HTTP/1.0\r\n"
                "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:51.0) Gecko/20100101 Firefox/51.0\r\n"
                "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                "Accept-Language: en-US,en;q=0.5\r\n"
                "Accept-Encoding: gzip, deflate, br\r\n\r\n",

        /* Content-Length不合法 */
        "POST http://www.bing.com/fd/ls/lsp.aspx HTTP/1.1\r\n"
                "Host: www.bing.com\r\n"
                "Content-Length: 80400000000000\r\n\r\n",

        "POST http://www.bing.com/fd/ls/lsp.aspx HTTP/1.1\r\n"
                "Host: www.bing.com\r\n"
                "Content-Length: -1\r\n\r\n",

        "GET http://static.blog.csdn.net/50x.html HTTP/1.1\r\n"
                "Host: static.blog.csdn.net\r\n\r\n",

        "GET http://static.blog.csdn.net/index.html HTTP/1.1\r\n"
                "Host: static.blog.csdn.net\r\n\r\n",

        "GET http://www.localhost.com:9877/ HTTP/1.1\r\n"
                "Host: www.localhost.com:9877\r\n"
                "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:51.0) Gecko/20100101 Firefox/51.0\r\n"
                "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                "Accept-Language: en-US,en;q=0.5\r\n"
                "Accept-Encoding: gzip, deflate\r\n"
                "Connection: keep-alive\r\n"
                "Upgrade-Insecure-Requests: 1\r\n\r\n"
};


static request *create_request(mem_pool *pool);
static void test_one_request(request *r, const char *data);

int main()
{
    mem_pool    *pool;
    request     *rqst;
    int         n;

    pool = mem_pool_create(MEM_POOL_DEFAULT_SIZE);
    if (pool == NULL) {
        err_quit("mem_pool_create error");
    }

    n = sizeof(dataset) / sizeof(*dataset);
    for (int i = 0; i < n; ++i) {
        rqst = create_request(pool);
        if (rqst == NULL) {
            err_quit("create_request error");
        }

        test_one_request(rqst, dataset[i]);
    }

    mem_pool_destroy(pool);
}

static request *create_request(mem_pool *pool)
{
    request     *rqst;

    rqst = pcalloc(pool, sizeof(*rqst));
    if (rqst == NULL) {
        return NULL;
    }

    rqst->line = pcalloc(pool, sizeof(*rqst->line));
    rqst->headers = pcalloc(pool, sizeof(*rqst->headers));
    rqst->header_in = buffer_create(pool, BUFFER_DEFAULT_SIZE);
    rqst->conn = pcalloc(pool, sizeof(*rqst->conn));

    if (rqst->line == NULL || rqst->headers == NULL || rqst->conn == NULL) {
        return NULL;
    }
    if (rqst->header_in == NULL) {
        return NULL;
    }

    return rqst;
}

static void test_one_request(request *r, const char *data)
{
    buffer      *in;
    size_t      i, n;
    int         err;

    in = r->header_in;
    n = strlen(data);

    for (i = 0; i < n; ++i) {
        if (buffer_write(in, &data[i], 1) == -1) {
            err_quit("buffer_write error");
            return;
        }

        err = parse_request_line(r);
        if (err == FCY_ERROR) {
            err_quit("parse_request_line error");
            return;
        }
        else if (err == FCY_OK) {
            break;
        }
        /* FCY_AGAIN */
    }

    for (++i; i < n; ++i) {
        if (buffer_write(in, &data[i], 1) == -1) {
            err_quit("buffer_write error");
            return;
        }

        err = parse_request_headers(r);
        if (err == FCY_ERROR) {
            err_quit("parse_request_headers error");
            return;
        }
        else if (err == FCY_OK) {
            break;
        }
    }


    err = process_request_header(r);
    if (err == FCY_OK && r->line->uri_static) {
        process_request_static(r);
    }

    request_print(r);
}
