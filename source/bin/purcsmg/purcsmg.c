/*
** purcsmg.c - The simple markup generator for PurCMC renderer.
**
** Copyright (C) 2021, 2022 FMSoft (http://www.fmsoft.cn)
**
** Author: Vincent Wei (https://github.com/VincentWei)
**
** This file is part of PurC Midnight Commander.
**
** PurCSMG is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** PurCSMG is distributed in the hope that it will be useful,
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
#include <getopt.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termio.h>
#include <signal.h>
#include <assert.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "purcmc_version.h"
#include "purcsmg.h"

struct run_info the_client;

static void print_copying (void)
{
    fprintf (stdout,
        "\n"
        "PurCSMG - a simple markup generator interacting with PurCMC renderer.\n"
        "\n"
        "Copyright (C) 2021, 2022 FMSoft <https://www.fmsoft.cn>\n"
        "\n"
        "PurCSMG is free software: you can redistribute it and/or modify\n"
        "it under the terms of the GNU General Public License as published by\n"
        "the Free Software Foundation, either version 3 of the License, or\n"
        "(at your option) any later version.\n"
        "\n"
        "PurCSMG is distributed in the hope that it will be useful,\n"
        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
        "GNU General Public License for more details.\n"
        "You should have received a copy of the GNU General Public License\n"
        "along with this program.  If not, see http://www.gnu.org/licenses/.\n"
        );
    fprintf (stdout, "\n");
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

/* Command line help. */
static void print_usage (void)
{
    printf ("PurCSMG (%s) - "
            "a simple markup generator interacting with PurCMC renderer\n\n",
            MC_CURRENT_VERSION);

    printf (
            "Usage: "
            "purcsmg [ options ... ]\n\n"
            ""
            "The following options can be supplied to the command:\n\n"
            ""
            "  -a --app=<app_name>          - Connect to PurcMC renderer with the specified app name.\n"
            "  -r --runner=<runner_name>    - Connect to PurcMC renderer with the specified runner name.\n"
            "  -f --file=<html_file>        - The initial HTML file to load.\n"
            "  -m --testmethod=<methodid>   - Run the specified test method; an integer between 0 ~ 13.\n"
            "  -n --noautochange            - Do not change the document automatically.\n"
            "  -c --cmdline                 - Use command line (NOT IMPLEMENTED).\n"
            "  -v --version                 - Display version information and exit.\n"
            "  -h --help                    - This help.\n"
            "\n"
            );
}

static char *load_doc_content(const char *file)
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

failed:
        fclose(f);
    }

    return buf;
}

static char short_options[] = "a:r:f:m:cvh";
static struct option long_opts[] = {
    {"app"            , required_argument , NULL , 'a' } ,
    {"runner"         , required_argument , NULL , 'r' } ,
    {"file"           , required_argument , NULL , 'f' } ,
    {"testmethod"     , required_argument , NULL , 'm' } ,
    {"noautochange"   , no_argument       , NULL , 'n' } ,
    {"cmdline"        , no_argument       , NULL , 'c' } ,
    {"version"        , no_argument       , NULL , 'v' } ,
    {"help"           , no_argument       , NULL , 'h' } ,
    {0, 0, 0, 0}
};

