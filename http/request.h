//
// Created by frank on 17_2_14.
//

#ifndef FANCY_REQUEST_H
#define FANCY_REQUEST_H

#include "base.h"
#include "event.h"

/* request method */
#define HTTP_M_OPTIONS      0
#define HTTP_M_GET          1
#define HTTP_M_HEAD         2
#define HTTP_M_POST         3
#define HTTP_M_PUT          4
#define HTTP_M_DELETE       5
#define HTTP_M_TRACE        6
#define HTTP_M_CONNECT      7

/* status code */
#define HTTP_R_OK                               0
#define HTTP_R_BAD_REQUEST                      1
#define HTTP_R_FORBIDDEN                        2
#define HTTP_R_NOT_FOUND                        3
#define HTTP_R_REQUEST_TIME_OUT                 4
#define HTTP_R_LENGTH_REQUIRED                  5
#define HTTP_R_PAYLOAD_TOO_LARGE                6
#define HTTP_R_URI_TOO_LONG                     7
#define HTTP_R_REQUEST_HEADER_FIELD_TOO_LARGE   8
#define HTTP_R_INTARNAL_SEARVE_ERROR            9
#define HTTP_R_NOT_IMPLEMENTED                  10

#define HTTP_V10            0
#define HTTP_V11            1

#define HTTP_POOL_SIZE      MEM_POOL_DEFAULT_SIZE
#define HTTP_HEADER_SIZE    BUFFER_DEFAULT_SIZE



extern const char *method_in_str[];
extern const char *version_in_str[];
extern const char *field_in_str[];

extern const char *status_code_out_str[];
extern const char *content_type_out_str[];

extern const char *file_suffix_str[];

typedef struct request          request;
typedef struct request_line     request_line;
typedef struct request_headers  request_headers;
typedef void (*request_handler)(request*);

struct request {
    int             method;
    int             version;

    request_line    *line;
    request_headers *headers;

    connection      *conn;

    mem_pool        *pool;
    buffer          *header_in;
    buffer          *header_out;

    int             send_fd;
    struct stat     sbuf;

    int             status_code;
    int             content_type;

    int             parse_state;
    int             parse_header_index;
};

request *request_create(connection *c);
void request_destroy(request *r);
void request_print(request *r); /* debug */

struct request_line {
    char        *method_start;
    char        *method_end;

    char        *schema_start;
    char        *schema_end;

    char        *host_start;
    char        *host_end;

    char        *port_start;
    char        *port_end;

    char        *uri_start;
    char        *uri_dynamic;
    char        *uri_static;
    char        *uri_suffix_start;
    char        *uri_suffix_end;
    char        *uri_sharp;
    char        *uri_end;

    char        *version_start;
    char        *version_end;
};

struct request_headers {
    /* general_headers 9 */
    char        *cache_control_start;
    char        *cache_control_end;
    char        *connection_start;
    char        *connection_end;
    char        *date_start;
    char        *date_end;
    char        *pragma_start;
    char        *pragma_end;
    char        *trailer_start;
    char        *trailer_end;
    char        *transfer_encoding_start;
    char        *transfer_encoding_end;
    char        *upgrade_start;
    char        *upgrade_end;
    char        *via_start;
    char        *via_end;
    char        *warning_start;
    char        *warning_end;

    /* entity_headers 10 */
    char        *allow_start;
    char        *allow_end;
    char        *content_encoding_start;
    char        *content_encoding_end;
    char        *content_language_start;
    char        *content_language_end;
    char        *content_length_start;
    char        *content_length_end;
    char        *content_location_start;
    char        *content_location_end;
    char        *content_MD5_start;
    char        *content_MD5_end;
    char        *content_range_start;
    char        *content_range_end;
    char        *content_type_start;
    char        *content_type_end;
    char        *expires_start;
    char        *expires_end;
    char        *last_modified_start;
    char        *last_modified_end;

    /* request_headers 19 */
    char        *accept_start;
    char        *accept_end;
    char        *accept_charset_start;
    char        *accept_charset_end;
    char        *accept_encoding_start;
    char        *accept_encoding_end;
    char        *accept_language_start;
    char        *accept_language_end;
    char        *authorization_start;
    char        *authorization_end;
    char        *expect_start;
    char        *expect_end;
    char        *from_start;
    char        *from_end;
    char        *host_start;
    char        *host_end;
    char        *if_match_start;
    char        *if_match_end;
    char        *if_modified_Since_start;
    char        *if_modified_Since_end;
    char        *if_none_Match_start;
    char        *if_none_Match_end;
    char        *if_range_start;
    char        *if_range_end;
    char        *if_unmodified_Since_start;
    char        *if_unmodified_Since_end;
    char        *max_forwards_start;
    char        *max_forwards_end;
    char        *proxy_authorization_start;
    char        *proxy_authorization_end;
    char        *range_start;
    char        *range_end;
    char        *referer_start;
    char        *referer_end;
    char        *te_start;
    char        *te_end;
    char        *user_agent_start;
    char        *user_agent_end;

    char        *cookie_start;
    char        *cookie_end;
};

enum parse_request_state {
    method_ = 0,
    method_sp_,
    host_,
    port_,
    uri_,
    uri_dynamic_,
    uri_static_,
    uri_sharp_,
    version_,
    line_done_,

    unknown_filed_name_,
    field_name_,
    field_value_,
    field_value_sp_,

    error_,
};

int parse_request_line(request *r);
int parse_request_headers(request *r);

int check_request_header_filed(request *r);
int process_request_static(request *r);


#endif //FANCY_REQUEST_H
