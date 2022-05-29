/*
** purcsex.c - A simple example of PurCMC.
**
** Copyright (C) 2021, 2022 FMSoft (http://www.fmsoft.cn)
**
** Author: Vincent Wei (https://github.com/VincentWei)
**
** This file is part of PurC Midnight Commander.
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see http://www.gnu.org/licenses/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <getopt.h>

#include <purc/purc.h>

#include "purcmc_version.h"

#define NR_WINDOWS  1

enum {
    STATE_INITIAL = 0,
    STATE_WINDOW_CREATED,
    STATE_DOCUMENT_WROTTEN,
    STATE_DOCUMENT_LOADED,
    STATE_EVENT_LOOP,
    STATE_WINDOW_DESTROYED,
    STATE_FATAL,
};

struct client_info {
    bool running;
    bool use_cmdline;

    time_t last_sigint_time;

    size_t run_times;
    size_t nr_destroyed_wins;

    char app_name[PURC_LEN_APP_NAME + 1];
    char runner_name[PURC_LEN_RUNNER_NAME + 1];

    char *doc_content;
    size_t len_content;

    char *doc_fragment;
    size_t len_fragment;

    int state[NR_WINDOWS];
    bool wait[NR_WINDOWS];

    size_t len_wrotten[NR_WINDOWS];

    // handles of windows.
    uint64_t win_handles[NR_WINDOWS];

    // handles of DOM.
    uint64_t dom_handles[NR_WINDOWS];
};

static void print_copying (void)
{
    fprintf (stdout,
        "\n"
        "purcsex - a sample showing news in the PurCMC renderer.\n"
        "\n"
        "Copyright (C) 2021, 2022 FMSoft <https://www.fmsoft.cn>\n"
        "\n"
        "This program is free software: you can redistribute it and/or modify\n"
        "it under the terms of the GNU General Public License as published by\n"
        "the Free Software Foundation, either version 3 of the License, or\n"
        "(at your option) any later version.\n"
        "\n"
        "This program is distributed in the hope that it will be useful,\n"
        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
        "GNU General Public License for more details.\n"
        "You should have received a copy of the GNU General Public License\n"
        "along with this program.  If not, see http://www.gnu.org/licenses/.\n"
        );
    fprintf (stdout, "\n");
}

/* Command line help. */
static void print_usage (void)
{
    printf ("purcsex (%s) - "
            "a sample showing news in the PurCMC renderer\n\n",
            MC_CURRENT_VERSION);

    printf (
            "Usage: "
            "purcsex [ options ... ]\n\n"
            ""
            "The following options can be supplied to the command:\n\n"
            ""
            "  -a --app=<app_name>          - Connect to PurcMC renderer with the specified app name.\n"
            "  -r --runner=<runner_name>    - Connect to PurcMC renderer with the specified runner name.\n"
            "  -n --name=<sample_name>      - The sample name like `purcsex`.\n"
            "  -v --version                 - Display version information and exit.\n"
            "  -h --help                    - This help.\n"
            "\n"
            );
}

static void format_current_time (char* buff, size_t sz, bool has_second)
{
    struct tm tm;
    time_t curr_time = time (NULL);

    localtime_r (&curr_time, &tm);
    if (has_second)
        strftime (buff, sz, "%H:%M:%S", &tm);
    else
        strftime (buff, sz, "%H:%M", &tm);
}

static char *load_file_content(const char *file, size_t *length)
{
    FILE *f = fopen(file, "r");
    char *buf = NULL;

    if (f) {
        if (fseek(f, 0, SEEK_END))
            goto failed;

        long len = ftell(f);
        if (len < 0)
            goto failed;

        buf = malloc(len + 1);
        if (buf == NULL)
            goto failed;

        fseek(f, 0, SEEK_SET);
        if (fread(buf, 1, len, f) < (size_t)len) {
            free(buf);
            buf = NULL;
        }
        buf[len] = '\0';

        if (length)
            *length = (size_t)len;
failed:
        fclose(f);
    }

    return buf;
}

