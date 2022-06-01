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

#define _GNU_SOURCE
#include "purcmc_version.h"
#include "purcsex.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <getopt.h>

#include <dlfcn.h>

#define DEF_LEN_ONE_WRITE   1024

static void print_copying(void)
{
    printf(
        "\n"
        "purcsex - a simple examples interacting with the PurCMC renderer.\n"
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
    printf("\n");
}

/* Command line help. */
static void print_usage(void)
{
    printf("purcsex (%s) - "
            "a simple example interacting with the PurCMC renderer\n\n",
            MC_CURRENT_VERSION);

    printf(
            "Usage: "
            "purcsex [ options ... ]\n\n"
            ""
            "The following options can be supplied to the command:\n\n"
            ""
            "  -a --app=<app_name>          - Connect to PurcMC renderer with the specified app name.\n"
            "  -r --runner=<runner_name>    - Connect to PurcMC renderer with the specified runner name.\n"
            "  -n --name=<sample_name>      - The sample name like `shownews`.\n"
            "  -i --interact                - Wait for confirmation before issuing another operation.\n"
            "  -v --version                 - Display version information and exit.\n"
            "  -h --help                    - This help.\n"
            "\n"
            );
}

static char short_options[] = "a:r:n:vh";
static struct option long_opts[] = {
    {"app"            , required_argument , NULL , 'a' } ,
    {"runner"         , required_argument , NULL , 'r' } ,
    {"name"           , required_argument , NULL , 'n' } ,
    {"interact"       , no_argument       , NULL , 'i' } ,
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
            print_usage();
            return -1;
        case 'v':
            printf("purcsex: %s\n", MC_CURRENT_VERSION);
            return -1;
        case 'i':
            client->interact = true;
            break;
        case 'a':
            if (purc_is_valid_app_name(optarg))
                strcpy(client->app_name, optarg);
            break;
        case 'r':
            if (purc_is_valid_runner_name(optarg))
                strcpy(client->runner_name, optarg);
            break;
        case 'n':
            if (purc_is_valid_token(optarg, PURC_LEN_IDENTIFIER))
                strcpy(client->sample_name, optarg);
            else {
                print_usage();
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

static void format_current_time(char* buff, size_t sz, bool has_second)
{
    struct tm tm;
    time_t curr_time = time(NULL);

    localtime_r(&curr_time, &tm);
    if (has_second)
        strftime(buff, sz, "%H:%M:%S", &tm);
    else
        strftime(buff, sz, "%H:%M", &tm);
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

static bool load_sample(struct client_info *info)
{
    char file[strlen(info->sample_name) + 8];
    strcpy(file, info->sample_name);
    strcat(file, ".json");

    info->sample = purc_variant_load_from_json_file(file);
    if (info->sample == PURC_VARIANT_INVALID) {
        LOG_ERROR("Failed to load the sample from JSON file (%s)\n",
                info->sample_name);
        return false;
    }

    info->nr_windows = 0;
    purc_variant_t tmp;
    if ((tmp = purc_variant_object_get_by_ckey(info->sample, "nrWindows"))) {
        uint32_t nr_windows;
        if (purc_variant_cast_to_uint32(tmp, &nr_windows, false))
            info->nr_windows = nr_windows;
    }

    if (info->nr_windows == 0 || info->nr_windows > MAX_NR_WINDOWS) {
        LOG_WARN("Wrong number of windows (%u)\n", info->nr_windows);
        info->nr_windows = 1;
    }

    info->initialOps = purc_variant_object_get_by_ckey(info->sample, "initialOps");
    if (info->initialOps == PURC_VARIANT_INVALID ||
            !purc_variant_array_size(info->initialOps, &info->nr_ops)) {
        LOG_ERROR("No valid `initialOps` defined.\n");
        return false;
    }

    info->namedOps = purc_variant_object_get_by_ckey(info->sample, "namedOps");
    if (info->namedOps == PURC_VARIANT_INVALID ||
            !purc_variant_is_object(info->namedOps)) {
        LOG_WARN("`namedOps` defined but not an object.\n");
        info->namedOps = PURC_VARIANT_INVALID;
    }

    info->events = purc_variant_object_get_by_ckey(info->sample, "events");
    if (info->events == PURC_VARIANT_INVALID ||
            !purc_variant_array_size(info->events, &info->nr_events)) {
        LOG_WARN("No valid `events` defined.\n");
        info->events = PURC_VARIANT_INVALID;
        info->nr_events = 0;
    }

    char buff[strlen(info->sample_name) + 12];

    sprintf(buff, "./lib%s.so", info->sample_name);
    LOG_INFO("Try to load module: %s\n", buff);
    info->sample_handle = dlopen(buff, RTLD_NOW | RTLD_GLOBAL);
    if (info->sample_handle) {
        sample_initializer_t init_func;
        init_func = dlsym(info->sample_handle, "sample_initializer");
        if (init_func)
            info->sample_data = init_func(info->sample_name);

        LOG_INFO("Module for sample loaded from %s; sample data: %p\n",
                buff, info->sample_data);
    }

    return true;
}

static void unload_sample(struct client_info *info)
{
    for (int i = 0; i < info->nr_windows; i++) {
        if (info->doc_content[i]) {
            free(info->doc_content[i]);
        }
    }

    if (info->sample) {
        purc_variant_unref(info->sample);
    }

    if (info->sample_handle) {

        sample_terminator_t term_func;
        term_func = dlsym(info->sample_handle, "sample_terminator");
        if (term_func)
            term_func(info->sample_name, info->sample_data);

        dlclose(info->sample_handle);

        LOG_INFO("Module for sample `%s` unloaded; sample data: %p\n",
                info->sample_name, info->sample_data);
    }

    memset(info, 0, sizeof(*info));
}

static int split_target(const char *target, char *target_name)
{
    char *sep = strchr(target, '/');
    if (sep == NULL)
        return -1;

    size_t name_len = sep - target;
    if (name_len > PURC_LEN_IDENTIFIER) {
        return -1;
    }

    strncpy(target_name, target, name_len);
    target_name[name_len] = 0;

    sep++;
    if (sep[0] == 0)
        return -1;

    char *end;
    long l = strtol(sep, &end, 10);
    if (*end == 0)
        return (int)l;

    return -1;
}

static int transfer_target_info(struct client_info *info,
        const char *source, uint64_t *target_value)
{
    int type = -1;
    char target_name[PURC_LEN_IDENTIFIER + 1];

    int value = split_target(source, target_name);
    if (strcmp(target_name, "session") == 0) {
        type = PCRDR_MSG_TARGET_SESSION;
        *target_value = (uint64_t)value;
    }
    else if (strcmp(target_name, "workspace") == 0) {
        type = PCRDR_MSG_TARGET_WORKSPACE;
        *target_value = (uint64_t)value;
    }
    else if (strcmp(target_name, "plainwindow") == 0) {
        type = PCRDR_MSG_TARGET_PLAINWINDOW;
        if (value < 0 || value >= info->nr_windows)
            goto failed;

        *target_value = info->win_handles[value];
    }
    else if (strcmp(target_name, "dom") == 0) {
        type = PCRDR_MSG_TARGET_DOM;
        if (value < 0 || value >= info->nr_windows)
            goto failed;

        *target_value = info->dom_handles[value];
    }

    return type;

failed:
    return -1;
}

static const char* split_element(const char *element, char *element_type)
{
    char *sep = strchr(element, '/');
    if (sep == NULL)
        return NULL;

    size_t type_len = sep - element;
    if (type_len > PURC_LEN_IDENTIFIER) {
        return NULL;
    }

    strncpy(element_type, element, type_len);
    element_type[type_len] = 0;

    sep++;
    if (sep[0] == 0)
        return NULL;

    return sep;
}

static const char *transfer_element_info(struct client_info *info,
        const char *element, int *element_type)
{
    const char *value;
    char buff[PURC_LEN_IDENTIFIER + 1];

    value = split_element(element, buff);
    if (value) {
        if (strcmp(buff, "handle") == 0) {
            *element_type = PCRDR_MSG_ELEMENT_TYPE_HANDLE;
        }
        else if (strcmp(buff, "id") == 0) {
            *element_type = PCRDR_MSG_ELEMENT_TYPE_ID;
        }
        else if (strcmp(buff, "plainwindow") == 0) {
            *element_type = PCRDR_MSG_ELEMENT_TYPE_HANDLE;

            int win = atoi(value);
            if (win < 0 || win >= info->nr_windows) {
                value = NULL;
            }
            else {
                sprintf(info->buff, "%llx", (long long)info->win_handles[win]);
                value = info->buff;
            }
        }
    }

    return value;
}

static int issue_operation(pcrdr_conn* conn, purc_variant_t op);

static inline int issue_first_operation(pcrdr_conn* conn)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    info->ops_issued = 0;

    purc_variant_t op;
    op = purc_variant_array_get(info->initialOps, info->ops_issued);
    assert(op);
    return issue_operation(conn, op);
}

static inline int issue_next_operation(pcrdr_conn* conn)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    if (info->ops_issued < info->nr_ops) {
        info->ops_issued++;
        purc_variant_t op;
        op = purc_variant_array_get(info->initialOps, info->ops_issued);
        if (op) {

            if (info->interact) {
                printf("Please press ENTER to issue next operation:\n");
                do {
                    int ch;
                    do {
                        ch = getchar();
                    } while (ch != '\n');
                } while (0);
            }

            return issue_operation(conn, op);
        }
    }

    return 0;
}

static int plainwin_created_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    int win = (int)(uintptr_t)context;
    assert(win < info->nr_windows);

    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        return 0;
    }

    LOG_INFO("Got a response for request (%s) to create plainwin (%d): %d\n",
            purc_variant_get_string_const(response_msg->requestId), win,
            response_msg->retCode);

    if (response_msg->retCode == PCRDR_SC_OK) {
        info->nr_windows_created++;
        info->win_handles[win] = response_msg->resultValue;
        issue_next_operation(conn);
    }
    else {
        LOG_ERROR("failed to create a plain window\n");
        // info->running = false;
    }

    return 0;
}

