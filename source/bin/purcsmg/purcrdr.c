/*
** purcrdr.c -- The implementation of API for the simple markup generator of
**      PurC Renderer.
**
** Copyright (c) 2021 FMSoft (http://www.fmsoft.cn)
**
** Author: Vincent Wei (https://github.com/VincentWei)
**
** This file is part of PurC Midnight Commander.
**
** PurCRDR is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** PurCRDR is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see http://www.gnu.org/licenses/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/un.h>
#include <sys/time.h>

#include "lib/utils.h"
#include "lib/ulog.h"
#include "hibox/md5.h"
#include "hibox/kvlist.h"

#include "purcrdr.h"

struct _pcrdr_conn {
    int type;
    int fd;
    int last_ret_code;
    int padding_;

    char* srv_host_name;
    char* own_host_name;
    char* app_name;
    char* runner_name;

    struct kvlist method_list;
    struct kvlist bubble_list;
    struct kvlist call_list;
    struct kvlist subscribed_list;

    pcrdr_error_handler error_handler;
    void *user_data;
};

typedef enum  {
    MHT_STRING  = 0,
    MHT_CONST_STRING = 1,
} method_handler_type;

struct method_handler_info {
    method_handler_type type;
    void* handler;
};

static int mhi_get_len (struct kvlist *kv, const void *data)
{
    return sizeof (struct method_handler_info);
}

pcrdr_error_handler pcrdr_conn_get_error_handler (pcrdr_conn *conn)
{
    return conn->error_handler;
}

pcrdr_error_handler pcrdr_conn_set_error_handler (pcrdr_conn *conn,
        pcrdr_error_handler error_handler)
{
    pcrdr_error_handler old = conn->error_handler;
    conn->error_handler = error_handler;

    return old;
}

void *pcrdr_conn_get_user_data (pcrdr_conn *conn)
{
    return conn->user_data;
}

void *pcrdr_conn_set_user_data (pcrdr_conn *conn, void *user_data)
{
    void *old = conn->user_data;
    conn->user_data = user_data;

    return old;
}

int pcrdr_conn_get_last_ret_code (pcrdr_conn *conn)
{
    return conn->last_ret_code;
}

int pcrdr_conn_endpoint_name (pcrdr_conn* conn, char *buff)
{
    if (conn->own_host_name && conn->app_name && conn->runner_name) {
        return pcrdr_assemble_endpoint_name (conn->own_host_name,
                conn->app_name, conn->runner_name, buff);
    }

    return 0;
}

char *pcrdr_conn_endpoint_name_alloc (pcrdr_conn* conn)
{
    if (conn->own_host_name && conn->app_name && conn->runner_name) {
        return pcrdr_assemble_endpoint_name_alloc (conn->own_host_name,
                conn->app_name, conn->runner_name);
    }

    return NULL;
}

/* return NULL for error */
static char* read_text_payload_from_us (int fd, int* len)
{
    ssize_t n = 0;
    USFrameHeader header;
    char *payload = NULL;

    n = read (fd, &header, sizeof (USFrameHeader));
    if (n > 0) {
        if (header.op == US_OPCODE_TEXT &&
                header.sz_payload > 0) {
            payload = malloc (header.sz_payload + 1);
        }
        else {
            ULOG_WARN ("Bad payload type (%d) and length (%d)\n",
                    header.op, header.sz_payload);
            return NULL;  /* must not the challenge code */
        }
    }

    if (payload == NULL) {
        ULOG_ERR ("Failed to allocate memory for payload.\n");
        return NULL;
    }
    else {
        n = read (fd, payload, header.sz_payload);
        if (n < header.sz_payload) {
            ULOG_ERR ("Failed to read payload.\n");
            goto failed;
        }

        payload [header.sz_payload] = 0;
        if (len)
            *len = header.sz_payload;
    }

    return payload;

failed:
    free (payload);
    return NULL;
}

static int get_challenge_code (pcrdr_conn *conn, char **challenge)
{
    int err_code = 0;
    char* payload;
    int len;
    pcrdr_json *jo = NULL, *jo_tmp;
    const char *ch_code = NULL;

    // TODO: handle WebSocket connection
    payload = read_text_payload_from_us (conn->fd, &len);
    if (payload == NULL) {
        err_code = PURCRDR_EC_NOMEM;
        goto failed;
    }

    jo = pcrdr_json_object_from_string (payload, len, 2);
    if (jo == NULL) {
        err_code = PURCRDR_EC_BAD_PACKET;
        goto failed;
    }

    free (payload);
    payload = NULL;

    if (json_object_object_get_ex (jo, "packetType", &jo_tmp)) {
        const char *pack_type;
        pack_type = json_object_get_string (jo_tmp);

        if (strcasecmp (pack_type, "error") == 0) {
            const char* prot_name = PURCRDR_NOT_AVAILABLE;
            int prot_ver = 0, ret_code = 0;
            const char *ret_msg = PURCRDR_NOT_AVAILABLE, *extra_msg = PURCRDR_NOT_AVAILABLE;

            ULOG_WARN ("Refued by server:\n");
            if (json_object_object_get_ex (jo, "protocolName", &jo_tmp)) {
                prot_name = json_object_get_string (jo_tmp);
            }

            if (json_object_object_get_ex (jo, "protocolVersion", &jo_tmp)) {
                prot_ver = json_object_get_int (jo_tmp);
            }
            ULOG_WARN ("  Protocol: %s/%d\n", prot_name, prot_ver);

            if (json_object_object_get_ex (jo, "retCode", &jo_tmp)) {
                ret_code = json_object_get_int (jo_tmp);
            }
            if (json_object_object_get_ex (jo, "retMsg", &jo_tmp)) {
                ret_msg = json_object_get_string (jo_tmp);
            }
            if (json_object_object_get_ex (jo, "extraMsg", &jo_tmp)) {
                extra_msg = json_object_get_string (jo_tmp);
            }
            ULOG_WARN ("  Error Info: %d (%s): %s\n", ret_code, ret_msg, extra_msg);

            err_code = PURCRDR_EC_SERVER_REFUSED;
            goto failed;
        }
        else if (strcasecmp (pack_type, "auth") == 0) {
            const char *prot_name = PURCRDR_NOT_AVAILABLE;
            int prot_ver = 0;

            if (json_object_object_get_ex (jo, "challengeCode", &jo_tmp)) {
                ch_code = json_object_get_string (jo_tmp);
            }

            if (json_object_object_get_ex (jo, "protocolName", &jo_tmp)) {
                prot_name = json_object_get_string (jo_tmp);
            }
            if (json_object_object_get_ex (jo, "protocolVersion", &jo_tmp)) {
                prot_ver = json_object_get_int (jo_tmp);
            }

            if (ch_code == NULL) {
                ULOG_WARN ("Null challenge code\n");
                err_code = PURCRDR_EC_BAD_PACKET;
                goto failed;
            }
            else if (strcasecmp (prot_name, PURCRDR_PROTOCOL_NAME) ||
                    prot_ver < PURCRDR_PROTOCOL_VERSION) {
                ULOG_WARN ("Protocol not matched: %s/%d\n", prot_name, prot_ver);
                err_code = PURCRDR_EC_PROTOCOL;
                goto failed;
            }
        }
    }
    else {
        ULOG_WARN ("No packetType field\n");
        err_code = PURCRDR_EC_BAD_PACKET;
        goto failed;
    }

    assert (ch_code);
    *challenge = strdup (ch_code);
    if (*challenge == NULL)
        err_code = PURCRDR_EC_NOMEM;

failed:
    if (jo)
        json_object_put (jo);
    if (payload)
        free (payload);

    return err_code;
}

static int send_auth_info (pcrdr_conn *conn, const char* ch_code)
{
    int err_code = 0, n;
    unsigned char* sig;
    unsigned int sig_len;
    char* enc_sig = NULL;
    unsigned int enc_sig_len;
    char buff [PURCRDR_DEF_PACKET_BUFF_SIZE];

    err_code = pcrdr_sign_data (conn->app_name,
            (const unsigned char *)ch_code, strlen (ch_code),
            &sig, &sig_len);
    if (err_code) {
        return err_code;
    }

    enc_sig_len = B64_ENCODE_LEN (sig_len);
    enc_sig = malloc (enc_sig_len);
    if (enc_sig == NULL) {
        err_code = PURCRDR_EC_NOMEM;
        goto failed;
    }

    // When encode the signature in base64 or exadecimal notation,
    // there will be no any '"' and '\' charecters.
    b64_encode (sig, sig_len, enc_sig, enc_sig_len);

    free (sig);
    sig = NULL;

    n = snprintf (buff, sizeof (buff), 
            "{"
            "\"packetType\":\"auth\","
            "\"protocolName\":\"%s\","
            "\"protocolVersion\":%d,"
            "\"hostName\":\"%s\","
            "\"appName\":\"%s\","
            "\"runnerName\":\"%s\","
            "\"signature\":\"%s\","
            "\"encodedIn\":\"base64\""
            "}",
            PURCRDR_PROTOCOL_NAME, PURCRDR_PROTOCOL_VERSION,
            conn->own_host_name, conn->app_name, conn->runner_name, enc_sig);

    if (n < 0) {
        err_code = PURCRDR_EC_UNEXPECTED;
        goto failed;
    }
    else if ((size_t)n >= sizeof (buff)) {
        ULOG_ERR ("Too small buffer for signature (%s) in send_auth_info.\n", enc_sig);
        err_code = PURCRDR_EC_TOO_SMALL_BUFF;
        goto failed;
    }

    if (pcrdr_send_text_packet (conn, buff, n)) {
        ULOG_ERR ("Failed to send text packet to PurCRDR server in send_auth_info.\n");
        err_code = PURCRDR_EC_IO;
        goto failed;
    }

    free (enc_sig);
    return 0;

failed:
    if (sig)
        free (sig);
    if (enc_sig)
        free (enc_sig);
    return err_code;
}

