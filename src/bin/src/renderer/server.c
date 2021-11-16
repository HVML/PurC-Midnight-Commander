/*
** server.c -- the renderer server.
**
** Copyright (c) 2020, 2021 FMSoft (http://www.fmsoft.cn)
**
** Author: Vincent Wei (https://github.com/VincentWei)
**
** This file is part of PurC Midnight Commander.
**
** PurcMC is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** PurcMC is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see http://www.gnu.org/licenses/.
*/

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <time.h>

#include <unistd.h>
#if HAVE(SYS_EPOLL_H)
#include <sys/epoll.h>
#elif HAVE(SYS_SELECT_H)
#include <sys/select.h>
#else
#error no `epoll` either `select` found.
#endif

#include "lib/kvlist.h"
#include "lib/hook.h"
#include "lib/global.h"

#include "ulog.h"
#include "server.h"
#include "websocket.h"
#include "unixsocket.h"
#include "endpoint.h"

extern hook_t *idle_hook;

static Server the_server;

#define srvcfg mc_global.rdr

/* callbacks for socket servers */
// Allocate a Endpoint structure for a new client and send `auth` packet.
static int
on_accepted (void* sock_srv, SockClient* client)
{
    Endpoint* endpoint;

    (void)sock_srv;
    endpoint = new_endpoint (&the_server,
            (client->ct == CT_WEB_SOCKET) ? ET_WEB_SOCKET : ET_UNIX_SOCKET,
            client);

    if (endpoint == NULL)
        return SERVER_SC_INSUFFICIENT_STORAGE;

#if 0
    int ret_code;
    // send challenge code
    ret_code = send_challenge_code (&the_server, endpoint);
    if (ret_code != SERVER_SC_OK)
        return ret_code;
#endif

    return SERVER_SC_OK;
}

static int
on_packet (void* sock_srv, SockClient* client,
            const char* body, unsigned int sz_body, int type)
{
    assert (client->entity);

    (void)sock_srv;
    if (type == PT_TEXT) {
        (void)client;
        (void)body;
        (void)sz_body;
        // TODO: handle packet
    }
    else {
        /* discard all packet in binary */
        return SERVER_SC_NOT_ACCEPTABLE;
    }

    return SERVER_SC_OK;
}

static int
on_pending (void* sock_srv, SockClient* client)
{
#if HAVE(SYS_EPOLL_H)
    struct epoll_event ev;

    (void)sock_srv;
    ev.events = EPOLLIN | EPOLLOUT;
    ev.data.ptr = client;
    if (epoll_ctl (the_server.epollfd, EPOLL_CTL_MOD, client->fd, &ev) == -1) {
        ULOG_ERR ("Failed epoll_ctl to the client fd (%d): %s\n",
                client->fd, strerror (errno));
        assert (0);
    }
#elif HAVE(SYS_SELECT_H)
    (void)sock_srv;
    (void)client;
    // TODO
#endif

    return 0;
}

static int
on_close (void* sock_srv, SockClient* client)
{
    (void)sock_srv;
#if HAVE(SYS_EPOLL_H)
    if (epoll_ctl (the_server.epollfd, EPOLL_CTL_DEL, client->fd, NULL) == -1) {
        ULOG_WARN ("Failed to call epoll_ctl to delete the client fd (%d): %s\n",
                client->fd, strerror (errno));
    }
#elif HAVE(SYS_SELECT_H)
    // TODO
#endif

    if (client->entity) {
        Endpoint *endpoint = container_of (client->entity, Endpoint, entity);
        char endpoint_name [SERVER_LEN_ENDPOINT_NAME + 1];

        if (endpoint->status == ES_AUTHING) {
            remove_dangling_endpoint (&the_server, endpoint);
            ULOG_INFO ("An endpoint not authenticated removed: (%p), %d endpoints left.\n",
                    endpoint, the_server.nr_endpoints);
        }
        else {
            assemble_endpoint_name (endpoint, endpoint_name);
            if (kvlist_delete (&the_server.endpoint_list, endpoint_name))
                the_server.nr_endpoints--;

            ULOG_INFO ("An authenticated endpoint removed: %s (%p), %d endpoints left.\n",
                    endpoint_name, endpoint, the_server.nr_endpoints);
        }
        del_endpoint (&the_server, endpoint, CDE_LOST_CONNECTION);

        client->entity = NULL;
    }

    return 0;
}