static int create_plain_win(pcrdr_conn* conn, purc_variant_t op)
{
    pcrdr_msg *msg;
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    if (info->nr_windows == info->nr_windows_created)
        goto failed;
    int win = info->nr_windows_created;

    msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_WORKSPACE, 0,
            PCRDR_OPERATION_CREATEPLAINWINDOW, NULL,
            PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    if (msg == NULL) {
        goto failed;
    }

    char name_buff[64];
    sprintf(name_buff, "the-plain-window-%d", win);

    purc_variant_t tmp;
    const char *title = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "title"))) {
        title = purc_variant_get_string_const(tmp);
    }
    if (title == NULL)
        title = "No Title";

    purc_variant_t data = purc_variant_make_object(2,
            purc_variant_make_string_static("name", false),
            purc_variant_make_string_static(name_buff, false),
            purc_variant_make_string_static("title", false),
            purc_variant_make_string_static(title, false));
    if (data == PURC_VARIANT_INVALID) {
        goto failed;
    }

    msg->dataType = PCRDR_MSG_DATA_TYPE_JSON;
    msg->data = data;

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, (void *)(uintptr_t)win,
                plainwin_created_handler) < 0) {
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` for window %d sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation), win);
    pcrdr_release_message(msg);
    return 0;

failed:
    LOG_ERROR("Failed call for window %d\n", win);

    if (msg) {
        pcrdr_release_message(msg);
    }
    else if (data) {
        purc_variant_unref(data);
    }

    return -1;
}

static int plainwin_destroyed_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    int win = (int)(uintptr_t)context;
    assert(win < info->nr_windows);

    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        return 0;
    }

    LOG_INFO("Got a response for request (%s) to destroy plainwin (%d): %d\n",
            purc_variant_get_string_const(response_msg->requestId), win,
            response_msg->retCode);

    if (response_msg->retCode == PCRDR_SC_OK) {
        info->nr_windows_created--;
        info->win_handles[win] = 0;
        if (info->nr_windows_created == 0) {
            info->running = false;
        }
    }
    else {
        LOG_ERROR("failed to create a plain window\n");
        // info->running = false;
    }

    return 0;
}

static int destroy_plain_win(pcrdr_conn* conn, purc_variant_t op)
{
    pcrdr_msg *msg;
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    const char *element = NULL;
    purc_variant_t tmp;
    if ((tmp = purc_variant_object_get_by_ckey(op, "element"))) {
        element = purc_variant_get_string_const(tmp);
    }

    if (element == NULL) {
        goto failed;
    }

    int win = -1;
    char element_name[PURC_LEN_IDENTIFIER + 1];
    win = split_target(element, element_name);
    if (win < 0 || win > info->nr_windows_created || !info->win_handles[win]) {
        goto failed;
    }

    if (strcmp(element_name, "plainwindow")) {
        goto failed;
    }

    char handle[32];
    sprintf(handle, "%llx", (long long)info->win_handles[win]);
    msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_WORKSPACE, 0,
            PCRDR_OPERATION_DESTROYPLAINWINDOW, "-",
            PCRDR_MSG_ELEMENT_TYPE_HANDLE, handle, NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    if (msg == NULL) {
        goto failed;
    }

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, (void *)(uintptr_t)win,
                plainwin_destroyed_handler) < 0) {
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` for window %d sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation), win);
    pcrdr_release_message(msg);
    return 0;