static bool load_doc_content_and_fragment(struct client_info *info,
        const char* name)
{
    char file[strlen(name) + 8];
    strcpy(file, name);
    strcat(file, ".html");

    info->doc_content = load_file_content(file, &info->len_content);
    if (info->doc_content == NULL)
        return false;

    strcpy(file, name);
    strcat(file, ".frag");
    info->doc_fragment = load_file_content(file, &info->len_fragment);
    if (info->doc_fragment == NULL) {
        free(info->doc_content);
        return false;
    }

    return true;
}

static char short_options[] = "a:r:f:m:cvh";
static struct option long_opts[] = {
    {"app"            , required_argument , NULL , 'a' } ,
    {"runner"         , required_argument , NULL , 'r' } ,
    {"name"           , required_argument , NULL , 'n' } ,
    {"version"        , no_argument       , NULL , 'v' } ,
    {"help"           , no_argument       , NULL , 'h' } ,
    {0, 0, 0, 0}
};

static int read_option_args(struct client_info *client, int argc, char **argv)
{
    int o, idx = 0;

    while ((o = getopt_long(argc, argv, short_options, long_opts, &idx)) >= 0) {
        if (-1 == o || EOF == o)
            break;
        switch (o) {
        case 'h':
            print_usage ();
            return -1;
        case 'v':
            fprintf (stdout, "purcsex: %s\n", MC_CURRENT_VERSION);
            return -1;
        case 'a':
            if (strlen(optarg) < PURC_LEN_APP_NAME)
                strcpy(client->app_name, optarg);
            break;
        case 'r':
            if (strlen (optarg) < PURC_LEN_RUNNER_NAME)
                strcpy (client->runner_name, optarg);
            break;
        case 'n':
            if (!load_doc_content_and_fragment(client, optarg)) {
                return -1;
            }
            break;
        case '?':
            print_usage ();
            return -1;
        default:
            return -1;
        }
    }

    if (optind < argc) {
        print_usage ();
        return -1;
    }

    return 0;
}

static int my_response_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    int win = (int)(uintptr_t)context;

    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        return 0;
    }

    printf("Got a response for request (%s) for window %d: %d\n",
            purc_variant_get_string_const(response_msg->requestId), win,
            response_msg->retCode);

    info->wait[win] = false;
    switch (info->state[win]) {
        case STATE_INITIAL:
            info->state[win] = STATE_WINDOW_CREATED;
            info->win_handles[win] = response_msg->resultValue;
            break;

        case STATE_WINDOW_CREATED:
            if (info->len_wrotten[win] < info->len_content) {
                info->state[win] = STATE_DOCUMENT_WROTTEN;
            }
            else {
                info->state[win] = STATE_DOCUMENT_LOADED;
                info->dom_handles[win] = response_msg->resultValue;
            }
            break;

        case STATE_DOCUMENT_WROTTEN:
            if (info->len_wrotten[win] == info->len_content) {
                info->state[win] = STATE_DOCUMENT_LOADED;
                info->dom_handles[win] = response_msg->resultValue;
            }
            break;

        case STATE_DOCUMENT_LOADED:
            info->state[win] = STATE_EVENT_LOOP;
            break;
    }

    // we only allow failed request when we are running testing.
    if (info->state[win] != STATE_EVENT_LOOP &&
            response_msg->retCode != PCRDR_SC_OK) {
        info->state[win] = STATE_FATAL;

        printf("Window %d encountered a fatal error\n", win);
    }

    return 0;
}

static int create_plain_win(pcrdr_conn* conn, int win)
{
    pcrdr_msg *msg;
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_WORKSPACE, 0,
            PCRDR_OPERATION_CREATEPLAINWINDOW, NULL,
            PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    if (msg == NULL) {
        goto failed;
    }

    char name_buff[64];
    char title_buff[64];
    sprintf(name_buff, "the-plain-window-%d", win);
    sprintf(title_buff, "The Plain Window No. %d", win);

    purc_variant_t data = purc_variant_make_object(2,
            purc_variant_make_string_static("name", false),
            purc_variant_make_string_static(name_buff, false),
            purc_variant_make_string_static("title", false),
            purc_variant_make_string_static(title_buff, false));
    if (data == PURC_VARIANT_INVALID) {
        goto failed;
    }

    msg->dataType = PCRDR_MSG_DATA_TYPE_EJSON;
    msg->data = data;

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, (void *)(uintptr_t)win,
                my_response_handler) < 0) {
        goto failed;
    }

    info->wait[win] = true;

    printf("Request (%s) `%s` for window %d sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation), win);
    pcrdr_release_message(msg);
    return 0;

failed:
    printf("Failed call to (%s) for window %d\n", __func__, win);

    if (msg) {
        pcrdr_release_message(msg);
    }
    else if (data) {
        purc_variant_unref(data);
    }

    return -1;
}

