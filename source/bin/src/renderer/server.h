/*
** server.h -- The internal interface for renderer server.
**
** Copyright (c) 2020 ~ 2021 FMSoft (http://www.fmsoft.cn)
**
** Author: Vincent Wei (https://github.com/VincentWei)
**
** This file is part of PurC Midnight Commander (PurcMC).
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

#ifndef MC_RENDERER_SERVER_H_
#define MC_RENDERER_SERVER_H_

#include <config.h>
#include <time.h>

#include <unistd.h>
#if HAVE(SYS_EPOLL_H)
#include <sys/epoll.h>
#elif HAVE(SYS_SELECT_H)
#include <sys/select.h>
#else
#error no `epoll` either `select` found.
#endif

#include "lib/list.h"
#include "lib/kvlist.h"
#include "lib/gslist.h"
#include "lib/sorted-array.h"

/* Constants */
#define SERVER_PROTOCOL_NAME             "PURCRDR"
#define SERVER_PROTOCOL_VERSION          100
#define SERVER_MINIMAL_PROTOCOL_VERSION  100

#define SERVER_US_PATH                   "/var/tmp/purcrdr.sock"
#define SERVER_WS_PORT                   "7702"
#define SERVER_WS_PORT_RESERVED          "7703"

#define SERVER_LOCALHOST                 "localhost"

/* Status Codes */
#define SERVER_SC_IOERR                  1
#define SERVER_SC_OK                     200
#define SERVER_SC_CREATED                201
#define SERVER_SC_ACCEPTED               202
#define SERVER_SC_NO_CONTENT             204
#define SERVER_SC_RESET_CONTENT          205
#define SERVER_SC_PARTIAL_CONTENT        206
#define SERVER_SC_BAD_REQUEST            400
#define SERVER_SC_UNAUTHORIZED           401
#define SERVER_SC_FORBIDDEN              403
#define SERVER_SC_NOT_FOUND              404
#define SERVER_SC_METHOD_NOT_ALLOWED     405
#define SERVER_SC_NOT_ACCEPTABLE         406
#define SERVER_SC_CONFLICT               409
#define SERVER_SC_GONE                   410
#define SERVER_SC_PRECONDITION_FAILED    412
#define SERVER_SC_PACKET_TOO_LARGE       413
#define SERVER_SC_EXPECTATION_FAILED     417
#define SERVER_SC_IM_A_TEAPOT            418
#define SERVER_SC_UNPROCESSABLE_PACKET   422
#define SERVER_SC_LOCKED                 423
#define SERVER_SC_FAILED_DEPENDENCY      424
#define SERVER_SC_TOO_EARLY              425
#define SERVER_SC_UPGRADE_REQUIRED       426
#define SERVER_SC_RETRY_WITH             449
#define SERVER_SC_UNAVAILABLE_FOR_LEGAL_REASONS             451
#define SERVER_SC_INTERNAL_SERVER_ERROR  500
#define SERVER_SC_NOT_IMPLEMENTED        501
#define SERVER_SC_BAD_CALLEE             502
#define SERVER_SC_SERVICE_UNAVAILABLE    503
#define SERVER_SC_CALLEE_TIMEOUT         504
#define SERVER_SC_INSUFFICIENT_STORAGE   507

