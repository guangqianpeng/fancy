//
// Created by frank on 17-5-18.
//

#include "upstream.h"

upstream *upstream_create(peer_connection *conn)
{
    // TODO
    static upstream upstm;
    return &upstm;
}

void upstream_destroy(upstream *conn)
{

}