#define DEF_LEN_ONE_WRITE   1024

static int load_or_write_doucment(pcrdr_conn* conn, int win)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(win < NR_WINDOWS && info);

    pcrdr_msg *msg = NULL;
    purc_variant_t data = PURC_VARIANT_INVALID;

    if (info->len_content > PCRDR_MAX_INMEM_PAYLOAD_SIZE) {
        // use writeBegin
        msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_PLAINWINDOW,
                info->win_handles[win],
                PCRDR_OPERATION_WRITEBEGIN, NULL,
                PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
                PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);

        const char *start = info->doc_content;
        const char *end;
        pcutils_string_check_utf8_len(start, DEF_LEN_ONE_WRITE, NULL, &end);
        if (end > start) {
            size_t len_to_write = end - start;

            data = purc_variant_make_string_ex(start, len_to_write, false);
            info->len_wrotten[win] = len_to_write;
        }
        else {
            printf("In %s for window %d: no valid character\n", __func__, win);
            goto failed;
        }
    }
    else {
        // use load
        msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_PLAINWINDOW,
                info->win_handles[win],
                PCRDR_OPERATION_LOAD, NULL,
                PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
                PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);

        data = purc_variant_make_string_static(info->doc_content, false);
        info->len_wrotten[win] = info->len_content;
    }

    if (msg == NULL || data == PURC_VARIANT_INVALID) {
        goto failed;
    }

    msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    msg->data = data;

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, (void *)(uintptr_t)win,
                my_response_handler) < 0) {
        goto failed;
    }

    info->wait[win] = true;

    printf("Request (%s) `%s` for window %d sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation), win);
    pcrdr_release_message(msg);
    return 0;

failed:
    printf("Failed call to (%s) for window %d\n", __func__, win);

    if (msg) {
        pcrdr_release_message(msg);
    }
    else if (data) {
        purc_variant_unref(data);
    }

    return -1;
}

static int write_more_doucment(pcrdr_conn* conn, int win)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(win < NR_WINDOWS && info);

    pcrdr_msg *msg = NULL;
    purc_variant_t data = PURC_VARIANT_INVALID;

    if (info->len_wrotten[win] + DEF_LEN_ONE_WRITE > info->len_content) {
        // writeEnd
        msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_PLAINWINDOW,
                info->win_handles[win],
                PCRDR_OPERATION_WRITEEND, NULL,
                PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
                PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);

        data = purc_variant_make_string(
                info->doc_content + info->len_wrotten[win], false);
        info->len_wrotten[win] = info->len_content;
    }
    else {
        // writeMore
        msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_PLAINWINDOW,
                info->win_handles[win],
                PCRDR_OPERATION_WRITEMORE, NULL,
                PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
                PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);

        const char *start = info->doc_content + info->len_wrotten[win];
        const char *end;
        pcutils_string_check_utf8_len(start, DEF_LEN_ONE_WRITE, NULL, &end);
        if (end > start) {
            size_t len_to_write = end - start;

            data = purc_variant_make_string_ex(start, len_to_write, false);
            info->len_wrotten[win] += len_to_write;
        }
        else {
            printf("In %s for window %d: no valid character\n", __func__, win);
            goto failed;
        }
    }

    if (msg == NULL || data == PURC_VARIANT_INVALID) {
        goto failed;
    }

    msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    msg->data = data;
    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, (void *)(uintptr_t)win,
                my_response_handler) < 0) {
        goto failed;
    }

    info->wait[win] = true;

    printf("Request (%s) `%s` for window %d sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation), win);
    pcrdr_release_message(msg);
    return 0;

failed:
    printf("Failed call to (%s) for window %d\n", __func__, win);

    if (msg) {
        pcrdr_release_message(msg);
    }
    else if (data) {
        purc_variant_unref(data);
    }

    return -1;
}