static int read_option_args (int argc, char **argv)
{
    int o, idx = 0;

    while ((o = getopt_long (argc, argv, short_options, long_opts, &idx)) >= 0) {
        if (-1 == o || EOF == o)
            break;
        switch (o) {
        case 'h':
            print_usage ();
            return -1;
        case 'v':
            fprintf (stdout, "PurCSMG: %s\n", MC_CURRENT_VERSION);
            return -1;
        case 'a':
            if (strlen (optarg) < PURC_LEN_APP_NAME)
                strcpy (the_client.app_name, optarg);
            break;
        case 'r':
            if (strlen (optarg) < PURC_LEN_RUNNER_NAME)
                strcpy (the_client.runner_name, optarg);
            break;
        case 'f':
            the_client.doc_content = load_doc_content(optarg);
            if (the_client.doc_content == NULL) {
                return -1;
            }
            break;
        case 'm':
            the_client.test_method = atoi(optarg);
            the_client.nr_windows = 1;
            break;
        case 'n':
            the_client.noautochange = true;
            break;
        case 'c':
            the_client.use_cmdline = true;
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

static const char *test_content =
    "<html><body>"
    "<div hvml-handle='1'></div>"
    "<div hvml-handle='2'></div>"
    "<div hvml-handle='3'></div>"
    "<div hvml-handle='4'></div>"
    "<div hvml-handle='5'></div>"
    "</body></html>";

static void init_autotest(pcrdr_conn* conn)
{
    struct run_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    if (info->doc_content == NULL) {
        info->doc_content = strdup(test_content);
    }

    if (info->nr_windows != 1) {
        // autotest
        info->nr_windows = MAX_NR_WINDOWS;

        for (int i = 0; i < MAX_NR_WINDOWS; i++) {
            info->max_changes[i] = (int) (time(NULL) % MAX_CHANGES);
            if (info->max_changes[i] < 8)
                info->max_changes[i] = 8;
        }
    }
    else {
        info->max_changes[0] = 128;
    }

    const char *end;
    pcutils_string_check_utf8(info->doc_content, -1, &info->nr_chars, &end);
    if (info->nr_chars == 0) {
        fprintf(stderr, "No valid UTF-8 characters in the content\n");
        exit(1);
    }

    info->len_content = end - info->doc_content;
}

static int my_response_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    struct run_info *info = pcrdr_conn_get_user_data(conn);
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
            info->state[win] = STATE_DOCUMENT_TESTING;
            break;

        case STATE_DOCUMENT_TESTING:
            if (info->changes[win] == info->max_changes[win]) {
                info->state[win] = STATE_DOCUMENT_RESET;
                return 0;
            }
            break;

        case STATE_DOCUMENT_RESET:
            info->dom_handles[win] = response_msg->resultValue;
            info->state[win] = STATE_WINDOW_DESTROYED;
            info->nr_destroyed_wins++;
            break;

        case STATE_WINDOW_DESTROYED:
            // do nothing.
            break;
    }

    // we only allow failed request when we are running testing.
    if (info->state[win] != STATE_DOCUMENT_TESTING &&
            response_msg->retCode != PCRDR_SC_OK) {
        info->state[win] = STATE_FATAL;

        printf("Window %d encountered a fatal error\n", win);
    }

    return 0;
}

static unsigned int run_times;

static int create_plain_win(pcrdr_conn* conn, int win)
{
    pcrdr_msg *msg;
    struct run_info *info = pcrdr_conn_get_user_data(conn);

    msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_WORKSPACE, 0,
            PCRDR_OPERATION_CREATEPLAINWINDOW, NULL, NULL,
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

    msg->dataType = PCRDR_MSG_DATA_TYPE_JSON;
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
    struct run_info *info = pcrdr_conn_get_user_data(conn);
    assert(win < info->nr_windows && info);

    pcrdr_msg *msg = NULL;
    purc_variant_t data = PURC_VARIANT_INVALID;

    if (info->len_content > PCRDR_MAX_INMEM_PAYLOAD_SIZE && (run_times % 2)) {
        // use writeBegin
        msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_PLAINWINDOW,
                info->win_handles[win],
                PCRDR_OPERATION_WRITEBEGIN, NULL, NULL,
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
                PCRDR_OPERATION_LOAD, NULL, NULL,
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
    struct run_info *info = pcrdr_conn_get_user_data(conn);
    assert(win < info->nr_windows && info);

    pcrdr_msg *msg = NULL;
    purc_variant_t data = PURC_VARIANT_INVALID;

    if (info->len_wrotten[win] + DEF_LEN_ONE_WRITE > info->len_content) {
        // writeEnd
        msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_PLAINWINDOW,
                info->win_handles[win],
                PCRDR_OPERATION_WRITEEND, NULL, NULL,
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
                PCRDR_OPERATION_WRITEMORE, NULL, NULL,
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

static pcrdr_msg *make_change_message_0(struct run_info *info, int win)
{
    char handle[64];
    sprintf(handle, "%llx", (long long)HANDLE_TEXTCONTENT_CLOCK1);

    char text[128];
    format_current_time (text, sizeof(text) - 1, true);

    return pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, info->dom_handles[win],
            PCRDR_OPERATION_UPDATE, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLE, handle,
            "textContent",
            PCRDR_MSG_DATA_TYPE_TEXT, text, strlen(text));
}

static pcrdr_msg *make_change_message_1(struct run_info *info, int win)
{
    char handles[128];
    sprintf(handles, "%llx,%llx",
            (long long)HANDLE_TEXTCONTENT_CLOCK1,
            (long long)HANDLE_TEXTCONTENT_CLOCK2);

    char text[128];
    format_current_time (text, sizeof(text) - 1, true);

    return pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, info->dom_handles[win],
            PCRDR_OPERATION_UPDATE, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLES, handles,
            "textContent",
            PCRDR_MSG_DATA_TYPE_TEXT, text, strlen(text));
}

static pcrdr_msg *make_change_message_2(struct run_info *info, int win)
{
    char handle[64];
    sprintf(handle, "%llx", (long long)HANDLE_ATTR_VALUE1);

    char text[128];
    format_current_time(text, sizeof(text) - 1, true);

    return pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, info->dom_handles[win],
            PCRDR_OPERATION_UPDATE, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLE, handle,
            "attr.value",
            PCRDR_MSG_DATA_TYPE_TEXT, text, strlen(text));
}

static pcrdr_msg *make_change_message_3(struct run_info *info, int win)
{
    char handles[128];
    sprintf(handles, "%llx,%llx",
            (long long)HANDLE_ATTR_VALUE1,
            (long long)HANDLE_ATTR_VALUE2);

    char text[128];
    format_current_time (text, sizeof(text) - 1, true);

    return pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, info->dom_handles[win],
            PCRDR_OPERATION_UPDATE, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLES, handles,
            "attr.value",
            PCRDR_MSG_DATA_TYPE_TEXT, text, strlen(text));
}