static void
on_error (void* sock_srv, SockClient* client, int err_code)
{
    int n;
    char buff [SERVER_MIN_PACKET_BUFF_SIZE];

    if (err_code == SERVER_SC_IOERR)
        return;

    n = snprintf (buff, sizeof (buff), 
            "{"
            "\"packetType\":\"error\","
            "\"protocolName\":\"%s\","
            "\"protocolVersion\":%d,"
            "\"retCode\":%d,"
            "\"retMsg\":\"%s\""
            "}",
            SERVER_PROTOCOL_NAME, SERVER_PROTOCOL_VERSION,
            err_code, server_get_ret_message (err_code));

    if (n < 0 || (size_t)n >= sizeof (buff)) {
        // should never reach here
        assert (0);
    }

    if (client->ct == CT_UNIX_SOCKET) {
        us_send_packet (sock_srv, (USClient *)client, US_OPCODE_TEXT, buff, n);
    }
    else {
        ws_send_packet (sock_srv, (WSClient *)client, WS_OPCODE_TEXT, buff, n);
    }
}

static inline void
update_endpoint_living_time (Server *srv, Endpoint* endpoint)
{
    if (endpoint && endpoint->avl.key) {
        time_t t_curr = server_get_monotoic_time ();

        if (endpoint->t_living != t_curr) {
            endpoint->t_living = t_curr;
            avl_delete (&srv->living_avl, &endpoint->avl);
            avl_insert (&srv->living_avl, &endpoint->avl);
        }
    }
}

/* max events for epoll */
#define MAX_EVENTS          10
#define PTR_FOR_US_LISTENER ((void *)1)
#define PTR_FOR_WS_LISTENER ((void *)2)

static void
prepare_server (void)
{
    int us_listener = -1, ws_listener = -1;
#if HAVE(SYS_EPOLL_H)
    struct epoll_event ev, events[MAX_EVENTS];
    time_t t_start = server_get_monotoic_time ();
    time_t t_elapsed, t_elapsed_last = 0;
#endif

    // create unix socket
    if ((us_listener = us_listen (the_server.us_srv)) < 0) {
        ULOG_ERR ("Unable to listen on Unix socket (%s)\n",
                srvcfg.unixsocket);
        goto error;
    }
    ULOG_NOTE ("Listening on Unix Socket (%s)...\n", srvcfg.unixsocket);

    the_server.us_srv->on_accepted = on_accepted;
    the_server.us_srv->on_packet = on_packet;
    the_server.us_srv->on_pending = on_pending;
    the_server.us_srv->on_close = on_close;
    the_server.us_srv->on_error = on_error;

    // create web socket listener if enabled
    if (the_server.ws_srv) {
#if HAVE(LIBSSL)
        if (srvcfg.sslcert && srvcfg.sslkey) {
            ULOG_NOTE ("==Using TLS/SSL==\n");
            srvcfg.use_ssl = 1;
            if (ws_initialize_ssl_ctx (the_server.ws_srv)) {
                ULOG_ERR ("Unable to initialize_ssl_ctx\n");
                goto error;
            }
        }
#else
        srvcfg.sslcert = srvcfg.sslkey = NULL;
#endif

        if ((ws_listener = ws_listen (the_server.ws_srv)) < 0) {
            ULOG_ERR ("Unable to listen on Web socket (%s, %s)\n",
                    srvcfg.addr, srvcfg.port);
            goto error;
        }

        the_server.ws_srv->on_accepted = on_accepted;
        the_server.ws_srv->on_packet = on_packet;
        the_server.ws_srv->on_pending = on_pending;
        the_server.ws_srv->on_close = on_close;
        the_server.ws_srv->on_error = on_error;
    }
    ULOG_NOTE ("Listening on Web Socket (%s, %s) %s SSL...\n",
            srvcfg.addr, srvcfg.port, srvcfg.sslcert ? "with" : "without");

#if HAVE(SYS_EPOLL_H)
    the_server.epollfd = epoll_create1 (EPOLL_CLOEXEC);
    if (the_server.epollfd == -1) {
        ULOG_ERR ("Failed to call epoll_create1: %s\n", strerror (errno));
        goto error;
    }

    ev.events = EPOLLIN;
    ev.data.ptr = PTR_FOR_US_LISTENER;
    if (epoll_ctl (the_server.epollfd, EPOLL_CTL_ADD, us_listener, &ev) == -1) {
        ULOG_ERR ("Failed to call epoll_ctl with us_listener (%d): %s\n",
                us_listener, strerror (errno));
        goto error;
    }

    if (ws_listener >= 0) {
        ev.events = EPOLLIN;
        ev.data.ptr = PTR_FOR_WS_LISTENER;
        if (epoll_ctl (the_server.epollfd, EPOLL_CTL_ADD, ws_listener, &ev) == -1) {
            ULOG_ERR ("Failed to call epoll_ctl with ws_listener (%d): %s\n",
                    ws_listener, strerror (errno));
            goto error;
        }
    }
#elif HAVE(SYS_SELECT_H)
    // TODO
#endif

error:
    return;
}

