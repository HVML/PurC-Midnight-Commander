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

#include "lib/hiboxcompat.h"
#include "lib/md5.h"
#include "lib/kvlist.h"
#include "lib/purcrdr.h"

struct _pcrdr_conn {
    int type;
    int fd;
    int last_ret_code;
    int padding_;

    char* srv_host_name;
    char* own_host_name;
    char* app_name;
    char* runner_name;

    struct kvlist call_list;

    pcrdr_event_handler event_handler;
    void *user_data;
};

pcrdr_event_handler pcrdr_conn_get_event_handler (pcrdr_conn *conn)
{
    return conn->event_handler;
}

pcrdr_event_handler pcrdr_conn_set_event_handler (pcrdr_conn *conn,
        pcrdr_event_handler event_handler)
{
    pcrdr_event_handler old = conn->event_handler;
    conn->event_handler = event_handler;

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

#if 0
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
#endif

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

    kvlist_init (&(*conn)->call_list, NULL);

#if 0
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
#endif

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

    kvlist_free (&conn->call_list);

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

int pcrdr_read_packet (pcrdr_conn* conn, char* packet_buf, size_t *sz_packet)
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
            *sz_packet = 0;
            return 0;
        }
        else if (header.op == US_OPCODE_PING) {
            header.op = US_OPCODE_PONG;
            header.sz_payload = 0;
            if (conn_write (conn->fd, &header, sizeof (USFrameHeader))) {
                err_code = PURCRDR_EC_IO;
                goto done;
            }
            *sz_packet = 0;
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
                *sz_packet = offset + 1;
            }
            else {
                *sz_packet = offset;
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

int pcrdr_read_packet_alloc (pcrdr_conn* conn, void **packet, size_t *sz_packet)
{
    char* packet_buf = NULL;
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
            *sz_packet = 0;
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
            *sz_packet = 0;
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
                *sz_packet = offset + 1;
            }
            else {
                *sz_packet = offset;
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

int pcrdr_send_text_packet (pcrdr_conn* conn, const char* text, size_t len)
{
    int retv = 0;

    if (conn->type == CT_UNIX_SOCKET) {
        USFrameHeader header;

        if (len > PURCRDR_MAX_FRAME_PAYLOAD_SIZE) {
            size_t left = len;

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

#if 0
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
            err_code = pcrdr_read_packet_alloc (conn, &message, &data_len);

            if (err_code) {
                ULOG_ERR ("Failed to read packet\n");
                break;
            }

            if (data_len == 0)
                continue;

            retval = pcrdr_json_packet_to_object (message, data_len, &jo);
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

#endif

int pcrdr_read_and_dispatch_packet (pcrdr_conn* conn)
{
    void* packet;
    size_t data_len;
    pcrdr_msg* msg = NULL;
    int err_code, retval;

    err_code = pcrdr_read_packet_alloc (conn, &packet, &data_len);
    if (err_code) {
        ULOG_ERR ("Failed to read packet\n");
        goto done;
    }

    if (data_len == 0) { // no data
        return 0;
    }

    retval = pcrdr_parse_packet (packet, data_len, &msg);
    free (packet);

    if (retval < 0) {
        ULOG_ERR ("Failed to parse JSON packet; quit...\n");
        err_code = PURCRDR_EC_BAD_PACKET;
    }
    else if (retval == PCRDR_MSG_TYPE_EVENT) {
        ULOG_INFO ("The server gives an event packet\n");
        if (conn->event_handler) {
            conn->event_handler (conn, msg);
        }
    }
    else if (retval == PCRDR_MSG_TYPE_REQUEST) {
        ULOG_INFO ("The server gives a request packet\n");
    }
    else if (retval == PCRDR_MSG_TYPE_RESPONSE) {
        ULOG_INFO ("The server gives a response packet\n");
    }
    else {
        ULOG_ERR ("Unknown packet type; quit...\n");
        err_code = PURCRDR_EC_PROTOCOL;
    }

done:
    if (msg)
        pcrdr_release_message (msg);

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