static void on_lost_event_generator (pcrdr_conn* conn,
        const char* from_endpoint, const char* from_bubble,
        const char* bubble_data)
{
    pcrdr_json *jo = NULL, *jo_tmp;
    const char *endpoint_name = NULL;
    const char* event_name;
    void *next, *data;

    jo = pcrdr_json_object_from_string (bubble_data, strlen (bubble_data), 2);
    if (jo == NULL) {
        ULOG_ERR ("Failed to parse bubble data for bubble `LOSTEVENTGENERATOR`\n");
        return;
    }

    if (json_object_object_get_ex (jo, "endpointName", &jo_tmp) &&
            (endpoint_name = json_object_get_string (jo_tmp))) {
    }
    else {
        ULOG_ERR ("Fatal error: no endpointName field in the packet!\n");
        return;
    }

    kvlist_for_each_safe (&conn->subscribed_list, event_name, next, data) {
        const char* end_of_endpoint = strrchr (event_name, '/');

        if (strncasecmp (event_name, endpoint_name, end_of_endpoint - event_name) == 0) {
            ULOG_INFO ("Matched an event (%s) in subscribed events for %s\n",
                    event_name, endpoint_name);

            kvlist_delete (&conn->subscribed_list, event_name);
        }
    }
}

static void on_lost_event_bubble (pcrdr_conn* conn,
        const char* from_endpoint, const char* from_bubble,
        const char* bubble_data)
{
    int n;
    pcrdr_json *jo = NULL, *jo_tmp;
    const char *endpoint_name = NULL;
    const char *bubble_name = NULL;
    char event_name [PURCRDR_LEN_ENDPOINT_NAME + PURCRDR_LEN_BUBBLE_NAME + 2];

    jo = pcrdr_json_object_from_string (bubble_data, strlen (bubble_data), 2);
    if (jo == NULL) {
        ULOG_ERR ("Failed to parse bubble data for bubble `LOSTEVENTBUBBLE`\n");
        return;
    }

    if (json_object_object_get_ex (jo, "endpointName", &jo_tmp) &&
            (endpoint_name = json_object_get_string (jo_tmp))) {
    }
    else {
        ULOG_ERR ("Fatal error: no endpointName in the packet!\n");
        return;
    }

    if (json_object_object_get_ex (jo, "bubbleName", &jo_tmp) &&
            (bubble_name = json_object_get_string (jo_tmp))) {
    }
    else {
        ULOG_ERR ("Fatal error: no bubbleName in the packet!\n");
        return;
    }

    n = pcrdr_name_tolower_copy (endpoint_name, event_name, PURCRDR_LEN_ENDPOINT_NAME);
    event_name [n++] = '/';
    event_name [n] = '\0';
    pcrdr_name_toupper_copy (bubble_name, event_name + n, PURCRDR_LEN_BUBBLE_NAME);
    if (!kvlist_get (&conn->subscribed_list, event_name))
        return;

    kvlist_delete (&conn->subscribed_list, event_name);
}

/* add systen event handlers here */
static int on_auth_passed (pcrdr_conn* conn, const pcrdr_json *jo)
{
    int n;
    pcrdr_json *jo_tmp;
    char event_name [PURCRDR_LEN_ENDPOINT_NAME + PURCRDR_LEN_BUBBLE_NAME + 2];
    const char* srv_host_name;
    const char* own_host_name;
    pcrdr_event_handler event_handler;

    if (json_object_object_get_ex (jo, "serverHostName", &jo_tmp) &&
            (srv_host_name = json_object_get_string (jo_tmp))) {
        if (conn->srv_host_name)
            free (conn->srv_host_name);

        conn->srv_host_name = strdup (srv_host_name);
    }
    else {
        ULOG_ERR ("Fatal error: no serverHostName in authPassed packet!\n");
        return PURCRDR_EC_PROTOCOL;
    }

    if (json_object_object_get_ex (jo, "reassignedHostName", &jo_tmp) &&
            (own_host_name = json_object_get_string (jo_tmp))) {
        if (conn->own_host_name)
            free (conn->own_host_name);

        conn->own_host_name = strdup (own_host_name);
    }
    else {
        ULOG_ERR ("Fatal error: no reassignedHostName in authPassed packet!\n");
        return PURCRDR_EC_PROTOCOL;
    }

    n = pcrdr_assemble_endpoint_name (srv_host_name,
            PURCRDR_APP_HIBUS, PURCRDR_RUNNER_BUILITIN, event_name);
    event_name [n++] = '/';
    event_name [n] = '\0';
    strcat (event_name, "LOSTEVENTGENERATOR");

    event_handler = on_lost_event_generator;
    if (!kvlist_set (&conn->subscribed_list, event_name, &event_handler)) {
        ULOG_ERR ("Failed to register callback for system event `LOSTEVENTGENERATOR`!\n");
        return PURCRDR_EC_UNEXPECTED;
    }

    n = pcrdr_assemble_endpoint_name (srv_host_name,
            PURCRDR_APP_HIBUS, PURCRDR_RUNNER_BUILITIN, event_name);
    event_name [n++] = '/';
    event_name [n] = '\0';
    strcat (event_name, "LOSTEVENTBUBBLE");

    event_handler = on_lost_event_bubble;
    if (!kvlist_set (&conn->subscribed_list, event_name, &event_handler)) {
        ULOG_ERR ("Failed to register callback for system event `LOSTEVENTBUBBLE`!\n");
        return PURCRDR_EC_UNEXPECTED;
    }

    return 0;
}

static int check_auth_result (pcrdr_conn* conn)
{
    void *packet;
    unsigned int data_len;
    pcrdr_json *jo;
    int retval, err_code;

    err_code = pcrdr_read_packet_alloc (conn, &packet, &data_len);
    if (err_code) {
        ULOG_ERR ("Failed to read packet\n");
        return err_code;
    }

    if (data_len == 0) {
        ULOG_ERR ("Unexpected\n");
        return PURCRDR_EC_UNEXPECTED;
    }

    retval = pcrdr_json_packet_to_object (packet, data_len, &jo);
    free (packet);

    if (retval < 0) {
        ULOG_ERR ("Failed to parse JSON packet\n");
        err_code = PURCRDR_EC_BAD_PACKET;
    }
    else if (retval == JPT_AUTH_PASSED) {
        ULOG_WARN ("Passed the authentication\n");
        err_code = on_auth_passed (conn, jo);
        ULOG_ERR ("return value of on_auth_passed: %d\n", retval);
    }
    else if (retval == JPT_AUTH_FAILED) {
        ULOG_WARN ("Failed the authentication\n");
        err_code = PURCRDR_EC_AUTH_FAILED;
    }
    else if (retval == JPT_ERROR) {
        ULOG_WARN ("Got an error\n");
        err_code = PURCRDR_EC_SERVER_REFUSED;
    }
    else {
        ULOG_WARN ("Got an unexpected packet: %d\n", retval);
        err_code = PURCRDR_EC_UNEXPECTED;
    }

    json_object_put (jo);
    return err_code;
}

#define CLI_PATH    "/var/tmp/"
#define CLI_PERM    S_IRWXU

