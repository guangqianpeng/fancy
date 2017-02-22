//
// Created by frank on 17-2-12.
//

#include "conn_pool.h"

static int          n_conn;
static connection   *conns;
static event        *revents;
static event        *wevents;
static list         conn_list;

static int conn_init(connection *conn);
static void conn_free(connection *conn);
static void event_set_field(event *ev);

int conn_pool_init(mem_pool *p, int size)
{
    n_conn = size;

    list_init(&conn_list);

    conns = palloc(p, n_conn * sizeof(connection));
    revents = palloc(p, n_conn * sizeof(event));
    wevents = palloc(p, n_conn * sizeof(event));

    if (conns == NULL || revents == NULL || wevents == NULL) {
        return FCY_ERROR;
    }

    for (int i = n_conn - 1; i >= 0; --i) {
        conns[i].read = &revents[i];
        conns[i].write = &wevents[i];
        revents[i].conn = wevents[i].conn = &conns[i];

        list_insert_head(&conn_list, &conns[i].node);
    }

    return FCY_OK;
}

connection *conn_pool_get()
{
    list_node   *head;
    connection  *conn;

    if (list_empty(&conn_list)) {
        return NULL;
    }

    head = list_head(&conn_list);
    list_remove(head);

    conn = link_data(head, connection, node);

    if (conn_init(conn) == -1) {
        return NULL;
    }

    return conn;
}

void conn_pool_free(connection *conn)
{
    conn_free(conn);
    list_insert_head(&conn_list, &conn->node);
}

static int conn_init(connection *conn)
{
    conn->pool = mem_pool_create(MEM_POOL_DEFAULT_SIZE);
    if (conn->pool == NULL) {
        return -1;
    }

    conn->buf = buffer_create(conn->pool, BUFFER_DEFAULT_SIZE);
    if (conn->buf == NULL) {
        return -1;
    }

    conn->fd = -1;
    conn->app_count = 0;
    conn->app = NULL;
    bzero(&conn->addr, sizeof(conn->addr));

    event_set_field(conn->read);
    event_set_field(conn->write);

    return 0;
}

static void conn_free(connection *conn)
{
    mem_pool_destroy(conn->pool);
}

static void event_set_field(event *ev)
{
    ev->active = 0;
    ev->read = 0;
    ev->write = 0;
    ev->timer_set = 0;
    ev->timeout = 0;
    ev->handler = NULL;

    bzero(&ev->rb_node, sizeof(ev->rb_node));
}