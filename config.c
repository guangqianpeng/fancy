//
// Created by frank on 17-5-27.
//

#include <request.h>
#include "config.h"
#include "palloc.h"
#include "request.h"
#include "log.h"

static mem_pool *pool;

static const char *config_main(const char *s, void *);
static const char *config_events(const char *s, void *);
static const char *config_server(const char *s, void *);
static const char *config_location(const char *s, void *);

static const char *config_bool(const char *s, void *d);
static const char *config_num_positive(const char *s, void *d);
static const char *config_log_path(const char *s, void *d);

static const char *config_str_semicolons(const char *s, void *d);
static const char *config_str_brace(const char *s, void *d);

static const char *config_log_level(const char *s, void *d);
static const char *config_proxy_pass(const char *s, void *d);
static const char *config_root(const char *s, void *d);
static const char *config_index(const char *s, void *d);

static const char *config_comment(const char *s, void *d);
static void config_error(const char *expect, const char *see);

static const char *first_not_space(const char *s);
static const char *expect(const char *s, char ch);

typedef struct conf_block   conf_block;
typedef const char *(*conf_callback)(const char*, void*);

struct conf_block {
    const char      *str;
    size_t          len;
    conf_callback   cb;
    void            *data;
};

/* main conf */
int         master_process      = -1;
int         worker_processes    = -1;
int         log_level = -1;
const char  *log_path = NULL;

/* events conf */
int worker_connections  = -1;
int epoll_events        = -1;

/* server conf */
int listen_on           = -1;
int request_timeout     = -1;
int upstream_timeout    = -1;
int keep_alive_requests = -1;
int accept_defer        = -1;

/*location conf*/
array    *locations;

static conf_block conf_main_block[] = {
        {"master_process", 14, config_bool, &master_process},
        {"worker_processes", 16, config_num_positive, &worker_processes},
        {"log_level", 9, config_log_level, &log_level},
        {"log_path", 8, config_log_path, &log_path},
        {"events", 6, config_events, NULL},
        {"server", 6, config_server, NULL},
        {"#", 1, config_comment, NULL},
        {NULL},
};

static conf_block conf_events_block[] = {
        {"worker_connections", 18, config_num_positive, &worker_connections},
        {"epoll_events", 12, config_num_positive, &epoll_events},
        {"#", 1, config_comment, NULL},
        {NULL},
};

static conf_block conf_server_block[] = {
        {"listen_on", 9, config_num_positive, &listen_on},
        {"request_timeout", 15, config_num_positive, &request_timeout},
        {"upstream_timeout", 16, config_num_positive, &upstream_timeout},
        {"keep_alive_requests", 19, config_num_positive, &keep_alive_requests},
        {"accept_defer", 12, config_num_positive, &accept_defer},
        {"location", 8, config_location, NULL},
        {"#", 1, config_comment, NULL},
        {NULL},
};

static conf_block conf_location_block[] = {
        {"root", 4, config_root, NULL},
        {"index", 5, config_index, NULL},
        {"proxy_pass", 10, config_proxy_pass, NULL},
        {"#", 1, config_comment, NULL},
        {NULL},
};

void config(const char *path, mem_pool *p)
{
    pool = p;
    locations = array_create(pool, 4, sizeof(location));
    struct stat sbuf;
    if (stat(path, &sbuf) == -1) {
        perror("config stat error");
        exit(1);
    }

    char file[sbuf.st_size + 1];
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("open error");
        exit(1);
    }

    if (read(fd, file, (size_t)sbuf.st_size) != sbuf.st_size) {
        perror("read error");
        exit(1);
    }

    file[sbuf.st_size] = '\0';

    CHECK(close(fd));
    config_main(file, NULL);

    for (size_t i = 0; i < locations->size; ++i) {
        location *loc = array_at(locations, i);
        if (loc->use_proxy) {
            printf("proxy_pass %s:%hu\n", inet_ntoa(loc->proxy_pass.sin_addr), ntohs(loc->proxy_pass.sin_port));
        }
        else {
            printf("root: %s\n", loc->s.root);
            printf("index:");
            for (size_t j = 0; loc->s.index[j] != NULL; ++j) {
                printf(" %s", loc->s.index[j]);
            }
            printf("\n");
        }
    }
}

static const char *config_main(const char *s, void *d)
{
    conf_block *b = conf_main_block;

    s = first_not_space(s);
    for (; *s != '\0'; s = first_not_space(s)) {
        int i = 0;
        for (; b[i].str != NULL; ++i) {
            if (strncmp(s, b[i].str, b[i].len) == 0) {
                s += b[i].len;
                s = b[i].cb(s, b[i].data);
                break;
            }
        }
        if (b[i].str == NULL) {
            fprintf(stderr, "unknown config %s", s);
            exit(1);
        }
    }
    return s;
}

static const char *config_events(const char *s, void *d)
{
    conf_block *b = conf_events_block;

    s = expect(s, '{');
    s = first_not_space(s);
    for ( ;*s != '}' ; s = first_not_space(s)) {
        int i = 0;
        for (; b[i].str != NULL; ++i) {
            if (strncmp(s, b[i].str, b[i].len) == 0) {
                s += b[i].len;
                s = b[i].cb(s, b[i].data);
                break;
            }
        }
        if (b[i].str == NULL) {
            fprintf(stderr, "unknown config %s", s);
            exit(1);
        }
    }
    return expect(s, '}');
}