/* returns fd if all OK, -1 on error */
int pcrdr_connect_via_unix_socket (const char* path_to_socket,
        const char* app_name, const char* runner_name, pcrdr_conn** conn)
{
    int fd, len, err_code = PURCRDR_EC_BAD_CONNECTION;
    struct sockaddr_un unix_addr;
    char peer_name [33];
    char *ch_code = NULL;

    if ((*conn = calloc (1, sizeof (pcrdr_conn))) == NULL) {
        ULOG_ERR ("Failed to callocate space for connection: %s\n",
                strerror (errno));
        return PURCRDR_EC_NOMEM;
    }

    /* create a Unix domain stream socket */
    if ((fd = socket (AF_UNIX, SOCK_STREAM, 0)) < 0) {
        ULOG_ERR ("Failed to call `socket` in pcrdr_connect_via_unix_socket: %s\n",
                strerror (errno));
        return PURCRDR_EC_IO;
    }

    {
        md5_ctx_t ctx;
        unsigned char md5_digest[16];

        md5_begin (&ctx);
        md5_hash (app_name, strlen (app_name), &ctx);
        md5_hash ("/", 1, &ctx);
        md5_hash (runner_name, strlen (runner_name), &ctx);
        md5_end (md5_digest, &ctx);
        bin2hex (md5_digest, 16, peer_name);
    }

    /* fill socket address structure w/our address */
    memset (&unix_addr, 0, sizeof(unix_addr));
    unix_addr.sun_family = AF_UNIX;
    /* On Linux sun_path is 108 bytes in size */
    sprintf (unix_addr.sun_path, "%s%s-%05d", CLI_PATH, peer_name, getpid());
    len = sizeof (unix_addr.sun_family) + strlen (unix_addr.sun_path);

    unlink (unix_addr.sun_path);        /* in case it already exists */
    if (bind (fd, (struct sockaddr *) &unix_addr, len) < 0) {
        ULOG_ERR ("Failed to call `bind` in pcrdr_connect_via_unix_socket: %s\n",
                strerror (errno));
        goto error;
    }
    if (chmod (unix_addr.sun_path, CLI_PERM) < 0) {
        ULOG_ERR ("Failed to call `chmod` in pcrdr_connect_via_unix_socket: %s\n",
                strerror (errno));
        goto error;
    }

    /* fill socket address structure w/server's addr */
    memset (&unix_addr, 0, sizeof(unix_addr));
    unix_addr.sun_family = AF_UNIX;
    strcpy (unix_addr.sun_path, path_to_socket);
    len = sizeof (unix_addr.sun_family) + strlen (unix_addr.sun_path);

    if (connect (fd, (struct sockaddr *) &unix_addr, len) < 0) {
        ULOG_ERR ("Failed to call `connect` in pcrdr_connect_via_unix_socket: %s\n",
                strerror (errno));
        goto error;
    }

    (*conn)->type = CT_UNIX_SOCKET;
    (*conn)->fd = fd;
    (*conn)->srv_host_name = NULL;
    (*conn)->own_host_name = strdup (PURCRDR_LOCALHOST);
    (*conn)->app_name = strdup (app_name);
    (*conn)->runner_name = strdup (runner_name);

    kvlist_init (&(*conn)->method_list, mhi_get_len);
    kvlist_init (&(*conn)->bubble_list, NULL);
    kvlist_init (&(*conn)->call_list, NULL);
    kvlist_init (&(*conn)->subscribed_list, NULL);

    /* try to read challenge code */
    if ((err_code = get_challenge_code (*conn, &ch_code)))
        goto error;

    if ((err_code = send_auth_info (*conn, ch_code))) {
        goto error;
    }

    free (ch_code);
    ch_code = NULL;

    if ((err_code = check_auth_result (*conn))) {
        goto error;
    }

    return fd;

error:
    close (fd);

    if (ch_code)
        free (ch_code);
    if ((*conn)->own_host_name)
       free ((*conn)->own_host_name);
    if ((*conn)->app_name)
       free ((*conn)->app_name);
    if ((*conn)->runner_name)
       free ((*conn)->runner_name);
    free (*conn);
    *conn = NULL;

    return err_code;
}

int pcrdr_connect_via_web_socket (const char* host_name, int port,
        const char* app_name, const char* runner_name, pcrdr_conn** conn)
{
    return PURCRDR_EC_NOT_IMPLEMENTED;
}

const char* pcrdr_conn_srv_host_name (pcrdr_conn* conn)
{
    return conn->srv_host_name;
}

const char* pcrdr_conn_own_host_name (pcrdr_conn* conn)
{
    return conn->own_host_name;
}

const char* pcrdr_conn_app_name (pcrdr_conn* conn)
{
    return conn->app_name;
}

const char* pcrdr_conn_runner_name (pcrdr_conn* conn)
{
    return conn->runner_name;
}

int pcrdr_conn_socket_fd (pcrdr_conn* conn)
{
    return conn->fd;
}

int pcrdr_conn_socket_type (pcrdr_conn* conn)
{
    return conn->type;
}

static inline int conn_read (int fd, void *buff, ssize_t sz)
{
    if (read (fd, buff, sz) == sz) {
        return 0;
    }

    return PURCRDR_EC_IO;
}

static inline int conn_write (int fd, const void *data, ssize_t sz)
{
    if (write (fd, data, sz) == sz) {
        return 0;
    }

    return PURCRDR_EC_IO;
}

int pcrdr_free_connection (pcrdr_conn* conn)
{
    assert (conn);

    if (conn->srv_host_name)
        free (conn->srv_host_name);
    free (conn->own_host_name);
    free (conn->app_name);
    free (conn->runner_name);
    close (conn->fd);

    kvlist_free (&conn->method_list);
    kvlist_free (&conn->bubble_list);
    kvlist_free (&conn->call_list);
    kvlist_free (&conn->subscribed_list);

    free (conn);

    return 0;
}

int pcrdr_disconnect (pcrdr_conn* conn)
{
    int err_code = 0;

    if (conn->type == CT_UNIX_SOCKET) {
        USFrameHeader header;

        header.op = US_OPCODE_CLOSE;
        header.fragmented = 0;
        header.sz_payload = 0;
        if (conn_write (conn->fd, &header, sizeof (USFrameHeader))) {
            ULOG_ERR ("Error when wirting to Unix Socket: %s\n", strerror (errno));
            err_code = PURCRDR_EC_IO;
        }
    }
    else if (conn->type == CT_WEB_SOCKET) {
        /* TODO */
        err_code = PURCRDR_EC_NOT_IMPLEMENTED;
    }
    else {
        err_code = PURCRDR_EC_INVALID_VALUE;
    }

    pcrdr_free_connection (conn);

    return err_code;
}

int pcrdr_read_packet (pcrdr_conn* conn, void* packet_buf, unsigned int *packet_len)
{
    unsigned int offset;
    int err_code = 0;

    if (conn->type == CT_UNIX_SOCKET) {
        USFrameHeader header;

        if (conn_read (conn->fd, &header, sizeof (USFrameHeader))) {
            ULOG_ERR ("Failed to read frame header from Unix socket\n");
            err_code = PURCRDR_EC_IO;
            goto done;
        }

        if (header.op == US_OPCODE_PONG) {
            // TODO
            *packet_len = 0;
            return 0;
        }
        else if (header.op == US_OPCODE_PING) {
            header.op = US_OPCODE_PONG;
            header.sz_payload = 0;
            if (conn_write (conn->fd, &header, sizeof (USFrameHeader))) {
                err_code = PURCRDR_EC_IO;
                goto done;
            }
            *packet_len = 0;
            return 0;
        }
        else if (header.op == US_OPCODE_CLOSE) {
            ULOG_WARN ("Peer closed\n");
            err_code = PURCRDR_EC_CLOSED;
            goto done;
        }
        else if (header.op == US_OPCODE_TEXT ||
                header.op == US_OPCODE_BIN) {
            unsigned int left;

            if (header.fragmented > PURCRDR_MAX_INMEM_PAYLOAD_SIZE) {
                err_code = PURCRDR_EC_TOO_LARGE;
                goto done;
            }

            int is_text;
            if (header.op == US_OPCODE_TEXT) {
                is_text = 1;
            }
            else {
                is_text = 0;
            }

            if (conn_read (conn->fd, packet_buf, header.sz_payload)) {
                ULOG_ERR ("Failed to read packet from Unix socket\n");
                err_code = PURCRDR_EC_IO;
                goto done;
            }

            if (header.fragmented > header.sz_payload) {
                left = header.fragmented - header.sz_payload;
            }
            else
                left = 0;
            offset = header.sz_payload;
            while (left > 0) {
                if (conn_read (conn->fd, &header, sizeof (USFrameHeader))) {
                    ULOG_ERR ("Failed to read frame header from Unix socket\n");
                    err_code = PURCRDR_EC_IO;
                    goto done;
                }

                if (header.op != US_OPCODE_CONTINUATION &&
                        header.op != US_OPCODE_END) {
                    ULOG_ERR ("Not a continuation frame\n");
                    err_code = PURCRDR_EC_PROTOCOL;
                    goto done;
                }

                if (conn_read (conn->fd, packet_buf + offset, header.sz_payload)) {
                    ULOG_ERR ("Failed to read packet from Unix socket\n");
                    err_code = PURCRDR_EC_IO;
                    goto done;
                }

                offset += header.sz_payload;
                left -= header.sz_payload;

                if (header.op == US_OPCODE_END) {
                    break;
                }
            }

            if (is_text) {
                ((char *)packet_buf) [offset] = '\0';
                *packet_len = offset + 1;
            }
            else {
                *packet_len = offset;
            }
        }
        else {
            ULOG_ERR ("Bad packet op code: %d\n", header.op);
            err_code = PURCRDR_EC_PROTOCOL;
        }
    }
    else if (conn->type == CT_WEB_SOCKET) {
        /* TODO */
        err_code = PURCRDR_EC_NOT_IMPLEMENTED;
    }
    else {
        err_code = PURCRDR_EC_INVALID_VALUE;
    }

done:
    return err_code;
}