failed:
    LOG_ERROR("Failed call for window %d\n", win);

    if (msg) {
        pcrdr_release_message(msg);
    }

    return -1;
}

static int loaded_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    int win = (int)(uintptr_t)context;
    assert(win < info->nr_windows);

    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        return 0;
    }

    LOG_INFO("Got a response for request (%s) to load document content (%d): %d\n",
            purc_variant_get_string_const(response_msg->requestId), win,
            response_msg->retCode);

    if (response_msg->retCode == PCRDR_SC_OK) {
        info->dom_handles[win] = response_msg->resultValue;

        free(info->doc_content[win]);
        info->doc_content[win] = NULL;

        issue_next_operation(conn);
    }
    else {
        LOG_ERROR("failed to load document\n");
        // info->running = false;
    }

    return 0;
}

static int write_more_doucment(pcrdr_conn* conn, int win);

static int wrotten_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    int win = (int)(uintptr_t)context;
    assert(win < info->nr_windows);

    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        return 0;
    }

    LOG_INFO("Got a response for request (%s) to write content (%d): %d\n",
            purc_variant_get_string_const(response_msg->requestId), win,
            response_msg->retCode);

    if (response_msg->retCode == PCRDR_SC_OK) {
        if (info->len_wrotten[win] == info->len_content[win]) {
            info->dom_handles[win] = response_msg->resultValue;

            free(info->doc_content[win]);
            info->doc_content[win] = NULL;

            issue_next_operation(conn);
        }
        else {
            write_more_doucment(conn, win);
        }
    }
    else {
        LOG_ERROR("failed to write content\n");
        // info->running = false;
    }

    return 0;
}