static const char *fragment =
"<li class=""><a href=\"https://www.fmsoft.cn/products\" target=\"_self\" class=\"\">产品</a></li>"
"<li class=""><a href=\"https://www.fmsoft.cn/blog\" target=\"_self\" class=\"\">动态</a></li>"
"<li class=""><a href=\"https://www.fmsoft.cn/partners\" target=\"_self\" class=\"\">合作伙伴</a></li>"
"<li class=""><a href=\"https://www.fmsoft.cn/about\" target=\"_self\" class="">关于</a></li>";

static pcrdr_msg *make_change_message_4(struct run_info *info, int win)
{
    pcrdr_msg *msg;

    char handle[64];
    sprintf(handle, "%llx", (long long)HANDLE_HTMLCONTENT);

    msg = pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, info->dom_handles[win],
            PCRDR_OPERATION_APPEND, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLE, handle,
            NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    msg->data = purc_variant_make_string_static(fragment, false);
    if (msg->data) {
        msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    }
    else {
        pcrdr_release_message(msg);
        return NULL;
    }

    return msg;
}

static pcrdr_msg *make_change_message_5(struct run_info *info, int win)
{
    pcrdr_msg *msg;

    char handle[64];
    sprintf(handle, "%llx", (long long)HANDLE_HTMLCONTENT);

    msg = pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, info->dom_handles[win],
            PCRDR_OPERATION_PREPEND, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLE, handle,
            NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    msg->data = purc_variant_make_string_static(fragment, false);
    if (msg->data) {
        msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    }
    else {
        pcrdr_release_message(msg);
        return NULL;
    }

    return msg;
}