#if HAVE(SYS_EPOLL_H)
static void check_server_on_idle (void *data)
{
    int nfds, n;
    struct epoll_event ev, events[MAX_EVENTS];

    (void)data;

    nfds = epoll_wait (the_server.epollfd, events, MAX_EVENTS, 500);
    if (nfds < 0) {
        if (errno == EINTR) {
            continue;
        }

        ULOG_ERR ("Failed to call epoll_wait: %s\n", strerror (errno));
        goto error;
    }
    else if (nfds == 0) {
        t_elapsed = server_get_monotoic_time () - t_start;
        if (t_elapsed != t_elapsed_last) {
            if (t_elapsed % 10 == 0) {
                check_no_responding_endpoints (&the_server);
            }
            else if (t_elapsed % 5 == 0) {
                check_dangling_endpoints (&the_server);
            }

            t_elapsed_last = t_elapsed;
        }
    }

    for (n = 0; n < nfds; ++n) {
        if (events[n].data.ptr == PTR_FOR_US_LISTENER) {
            USClient * client = us_handle_accept (the_server.us_srv);
            if (client == NULL) {
                ULOG_NOTE ("Refused a client\n");
            }
            else {
                ev.events = EPOLLIN; /* do not use EPOLLET */
                ev.data.ptr = client;
                if (epoll_ctl (the_server.epollfd,
                            EPOLL_CTL_ADD, client->fd, &ev) == -1) {
                    ULOG_ERR ("Failed epoll_ctl for connected unix socket (%d): %s\n",
                            client->fd, strerror (errno));
                    goto error;
                }
            }
        }
        else if (events[n].data.ptr == PTR_FOR_WS_LISTENER) {
            WSClient * client = ws_handle_accept (the_server.ws_srv, ws_listener);
            if (client == NULL) {
                ULOG_NOTE ("Refused a client\n");
            }
            else {
                ev.events = EPOLLIN; /* do not use EPOLLET */
                ev.data.ptr = client;
                if (epoll_ctl(the_server.epollfd,
                            EPOLL_CTL_ADD, client->fd, &ev) == -1) {
                    ULOG_ERR ("Failed epoll_ctl for connected web socket (%d): %s\n",
                            client->fd, strerror (errno));
                    goto error;
                }
            }
        }
        else {
            USClient *usc = (USClient *)events[n].data.ptr;
            if (usc->ct == CT_UNIX_SOCKET) {

                if (events[n].events & EPOLLIN) {

                    if (usc->entity) {
                        Endpoint *endpoint = container_of (usc->entity,
                                Endpoint, entity);
                        update_endpoint_living_time (&the_server, endpoint);
                    }

                    us_handle_reads (the_server.us_srv, usc);
                }

                if (events[n].events & EPOLLOUT) {
                    us_handle_writes (the_server.us_srv, usc);

                    if (!(usc->status & US_SENDING) && !(usc->status & US_CLOSE)) {
                        ev.events = EPOLLIN;
                        ev.data.ptr = usc;
                        if (epoll_ctl (the_server.epollfd,
                                    EPOLL_CTL_MOD, usc->fd, &ev) == -1) {
                            ULOG_ERR ("Failed epoll_ctl for unix socket (%d): %s\n",
                                    usc->fd, strerror (errno));
                            goto error;
                        }
                    }
                }
            }
            else if (usc->ct == CT_WEB_SOCKET) {
                WSClient *wsc = (WSClient *)events[n].data.ptr;

                if (events[n].events & EPOLLIN) {
                    if (wsc->entity) {
                        Endpoint *endpoint = container_of (usc->entity,
                                Endpoint, entity);
                        update_endpoint_living_time (&the_server, endpoint);
                    }

                    ws_handle_reads (the_server.ws_srv, wsc);
                }

                if (events[n].events & EPOLLOUT) {
                    ws_handle_writes (the_server.ws_srv, wsc);

                    if (!(wsc->status & WS_SENDING) && !(wsc->status & WS_CLOSE)) {
                        ev.events = EPOLLIN;
                        ev.data.ptr = wsc;
                        if (epoll_ctl (the_server.epollfd,
                                    EPOLL_CTL_MOD, wsc->fd, &ev) == -1) {
                            ULOG_ERR ("Failed epoll_ctl for web socket (%d): %s\n",
                                    usc->fd, strerror (errno));
                            goto error;
                        }
                    }
                }
            }
            else {
                ULOG_ERR ("Bad socket type (%d): %s\n",
                        usc->ct, strerror (errno));
                goto error;
            }
        }
    }
}

#elif HAVE(SYS_SELECT_H)

static void check_server_on_idle (void *data)
{
    // TODO
    (void)data;
    update_endpoint_living_time (&the_server, NULL);
}