static int check_quit(pcrdr_conn* conn, int win)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    if (info->nr_destroyed_wins == NR_WINDOWS) {
        printf("all windows destroyed; quitting...\n");
        return -1;
    }

    return 0;
}

/* TODO: change document here */
static int change_document(pcrdr_conn* conn, int win);

/*
 * this function will be called every second.
 * returns -1 on failure.
 */
static int run_autotest(pcrdr_conn* conn)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    assert(info);

    int win = info->run_times % NR_WINDOWS;
    info->run_times++;

    switch (info->state[win]) {
    case STATE_INITIAL:
        if (info->wait[win])
            return 0;
        return create_plain_win(conn, win);
    case STATE_WINDOW_CREATED:
        if (info->wait[win])
            return 0;
        return load_or_write_doucment(conn, win);
    case STATE_DOCUMENT_WROTTEN:
        if (info->wait[win])
            return 0;
        return write_more_doucment(conn, win);
    case STATE_DOCUMENT_LOADED:
        if (info->wait[win])
            return 0;
        return change_document(conn, win);
    case STATE_EVENT_LOOP:
        if (info->wait[win])
            return 0;
        return check_quit(conn, win);
    case STATE_FATAL:
        return -1;
    }

    return -1;
}

static ssize_t stdio_write(void *ctxt, const void *buf, size_t count)
{
    return fwrite(buf, 1, count, (FILE *)ctxt);
}

#define HANDLE_MODAL  "789046"

static pcrdr_msg *make_change_message(struct client_info *info, int win)
{
    pcrdr_msg *msg;

    msg = pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, info->dom_handles[win],
            PCRDR_OPERATION_DISPLACE, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLE, HANDLE_MODAL,
            NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    msg->data = purc_variant_make_string_static(info->doc_fragment, false);
    if (msg->data) {
        msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    }
    else {
        pcrdr_release_message(msg);
        return NULL;
    }

    return msg;
}

/* TODO: change document here */
static int change_document(pcrdr_conn* conn, int win)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(win < NR_WINDOWS && info);

    pcrdr_msg *msg = make_change_message(info, win);
    if (msg == NULL)
        return -1;

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, (void *)(uintptr_t)win,
                my_response_handler) < 0) {
        goto failed;
    }

    info->wait[win] = true;

    printf("Request (%s) `%s` (%s) for window %d sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation),
            msg->property?purc_variant_get_string_const(msg->property):"N/A",
            win);
    pcrdr_release_message(msg);
    return 0;

failed:
    printf("Failed call to (%s) for window %d\n", __func__, win);

    pcrdr_release_message(msg);
    return -1;
}

/* TODO: handle event here */
static void my_event_handler(pcrdr_conn* conn, const pcrdr_msg *msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    switch (msg->target) {
    case PCRDR_MSG_TARGET_PLAINWINDOW:
        printf("Got an event to plainwindow (%p): %s\n",
                (void *)(uintptr_t)msg->targetValue,
                purc_variant_get_string_const(msg->event));

        int win = -1;
        for (int i = 0; i < NR_WINDOWS; i++) {
            if (info->win_handles[i] == msg->targetValue) {
                win = i;
                break;
            }
        }

        if (win >= 0) {
            info->state[win] = STATE_WINDOW_DESTROYED;
            info->nr_destroyed_wins++;
        }
        else {
            printf("Window not found: (%p)\n",
                    (void *)(uintptr_t)msg->targetValue);
        }
        break;

    case PCRDR_MSG_TARGET_SESSION:
    case PCRDR_MSG_TARGET_WORKSPACE:
    case PCRDR_MSG_TARGET_PAGE:
    case PCRDR_MSG_TARGET_DOM:
    default:
        printf("Got an event not intrested in (target: %d/%p): %s\n",
                msg->target, (void *)(uintptr_t)msg->targetValue,
                purc_variant_get_string_const(msg->event));

        if (msg->target == PCRDR_MSG_TARGET_DOM) {
            printf("    The handle of the source element: %s\n",
                purc_variant_get_string_const(msg->element));
        }

        if (msg->dataType == PCRDR_MSG_DATA_TYPE_TEXT) {
            printf("    The attached data is TEXT:\n%s\n",
                purc_variant_get_string_const(msg->data));
        }
        else if (msg->dataType == PCRDR_MSG_DATA_TYPE_EJSON) {
            purc_rwstream_t rws = purc_rwstream_new_for_dump(stdout, stdio_write);

            printf("    The attached data is EJSON:\n");
            purc_variant_serialize(msg->data, rws, 0, 0, NULL);
            purc_rwstream_destroy(rws);
            printf("\n");
        }
        else {
            printf("    The attached data is VOID\n");
        }
        break;
    }

}