static int write_more_doucment(pcrdr_conn* conn, int win)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(win < info->nr_windows && info);

    pcrdr_msg *msg = NULL;
    purc_variant_t data = PURC_VARIANT_INVALID;

    pcrdr_response_handler handler;
    if (info->len_wrotten[win] + DEF_LEN_ONE_WRITE > info->len_content[win]) {
        // writeEnd
        msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_PLAINWINDOW,
                info->win_handles[win],
                PCRDR_OPERATION_WRITEEND, NULL,
                PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
                PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);

        data = purc_variant_make_string(
                info->doc_content[win] + info->len_wrotten[win], false);
        info->len_wrotten[win] = info->len_content[win];
        handler = loaded_handler;
    }
    else {
        // writeMore
        msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_PLAINWINDOW,
                info->win_handles[win],
                PCRDR_OPERATION_WRITEMORE, NULL,
                PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
                PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);

        const char *start = info->doc_content[win] + info->len_wrotten[win];
        const char *end;
        pcutils_string_check_utf8_len(start, DEF_LEN_ONE_WRITE, NULL, &end);
        if (end > start) {
            size_t len_to_write = end - start;

            data = purc_variant_make_string_ex(start, len_to_write, false);
            info->len_wrotten[win] += len_to_write;
        }
        else {
            LOG_WARN("no valid character for window %d\n", win);
            goto failed;
        }
        handler = wrotten_handler;
    }

    if (msg == NULL || data == PURC_VARIANT_INVALID) {
        goto failed;
    }

    msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    msg->data = data;
    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, (void *)(uintptr_t)win,
                handler) < 0) {
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` for window %d sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation), win);
    pcrdr_release_message(msg);
    return 0;

failed:
    LOG_ERROR("Failed call for window %d\n", win);

    if (msg) {
        pcrdr_release_message(msg);
    }
    else if (data) {
        purc_variant_unref(data);
    }

    return -1;
}

static int load_or_write_doucment(pcrdr_conn* conn, purc_variant_t op)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    purc_variant_t tmp;
    const char *target = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "target"))) {
        target = purc_variant_get_string_const(tmp);
    }

    if (target == NULL) {
        goto failed;
    }

    char target_name[PURC_LEN_IDENTIFIER + 1];
    int win = 0;
    win = split_target(target, target_name);
    if (win < 0 || win > info->nr_windows_created || !info->win_handles[win]) {
        goto failed;
    }

    if (strcmp(target_name, "plainwindow")) {
        goto failed;
    }

    if (info->doc_content[win] == NULL) {
        const char *content;
        if ((tmp = purc_variant_object_get_by_ckey(op, "content"))) {
            content = purc_variant_get_string_const(tmp);
        }

        if (content) {
            info->doc_content[win] = load_file_content(content,
                    &info->len_content[win]);
        }

        if (info->doc_content[win] == NULL) {
            LOG_ERROR("Failed to load document content from %s\n", content);
            goto failed;
        }
    }

    pcrdr_msg *msg = NULL;
    purc_variant_t data = PURC_VARIANT_INVALID;

    pcrdr_response_handler handler;
    if (info->len_content[win] > DEF_LEN_ONE_WRITE) {
        // use writeBegin
        msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_PLAINWINDOW,
                info->win_handles[win],
                PCRDR_OPERATION_WRITEBEGIN, NULL,
                PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
                PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);

        const char *start = info->doc_content[win];
        const char *end;
        pcutils_string_check_utf8_len(start, DEF_LEN_ONE_WRITE, NULL, &end);
        if (end > start) {
            size_t len_to_write = end - start;

            data = purc_variant_make_string_ex(start, len_to_write, false);
            info->len_wrotten[win] = len_to_write;
        }
        else {
            LOG_WARN("no valid character for window: %d\n", win);
            goto failed;
        }

        handler = wrotten_handler;
    }
    else {
        // use load
        msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_PLAINWINDOW,
                info->win_handles[win],
                PCRDR_OPERATION_LOAD, NULL,
                PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
                PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);

        data = purc_variant_make_string_static(info->doc_content[win], false);
        info->len_wrotten[win] = info->len_content[win];
        handler = loaded_handler;
    }

    if (msg == NULL || data == PURC_VARIANT_INVALID) {
        goto failed;
    }

    msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    msg->data = data;

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, (void *)(uintptr_t)win,
                handler) < 0) {
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` for window %d sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation), win);
    pcrdr_release_message(msg);
    return 0;