static pcrdr_msg *make_change_message_6(struct run_info *info, int win)
{
    pcrdr_msg *msg;

    char handle[64];
    sprintf(handle, "%llx", (long long)HANDLE_HTMLCONTENT);

    msg = pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, info->dom_handles[win],
            PCRDR_OPERATION_INSERTBEFORE, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLE, handle,
            NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    msg->data = purc_variant_make_string_static(fragment, false);
    if (msg->data) {
        msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    }
    else {
        pcrdr_release_message(msg);
        return NULL;
    }

    return msg;
}

static pcrdr_msg *make_change_message_7(struct run_info *info, int win)
{
    pcrdr_msg *msg;

    char handle[64];
    sprintf(handle, "%llx", (long long)HANDLE_HTMLCONTENT);

    msg = pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, info->dom_handles[win],
            PCRDR_OPERATION_INSERTAFTER, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLE, handle,
            NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    msg->data = purc_variant_make_string_static(fragment, false);
    if (msg->data) {
        msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    }
    else {
        pcrdr_release_message(msg);
        return NULL;
    }

    return msg;
}

static pcrdr_msg *make_change_message_8(struct run_info *info, int win)
{
    pcrdr_msg *msg;

    char handle[64];
    sprintf(handle, "%llx", (long long)HANDLE_HTMLCONTENT);

    msg = pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, info->dom_handles[win],
            PCRDR_OPERATION_DISPLACE, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLE, handle,
            NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    msg->data = purc_variant_make_string_static(fragment, false);
    if (msg->data) {
        msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    }
    else {
        pcrdr_release_message(msg);
        return NULL;
    }

    return msg;
}

static pcrdr_msg *make_change_message_9(struct run_info *info, int win)
{
    char handle[64];
    sprintf(handle, "%llx", (long long)HANDLE_HTMLCONTENT);

    return pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, info->dom_handles[win],
            PCRDR_OPERATION_ERASE, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLE, handle,
            NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
}

static pcrdr_msg *make_change_message_a(struct run_info *info, int win)
{
    char handle[64];
    sprintf(handle, "%llx", (long long)HANDLE_HTMLCONTENT);

    return pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, info->dom_handles[win],
            PCRDR_OPERATION_CLEAR, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLE, handle,
            NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
}

static pcrdr_msg *make_change_message_b(struct run_info *info, int win)
{
    char handle[64];
    sprintf(handle, "%llx", (long long)HANDLE_ATTR_VALUE1);

    return pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, info->dom_handles[win],
            PCRDR_OPERATION_ERASE, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLE, handle,
            "attr.value",
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
}

static pcrdr_msg *make_change_message_c(struct run_info *info, int win)
{
    char handles[128];
    sprintf(handles, "%llx,%llx",
            (long long)HANDLE_ATTR_VALUE1,
            (long long)HANDLE_ATTR_VALUE2);

    return pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, info->dom_handles[win],
            PCRDR_OPERATION_ERASE, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLES, handles,
            "attr.value",
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
}

static pcrdr_msg *make_change_message_d(struct run_info *info, int win)
{
    pcrdr_msg *msg;
    char handle[64];
    sprintf(handle, "%llx", (long long)HANDLE_TEXTCONTENT_TITLE);

    msg = pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, info->dom_handles[win],
            PCRDR_OPERATION_UPDATE, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLE, handle,
            "textContent",
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    msg->data = purc_variant_make_string_static("FMSoft", false);
    if (msg->data) {
        msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    }
    else {
        pcrdr_release_message(msg);
        return NULL;
    }

    return msg;
}