#define SERVER_EC_IO                     (-1)
#define SERVER_EC_CLOSED                 (-2)
#define SERVER_EC_NOMEM                  (-3)
#define SERVER_EC_TOO_LARGE              (-4)
#define SERVER_EC_PROTOCOL               (-5)
#define SERVER_EC_UPPER                  (-6)
#define SERVER_EC_NOT_IMPLEMENTED        (-7)
#define SERVER_EC_INVALID_VALUE          (-8)
#define SERVER_EC_DUPLICATED             (-9)
#define SERVER_EC_TOO_SMALL_BUFF         (-10)
#define SERVER_EC_BAD_SYSTEM_CALL        (-11)
#define SERVER_EC_AUTH_FAILED            (-12)
#define SERVER_EC_SERVER_ERROR           (-13)
#define SERVER_EC_TIMEOUT                (-14)
#define SERVER_EC_UNKNOWN_EVENT          (-15)
#define SERVER_EC_UNKNOWN_RESULT         (-16)
#define SERVER_EC_UNKNOWN_METHOD         (-17)
#define SERVER_EC_UNEXPECTED             (-18)
#define SERVER_EC_SERVER_REFUSED         (-19)
#define SERVER_EC_BAD_PACKET             (-20)
#define SERVER_EC_BAD_CONNECTION         (-21)
#define SERVER_EC_CANT_LOAD              (-22)
#define SERVER_EC_BAD_KEY                (-23)

#define SERVER_LEN_HOST_NAME             127
#define SERVER_LEN_APP_NAME              127
#define SERVER_LEN_RUNNER_NAME           63
#define SERVER_LEN_ENDPOINT_NAME         \
    (SERVER_LEN_HOST_NAME + SERVER_LEN_APP_NAME + SERVER_LEN_RUNNER_NAME + 3)
#define SERVER_LEN_UNIQUE_ID             63

#define SERVER_MIN_PACKET_BUFF_SIZE      512
#define SERVER_DEF_PACKET_BUFF_SIZE      1024
#define SERVER_DEF_TIME_EXPECTED         5   /* 5 seconds */

/* the maximal size of a payload in a frame (4KiB) */
#define SERVER_MAX_FRAME_PAYLOAD_SIZE    4096

/* the maximal size of a payload which will be held in memory (40KiB) */
#define SERVER_MAX_INMEM_PAYLOAD_SIZE    40960

/* the maximal time to ping client (60 seconds) */
#define SERVER_MAX_PING_TIME             60

/* the maximal no responding time (90 seconds) */
#define SERVER_MAX_NO_RESPONDING_TIME    90

/* max clients for each web socket and unix socket */
#define MAX_CLIENTS_EACH    512

/* 1 MiB throttle threshold per client */
#define SOCK_THROTTLE_THLD  (1024 * 1024)

/* Connection types */
enum {
    CT_UNIX_SOCKET = 1,
    CT_WEB_SOCKET,
};

/* The frame operation codes for UnixSocket */
typedef enum USOpcode_ {
    US_OPCODE_CONTINUATION = 0x00,
    US_OPCODE_TEXT = 0x01,
    US_OPCODE_BIN = 0x02,
    US_OPCODE_END = 0x03,
    US_OPCODE_CLOSE = 0x08,
    US_OPCODE_PING = 0x09,
    US_OPCODE_PONG = 0x0A,
} USOpcode;

/* The frame header for UnixSocket */
typedef struct USFrameHeader_ {
    int op;
    unsigned int fragmented;
    unsigned int sz_payload;
    unsigned char payload[0];
} USFrameHeader;

/* packet body types */
enum {
    PT_TEXT = 0,
    PT_BINARY,
};

/* Endpoint types */
enum {
    ET_BUILTIN = 0,
    ET_UNIX_SOCKET,
    ET_WEB_SOCKET,
};

/* Endpoint status */
enum {
    ES_AUTHING = 0,     // authenticating
    ES_CLOSING,         // force to close the endpoint due to the failed authentication,
                        // RPC timeout, or ping-pong timeout.
    ES_READY,           // the endpoint is ready.
    ES_BUSY,            // the endpoint is busy for a call to procedure.
};

struct SockClient_;

/* A upper entity */
typedef struct UpperEntity_ {
    /* the size of memory used by the socket layer */
    size_t                  sz_sock_mem;

    /* the peak size of memory used by the socket layer */
    size_t                  peak_sz_sock_mem;

    /* the pointer to the socket client */
    struct SockClient_     *client;
} UpperEntity;

