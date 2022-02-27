/*
** endpoint.c -- The endpoint session management.
**
** Copyright (c) 2020 ~ 2022 FMSoft (http://www.fmsoft.cn)
**
** Author: Vincent Wei (https://github.com/VincentWei)
**
** This file is part of PurC Midnigth Commander.
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

#include <purc/purc-dom.h>

#include "lib/hiboxcompat.h"

#include "endpoint.h"
#include "unixsocket.h"
#include "websocket.h"

typedef struct PlainWindow {
    purc_variant_t      name;
    purc_variant_t      title;
    pcdom_document_t    *dom;
} PlainWindow;

struct SessionInfo_ {
    struct kvlist       wins;
    unsigned int        nr_wins;
};

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
#if 0
        if (cause == CDE_LOST_CONNECTION || cause == CDE_NO_RESPONDING) {
            fire_system_event (srv, SBT_BROKEN_ENDPOINT, endpoint, NULL,
                    (cause == CDE_LOST_CONNECTION) ? "lostConnection" : "noResponding");
        }
#endif

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

static int send_simple_response(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    size_t n;
    char buff [PCRDR_DEF_PACKET_BUFF_SIZE];

    n = pcrdr_serialize_message_to_buffer (msg, buff, sizeof(buff));
    if (n > sizeof(buff)) {
        ULOG_ERR ("The size of buffer for simple response packet is too small.\n");
        retv = PCRDR_SC_INTERNAL_SERVER_ERROR;
    }
    else if (send_packet_to_endpoint (srv, endpoint, buff, n)) {
        endpoint->status = ES_CLOSING;
        retv = PCRDR_SC_IOERR;
    }

    return retv;
}

int send_initial_response (Server* srv, Endpoint* endpoint)
{
    int retv = PCRDR_SC_OK;
    pcrdr_msg *msg = NULL;

    msg = pcrdr_make_response_message ("0",
            PCRDR_SC_OK, 0,
            PCRDR_MSG_DATA_TYPE_TEXT, SERVER_FEATURES,
            sizeof (SERVER_FEATURES) - 1);
    if (msg == NULL) {
        retv = PCRDR_SC_INTERNAL_SERVER_ERROR;
        goto failed;
    }

    retv = send_simple_response(srv, endpoint, msg);

    pcrdr_release_message (msg);

failed:
    return retv;
}

typedef int (*request_handler)(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg);

static int authenticate_endpoint(Server* srv, Endpoint* endpoint,
        purc_variant_t data)
{
    const char* prot_name = NULL;
    const char *host_name = NULL, *app_name = NULL, *runner_name = NULL;
    uint64_t prot_ver = 0;
    char norm_host_name [PCRDR_LEN_HOST_NAME + 1];
    char norm_app_name [PCRDR_LEN_APP_NAME + 1];
    char norm_runner_name [PCRDR_LEN_RUNNER_NAME + 1];
    char endpoint_name [PCRDR_LEN_ENDPOINT_NAME + 1];
    purc_variant_t tmp;

    if ((tmp = purc_variant_object_get_by_ckey(data,
                    "protocolName", false))) {
        prot_name = purc_variant_get_string_const(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(data,
                    "protocolVersion", false))) {
        purc_variant_cast_to_ulongint(tmp, &prot_ver, true);
    }

    if ((tmp = purc_variant_object_get_by_ckey(data, "hostName", false))) {
        host_name = purc_variant_get_string_const(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(data, "appName", false))) {
        app_name = purc_variant_get_string_const(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(data, "runnerName", false))) {
        runner_name = purc_variant_get_string_const(tmp);
    }

    if (prot_name == NULL || prot_ver > PCRDR_PURCMC_PROTOCOL_VERSION ||
            host_name == NULL || app_name == NULL || runner_name == NULL ||
            strcasecmp (prot_name, PCRDR_PURCMC_PROTOCOL_NAME)) {
        ULOG_WARN ("Bad packet data for authentication: %s, %s, %s, %s\n",
                prot_name, host_name, app_name, runner_name);
        return PCRDR_SC_BAD_REQUEST;
    }

    if (prot_ver < PCRDR_PURCMC_MINIMAL_PROTOCOL_VERSION)
        return PCRDR_SC_UPGRADE_REQUIRED;

    if (!pcrdr_is_valid_host_name (host_name) ||
            !pcrdr_is_valid_app_name (app_name) ||
            !pcrdr_is_valid_token (runner_name, PCRDR_LEN_RUNNER_NAME)) {
        ULOG_WARN ("Bad endpoint name: @%s/%s/%s\n",
                host_name, app_name, runner_name);
        return PCRDR_SC_NOT_ACCEPTABLE;
    }

    pcrdr_name_tolower_copy (host_name, norm_host_name, PCRDR_LEN_HOST_NAME);
    pcrdr_name_tolower_copy (app_name, norm_app_name, PCRDR_LEN_APP_NAME);
    pcrdr_name_tolower_copy (runner_name, norm_runner_name, PCRDR_LEN_RUNNER_NAME);
    host_name = norm_host_name;
    app_name = norm_app_name;
    runner_name = norm_runner_name;

    /* make endpoint ready here */
    if (endpoint->type == CT_UNIX_SOCKET) {
        /* override the host name */
        host_name = PCRDR_LOCALHOST;
    }
    else {
        /* TODO: handle hostname for web socket connections here */
        host_name = PCRDR_LOCALHOST;
    }

    pcrdr_assemble_endpoint_name (host_name,
                    app_name, runner_name, endpoint_name);

    ULOG_INFO ("New endpoint: %s (%p)\n", endpoint_name, endpoint);

    if (kvlist_get (&srv->endpoint_list, endpoint_name)) {
        ULOG_WARN ("Duplicated endpoint: %s\n", endpoint_name);
        return PCRDR_SC_CONFLICT;
    }

    if (!make_endpoint_ready (srv, endpoint_name, endpoint)) {
        ULOG_ERR ("Failed to store the endpoint: %s\n", endpoint_name);
        return PCRDR_SC_INSUFFICIENT_STORAGE;
    }

    ULOG_INFO ("New endpoint stored: %s (%p), %d endpoints totally.\n",
            endpoint_name, endpoint, srv->nr_endpoints);

    endpoint->host_name = strdup (host_name);
    endpoint->app_name = strdup (app_name);
    endpoint->runner_name = strdup (runner_name);
    endpoint->status = ES_READY;

    return PCRDR_SC_OK;
}