failed:
    LOG_ERROR("Failed call for window %d\n", win);

    if (msg) {
        pcrdr_release_message(msg);
    }
    else if (data) {
        purc_variant_unref(data);
    }

    return -1;
}

static pcrdr_msg *make_change_message(struct client_info *info,
        int op_id, const char *operation, purc_variant_t op, int win)
{
    purc_variant_t tmp;
    const char *element = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "element"))) {
        element = purc_variant_get_string_const(tmp);
    }

    if (element == NULL) {
        goto failed;
    }

    char element_type_str[PURC_LEN_IDENTIFIER + 1];
    const char *element_value;

    element_value = split_element(element, element_type_str);
    if (element_value == NULL) {
        goto failed;
    }

    pcrdr_msg_element_type element_type;
    if (strcmp(element_type_str, "handle") == 0) {
        element_type = PCRDR_MSG_ELEMENT_TYPE_HANDLE;
    }
    else if (strcmp(element_type_str, "id") == 0) {
        element_type = PCRDR_MSG_ELEMENT_TYPE_ID;
    }
    else {
        LOG_ERROR("Not supported element type: %s\n", element_type_str);
        goto failed;
    }

    const char *property = NULL;
    const char *content = NULL;
    char *content_loaded = NULL;
    size_t content_length;
    if (op_id == PCRDR_K_OPERATION_UPDATE) {
        if ((tmp = purc_variant_object_get_by_ckey(op, "property"))) {
            property = purc_variant_get_string_const(tmp);
        }

        if ((tmp = purc_variant_object_get_by_ckey(op, "content"))) {
            content = purc_variant_get_string_const(tmp);
        }

        if (content == NULL) {
            goto failed;
        }
    }
    else if (op_id == PCRDR_K_OPERATION_ERASE ||
            op_id == PCRDR_K_OPERATION_CLEAR) {
        if ((tmp = purc_variant_object_get_by_ckey(op, "property"))) {
            property = purc_variant_get_string_const(tmp);
        }
    }
    else {
        if ((tmp = purc_variant_object_get_by_ckey(op, "content"))) {
            content = purc_variant_get_string_const(tmp);
        }

        if (content) {
            content_loaded = load_file_content(content, &content_length);
            content = content_loaded;
        }

        if (content == NULL) {
            goto failed;
        }
    }

    pcrdr_msg *msg;
    msg = pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, info->dom_handles[win],
            operation, NULL,
            element_type, element_value,
            property,
            content ? PCRDR_MSG_DATA_TYPE_TEXT : PCRDR_MSG_DATA_TYPE_VOID,
            content, content_length);

    if (content_loaded) {
        free(content_loaded);
    }

    return msg;