static const char *config_server(const char *s, void *d)
{
    conf_block *b = conf_server_block;

    s = expect(s, '{');
    s = first_not_space(s);
    for (; *s != '}'; s = first_not_space(s)) {
        int i = 0;
        for (; b[i].str != NULL; ++i) {
            if (strncmp(s, b[i].str, b[i].len) == 0) {
                s += b[i].len;
                s = b[i].cb(s, b[i].data);
                break;
            }
        }
        if (b[i].str == NULL) {
            fprintf(stderr, "unknown config %s", s);
            exit(1);
        }
    }
    return expect(s, '}');
}

static const char *config_location(const char *s, void *d)
{
    conf_block  *b = conf_location_block;
    location    *loc = array_alloc(locations);

    s = config_str_brace(s, &loc->prefix);
    s = expect(s, '{');
    s = first_not_space(s);
    for (; *s != '}'; s = first_not_space(s)) {

        int i = 0;
        for (; b[i].str != NULL; ++i) {
            if (strncmp(s, b[i].str, b[i].len) == 0) {
                s += b[i].len;
                s = b[i].cb(s, loc);
                break;
            }
        }

        if (b[i].str == NULL) {
            fprintf(stderr, "unknown config %s", s);
            exit(1);
        }
    }
    return expect(s, '}');
}

static const char *config_bool(const char *s, void *d)
{
    int *on = d;

    s = first_not_space(s);

    if (strncmp(s, "on", 2) == 0) {
        *on = 1;
        s += 2;
    }
    else if (strncmp(s, "off", 3) == 0) {
        *on = 0;
        s += 3;
    }
    else {
        config_error("on/off", s);
    }

    return expect(s, ';');
}

static const char *config_num_positive(const char *s, void *d)
{
    int *num = d;

    s = first_not_space(s);
    if (isdigit(*s)) {
        *num = atoi(s);
        if (*num <= 0) {
            config_error("positive number", s);
        }
    }
    else {
        config_error("number", s);
    }

    while (isdigit(*s))
        ++s;
    return expect(s, ';');
}

static const char *config_log_path(const char *s, void *d)
{
    s = config_str_semicolons(s, d);
    return expect(s, ';');
}

static const char *config_str_semicolons(const char *s, void *d)
{
    char        **str = d;

    s = first_not_space(s);

    const char  *end = s;

    while (*end != '\0' && !isspace(*end) && *end != ';')
        ++end;

    *str = pcalloc(pool, end - s + 1);
    strncpy(*str, s, end - s);
    s = end;

    return s;
}

static const char *config_str_brace(const char *s, void *d)
{
    char        **str = d;

    s = first_not_space(s);

    const char  *end = s;

    while (*end != '\0' && !isspace(*end) && *end != '{')
        ++end;

    *str = pcalloc(pool, end - s + 1);
    strncpy(*str, s, end - s);
    s = end;

    return s;
}

static const char *config_log_level(const char *s, void *d)
{
    const static char *level_str[] = {
            "debug",
            "info",
            "warn",
            "error",
            "fatal",
            "off",
            NULL,
    };

    s = first_not_space(s);

    int *level = d;
    int i = 0;
    for (; level_str[i] != NULL; ++i) {
        if (strncmp(s, level_str[i], strlen(level_str[i])) == 0) {
            *level = i;
            break;
        }
    }

    if (level_str[i] == NULL) {
        config_error("log level%s", s);
    }

    s += strlen(level_str[i]);

    return expect(s, ';');
}

static const char *config_proxy_pass(const char *s, void *d)
{
    location            *loc = d;
    struct sockaddr_in  *addr = &loc->proxy_pass;

    s = first_not_space(s);

    addr->sin_family = AF_INET;

    const char *colon = strchr(s, ':');
    char       temp[colon - s + 1];
    strncpy(temp, s, colon - s);
    temp[colon - s] = '\0';

    if (inet_pton(AF_INET, temp, &addr->sin_addr) != 1) {
        goto error;
    }

    if (!isdigit(colon[1])) {
        goto error;
    }

    int port = atoi(colon + 1);
    if (port <= 0 || port > USHRT_MAX) {
        goto error;
    }

    addr->sin_port = htons((uint16_t)port);
    loc->use_proxy = 1;

    s = colon + 2;
    while (isdigit(*s)) {
        ++s;
    }

    return expect(s, ';');

    error:
    fprintf(stderr, "invalid proxy_pass %s", s);
    exit(1);
}

static const char *config_root(const char *s, void *d)
{
    location *loc = d;
    int      dirfd;

    s = config_str_semicolons(s, &loc->s.root);
    dirfd = open(loc->s.root, O_DIRECTORY | O_RDONLY);
    if (dirfd == -1) {
        perror("open error");
        exit(1);
    }
    loc->s.root_dirfd = dirfd;

    return expect(s, ';');
}

static const char *config_index(const char *s, void *d)
{
    location    *loc = d;
    int         i = 0;

    for ( ;i < MAX_INDEX; ++i) {
        s = first_not_space(s);
        if (!isalnum(*s)) {
            break;
        }
        s = config_str_semicolons(s, &loc->s.index[i]);
    }

    if (i == MAX_INDEX) {
        fprintf(stderr, "config error: too many index %s", s);
        exit(1);
    }

    loc->s.index[i] = NULL;

    return expect(s, ';');
}

static const char *config_comment(const char *s, void *d)
{
    while (*s != '\0' && *s != '\n')
        ++s;
    return s;
}

static void config_error(const char *expect, const char *see)
{
    fprintf(stderr, "config error: %s\nbut see %s", expect, see);
    exit(1);
}

static const char *first_not_space(const char *s)
{
    while (isspace(*s))
        ++s;
    return s;
}

static const char *expect(const char *s, char ch)
{
    while (isspace(*s))
        ++s;
    if (*s != ch) {
        fprintf(stderr, "config error: missing \"%c\" before %s", ch, s);
        exit(1);
    }
    else {
        return s + 1;
    }
}