static inline void my_log (const char* str)
{
    ssize_t n = write (2, str, strlen (str));
    n = n & n;
}

int pcrdr_read_packet_alloc (pcrdr_conn* conn, void **packet, unsigned int *packet_len)
{
    void* packet_buf = NULL;
    int err_code = 0;

    if (conn->type == CT_UNIX_SOCKET) {
        USFrameHeader header;

        if (conn_read (conn->fd, &header, sizeof (USFrameHeader))) {
            ULOG_ERR ("Failed to read frame header from Unix socket\n");
            err_code = PURCRDR_EC_IO;
            goto done;
        }

        if (header.op == US_OPCODE_PONG) {
            // TODO
            *packet = NULL;
            *packet_len = 0;
            return 0;
        }
        else if (header.op == US_OPCODE_PING) {
            header.op = US_OPCODE_PONG;
            header.sz_payload = 0;
            if (conn_write (conn->fd, &header, sizeof (USFrameHeader))) {
                err_code = PURCRDR_EC_IO;
                goto done;
            }

            *packet = NULL;
            *packet_len = 0;
            return 0;
        }
        else if (header.op == US_OPCODE_CLOSE) {
            ULOG_WARN ("Peer closed\n");
            err_code = PURCRDR_EC_CLOSED;
            goto done;
        }
        else if (header.op == US_OPCODE_TEXT ||
                header.op == US_OPCODE_BIN) {
            unsigned int total_len, left;
            unsigned int offset;
            int is_text;

            if (header.fragmented > PURCRDR_MAX_INMEM_PAYLOAD_SIZE) {
                err_code = PURCRDR_EC_TOO_LARGE;
                goto done;
            }

            if (header.op == US_OPCODE_TEXT) {
                is_text = 1;
            }
            else {
                is_text = 0;
            }

            if (header.fragmented > header.sz_payload) {
                total_len = header.fragmented;
                offset = header.sz_payload;
                left = total_len - header.sz_payload;
            }
            else {
                total_len = header.sz_payload;
                offset = header.sz_payload;
                left = 0;
            }

            if ((packet_buf = malloc (total_len + 1)) == NULL) {
                err_code = PURCRDR_EC_NOMEM;
                goto done;
            }

            if (conn_read (conn->fd, packet_buf, header.sz_payload)) {
                ULOG_ERR ("Failed to read packet from Unix socket\n");
                err_code = PURCRDR_EC_IO;
                goto done;
            }

            while (left > 0) {
                if (conn_read (conn->fd, &header, sizeof (USFrameHeader))) {
                    ULOG_ERR ("Failed to read frame header from Unix socket\n");
                    err_code = PURCRDR_EC_IO;
                    goto done;
                }

                if (header.op != US_OPCODE_CONTINUATION &&
                        header.op != US_OPCODE_END) {
                    ULOG_ERR ("Not a continuation frame\n");
                    err_code = PURCRDR_EC_PROTOCOL;
                    goto done;
                }

                if (conn_read (conn->fd, packet_buf + offset, header.sz_payload)) {
                    ULOG_ERR ("Failed to read packet from Unix socket\n");
                    err_code = PURCRDR_EC_IO;
                    goto done;
                }

                left -= header.sz_payload;
                offset += header.sz_payload;
                if (header.op == US_OPCODE_END) {
                    break;
                }
            }

            if (is_text) {
                ((char *)packet_buf) [offset] = '\0';
                *packet_len = offset + 1;
            }
            else {
                *packet_len = offset;
            }

            goto done;
        }
        else {
            ULOG_ERR ("Bad packet op code: %d\n", header.op);
            err_code = PURCRDR_EC_PROTOCOL;
            goto done;
        }
    }
    else if (conn->type == CT_WEB_SOCKET) {
        /* TODO */
        err_code = PURCRDR_EC_NOT_IMPLEMENTED;
        goto done;
    }
    else {
        assert (0);
        err_code = PURCRDR_EC_INVALID_VALUE;
        goto done;
    }

done:
    if (err_code) {
        if (packet_buf)
            free (packet_buf);
        *packet = NULL;
        return err_code;
    }

    *packet = packet_buf;
    return 0;
}

int pcrdr_send_text_packet (pcrdr_conn* conn, const char* text, unsigned int len)
{
    int retv = 0;

    if (conn->type == CT_UNIX_SOCKET) {
        USFrameHeader header;

        if (len > PURCRDR_MAX_FRAME_PAYLOAD_SIZE) {
            unsigned int left = len;

            do {
                if (left == len) {
                    header.op = US_OPCODE_TEXT;
                    header.fragmented = len;
                    header.sz_payload = PURCRDR_MAX_FRAME_PAYLOAD_SIZE;
                    left -= PURCRDR_MAX_FRAME_PAYLOAD_SIZE;
                }
                else if (left > PURCRDR_MAX_FRAME_PAYLOAD_SIZE) {
                    header.op = US_OPCODE_CONTINUATION;
                    header.fragmented = 0;
                    header.sz_payload = PURCRDR_MAX_FRAME_PAYLOAD_SIZE;
                    left -= PURCRDR_MAX_FRAME_PAYLOAD_SIZE;
                }
                else {
                    header.op = US_OPCODE_END;
                    header.fragmented = 0;
                    header.sz_payload = left;
                    left = 0;
                }

                if (conn_write (conn->fd, &header, sizeof (USFrameHeader)) == 0) {
                    retv = conn_write (conn->fd, text, header.sz_payload);
                    text += header.sz_payload;
                }

            } while (left > 0 && retv == 0);
        }
        else {
            header.op = US_OPCODE_TEXT;
            header.fragmented = 0;
            header.sz_payload = len;
            if (conn_write (conn->fd, &header, sizeof (USFrameHeader)) == 0)
                retv = conn_write (conn->fd, text, len);
        }
    }
    else if (conn->type == CT_WEB_SOCKET) {
        /* TODO */
        retv = PURCRDR_EC_NOT_IMPLEMENTED;
    }
    else
        retv = PURCRDR_EC_INVALID_VALUE;

    return retv;
}

int pcrdr_ping_server (pcrdr_conn* conn)
{
    int err_code = 0;

    if (conn->type == CT_UNIX_SOCKET) {
        USFrameHeader header;

        header.op = US_OPCODE_PING;
        header.fragmented = 0;
        header.sz_payload = 0;
        if (conn_write (conn->fd, &header, sizeof (USFrameHeader))) {
            ULOG_ERR ("Error when wirting to Unix Socket: %s\n", strerror (errno));
            err_code = PURCRDR_EC_IO;
        }
    }
    else if (conn->type == CT_WEB_SOCKET) {
        /* TODO */
        err_code = PURCRDR_EC_NOT_IMPLEMENTED;
    }
    else {
        err_code = PURCRDR_EC_INVALID_VALUE;
    }

    return err_code;
}

static int wait_for_specific_call_result_packet (pcrdr_conn* conn, 
        const char* call_id, int time_expected, int *ret_code, char** ret_value);

int pcrdr_call_procedure_and_wait (pcrdr_conn* conn, const char* endpoint,
        const char* method_name, const char* method_param,
        int time_expected, int *ret_code, char** ret_value)
{
    int n;
    char call_id [PURCRDR_LEN_UNIQUE_ID + 1];
    char buff [PURCRDR_DEF_PACKET_BUFF_SIZE];
    char* escaped_param;

    if (!pcrdr_is_valid_method_name (method_name))
        return PURCRDR_EC_INVALID_VALUE;

    escaped_param = pcrdr_escape_string_for_json (method_param);
    if (escaped_param == NULL)
        return PURCRDR_EC_NOMEM;

    pcrdr_generate_unique_id (call_id, "call");

    n = snprintf (buff, sizeof (buff), 
            "{"
            "\"packetType\": \"call\","
            "\"callId\": \"%s\","
            "\"toEndpoint\": \"%s\","
            "\"toMethod\": \"%s\","
            "\"expectedTime\": %d,"
            "\"parameter\": \"%s\""
            "}",
            call_id,
            endpoint,
            method_name,
            time_expected,
            escaped_param);
    free (escaped_param);

    if (n < 0) {
        return PURCRDR_EC_UNEXPECTED;
    }
    else if ((size_t)n >= sizeof (buff)) {
        return PURCRDR_EC_TOO_SMALL_BUFF;
    }

    if (pcrdr_send_text_packet (conn, buff, n)) {
        return PURCRDR_EC_IO;
    }

    return wait_for_specific_call_result_packet (conn,
            call_id, time_expected, ret_code, ret_value);
}