failed:
    return NULL;
}

static int changed_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    int win = (int)(uintptr_t)context;

    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        return 0;
    }

    LOG_INFO("Got a response for request (%s) to change document (%d): %d\n",
            purc_variant_get_string_const(response_msg->requestId), win,
            response_msg->retCode);

    if (response_msg->retCode == PCRDR_SC_OK) {
        issue_next_operation(conn);
    }
    else {
        LOG_ERROR("failed to change document\n");
        // struct client_info *info = pcrdr_conn_get_user_data(conn);
        // info->running = false;
    }

    return 0;
}

static int change_document(pcrdr_conn* conn,
        int op_id, const char *operation, purc_variant_t op)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    purc_variant_t tmp;
    const char *target = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "target"))) {
        target = purc_variant_get_string_const(tmp);
    }

    if (target == NULL) {
        goto failed;
    }

    char target_name[PURC_LEN_IDENTIFIER + 1];
    int win = 0;
    win = split_target(target, target_name);
    if (win < 0 || win > info->nr_windows_created || !info->win_handles[win]) {
        goto failed;
    }

    if (strcmp(target_name, "dom")) {
        goto failed;
    }

    pcrdr_msg *msg = make_change_message(info, op_id, operation, op, win);
    if (msg == NULL)
        return -1;

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, (void *)(uintptr_t)win,
                changed_handler) < 0) {
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` (%s) for window %d sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation),
            msg->property?purc_variant_get_string_const(msg->property):"N/A",
            win);
    pcrdr_release_message(msg);
    return 0;

failed:
    LOG_ERROR("Failed call for window %d\n", win);

    pcrdr_release_message(msg);
    return -1;
}


static int issue_operation(pcrdr_conn* conn, purc_variant_t op)
{
    // struct client_info *info = pcrdr_conn_get_user_data(conn);

    purc_variant_t tmp;
    const char *operation = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "operation"))) {
        operation = purc_variant_get_string_const(tmp);
    }

    if (operation == NULL) {
        LOG_ERROR("No valid `operation` defined in the operation.\n");
        assert(0);
        return -1;
    }

    unsigned int op_id;
    purc_atom_t op_atom = pcrdr_try_operation_atom(operation);
    if (op_atom == 0 || pcrdr_operation_from_atom(op_atom, &op_id) == NULL) {
        LOG_ERROR("Unknown operation: %s.\n", operation);
    }

    int retv;
    switch (op_id) {
    case PCRDR_K_OPERATION_CREATEPLAINWINDOW:
        retv = create_plain_win(conn, op);
        break;

    case PCRDR_K_OPERATION_DESTROYPLAINWINDOW:
        retv = destroy_plain_win(conn, op);
        break;

    case PCRDR_K_OPERATION_LOAD:
        retv = load_or_write_doucment(conn, op);
        break;

    case PCRDR_K_OPERATION_APPEND:
    case PCRDR_K_OPERATION_PREPEND:
    case PCRDR_K_OPERATION_INSERTBEFORE:
    case PCRDR_K_OPERATION_INSERTAFTER:
    case PCRDR_K_OPERATION_DISPLACE:
    case PCRDR_K_OPERATION_UPDATE:
    case PCRDR_K_OPERATION_ERASE:
    case PCRDR_K_OPERATION_CLEAR:
        retv = change_document(conn, op_id, operation, op);
        break;

    default:
        LOG_ERROR("Not implemented operation: %s.\n", operation);
        retv = -1;
        break;
    }

    return retv;
}

static ssize_t stdio_write(void *ctxt, const void *buf, size_t count)
{
    return fwrite(buf, 1, count, (FILE *)ctxt);
}

static const char *match_event(pcrdr_conn* conn,
        purc_variant_t evt_vrt, const pcrdr_msg *evt_msg)
{
    const char *event = NULL;
    const char *source = NULL;
    const char *element = NULL;
    const char *namedOp = NULL;

    purc_variant_t tmp;
    if ((tmp = purc_variant_object_get_by_ckey(evt_vrt, "event"))) {
        event = purc_variant_get_string_const(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(evt_vrt, "source"))) {
        source = purc_variant_get_string_const(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(evt_vrt, "element"))) {
        element = purc_variant_get_string_const(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(evt_vrt, "namedOp"))) {
        namedOp = purc_variant_get_string_const(tmp);
    }

    if (event == NULL || source == NULL || namedOp == NULL) {
        goto failed;
    }

    if (strcmp(event, purc_variant_get_string_const(evt_msg->eventName)))
        goto failed;

    struct client_info *info = pcrdr_conn_get_user_data(conn);

    int target_type;
    uint64_t target_value;
    target_type = transfer_target_info(info, source, &target_value);
    if (target_type != evt_msg->target ||
            target_value != evt_msg->targetValue) {
        goto failed;
    }

    if (element) {
        int element_type;
        const char* element_value;
        element_value = transfer_element_info(info, element, &element_type);

        if (element_type != evt_msg->elementType || element_value == NULL ||
                strcmp(element_value,
                    purc_variant_get_string_const(evt_msg->elementValue))) {
            LOG_DEBUG("element (%d vs %d; %s vs %s) not matched\n",
                    element_type, evt_msg->elementType,
                    element, purc_variant_get_string_const(evt_msg->element));
            goto failed;
        }
    }

    return namedOp;

failed:
    return NULL;
}

static void my_event_handler(pcrdr_conn* conn, const pcrdr_msg *msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    const char *op_name = NULL;
    purc_variant_t event;
    for (size_t i = 0; i < info->nr_events; i++) {
        event = purc_variant_array_get(info->events, i);
        op_name = match_event(conn, event, msg);
        if (op_name)
            break;
    }

    if (op_name) {
        // reserved built-in operation.
        if (strcmp(op_name, "func:quit") == 0) {
            info->running = false;
        }
        else if (strncmp(op_name, "func:", 5) == 0 && info->sample_handle) {

            sample_event_handler_t event_handler;
            event_handler = dlsym(info->sample_handle, op_name + 5);

            if (event_handler) {
                event_handler(conn, event, msg);
            }
            else {
                LOG_ERROR("cannot find function in module: `%s` (%s)\n",
                        op_name + 5, dlerror());
            }
        }
        else {
            purc_variant_t op;
            op = purc_variant_object_get_by_ckey(info->namedOps, op_name);
            if (op == PURC_VARIANT_INVALID || !purc_variant_is_object(op)) {
                LOG_ERROR("Bad named operation: %s\n", op_name);
            }
            else {
                LOG_INFO("Issue the named operation: %s\n", op_name);
                issue_operation(conn, op);
            }
        }
    }
    else {
        LOG_INFO("Got an event not intrested in (target: %d/%p): %s (%s)\n",
                msg->target, (void *)(uintptr_t)msg->targetValue,
                purc_variant_get_string_const(msg->eventName),
                purc_variant_get_string_const(msg->eventSource));

        if (msg->target == PCRDR_MSG_TARGET_DOM) {
            LOG_INFO("    The handle of the source element: %s\n",
                purc_variant_get_string_const(msg->elementValue));
        }

        if (msg->dataType == PCRDR_MSG_DATA_TYPE_TEXT) {
            LOG_INFO("    The attached data is TEXT:\n%s\n",
                purc_variant_get_string_const(msg->data));
        }
        else if (msg->dataType == PCRDR_MSG_DATA_TYPE_JSON) {
            purc_rwstream_t rws = purc_rwstream_new_for_dump(stdout, stdio_write);

            LOG_INFO("    The attached data is EJSON:\n");
            purc_variant_serialize(msg->data, rws, 0, 0, NULL);
            purc_rwstream_destroy(rws);
            fprintf(stdout, "\n");
        }
        else {
            LOG_INFO("    The attached data is VOID\n");
        }
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
    if (!client.sample_name[0])
        strcpy(client.sample_name, client.runner_name);

    ret = purc_init_ex(PURC_MODULE_PCRDR, client.app_name,
            client.runner_name, &extra_info);
    if (ret != PURC_ERROR_OK) {
        LOG_ERROR("Failed to initialize the PurC instance: %s\n",
                purc_get_error_message(ret));
        return EXIT_FAILURE;
    }

    my_log_enable(true, NULL);

    conn = purc_get_conn_to_renderer();
    if (conn == NULL) {
        LOG_ERROR("Failed to connect PURCMC renderer: %s\n",
                extra_info.renderer_uri);
        purc_cleanup();
        return EXIT_FAILURE;
    }

    if (!load_sample(&client)) {
        purc_cleanup();
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

    issue_first_operation(conn);

    maxfd = cnnfd;
    do {
        int retval;
        char new_clock[16];
        time_t old_time;

        FD_ZERO(&rfds);
        FD_SET(cnnfd, &rfds);

        tv.tv_sec = 0;
        tv.tv_usec = 200 * 1000;
        retval = select(maxfd + 1, &rfds, NULL, NULL, &tv);

        if (retval == -1) {
            if (errno == EINTR)
                continue;
            else
                break;
        }
        else if (retval) {
            if (FD_ISSET(cnnfd, &rfds)) {
                int err_code = pcrdr_read_and_dispatch_message(conn);
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
            }
        }

        if (purc_get_monotoic_time() > client.last_sigint_time + 5) {
            // cancel quit
            client.last_sigint_time = 0;
        }

    } while (client.running);

    fputs ("\n", stderr);

    unload_sample(&client);

    purc_cleanup();

    return EXIT_SUCCESS;
}