static inline void update_upper_entity_stats (UpperEntity *entity,
        size_t sz_pending_data, size_t sz_reading_data)
{
    if (entity) {
        entity->sz_sock_mem = sz_pending_data + sz_reading_data;
        if (entity->sz_sock_mem > entity->peak_sz_sock_mem)
            entity->peak_sz_sock_mem = entity->sz_sock_mem;
    }
}

/* A socket client */
typedef struct SockClient_ {
    /* the connection type of the socket */
    int                     ct;

    /* the file descriptor of the socket */
    int                     fd;

    /* time got the first frame of the current reading packet/message */
    struct timespec         ts;

    /* the pointer to the upper entity */
    struct UpperEntity_    *entity;
} SockClient;

/* A PurcMC Endpoint */
typedef struct Endpoint_
{
    int             type;
    unsigned int    status;
    UpperEntity     entity;

    time_t  t_created;
    time_t  t_living;

    char*   host_name;
    char*   app_name;
    char*   runner_name;

    /* AVL node for the AVL tree sorted by living time */
    struct avl_node avl;
} Endpoint;

struct WSServer_;
struct USServer_;

/* The PurcMC Server */
typedef struct Server_
{
    int us_listener;
    int ws_listener;
#if HAVE(SYS_EPOLL_H)
    int epollfd;
#elif HAVE(SYS_SELECT_H)
    int maxfd;
    fd_set rfdset, wfdset;
    /* the AVL tree for the map from fd to client */
    struct sorted_array *fd2clients;
#endif
    unsigned int nr_endpoints;
    bool running;

    time_t t_start;
    time_t t_elapsed;
    time_t t_elapsed_last;

    char* server_name;

    struct WSServer_ *ws_srv;
    struct USServer_ *us_srv;

    /* The KV list using endpoint name as the key, and Endpoint* as the value */
    struct kvlist endpoint_list;

    /* The accepted endpoints but waiting for authentification */
    gs_list *dangling_endpoints;

    /* the AVL tree of endpoints sorted by living time */
    struct avl_tree living_avl;
} Server;

/* Config Options */
typedef struct ServerConfig_
{
    int nowebsocket;
    int accesslog;
    int use_ssl;
    char *unixsocket;
    char *origin;
    char *addr;
    char *port;
    char *sslcert;
    char *sslkey;
    int max_frm_size;
    int backlog;
} ServerConfig;

const char* server_get_ret_message (int err_code);

static inline time_t server_get_monotoic_time (void)
{
    struct timespec tp;

    clock_gettime(CLOCK_MONOTONIC, &tp);
    return tp.tv_sec;
}

const char *server_get_ret_message(int ret_code);
const char *server_get_err_message(int err_code);
int server_errcode_to_retcode(int err_code);
bool server_is_valid_token(const char *token, int max_len);
bool server_is_valid_host_name(const char *host_name);
bool server_is_valid_app_name(const char *app_name);
bool server_is_valid_endpoint_name(const char *endpoint_name);
int server_extract_host_name(const char *endpoint, char *buff);
int server_extract_app_name(const char *endpoint, char *buff);
int server_extract_runner_name(const char *endpoint, char *buff);
char *server_extract_host_name_alloc(const char *endpoint);
char *server_extract_app_name_alloc(const char *endpoint);
char *server_extract_runner_name_alloc(const char *endpoint);
int server_assemble_endpoint_name(const char *host_name, const char *app_name,
        const char *runner_name, char *buff);
char *server_assemble_endpoint_name_alloc(const char *host_name,
        const char *app_name, const char *runner_name);
bool server_is_valid_app_name(const char *app_name);

static inline bool
server_is_valid_runner_name(const char *runner_name)
{
    return server_is_valid_token(runner_name, SERVER_LEN_RUNNER_NAME);
}

#endif /* !MC_RENDERER_SERVER_H_*/