static int my_register_procedure (pcrdr_conn* conn, const char* method_name,
        const char* for_host, const char* for_app,
        const struct method_handler_info* mhi)
{
    int n, err_code, ret_code;
    char endpoint_name [PURCRDR_LEN_ENDPOINT_NAME + 1];
    char normalized_method [PURCRDR_LEN_METHOD_NAME + 1];
    char param_buff [PURCRDR_MIN_PACKET_BUFF_SIZE];
    char* ret_value;

    if (!pcrdr_is_valid_method_name (method_name))
        return PURCRDR_EC_INVALID_VALUE;

    if (for_host == NULL) for_host = "*";
    if (!pcrdr_is_valid_wildcard_pattern_list (for_host)) {
        return PURCRDR_EC_INVALID_VALUE;
    }

    if (for_app == NULL) for_app = "*";
    if (!pcrdr_is_valid_wildcard_pattern_list (for_app)) {
        return PURCRDR_EC_INVALID_VALUE;
    }
    pcrdr_name_tolower_copy (method_name, normalized_method, PURCRDR_LEN_METHOD_NAME);

    if (kvlist_get (&conn->method_list, normalized_method))
        return PURCRDR_EC_DUPLICATED;

    n = snprintf (param_buff, sizeof (param_buff), 
            "{"
            "\"methodName\": \"%s\","
            "\"forHost\": \"%s\","
            "\"forApp\": \"%s\""
            "}",
            normalized_method,
            for_host, for_app);

    if (n < 0) {
        return PURCRDR_EC_UNEXPECTED;
    }
    else if ((size_t)n >= sizeof (param_buff))
        return PURCRDR_EC_TOO_SMALL_BUFF;

    pcrdr_assemble_endpoint_name (conn->srv_host_name,
            PURCRDR_APP_HIBUS, PURCRDR_RUNNER_BUILITIN, endpoint_name);

    if ((err_code = pcrdr_call_procedure_and_wait (conn, endpoint_name,
                    "registerProcedure", param_buff,
                    PURCRDR_DEF_TIME_EXPECTED, &ret_code, &ret_value))) {
        return err_code;
    }

    if (ret_code == PURCRDR_SC_OK) {
        kvlist_set (&conn->method_list, normalized_method, mhi);
        if (ret_value)
            free (ret_value);
    }

    return 0;
}

int pcrdr_register_procedure (pcrdr_conn* conn, const char* method_name,
        const char* for_host, const char* for_app,
        pcrdr_method_handler method_handler)
{
    struct method_handler_info mhi = { MHT_STRING, method_handler };

    return my_register_procedure (conn, method_name, for_host, for_app, &mhi);
}

int pcrdr_register_procedure_const (pcrdr_conn* conn, const char* method_name,
        const char* for_host, const char* for_app,
        pcrdr_method_handler_const method_handler)
{
    struct method_handler_info mhi = { MHT_CONST_STRING, method_handler };

    return my_register_procedure (conn, method_name, for_host, for_app, &mhi);
}


int pcrdr_revoke_procedure (pcrdr_conn* conn, const char* method_name)
{
    int n, err_code, ret_code;
    char endpoint_name [PURCRDR_LEN_ENDPOINT_NAME + 1];
    char normalized_method [PURCRDR_LEN_METHOD_NAME + 1];
    char param_buff [PURCRDR_MIN_PACKET_BUFF_SIZE];
    char* ret_value;

    if (!pcrdr_is_valid_method_name (method_name))
        return PURCRDR_EC_INVALID_VALUE;

    pcrdr_name_tolower_copy (method_name, normalized_method, PURCRDR_LEN_METHOD_NAME);

    if (!kvlist_get (&conn->method_list, normalized_method))
        return PURCRDR_EC_INVALID_VALUE;

    n = snprintf (param_buff, sizeof (param_buff), 
            "{"
            "\"methodName\": \"%s\""
            "}",
            normalized_method);

    if (n < 0) {
        return PURCRDR_EC_UNEXPECTED;
    }
    else if ((size_t)n >= sizeof (param_buff))
        return PURCRDR_EC_TOO_SMALL_BUFF;

    pcrdr_assemble_endpoint_name (conn->srv_host_name,
            PURCRDR_APP_HIBUS, PURCRDR_RUNNER_BUILITIN, endpoint_name);

    if ((err_code = pcrdr_call_procedure_and_wait (conn, endpoint_name,
                    "revokeProcedure", param_buff,
                    PURCRDR_DEF_TIME_EXPECTED, &ret_code, &ret_value))) {
        return err_code;
    }

    if (ret_code == PURCRDR_SC_OK) {
        kvlist_delete (&conn->method_list, normalized_method);

        if (ret_value)
            free (ret_value);
    }

    return 0;
}

int pcrdr_register_event (pcrdr_conn* conn, const char* bubble_name,
        const char* for_host, const char* for_app)
{
    int n, err_code, ret_code;
    char endpoint_name [PURCRDR_LEN_ENDPOINT_NAME + 1];
    char normalized_bubble [PURCRDR_LEN_BUBBLE_NAME + 1];
    char param_buff [PURCRDR_MIN_PACKET_BUFF_SIZE];
    char* ret_value;

    if (!pcrdr_is_valid_bubble_name (bubble_name))
        return PURCRDR_EC_INVALID_VALUE;

    if (for_host == NULL) for_host = "*";
    if (!pcrdr_is_valid_wildcard_pattern_list (for_host)) {
        return PURCRDR_EC_INVALID_VALUE;
    }

    if (for_app == NULL) for_app = "*";
    if (!pcrdr_is_valid_wildcard_pattern_list (for_app)) {
        return PURCRDR_EC_INVALID_VALUE;
    }

    pcrdr_name_toupper_copy (bubble_name, normalized_bubble, PURCRDR_LEN_BUBBLE_NAME);
    if (kvlist_get (&conn->bubble_list, normalized_bubble))
        return PURCRDR_EC_DUPLICATED;

    n = snprintf (param_buff, sizeof (param_buff), 
            "{"
            "\"bubbleName\": \"%s\","
            "\"forHost\": \"%s\","
            "\"forApp\": \"%s\""
            "}",
            normalized_bubble,
            for_host, for_app);

    if (n < 0) {
        return PURCRDR_EC_UNEXPECTED;
    }
    else if ((size_t)n >= sizeof (param_buff))
        return PURCRDR_EC_TOO_SMALL_BUFF;

    pcrdr_assemble_endpoint_name (conn->srv_host_name,
            PURCRDR_APP_HIBUS, PURCRDR_RUNNER_BUILITIN, endpoint_name);

    if ((err_code = pcrdr_call_procedure_and_wait (conn, endpoint_name,
                    "registerEvent", param_buff,
                    PURCRDR_DEF_TIME_EXPECTED, &ret_code, &ret_value))) {
        return err_code;
    }

    if (ret_code == PURCRDR_SC_OK) {
        kvlist_set (&conn->bubble_list, normalized_bubble, pcrdr_register_event);

        if (ret_value)
            free (ret_value);
    }
    else {
        err_code = PURCRDR_EC_SERVER_ERROR;
    }

    return err_code;
}

int pcrdr_revoke_event (pcrdr_conn* conn, const char* bubble_name)
{
    int n, err_code, ret_code;
    char endpoint_name [PURCRDR_LEN_ENDPOINT_NAME + 1];
    char normalized_bubble [PURCRDR_LEN_BUBBLE_NAME + 1];
    char param_buff [PURCRDR_MIN_PACKET_BUFF_SIZE];
    char* ret_value;

    if (!pcrdr_is_valid_bubble_name (bubble_name))
        return PURCRDR_EC_INVALID_VALUE;

    pcrdr_name_toupper_copy (bubble_name, normalized_bubble, PURCRDR_LEN_BUBBLE_NAME);
    if (!kvlist_get (&conn->bubble_list, normalized_bubble))
        return PURCRDR_EC_INVALID_VALUE;

    n = snprintf (param_buff, sizeof (param_buff), 
            "{"
            "\"bubbleName\": \"%s\""
            "}",
            normalized_bubble);

    if (n < 0) {
        return PURCRDR_EC_UNEXPECTED;
    }
    else if ((size_t)n >= sizeof (param_buff))
        return PURCRDR_EC_TOO_SMALL_BUFF;

    pcrdr_assemble_endpoint_name (conn->srv_host_name,
            PURCRDR_APP_HIBUS, PURCRDR_RUNNER_BUILITIN, endpoint_name);

    if ((err_code = pcrdr_call_procedure_and_wait (conn, endpoint_name,
                    "revokeEvent", param_buff,
                    PURCRDR_DEF_TIME_EXPECTED, &ret_code, &ret_value))) {
        return err_code;
    }

    if (ret_code == PURCRDR_SC_OK) {
        kvlist_delete (&conn->bubble_list, normalized_bubble);

        if (ret_value)
            free (ret_value);
    }
    else {
        err_code = PURCRDR_EC_SERVER_ERROR;
    }

    return 0;
}