static int change_document(pcrdr_conn* conn, int win)
{
    struct run_info *info = pcrdr_conn_get_user_data(conn);
    assert(win < info->nr_windows && info);

    static pcrdr_msg *(*makers[])(struct run_info *info, int win) = {
        make_change_message_0,
        make_change_message_1,
        make_change_message_2,
        make_change_message_3,
        make_change_message_4,
        make_change_message_5,
        make_change_message_6,
        make_change_message_7,
        make_change_message_8,
        make_change_message_9,
        make_change_message_a,
        make_change_message_b,
        make_change_message_c,
        make_change_message_d,
    };

    pcrdr_msg *msg;

    unsigned int method;
    if (info->nr_windows == 1) {
        if (info->test_method > 0 && info->test_method < TABLESIZE(makers))
            method = (unsigned int)info->test_method;
        else
            method = 0;
    }
    else {
        method = run_times % TABLESIZE(makers);
    }

    msg = makers[method](info, win);
    if (msg == NULL)
        return -1;

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, (void *)(uintptr_t)win,
                my_response_handler) < 0) {
        goto failed;
    }

    info->wait[win] = true;

    info->changes[win]++;
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

static int reset_window(pcrdr_conn* conn, int win)
{
    struct run_info *info = pcrdr_conn_get_user_data(conn);
    assert(win < info->nr_windows && info);

    pcrdr_msg *msg = NULL;
    purc_variant_t data = PURC_VARIANT_INVALID;

    msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_PLAINWINDOW,
            info->win_handles[win],
            PCRDR_OPERATION_LOAD, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    data = purc_variant_make_string_static(test_content, false);

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

static int destroy_window(pcrdr_conn* conn, int win)
{
    struct run_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    pcrdr_msg *msg;
    char buff[128];

    if (run_times % 2) {
        // use identifier
        sprintf(buff, "the-plain-window-%d", win);
        msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_WORKSPACE, 0,
                PCRDR_OPERATION_DESTROYPLAINWINDOW, NULL, NULL,
                PCRDR_MSG_ELEMENT_TYPE_ID, buff, NULL,
                PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    }
    else {
        // use handle
        sprintf(buff, "%llx", (unsigned long long)info->win_handles[win]);
        msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_WORKSPACE, 0,
                PCRDR_OPERATION_DESTROYPLAINWINDOW, NULL, NULL,
                PCRDR_MSG_ELEMENT_TYPE_HANDLE, buff, NULL,
                PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    }

    if (msg == NULL) {
        return -1;
    }

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

    if (msg)
        pcrdr_release_message(msg);
    return -1;
}

static int check_quit(pcrdr_conn* conn, int win)
{
    struct run_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    if (info->nr_destroyed_wins == info->nr_windows) {
        printf("all window destroyed; quitting...\n");
        return -1;
    }

    return 0;
}

/*
 * this function will be called every second.
 * returns -1 on failure.
 */
static int run_autotest(pcrdr_conn* conn)
{
    struct run_info *info = pcrdr_conn_get_user_data(conn);

    assert(info);

    int win = run_times % info->nr_windows;
    run_times++;

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
    case STATE_DOCUMENT_TESTING:
        if (info->wait[win])
            return 0;
        if (info->noautochange)
            return 0;
        if (info->changes[win] == info->max_changes[win]) {
            return reset_window(conn, win);
        }
        return change_document(conn, win);
    case STATE_DOCUMENT_RESET:
        if (info->wait[win])
            return 0;
        return destroy_window(conn, win);
    case STATE_WINDOW_DESTROYED:
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

static void my_event_handler(pcrdr_conn* conn, const pcrdr_msg *msg)
{
    struct run_info *info = pcrdr_conn_get_user_data(conn);

    switch (msg->target) {
    case PCRDR_MSG_TARGET_PLAINWINDOW:
        printf("Got an event to plainwindow (%p): %s\n",
                (void *)(uintptr_t)msg->targetValue,
                purc_variant_get_string_const(msg->eventName));

        int win = -1;
        for (int i = 0; i < info->nr_windows; i++) {
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
                purc_variant_get_string_const(msg->eventName));

        if (msg->target == PCRDR_MSG_TARGET_DOM) {
            printf("    The handle of the target element: %s\n",
                purc_variant_get_string_const(msg->elementValue));
        }

        if (msg->dataType == PCRDR_MSG_DATA_TYPE_TEXT) {
            printf("    The attached data is TEXT:\n%s\n",
                purc_variant_get_string_const(msg->data));
        }
        else if (msg->dataType == PCRDR_MSG_DATA_TYPE_JSON) {
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

int main (int argc, char **argv)
{
    int ret, cnnfd = -1, ttyfd = -1, maxfd;
    pcrdr_conn* conn;
    fd_set rfds;
    struct timeval tv;
    char curr_time [16];

    purc_instance_extra_info extra_info = {
        .renderer_prot = PURC_RDRPROT_PURCMC,
        .renderer_uri = "unix://" PCRDR_PURCMC_US_PATH,
    };

    print_copying ();

    if (read_option_args (argc, argv)) {
        return EXIT_FAILURE;
    }

    if (!the_client.app_name[0])
        strcpy (the_client.app_name, "cn.fmsoft.hvml.purcsmg");
    if (!the_client.runner_name[0])
        strcpy (the_client.runner_name, "cmdline");

    ret = purc_init_ex (PURC_MODULE_PCRDR, the_client.app_name,
            the_client.runner_name, &extra_info);
    if (ret != PURC_ERROR_OK) {
        fprintf (stderr, "Failed to initialize the PurC instance: %s\n",
                purc_get_error_message (ret));
        return EXIT_FAILURE;
    }

    kvlist_init (&the_client.ret_value_list, NULL);
    the_client.running = true;
    the_client.last_sigint_time = 0;

    if (the_client.use_cmdline && (ttyfd = setup_tty ()) < 0)
        goto failed;

    conn = purc_get_conn_to_renderer();
    assert(conn != NULL);
    cnnfd = pcrdr_conn_socket_fd(conn);
    assert(cnnfd >= 0);

    the_client.ttyfd = ttyfd;
    the_client.curr_history_idx = -1;
    pcrdr_conn_set_user_data (conn, &the_client);
    pcrdr_conn_set_event_handler (conn, my_event_handler);

    format_current_time (curr_time, sizeof (curr_time) - 1, false);

    if (ttyfd >= 0)
        cmdline_print_prompt (conn, true);
    else
        init_autotest(conn);

    maxfd = cnnfd > ttyfd ? cnnfd : ttyfd;

    do {
        int retval;
        char new_clock [16];
        time_t old_time;

        FD_ZERO (&rfds);
        FD_SET (cnnfd, &rfds);
        if (ttyfd >= 0)
            FD_SET (ttyfd, &rfds);

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

                if (ttyfd >= 0) cmdline_print_prompt (conn, true);
            }
            else if (ttyfd >= 0 && FD_ISSET (ttyfd, &rfds)) {
                handle_tty_input (conn);
            }
        }
        else {
            format_current_time (new_clock, sizeof (new_clock) - 1, false);
            if (strcmp (new_clock, curr_time)) {
                strcpy (curr_time, new_clock);
                pcrdr_ping_renderer (conn);
            }

            time_t new_time = time(NULL);
            if (old_time != new_time) {
                old_time = new_time;
                if (ttyfd < 0 && run_autotest(conn) < 0)
                    goto failed;
            }
        }

        if (purc_get_monotoic_time () > the_client.last_sigint_time + 5) {
            // cancel quit
            the_client.last_sigint_time = 0;
        }

    } while (the_client.running);

    fputs ("\n", stderr);

failed:
    if (the_client.doc_content)
        free (the_client.doc_content);

    if (ttyfd >= 0)
        restore_tty (ttyfd);

    purc_cleanup ();

    return EXIT_SUCCESS;
}