static int on_start_session(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;
    SessionInfo *info = NULL;

    int retv = authenticate_endpoint(srv, endpoint, msg->data);

    if (retv == PCRDR_SC_OK) {
        info = calloc(1, sizeof(SessionInfo));
        if (info == NULL) {
            retv = PCRDR_SC_INSUFFICIENT_STORAGE;
        }
        else {
            kvlist_init(&info->wins, NULL);
            endpoint->session_info = info;
        }
    }

    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = retv;
    response.resultValue = (uintptr_t)info;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_end_session(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;
    const char *name;
    void *next, *data;
    // PlainWindow *win;

    kvlist_for_each_safe(&endpoint->session_info->wins, name, next, data) {
        // win = *(PlainWindow **)data;

        // TODO: delete window and DOM:
        // del_endpoint (&the_server, endpoint, CDE_EXITING);
        kvlist_delete (&endpoint->session_info->wins, name);
    }
    kvlist_free(&endpoint->session_info->wins);
    free(endpoint->session_info);
    endpoint->session_info = NULL;

    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = PCRDR_SC_OK;
    response.resultValue = 0;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_create_plain_window(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;

    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = PCRDR_SC_OK;
    response.resultValue = 0;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_update_plain_window(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;

    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = PCRDR_SC_OK;
    response.resultValue = 0;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_destroy_plain_window(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;

    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = PCRDR_SC_OK;
    response.resultValue = 0;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_load(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;

    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = PCRDR_SC_OK;
    response.resultValue = 0;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_update(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;

    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = PCRDR_SC_OK;
    response.resultValue = 0;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static struct request_handler {
    const char *operation;
    request_handler handler;
} handlers[] = {
    { PCRDR_OPERATION_CREATEPLAINWINDOW, on_create_plain_window },
    { PCRDR_OPERATION_DESTROYPLAINWINDOW, on_destroy_plain_window },
    { PCRDR_OPERATION_ENDSESSION, on_end_session },
    { PCRDR_OPERATION_LOAD, on_load },
    { PCRDR_OPERATION_STARTSESSION, on_start_session },
    { PCRDR_OPERATION_UPDATE, on_update },
    { PCRDR_OPERATION_UPDATEPLAINWINDOW, on_update_plain_window },
};

static request_handler find_request_handler(const char* operation)
{
    static ssize_t max = sizeof(handlers)/sizeof(handlers[0]) - 1;

    ssize_t low = 0, high = max, mid;
    while (low <= high) {
        int cmp;

        mid = (low + high) / 2;
        cmp = strcasecmp(operation, handlers[mid].operation);
        if (cmp == 0) {
            goto found;
        }
        else {
            if (cmp < 0) {
                high = mid - 1;
            }
            else {
                low = mid + 1;
            }
        }
    }

    return NULL;

found:
    return handlers[mid].handler;
}

int on_got_message(Server* srv, Endpoint* endpoint, const pcrdr_msg *msg)
{
    if (msg->type == PCRDR_MSG_TYPE_REQUEST) {
        request_handler handler = find_request_handler(
                purc_variant_get_string_const(msg->operation));

        ULOG_INFO("Got a rquest message: %s (handler: %p)\n",
                purc_variant_get_string_const(msg->operation), handler);

        if (handler) {
            return handler(srv, endpoint, msg);
        }
        else {
            pcrdr_msg response;
            response.type = PCRDR_MSG_TYPE_RESPONSE;
            response.requestId = msg->requestId;
            response.retCode = PCRDR_SC_BAD_REQUEST;
            response.resultValue = 0;
            response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

            return send_simple_response(srv, endpoint, &response);
        }
    }
    else if (msg->type == PCRDR_MSG_TYPE_EVENT) {
        // TODO
        ULOG_INFO("Got an event message: %s\n",
                purc_variant_get_string_const(msg->event));
    }
    else {
        // TODO
        ULOG_INFO("Got an unknown message: %d\n", msg->type);
    }

    return PCRDR_SC_OK;
}