int pcrdr_subscribe_event (pcrdr_conn* conn,
        const char* endpoint, const char* bubble_name,
        pcrdr_event_handler event_handler)
{
    int n, err_code, ret_code;
    char builtin_name [PURCRDR_LEN_ENDPOINT_NAME + 1];
    char param_buff [PURCRDR_MIN_PACKET_BUFF_SIZE];
    char event_name [PURCRDR_LEN_ENDPOINT_NAME + PURCRDR_LEN_BUBBLE_NAME + 2];
    char* ret_value;

    if (!pcrdr_is_valid_endpoint_name (endpoint))
        return PURCRDR_EC_INVALID_VALUE;

    if (!pcrdr_is_valid_bubble_name (bubble_name))
        return PURCRDR_EC_INVALID_VALUE;

    n = pcrdr_name_tolower_copy (endpoint, event_name, PURCRDR_LEN_ENDPOINT_NAME);
    event_name [n++] = '/';
    event_name [n] = '\0';
    pcrdr_name_toupper_copy (bubble_name, event_name + n, PURCRDR_LEN_BUBBLE_NAME);
    if (kvlist_get (&conn->subscribed_list, event_name))
        return PURCRDR_EC_INVALID_VALUE;

    n = snprintf (param_buff, sizeof (param_buff), 
            "{"
            "\"endpointName\": \"%s\","
            "\"bubbleName\": \"%s\""
            "}",
            endpoint,
            bubble_name);

    if (n < 0) {
        return PURCRDR_EC_UNEXPECTED;
    }
    else if ((size_t)n >= sizeof (param_buff))
        return PURCRDR_EC_TOO_SMALL_BUFF;

    pcrdr_assemble_endpoint_name (conn->srv_host_name,
            PURCRDR_APP_HIBUS, PURCRDR_RUNNER_BUILITIN, builtin_name);

    if ((err_code = pcrdr_call_procedure_and_wait (conn, builtin_name,
                    "subscribeEvent", param_buff,
                    PURCRDR_DEF_TIME_EXPECTED, &ret_code, &ret_value))) {
        return err_code;
    }

    if (ret_code == PURCRDR_SC_OK) {
        kvlist_set (&conn->subscribed_list, event_name, &event_handler);

        if (ret_value)
            free (ret_value);
    }

    return 0;
}

int pcrdr_unsubscribe_event (pcrdr_conn* conn,
        const char* endpoint, const char* bubble_name)
{
    int n, err_code, ret_code;
    char builtin_name [PURCRDR_LEN_ENDPOINT_NAME + 1];
    char param_buff [PURCRDR_MIN_PACKET_BUFF_SIZE];
    char event_name [PURCRDR_LEN_ENDPOINT_NAME + PURCRDR_LEN_BUBBLE_NAME + 2];
    char* ret_value;

    if (!pcrdr_is_valid_endpoint_name (endpoint))
        return PURCRDR_EC_INVALID_VALUE;

    if (!pcrdr_is_valid_bubble_name (bubble_name))
        return PURCRDR_EC_INVALID_VALUE;

    n = pcrdr_name_tolower_copy (endpoint, event_name, PURCRDR_LEN_ENDPOINT_NAME);
    event_name [n++] = '/';
    event_name [n] = '\0';
    pcrdr_name_toupper_copy (bubble_name, event_name + n, PURCRDR_LEN_BUBBLE_NAME);
    if (!kvlist_get (&conn->subscribed_list, event_name))
        return PURCRDR_EC_INVALID_VALUE;

    n = snprintf (param_buff, sizeof (param_buff), 
            "{"
            "\"endpointName\": \"%s\","
            "\"bubbleName\": \"%s\""
            "}",
            endpoint,
            bubble_name);

    if (n < 0) {
        return PURCRDR_EC_UNEXPECTED;
    }
    else if ((size_t)n >= sizeof (param_buff))
        return PURCRDR_EC_TOO_SMALL_BUFF;

    pcrdr_assemble_endpoint_name (conn->srv_host_name,
            PURCRDR_APP_HIBUS, PURCRDR_RUNNER_BUILITIN, builtin_name);

    if ((err_code = pcrdr_call_procedure_and_wait (conn, builtin_name,
                    "unsubscribeEvent", param_buff,
                    PURCRDR_DEF_TIME_EXPECTED, &ret_code, &ret_value))) {
        return err_code;
    }

    if (ret_code == PURCRDR_SC_OK) {
        kvlist_delete (&conn->subscribed_list, event_name);

        if (ret_value)
            free (ret_value);
    }

    return 0;
}

int pcrdr_call_procedure (pcrdr_conn* conn,
        const char* endpoint,
        const char* method_name, const char* method_param,
        int time_expected, pcrdr_result_handler result_handler,
        const char** call_id)
{
    int n, retv;
    char call_id_buf [PURCRDR_LEN_UNIQUE_ID + 1];
    char buff [PURCRDR_DEF_PACKET_BUFF_SIZE];
    char* escaped_param;

    if (!pcrdr_is_valid_endpoint_name (endpoint))
        return PURCRDR_EC_INVALID_VALUE;

    if (!pcrdr_is_valid_method_name (method_name))
        return PURCRDR_EC_INVALID_VALUE;

    escaped_param = pcrdr_escape_string_for_json (method_param);
    if (escaped_param == NULL)
        return PURCRDR_EC_NOMEM;

    pcrdr_generate_unique_id (call_id_buf, "call");

    n = snprintf (buff, sizeof (buff), 
            "{"
            "\"packetType\": \"call\","
            "\"callId\": \"%s\","
            "\"toEndpoint\": \"%s\","
            "\"toMethod\": \"%s\","
            "\"expectedTime\": %d,"
            "\"parameter\": \"%s\""
            "}",
            call_id_buf,
            endpoint,
            method_name,
            time_expected,
            escaped_param);
    free (escaped_param);

    if (n < 0) {
        return PURCRDR_EC_UNEXPECTED;
    }
    else if ((size_t)n >= sizeof (buff)) {
        return PURCRDR_EC_TOO_SMALL_BUFF;
    }

    if ((retv = pcrdr_send_text_packet (conn, buff, n)) == 0) {
        const char* p;
        p = kvlist_set_ex (&conn->call_list, call_id_buf, &result_handler);
        if (p) {
            if (call_id) {
                *call_id = p;
            }
        }
        else
            retv = PURCRDR_EC_NOMEM;
    }

    return retv;
}

int pcrdr_fire_event (pcrdr_conn* conn,
        const char* bubble_name, const char* bubble_data)
{
    int n;
    char normalized_bubble [PURCRDR_LEN_BUBBLE_NAME + 1];
    char event_id [PURCRDR_LEN_UNIQUE_ID + 1];

    char buff_in_stack [PURCRDR_DEF_PACKET_BUFF_SIZE];
    char* packet_buff = buff_in_stack;
    size_t len_data = bubble_data ? (strlen (bubble_data) * 2 + 1) : 0;
    size_t sz_packet_buff = sizeof (buff_in_stack);
    char* escaped_data;
    int err_code = 0;

    if (!pcrdr_is_valid_bubble_name (bubble_name)) {
        err_code = PURCRDR_EC_INVALID_VALUE;
        return PURCRDR_EC_INVALID_VALUE;
    }

    pcrdr_name_toupper_copy (bubble_name, normalized_bubble, PURCRDR_LEN_BUBBLE_NAME);
    if (!kvlist_get (&conn->bubble_list, normalized_bubble)) {
        err_code = PURCRDR_EC_INVALID_VALUE;
        return PURCRDR_EC_INVALID_VALUE;
    }

    if (len_data > PURCRDR_MIN_PACKET_BUFF_SIZE) {
        sz_packet_buff = PURCRDR_MIN_PACKET_BUFF_SIZE + len_data;
        packet_buff = malloc (PURCRDR_MIN_PACKET_BUFF_SIZE + len_data);
        if (packet_buff == NULL) {
            err_code = PURCRDR_EC_NOMEM;
            return PURCRDR_EC_NOMEM;
        }
    }

    if (bubble_data) {
        escaped_data = pcrdr_escape_string_for_json (bubble_data);
        if (escaped_data == NULL) {
            err_code = PURCRDR_EC_NOMEM;
            return PURCRDR_EC_NOMEM;
        }
    }
    else
        escaped_data = NULL;

    pcrdr_generate_unique_id (event_id, "event");
    n = snprintf (packet_buff, sz_packet_buff, 
            "{"
            "\"packetType\": \"event\","
            "\"eventId\": \"%s\","
            "\"bubbleName\": \"%s\","
            "\"bubbleData\": \"%s\""
            "}",
            event_id,
            normalized_bubble,
            escaped_data ? escaped_data : "");
    if (escaped_data)
        free (escaped_data);

    if (n < 0) {
        err_code = PURCRDR_EC_UNEXPECTED;
    }
    else if ((size_t)n >= sz_packet_buff) {
        err_code = PURCRDR_EC_TOO_SMALL_BUFF;
    }
    else
        err_code = pcrdr_send_text_packet (conn, packet_buff, n);

    if (packet_buff && packet_buff != buff_in_stack) {
        free (packet_buff);
    }

    return err_code;
}

