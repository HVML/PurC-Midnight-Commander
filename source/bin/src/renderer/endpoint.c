/*
** endpoint.c -- The endpoint (event/procedure/subscriber) management.
**
** Copyright (c) 2020 FMSoft (http://www.fmsoft.cn)
**
** Author: Vincent Wei (https://github.com/VincentWei)
**
** This file is part of hiBus.
**
** hiBus is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** hiBus is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see http://www.gnu.org/licenses/.
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "lib/hiboxcompat.h"

#include "endpoint.h"
#include "unixsocket.h"
#include "websocket.h"

Endpoint* new_endpoint (Server* srv, int type, void* client)
{
    struct timespec ts;
    Endpoint* endpoint = NULL;

    endpoint = (Endpoint *)calloc (sizeof (Endpoint), 1);
    if (endpoint == NULL)
        return NULL;

    clock_gettime (CLOCK_MONOTONIC, &ts);
    endpoint->t_created = ts.tv_sec;
    endpoint->t_living = ts.tv_sec;
    endpoint->avl.key = NULL;

    switch (type) {
        case ET_UNIX_SOCKET:
        case ET_WEB_SOCKET:
            endpoint->type = type;
            endpoint->status = ES_AUTHING;
            endpoint->entity.client = client;

            endpoint->host_name = NULL;
            endpoint->app_name = NULL;
            endpoint->runner_name = NULL;
            if (!store_dangling_endpoint (srv, endpoint)) {
                ULOG_ERR ("Failed to store dangling endpoint\n");
                free (endpoint);
                return NULL;
            }
            break;

        default:
            ULOG_ERR ("Bad endpoint type\n");
            free (endpoint);
            return NULL;
    }

    if (type == ET_UNIX_SOCKET) {
        USClient* usc = (USClient*)client;
        usc->entity = &endpoint->entity;
    }
    else if (type == ET_WEB_SOCKET) {
        WSClient* wsc = (WSClient*)client;
        wsc->entity = &endpoint->entity;
    }

    return endpoint;
}

int del_endpoint (Server* srv, Endpoint* endpoint, int cause)
{
    char endpoint_name [PCRDR_LEN_ENDPOINT_NAME + 1];

    if (assemble_endpoint_name (endpoint, endpoint_name) > 0) {
        ULOG_INFO ("Deleting an endpoint: %s (%p)\n", endpoint_name, endpoint);
        if (cause == CDE_LOST_CONNECTION || cause == CDE_NO_RESPONDING) {
#if 0
            fire_system_event (srv, SBT_BROKEN_ENDPOINT, endpoint, NULL,
                    (cause == CDE_LOST_CONNECTION) ? "lostConnection" : "noResponding");
#endif
        }

        if (endpoint->avl.key)
            avl_delete (&srv->living_avl, &endpoint->avl);
    }
    else {
        strcpy (endpoint_name, "@endpoint/not/authenticated");
    }

    if (endpoint->host_name) free (endpoint->host_name);
    if (endpoint->app_name) free (endpoint->app_name);
    if (endpoint->runner_name) free (endpoint->runner_name);

    free (endpoint);
    ULOG_WARN ("Endpoint (%s) removed\n", endpoint_name);
    return 0;
}

bool store_dangling_endpoint (Server* srv, Endpoint* endpoint)
{
    if (srv->dangling_endpoints == NULL)
        srv->dangling_endpoints = gslist_create (endpoint);
    else
        srv->dangling_endpoints =
            gslist_insert_append (srv->dangling_endpoints, endpoint);

    if (srv->dangling_endpoints)
        return true;

    return false;
}

bool remove_dangling_endpoint (Server* srv, Endpoint* endpoint)
{
    gs_list* node = srv->dangling_endpoints;

    while (node) {
        if (node->data == endpoint) {
            gslist_remove_node (&srv->dangling_endpoints, node);
            return true;
        }

        node = node->next;
    }

    return false;
}

bool make_endpoint_ready (Server* srv,
        const char* endpoint_name, Endpoint* endpoint)
{
    if (remove_dangling_endpoint (srv, endpoint)) {
        if (!kvlist_set (&srv->endpoint_list, endpoint_name, &endpoint)) {
            ULOG_ERR ("Failed to store the endpoint: %s\n", endpoint_name);
            return false;
        }

        endpoint->t_living = pcrdr_get_monotoic_time ();
        endpoint->avl.key = endpoint;
        if (avl_insert (&srv->living_avl, &endpoint->avl)) {
            ULOG_ERR ("Failed to insert to the living AVL tree: %s\n", endpoint_name);
            assert (0);
            return false;
        }
        srv->nr_endpoints++;
    }
    else {
        ULOG_ERR ("Not found endpoint in dangling list: %s\n", endpoint_name);
        return false;
    }

    return true;
}

static void cleanup_endpoint_client (Server *srv, Endpoint* endpoint)
{
    if (endpoint->type == ET_UNIX_SOCKET) {
        endpoint->entity.client->entity = NULL;
        us_cleanup_client (srv->us_srv, (USClient*)endpoint->entity.client);
    }
    else if (endpoint->type == ET_WEB_SOCKET) {
        endpoint->entity.client->entity = NULL;
        ws_cleanup_client (srv->ws_srv, (WSClient*)endpoint->entity.client);
    }

    ULOG_WARN ("The endpoint (@%s/%s/%s) client cleaned up\n",
            endpoint->host_name, endpoint->app_name, endpoint->runner_name);
}

int check_no_responding_endpoints (Server *srv)
{
    int n = 0;
#if 0
    struct timespec ts;
    const char* name;
    void *next, *data;

    clock_gettime (CLOCK_MONOTONIC, &ts);

    kvlist_for_each_safe (&srv->endpoint_list, name, next, data) {
        Endpoint* endpoint = *(Endpoint **)data;

        if (endpoint->type != ET_BUILTIN &&
                ts.tv_sec > endpoint->t_living + PCRDR_MAX_NO_RESPONDING_TIME) {
            kvlist_delete (&srv->endpoint_list, name);
            cleanup_endpoint_client (srv, endpoint);
            del_endpoint (srv, endpoint, CDE_NO_RESPONDING);
            n++;
        }
    }
#endif

    time_t t_curr = pcrdr_get_monotoic_time ();
    Endpoint *endpoint, *tmp;

    ULOG_INFO ("Checking no responding endpoints...\n");

    avl_for_each_element_safe (&srv->living_avl, endpoint, avl, tmp) {
        char name [PCRDR_LEN_ENDPOINT_NAME + 1];

        assert (endpoint->type != ET_BUILTIN);

        assemble_endpoint_name (endpoint, name);
        if (t_curr > endpoint->t_living + PCRDR_MAX_NO_RESPONDING_TIME) {

            kvlist_delete (&srv->endpoint_list, name);
            cleanup_endpoint_client (srv, endpoint);
            del_endpoint (srv, endpoint, CDE_NO_RESPONDING);
            srv->nr_endpoints--;
            n++;

            ULOG_INFO ("A no-responding client: %s\n", name);
        }
        else if (t_curr > endpoint->t_living + PCRDR_MAX_PING_TIME) {
            if (endpoint->type == ET_UNIX_SOCKET) {
                us_ping_client (srv->us_srv, (USClient *)endpoint->entity.client);
            }
            else if (endpoint->type == ET_WEB_SOCKET) {
                ws_ping_client (srv->ws_srv, (WSClient *)endpoint->entity.client);
            }

            ULOG_INFO ("Ping client: %s\n", name);
        }
        else {
            ULOG_INFO ("Skip left endpoints since (%s): %ld\n", name, endpoint->t_living);
            break;
        }
    }

    ULOG_INFO ("Total endpoints removed: %d\n", n);
    return n;
}

int check_dangling_endpoints (Server *srv)
{
    int n = 0;
    time_t t_curr = pcrdr_get_monotoic_time ();
    gs_list* node = srv->dangling_endpoints;

    while (node) {
        gs_list *next = node->next;
        Endpoint* endpoint = (Endpoint *)node->data;

        if (t_curr > endpoint->t_created + PCRDR_MAX_NO_RESPONDING_TIME) {
            gslist_remove_node (&srv->dangling_endpoints, node);
            cleanup_endpoint_client (srv, endpoint);
            del_endpoint (srv, endpoint, CDE_NO_RESPONDING);
            n++;
        }

        node = next;
    }

    return n;
}

int send_packet_to_endpoint (Server* srv,
        Endpoint* endpoint, const char* body, int len_body)
{
    if (endpoint->type == ET_UNIX_SOCKET) {
        return us_send_packet (srv->us_srv, (USClient *)endpoint->entity.client,
                US_OPCODE_TEXT, body, len_body);
    }
    else if (endpoint->type == ET_WEB_SOCKET) {
        return ws_send_packet (srv->ws_srv, (WSClient *)endpoint->entity.client,
                WS_OPCODE_TEXT, body, len_body);
    }

    return -1;
}

int send_initial_response (Server* srv, Endpoint* endpoint)
{
    int retv = PCRDR_SC_OK;
    pcrdr_msg *msg = NULL;
    char buff [PCRDR_DEF_PACKET_BUFF_SIZE];
    size_t n;

    msg = pcrdr_make_response_message ("0",
            PCRDR_SC_OK, 0,
            PCRDR_MSG_DATA_TYPE_TEXT, SERVER_FEATURES,
            sizeof (SERVER_FEATURES) - 1);
    if (msg == NULL) {
        retv = PCRDR_SC_INTERNAL_SERVER_ERROR;
        goto failed;
    }

    n = pcrdr_serialize_message_to_buffer (msg, buff, sizeof(buff));
    if (n > sizeof(buff)) {
        ULOG_ERR ("The size of buffer for packet is too small.\n");
        retv = PCRDR_SC_INTERNAL_SERVER_ERROR;
    }
    else if (send_packet_to_endpoint (srv, endpoint, buff, n)) {
        endpoint->status = ES_CLOSING;
        retv = PCRDR_SC_IOERR;
    }

    pcrdr_release_message (msg);

failed:
    return retv;
}