#endif /* HAVE(SYS_SELECT_H) */

static int
comp_living_time (const void *k1, const void *k2, void *ptr)
{
    const Endpoint *e1 = k1;
    const Endpoint *e2 = k2;

    (void)ptr;
    return e1->t_living - e2->t_living;
}

static int
init_server (void)
{
    the_server.nr_endpoints = 0;
    the_server.running = true;

    /* TODO for host name */
    the_server.server_name = strdup (SERVER_LOCALHOST);
    kvlist_init (&the_server.endpoint_list, NULL);
    avl_init (&the_server.living_avl, comp_living_time, true, NULL);

    return 0;
}

static void
deinit_server (void)
{
    const char* name;
    void *next, *data;
    Endpoint *endpoint, *tmp;

    avl_remove_all_elements (&the_server.living_avl, endpoint, avl, tmp) {
        if (endpoint->type == ET_UNIX_SOCKET) {
            us_close_client (the_server.us_srv, (USClient *)endpoint->entity.client);
        }
        else if (endpoint->type == ET_WEB_SOCKET) {
            ws_close_client (the_server.ws_srv, (WSClient *)endpoint->entity.client);
        }
    }

    kvlist_for_each_safe (&the_server.endpoint_list, name, next, data) {
        //memcpy (&endpoint, data, sizeof (Endpoint*));
        endpoint = *(Endpoint **)data;

        if (endpoint->type != ET_BUILTIN) {
            ULOG_INFO ("Deleting endpoint: %s (%p) in deinit_server\n", name, endpoint);

            if (endpoint->type == ET_UNIX_SOCKET && endpoint->entity.client) {
                // avoid a duplicated call of del_endpoint
                endpoint->entity.client->entity = NULL;
                us_cleanup_client (the_server.us_srv, (USClient *)endpoint->entity.client);
            }
            else if (endpoint->type == ET_WEB_SOCKET && endpoint->entity.client) {
                // avoid a duplicated call of del_endpoint
                endpoint->entity.client->entity = NULL;
                ws_cleanup_client (the_server.ws_srv, (WSClient *)endpoint->entity.client);
            }

            del_endpoint (&the_server, endpoint, CDE_EXITING);
            kvlist_delete (&the_server.endpoint_list, name);
            the_server.nr_endpoints--;
        }
    }

    kvlist_free (&the_server.endpoint_list);

    if (the_server.dangling_endpoints) {
        gs_list* node = the_server.dangling_endpoints;

        while (node) {
            endpoint = (Endpoint *)node->data;
            ULOG_WARN ("Removing dangling endpoint: %p, type (%d), status (%d)\n",
                    endpoint, endpoint->type, endpoint->status);

            if (endpoint->type == ET_UNIX_SOCKET) {
                USClient* usc = (USClient *)endpoint->entity.client;
                us_remove_dangling_client (the_server.us_srv, usc);
            }
            else if (endpoint->type == ET_WEB_SOCKET) {
                WSClient* wsc = (WSClient *)endpoint->entity.client;
                ws_remove_dangling_client (the_server.ws_srv, wsc);
            }
            else {
                ULOG_WARN ("Bad type of dangling endpoint\n");
            }

            del_endpoint (&the_server, endpoint, CDE_EXITING);

            node = node->next;
        }

        gslist_remove_nodes (the_server.dangling_endpoints);
    }

    us_stop (the_server.us_srv);
    if (the_server.ws_srv)
        ws_stop (the_server.ws_srv);

    free (the_server.server_name);

    ULOG_INFO ("the_server.nr_endpoints: %d\n", the_server.nr_endpoints);
    assert (the_server.nr_endpoints == 0);
}

int
purcmc_init_rdr_server (void)
{
    int retval;

    srandom (time (NULL));

    if ((retval = init_server ())) {
        ULOG_ERR ("Error during init_server: %s\n",
                server_get_ret_message (retval));
        goto error;
    }

    if ((the_server.us_srv = us_init ((ServerConfig *)&srvcfg)) == NULL) {
        ULOG_ERR ("Error during us_init\n");
        goto error;
    }

    if (!srvcfg.nowebsocket) {
        if ((the_server.ws_srv = ws_init ((ServerConfig *)&srvcfg)) == NULL) {
            ULOG_ERR ("Error during ws_init\n");
            goto error;
        }
    }
    else {
        the_server.ws_srv = NULL;
        ULOG_NOTE ("Skip web socket");
    }

    prepare_server ();
    add_hook (&idle_hook, check_server_on_idle, &the_server);

    return 0;

error:
    return 255;
}

int
purcmc_deinit_rdr_server (void)
{
    deinit_server ();
    return 0;
}