static int dispatch_call_packet (pcrdr_conn* conn, const pcrdr_json *jo)
{
    pcrdr_json *jo_tmp;
    const char* from_endpoint = NULL, *call_id=NULL, *result_id = NULL;
    const char* to_method;
    const char* parameter;
    char normalized_name [PURCRDR_LEN_METHOD_NAME + 1];
    void *data;
    char *ret_value = NULL;
    int err_code = 0;
    char buff_in_stack [PURCRDR_DEF_PACKET_BUFF_SIZE];
    char* packet_buff = buff_in_stack;
    size_t sz_packet_buff = sizeof (buff_in_stack);
    char* escaped_value = NULL;
    int n = 0, ret_code = 0;
    double time_consumed = 0.0f;

    if (json_object_object_get_ex (jo, "fromEndpoint", &jo_tmp) &&
            (from_endpoint = json_object_get_string (jo_tmp))) {
    }
    else {
        err_code = PURCRDR_EC_PROTOCOL;
        goto done;
    }

    if (json_object_object_get_ex (jo, "toMethod", &jo_tmp) &&
            (to_method = json_object_get_string (jo_tmp))) {
    }
    else {
        err_code = PURCRDR_EC_PROTOCOL;
        goto done;
    }

    if (json_object_object_get_ex (jo, "callId", &jo_tmp) &&
            (call_id = json_object_get_string (jo_tmp))) {
    }
    else {
        err_code = PURCRDR_EC_PROTOCOL;
        goto done;
    }

    if (json_object_object_get_ex (jo, "resultId", &jo_tmp) &&
            (result_id = json_object_get_string (jo_tmp))) {
    }
    else {
        err_code = PURCRDR_EC_PROTOCOL;
        goto done;
    }

    if (json_object_object_get_ex (jo, "parameter", &jo_tmp) &&
            (parameter = json_object_get_string (jo_tmp))) {
    }
    else {
        parameter = "";
    }

    pcrdr_name_tolower_copy (to_method, normalized_name, PURCRDR_LEN_METHOD_NAME);
    if ((data = kvlist_get (&conn->method_list, normalized_name)) == NULL) {
        err_code = PURCRDR_EC_UNKNOWN_METHOD;
        goto done;
    }
    else {
        struct timespec ts;
        struct method_handler_info mhi;
        const char *ret_value_const = NULL;

        mhi = *(struct method_handler_info *)data;

        clock_gettime (CLOCK_MONOTONIC, &ts);
        if (mhi.type == MHT_CONST_STRING) {
            pcrdr_method_handler_const method_handler = mhi.handler;
            ret_value_const = method_handler (conn, from_endpoint,
                    normalized_name, parameter, &err_code);
        }
        else {
            pcrdr_method_handler method_handler = mhi.handler;
            ret_value = method_handler (conn, from_endpoint,
                    normalized_name, parameter, &err_code);
            ret_value_const = ret_value;
        }

        time_consumed = pcrdr_get_elapsed_seconds (&ts, NULL);

        if (err_code == 0) {
            size_t len_value;

            if (ret_value_const) {
                escaped_value = pcrdr_escape_string_for_json (ret_value_const);
                if (escaped_value == NULL) {
                    err_code = PURCRDR_EC_NOMEM;
                    goto done;
                }
            }
            else
                escaped_value = NULL;

            len_value = escaped_value ? (strlen (escaped_value) + 2) : 2;
            if (len_value > PURCRDR_MIN_PACKET_BUFF_SIZE) {
                sz_packet_buff = PURCRDR_MIN_PACKET_BUFF_SIZE + len_value;
                packet_buff = malloc (PURCRDR_MIN_PACKET_BUFF_SIZE + len_value);
                if (packet_buff == NULL) {
                    packet_buff = buff_in_stack;
                    sz_packet_buff = sizeof (buff_in_stack);

                    err_code = PURCRDR_EC_NOMEM;
                    goto done;
                }
            }
        }
    }

done:
    if (ret_value)
        free (ret_value);

    ret_code = pcrdr_errcode_to_retcode (err_code);
    n = snprintf (packet_buff, sz_packet_buff, 
            "{"
            "\"packetType\": \"result\","
            "\"resultId\": \"%s\","
            "\"callId\": \"%s\","
            "\"fromMethod\": \"%s\","
            "\"timeConsumed\": %.9f,"
            "\"retCode\": %d,"
            "\"retMsg\": \"%s\","
            "\"retValue\": \"%s\""
            "}",
            result_id, call_id,
            normalized_name,
            time_consumed,
            ret_code,
            pcrdr_get_ret_message (ret_code),
            escaped_value ? escaped_value : "");
    if (escaped_value)
        free (escaped_value);

    if (n < 0) {
        err_code = PURCRDR_EC_UNEXPECTED;
    }
    else if ((size_t)n >= sz_packet_buff) {
        err_code = PURCRDR_EC_TOO_SMALL_BUFF;
    }
    else
        pcrdr_send_text_packet (conn, packet_buff, n);

    if (packet_buff && packet_buff != buff_in_stack) {
        free (packet_buff);
    }

    return err_code;
}

static int dispatch_result_packet (pcrdr_conn* conn, const pcrdr_json *jo)
{
    pcrdr_json *jo_tmp;
    const char* result_id = NULL, *call_id = NULL;
    const char* from_endpoint = NULL;
    const char* from_method = NULL;
    const char* ret_value;
    void *data;
    pcrdr_result_handler result_handler;
    int ret_code;
    double time_consumed;

    if (json_object_object_get_ex (jo, "resultId", &jo_tmp) &&
            (result_id = json_object_get_string (jo_tmp))) {
    }
    else {
        ULOG_WARN ("No resultId\n");
    }

    if (json_object_object_get_ex (jo, "callId", &jo_tmp) &&
            (call_id = json_object_get_string (jo_tmp))) {
    }
    else {
        return PURCRDR_EC_PROTOCOL;
    }

    data = kvlist_get (&conn->call_list, call_id);
    if (data == NULL) {
        ULOG_ERR ("Not found result handler for callId: %s\n", call_id);
        return PURCRDR_EC_INVALID_VALUE;
    }

    result_handler = *(pcrdr_result_handler *)data;
    if (result_handler == NULL) {
        /* ignore the result */
        return 0;
    }

    if (json_object_object_get_ex (jo, "fromEndpoint", &jo_tmp) &&
            (from_endpoint = json_object_get_string (jo_tmp))) {
    }
    else {
        return PURCRDR_EC_PROTOCOL;
    }

    if (json_object_object_get_ex (jo, "fromMethod", &jo_tmp) &&
            (from_method = json_object_get_string (jo_tmp))) {
    }
    else {
        return PURCRDR_EC_PROTOCOL;
    }

    if (json_object_object_get_ex (jo, "timeConsumed", &jo_tmp) &&
            (time_consumed = json_object_get_double (jo_tmp))) {
    }
    else {
        return PURCRDR_EC_PROTOCOL;
    }

    if (json_object_object_get_ex (jo, "retCode", &jo_tmp) &&
            (ret_code = json_object_get_int (jo_tmp))) {
        conn->last_ret_code = ret_code;
    }
    else {
        return PURCRDR_EC_PROTOCOL;
    }

    if (json_object_object_get_ex (jo, "retValue", &jo_tmp) &&
            (ret_value = json_object_get_string (jo_tmp))) {
    }
    else {
        return PURCRDR_EC_PROTOCOL;
    }

    if (result_handler (conn, from_endpoint, from_method, call_id,
                ret_code, ret_value) == 0)
        kvlist_delete (&conn->call_list, call_id);

    return 0;
}

static int dispatch_event_packet (pcrdr_conn* conn, const pcrdr_json *jo)
{
    pcrdr_json *jo_tmp;
    const char* from_endpoint = NULL;
    const char* from_bubble = NULL;
    const char* bubble_data;
    char event_name [PURCRDR_LEN_ENDPOINT_NAME + PURCRDR_LEN_BUBBLE_NAME + 2];
    pcrdr_event_handler event_handler;
    int n;
    void *data;

    if (json_object_object_get_ex (jo, "fromEndpoint", &jo_tmp) &&
            (from_endpoint = json_object_get_string (jo_tmp))) {
    }
    else {
        return PURCRDR_EC_PROTOCOL;
    }

    if (json_object_object_get_ex (jo, "fromBubble", &jo_tmp) &&
            (from_bubble = json_object_get_string (jo_tmp))) {
    }
    else {
        return PURCRDR_EC_PROTOCOL;
    }

    if (json_object_object_get_ex (jo, "bubbleData", &jo_tmp) &&
            (bubble_data = json_object_get_string (jo_tmp))) {
    }
    else {
        bubble_data = "";
    }

    n = pcrdr_name_tolower_copy (from_endpoint, event_name, PURCRDR_LEN_ENDPOINT_NAME);
    event_name [n++] = '/';
    event_name [n] = '\0';
    pcrdr_name_toupper_copy (from_bubble, event_name + n, PURCRDR_LEN_BUBBLE_NAME);
    if ((data = kvlist_get (&conn->subscribed_list, event_name)) == NULL) {
        fprintf (stderr, "Got a unsubscribed event: %s\n", event_name);
        return PURCRDR_EC_UNKNOWN_EVENT;
    }
    else {
        event_handler = *(pcrdr_event_handler *)data;
        event_handler (conn, from_endpoint, from_bubble, bubble_data);
    }

    return 0;
}