int main(int argc, char **argv)
{
    int ret, cnnfd = -1, maxfd;
    pcrdr_conn* conn;
    fd_set rfds;
    struct timeval tv;
    char curr_time[16];

    purc_instance_extra_info extra_info = {
        .renderer_prot = PURC_RDRPROT_PURCMC,
        .renderer_uri = "unix://" PCRDR_PURCMC_US_PATH,
    };

    print_copying();

    struct client_info client;
    memset(&client, 0, sizeof(client));

    if (read_option_args(&client, argc, argv)) {
        return EXIT_FAILURE;
    }

    if (!client.app_name[0])
        strcpy(client.app_name, "cn.fmsoft.hvml.purcmc");
    if (!client.runner_name[0])
        strcpy(client.runner_name, "sample");

    ret = purc_init_ex(PURC_MODULE_PCRDR, client.app_name,
            client.runner_name, &extra_info);
    if (ret != PURC_ERROR_OK) {
        fprintf (stderr, "Failed to initialize the PurC instance: %s\n",
                purc_get_error_message(ret));
        return EXIT_FAILURE;
    }

    client.running = true;
    client.last_sigint_time = 0;

    conn = purc_get_conn_to_renderer();
    assert(conn != NULL);
    cnnfd = pcrdr_conn_socket_fd(conn);
    assert(cnnfd >= 0);

    pcrdr_conn_set_user_data(conn, &client);
    pcrdr_conn_set_event_handler(conn, my_event_handler);

    format_current_time(curr_time, sizeof (curr_time) - 1, false);

    maxfd = cnnfd;
    do {
        int retval;
        char new_clock [16];
        time_t old_time;

        FD_ZERO (&rfds);
        FD_SET (cnnfd, &rfds);

        tv.tv_sec = 0;
        tv.tv_usec = 200 * 1000;
        retval = select (maxfd + 1, &rfds, NULL, NULL, &tv);

        if (retval == -1) {
            if (errno == EINTR)
                continue;
            else
                break;
        }
        else if (retval) {
            if (FD_ISSET (cnnfd, &rfds)) {
                int err_code = pcrdr_read_and_dispatch_message (conn);
                if (err_code < 0) {
                    fprintf (stderr, "Failed to read and dispatch message: %s\n",
                            purc_get_error_message(purc_get_last_error()));
                    break;
                }
            }
        }
        else {
            format_current_time(new_clock, sizeof (new_clock) - 1, false);
            if (strcmp(new_clock, curr_time)) {
                strcpy(curr_time, new_clock);
                pcrdr_ping_renderer(conn);
            }

            time_t new_time = time(NULL);
            if (old_time != new_time) {
                old_time = new_time;
                if (run_autotest(conn) < 0)
                    break;
            }
        }

        if (purc_get_monotoic_time() > client.last_sigint_time + 5) {
            // cancel quit
            client.last_sigint_time = 0;
        }

    } while (client.running);

    fputs ("\n", stderr);

    if (client.doc_content)
        free(client.doc_content);

    purc_cleanup();

    return EXIT_SUCCESS;
}