static int wait_for_specific_call_result_packet (pcrdr_conn* conn, 
        const char* call_id, int time_expected, int *ret_code, char** ret_value)
{
    fd_set rfds;
    struct timeval tv;
    int retval;
    void* packet;
    unsigned int data_len;
    pcrdr_json* jo = NULL;
    time_t time_to_return;
    int err_code = 0;

    *ret_value = NULL;

    if (time_expected <= 0) {
        time_to_return = pcrdr_get_monotoic_time () + PURCRDR_DEF_TIME_EXPECTED;
    }
    else {
        time_to_return = pcrdr_get_monotoic_time () + time_expected;
    }

    while (1 /* pcrdr_get_monotoic_time () < time_to_return */) {
        FD_ZERO (&rfds);
        FD_SET (conn->fd, &rfds);

        tv.tv_sec = time_to_return - pcrdr_get_monotoic_time ();
        tv.tv_usec = 0;
        retval = select (conn->fd + 1, &rfds, NULL, NULL, &tv);

        if (retval == -1) {
            ULOG_ERR ("Failed to call select(): %s\n", strerror (errno));
            err_code = PURCRDR_EC_BAD_SYSTEM_CALL;
        }
        else if (retval) {
            err_code = pcrdr_read_packet_alloc (conn, &packet, &data_len);

            if (err_code) {
                ULOG_ERR ("Failed to read packet\n");
                break;
            }

            if (data_len == 0)
                continue;

            retval = pcrdr_json_packet_to_object (packet, data_len, &jo);
            free (packet);

            if (retval < 0) {
                ULOG_ERR ("Failed to parse JSON packet;\n");
                err_code = PURCRDR_EC_BAD_PACKET;
            }
            else if (retval == JPT_RESULT) {
                pcrdr_json *jo_tmp;
                const char* str_tmp;
                if (json_object_object_get_ex (jo, "callId", &jo_tmp) &&
                        (str_tmp = json_object_get_string (jo_tmp)) &&
                        strcasecmp (str_tmp, call_id) == 0) {

                    if (json_object_object_get_ex (jo, "retCode", &jo_tmp)) {
                        *ret_code = json_object_get_int (jo_tmp);
                    }
                    else {
                        *ret_code = PURCRDR_SC_INTERNAL_SERVER_ERROR;
                    }
                    conn->last_ret_code = *ret_code;

                    if (*ret_code == PURCRDR_SC_OK) {
                        if (json_object_object_get_ex (jo, "retValue", &jo_tmp)) {
                            str_tmp = json_object_get_string (jo_tmp);
                            if (str_tmp) {
                                *ret_value = strdup (str_tmp);
                            }
                            else
                                *ret_value = NULL;
                        }
                        else {
                            *ret_value = NULL;
                        }

                        json_object_put (jo);
                        jo = NULL;
                        err_code = 0;
                        break;
                    }
                    else if (*ret_code == PURCRDR_SC_ACCEPTED) {
                        // wait for ok
                        err_code = 0;
                    }
                }
                else {
                    err_code = dispatch_result_packet (conn, jo);
                }
            }
            else if (retval == JPT_ERROR) {
                pcrdr_json *jo_tmp;

                if (json_object_object_get_ex (jo, "retCode", &jo_tmp)) {
                    *ret_code = json_object_get_int (jo_tmp);
                }
                else {
                    *ret_code = PURCRDR_SC_INTERNAL_SERVER_ERROR;
                }

                conn->last_ret_code = *ret_code;
                err_code = PURCRDR_EC_SERVER_ERROR;

                if (json_object_object_get_ex (jo, "causedBy", &jo_tmp) &&
                        strcasecmp (json_object_get_string (jo_tmp), "call") == 0 &&
                        json_object_object_get_ex (jo, "causedId", &jo_tmp) &&
                        strcasecmp (json_object_get_string (jo_tmp), call_id) == 0) {
                    break;
                }
            }
            else if (retval == JPT_AUTH) {
                ULOG_WARN ("Should not be here for packetType `auth`\n");
                err_code = 0;
            }
            else if (retval == JPT_CALL) {
                err_code = dispatch_call_packet (conn, jo);
            }
            else if (retval == JPT_RESULT_SENT) {
                err_code = 0;
            }
            else if (retval == JPT_EVENT) {
                err_code = dispatch_event_packet (conn, jo);
            }
            else if (retval == JPT_EVENT_SENT) {
                err_code = 0;
            }
            else if (retval == JPT_AUTH_PASSED) {
                ULOG_WARN ("Unexpected authPassed packet\n");
                err_code = PURCRDR_EC_UNEXPECTED;
            }
            else if (retval == JPT_AUTH_FAILED) {
                ULOG_WARN ("Unexpected authFailed packet\n");
                err_code = PURCRDR_EC_UNEXPECTED;
            }
            else {
                ULOG_ERR ("Unknown packet type; quit...\n");
                err_code = PURCRDR_EC_PROTOCOL;
            }

            json_object_put (jo);
            jo = NULL;
        }
        else {
            err_code = PURCRDR_EC_TIMEOUT;
            break;
        }
    }

    if (jo)
        json_object_put (jo);

    return err_code;
}

int pcrdr_read_and_dispatch_packet (pcrdr_conn* conn)
{
    void* packet;
    unsigned int data_len;
    pcrdr_json* jo = NULL;
    int err_code, retval;

    err_code = pcrdr_read_packet_alloc (conn, &packet, &data_len);
    if (err_code) {
        ULOG_ERR ("Failed to read packet\n");
        goto done;
    }

    if (data_len == 0) { // no data
        return 0;
    }

    retval = pcrdr_json_packet_to_object (packet, data_len, &jo);
    free (packet);

    if (retval < 0) {
        ULOG_ERR ("Failed to parse JSON packet; quit...\n");
        err_code = PURCRDR_EC_BAD_PACKET;
    }
    else if (retval == JPT_ERROR) {
        ULOG_ERR ("The server gives an error packet\n");
        if (conn->error_handler) {
            conn->error_handler (conn, jo);
        }
        err_code = PURCRDR_EC_SERVER_ERROR;
    }
    else if (retval == JPT_AUTH) {
        ULOG_WARN ("Should not be here for packetType `auth`; quit...\n");
        err_code = PURCRDR_EC_UNEXPECTED;
    }
    else if (retval == JPT_CALL) {
        err_code = dispatch_call_packet (conn, jo);
    }
    else if (retval == JPT_RESULT) {
        err_code = dispatch_result_packet (conn, jo);
    }
    else if (retval == JPT_RESULT_SENT) {
        err_code = 0;
    }
    else if (retval == JPT_EVENT) {
        err_code = dispatch_event_packet (conn, jo);
    }
    else if (retval == JPT_EVENT_SENT) {
        err_code = 0;
    }
    else if (retval == JPT_AUTH_PASSED) {
        ULOG_WARN ("Unexpected authPassed packet\n");
        err_code = PURCRDR_EC_UNEXPECTED;
    }
    else if (retval == JPT_AUTH_FAILED) {
        ULOG_WARN ("Unexpected authFailed packet\n");
        err_code = PURCRDR_EC_UNEXPECTED;
    }
    else {
        ULOG_ERR ("Unknown packet type; quit...\n");
        err_code = PURCRDR_EC_PROTOCOL;
    }

done:
    if (jo)
        json_object_put (jo);

    return err_code;
}

int pcrdr_wait_and_dispatch_packet (pcrdr_conn* conn, int timeout_ms)
{
    fd_set rfds;
    struct timeval tv;
    int err_code = 0;
    int retval;

    FD_ZERO (&rfds);
    FD_SET (conn->fd, &rfds);

    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        retval = select (conn->fd + 1, &rfds, NULL, NULL, &tv);
    }
    else {
        retval = select (conn->fd + 1, &rfds, NULL, NULL, NULL);
    }

    if (retval == -1) {
        err_code = PURCRDR_EC_BAD_SYSTEM_CALL;
    }
    else if (retval) {
        err_code = pcrdr_read_and_dispatch_packet (conn);
    }
    else {
        err_code = PURCRDR_EC_TIMEOUT;
    }

    return err_code;
}

