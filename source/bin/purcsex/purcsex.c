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

#include "purcmc_version.h"
#include "purcsex.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <ctype.h>

#include <dlfcn.h>

#define DEF_LEN_ONE_WRITE   1024

static void print_copying(void)
{
    printf(
        "\n"
        "purcsex - a simple example interacting with the PurCMC renderer.\n"
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
            "  -s --sample=<sample_name>    - The sample name like `calculator`.\n"
            "  -i --interact                - Wait for confirmation before issuing another operation.\n"
            "  -v --version                 - Display version information and exit.\n"
            "  -h --help                    - This help.\n"
            "\n"
            );
}

static char short_options[] = "a:r:s:vh";
static struct option long_opts[] = {
    {"app"            , required_argument , NULL , 'a' } ,
    {"runner"         , required_argument , NULL , 'r' } ,
    {"sample"         , required_argument , NULL , 's' } ,
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
        case 's':
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

static purc_variant_t load_operation_content(purc_variant_t op)
{
    purc_variant_t tmp;

    const char *file = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "content"))) {
        file = purc_variant_get_string_const(tmp);
    }

    if (file == NULL) {
        LOG_ERROR("No content defined in operation\n");
        return PURC_VARIANT_INVALID;
    }

    char *loaded;
    size_t len_content;
    if (file) {
        loaded = load_file_content(file, &len_content);
    }

    if (loaded == NULL) {
        LOG_ERROR("Failed to load content from %s\n", file);
        return PURC_VARIANT_INVALID;
    }

    return purc_variant_make_string_reuse_buff(loaded, len_content, false);
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

    info->initialOps =
        purc_variant_object_get_by_ckey(info->sample, "initialOps");
    if (info->initialOps == PURC_VARIANT_INVALID ||
            !purc_variant_is_array(info->initialOps)) {
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
    purc_variant_unref(info->handles);
    purc_variant_unref(info->doc_contents);
    purc_variant_unref(info->doc_wrotten_len);
    purc_variant_unref(info->batchOps);

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

static uint64_t split_target(purc_variant_t handles,
        const char *target, char *target_name)
{
    char *sep = strchr(target, '/');
    if (sep == NULL) {
        goto failed;
    }

    size_t name_len = sep - target;
    if (name_len > PURC_LEN_IDENTIFIER) {
        goto failed;
    }

    strncpy(target_name, target, name_len);
    target_name[name_len] = 0;

    sep++;
    if (sep[0] == 0) {
        goto failed;
    }

    if (isdigit(sep[0])) {
        char *end;
        unsigned long long ull = strtoull(sep, &end, 10);
        if (*end == 0)
            return (uint64_t)ull;
    }
    else {
        /* retrieve the handle from handles */
        purc_variant_t v = purc_variant_object_get_by_ckey(handles, target);
        uint64_t handle;

        if (v && purc_variant_cast_to_ulongint(v, &handle, false))
            return handle;
    }

failed:
    target_name[0] = 0; /* target_name is an empty string when failed */
    return 0;
}

static int transfer_target_info(struct client_info *info,
        const char *source, uint64_t *target_value)
{
    int type = -1;
    char target_name[PURC_LEN_IDENTIFIER + 1];

    *target_value = split_target(info->handles, source, target_name);
    if (strcmp(target_name, "session") == 0) {
        type = PCRDR_MSG_TARGET_SESSION;
    }
    else if (strcmp(target_name, "workspace") == 0) {
        type = PCRDR_MSG_TARGET_WORKSPACE;
    }
    else if (strcmp(target_name, "plainwindow") == 0) {
        type = PCRDR_MSG_TARGET_PLAINWINDOW;
    }
    else if (strcmp(target_name, "page") == 0) {
        type = PCRDR_MSG_TARGET_PAGE;
    }
    else if (strcmp(target_name, "dom") == 0) {
        type = PCRDR_MSG_TARGET_DOM;
    }

    return type;
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

            uint64_t handle;
            if (isdigit(value[0])) {
                handle = (uint64_t)atoi(value);
            }
            else {
                /* retrieve the handle from handles */
                purc_variant_t v;
                v = purc_variant_object_get_by_ckey(info->handles, element);
                if (!purc_variant_cast_to_ulongint(v, &handle, false)) {
                    value = NULL;
                    goto failed;
                }
            }

            sprintf(info->buff, "%llx", (long long)handle);
            value = info->buff;
        }
    }

failed:
    return value;
}

static int issue_operation(pcrdr_conn* conn, purc_variant_t op);

static int issue_next_batch_operation(pcrdr_conn* conn)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    LOG_INFO("batchOps: %u/%u\n", (unsigned)info->issued_ops, (unsigned)info->nr_ops);
    if (info->issued_ops < info->nr_ops) {
        purc_variant_t op;
        op = purc_variant_array_get(info->batchOps, info->issued_ops);
        info->issued_ops++;

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

    return 0;
}

static inline int queue_operations(pcrdr_conn* conn, purc_variant_t op)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    if (info->batchOps == PURC_VARIANT_INVALID) {
        info->batchOps = purc_variant_make_array_0();
    }

    if (purc_variant_is_array(op)) {
        size_t sz;
        purc_variant_array_size(op, &sz);
        for (size_t i = 0; i < sz; i++) {
            purc_variant_t v = purc_variant_array_get(op, i);
            purc_variant_array_append(info->batchOps, v);
        }
    }
    else {
        purc_variant_array_append(info->batchOps, op);
    }

    purc_variant_array_size(info->batchOps, &info->nr_ops);
    if (pcrdr_conn_pending_requests_count(conn) == 0) {
        issue_next_batch_operation(conn);
    }

    return 0;
}

static purc_variant_t make_result_key(purc_variant_t op, const char *prefix)
{
    const char *str;
    purc_variant_t v;
    size_t sz;
    if (!(v = purc_variant_object_get_by_ckey(op, "resultKey")) ||
            !(str = purc_variant_get_string_const_ex(v, &sz)) ||
            sz == 0) {
        return PURC_VARIANT_INVALID;
    }

    char buff[sz + strlen(prefix) + 1];
    strcpy(buff, prefix);
    strcat(buff, str);
    return purc_variant_make_string(buff, false);
}

static int plainwin_created_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    purc_variant_t result_key = (purc_variant_t)context;
    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        goto done;
    }

    LOG_INFO("Got a response for request (%s) to create plainwin (%s): %d\n",
            purc_variant_get_string_const(response_msg->requestId),
            purc_variant_get_string_const(result_key),
            response_msg->retCode);

    if (response_msg->retCode == PCRDR_SC_OK) {
        info->nr_windows_created++;

        purc_variant_t handle;
        handle = purc_variant_make_ulongint(response_msg->resultValue);
        purc_variant_object_set(info->handles, result_key, handle);

        issue_next_batch_operation(conn);
    }
    else {
        LOG_ERROR("failed to create the plainwin: %s\n",
            purc_variant_get_string_const(result_key));
    }

done:
    purc_variant_unref(result_key);
    return 0;
}

static int
create_plainwin(pcrdr_conn* conn, const char *op_name, purc_variant_t op)
{
    pcrdr_msg *msg = NULL;
    purc_variant_t data = PURC_VARIANT_INVALID;
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    purc_variant_t result_key = make_result_key(op, "plainwindow/");
    if (result_key == PURC_VARIANT_INVALID) {
        LOG_ERROR("No valid `resultKey` defined for %s\n", op_name);
        goto failed;
    }

    if (purc_variant_object_get(info->handles, result_key)) {
        LOG_ERROR("Duplicate `resultKey`\n");
        goto failed;
    }

    msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_WORKSPACE, 0,
            PCRDR_OPERATION_CREATEPLAINWINDOW, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    if (msg == NULL) {
        LOG_ERROR("Failed to make request message for %s\n", op_name);
        goto failed;
    }

    purc_variant_t tmp;
    if ((tmp = purc_variant_object_get_by_ckey(op, "element"))) {

        const char *str = purc_variant_get_string_const(tmp);
        if (str == NULL) {
            LOG_ERROR("Bad window group type: %s\n",
                    purc_variant_typename(purc_variant_get_type(tmp)));
            goto failed;
        }

        const char *value;
        char type[PURC_LEN_IDENTIFIER + 1];
        value = split_element(str, type);
        if (value == NULL) {
            LOG_ERROR("Bad window group value: %s\n", str);
            goto failed;
        }

        if (strcmp(type, "id")) {
            LOG_ERROR("Bad window group type: %s\n", type);
            goto failed;
        }

        msg->elementType = PCRDR_MSG_ELEMENT_TYPE_ID;
        msg->elementValue = purc_variant_make_string(value, false);
    }

    data = purc_variant_make_object_0();
    if ((tmp = purc_variant_object_get_by_ckey(op, "name"))) {
        purc_variant_object_set_by_static_ckey(data, "name", tmp);
    }
    else {
        static unsigned nr_wins = 0;
        char name[128];
        sprintf(name, "the-plain-window-%u", nr_wins++);

        tmp = purc_variant_make_string(name, false);
        purc_variant_object_set_by_static_ckey(data, "name", tmp);
        purc_variant_unref(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(op, "class"))) {
        purc_variant_object_set_by_static_ckey(data, "class", tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(op, "title"))) {
        purc_variant_object_set_by_static_ckey(data, "title", tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(op, "layoutStyle"))) {
        purc_variant_object_set_by_static_ckey(data, "layoutStyle", tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(op, "toolkitStyle"))) {
        purc_variant_object_set_by_static_ckey(data, "toolkitStyle", tmp);
    }

    msg->dataType = PCRDR_MSG_DATA_TYPE_JSON;
    msg->data = data;

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, purc_variant_ref(result_key),
                plainwin_created_handler) < 0) {
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` for window %s sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation),
            purc_variant_get_string_const(result_key));
    pcrdr_release_message(msg);
    return 0;

failed:

    if (result_key)
        purc_variant_unref(result_key);

    if (msg) {
        pcrdr_release_message(msg);
    }
    else if (data) {
        purc_variant_unref(data);
    }

    return -1;
}

static int plainwin_page_updated_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    //struct client_info *info = pcrdr_conn_get_user_data(conn);
    //assert(info);

    purc_variant_t result_key = (purc_variant_t)context;
    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        goto done;
    }

    LOG_INFO("Got a response for request (%s) to update window/page (%s)\n",
            purc_variant_get_string_const(response_msg->requestId),
            purc_variant_get_string_const(result_key));

    if (response_msg->retCode != PCRDR_SC_OK) {
        LOG_ERROR("failed to update a window/page (%s): %d\n",
            purc_variant_get_string_const(result_key),
            response_msg->retCode);
        issue_next_batch_operation(conn);
    }

done:
    purc_variant_unref(result_key);
    return 0;
}

static int
update_plainwin(pcrdr_conn* conn, const char *op_name, purc_variant_t op)
{
    pcrdr_msg *msg;
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    const char *element = NULL;
    purc_variant_t trace_key;
    if ((trace_key = purc_variant_object_get_by_ckey(op, "element"))) {
        element = purc_variant_get_string_const(trace_key);
    }

    if (element == NULL) {
        LOG_ERROR("No plainwin given: %s\n", op_name);
        goto failed;
    }

    uint64_t value;
    char element_name[PURC_LEN_IDENTIFIER + 1];
    value = split_target(info->handles, element, element_name);
    if (strcmp(element_name, "plainwindow")) {
        LOG_ERROR("Bad plainwin given: %s\n", element);
        goto failed;
    }
    char handle[32];
    sprintf(handle, "%llx", (long long)value);

    const char *property = NULL;
    purc_variant_t tmp;
    if ((tmp = purc_variant_object_get_by_ckey(op, "property"))) {
        property = purc_variant_get_string_const(tmp);
    }

    if (property == NULL) {
        LOG_ERROR("No property given: %s\n", op_name);
        goto failed;
    }

    purc_variant_t prop_value;
    if (!(prop_value = purc_variant_object_get_by_ckey(op, "value"))) {
        LOG_ERROR("No property value given: %s\n", op_name);
        goto failed;
    }

    msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_WORKSPACE, 0,
            PCRDR_OPERATION_UPDATEPLAINWINDOW, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLE, handle, property,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    if (msg == NULL) {
        LOG_ERROR("Failed to make request message for %s\n", op_name);
        goto failed;
    }

    if (purc_variant_get_string_const(prop_value)) {
        msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    }
    else {
        msg->dataType = PCRDR_MSG_DATA_TYPE_JSON;
    }
    msg->data = purc_variant_ref(prop_value);

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, purc_variant_ref(trace_key),
                plainwin_page_updated_handler) < 0) {
        LOG_ERROR("Failed to send request message\n");
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` for window %s sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation), element);
    pcrdr_release_message(msg);
    return 0;

failed:
    if (msg) {
        pcrdr_release_message(msg);
    }

    return -1;
}

static int plainwin_destroyed_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    purc_variant_t result_key = (purc_variant_t)context;
    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        goto done;
    }

    LOG_INFO("Got a response for request (%s) to destroy plainwin (%s): %d\n",
            purc_variant_get_string_const(response_msg->requestId),
            purc_variant_get_string_const(result_key),
            response_msg->retCode);

    if (response_msg->retCode == PCRDR_SC_OK) {
        if (!purc_variant_object_remove(info->handles, result_key, true)) {
            LOG_ERROR("Failed to remove the plainwin handle: %s\n",
                purc_variant_get_string_const(result_key));
        }

        info->nr_windows_created--;
        assert(info->nr_windows_created >= 0);
        issue_next_batch_operation(conn);
    }
    else {
        LOG_ERROR("failed to destroy a plain window\n");
    }

done:
    purc_variant_unref(result_key);
    return 0;
}

static int
destroy_plainwin(pcrdr_conn* conn, const char *op_name, purc_variant_t op)
{
    pcrdr_msg *msg;
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    const char *element = NULL;
    purc_variant_t result_key;
    if ((result_key = purc_variant_object_get_by_ckey(op, "element"))) {
        element = purc_variant_get_string_const(result_key);
    }

    if (element == NULL) {
        LOG_ERROR("No window given for %s\n", op_name);
        goto failed;
    }

    uint64_t value;
    char element_name[PURC_LEN_IDENTIFIER + 1];
    value = split_target(info->handles, element, element_name);
    if (strcmp(element_name, "plainwindow")) {
        LOG_ERROR("Bad window given for %s: %s\n", op_name, element);
        goto failed;
    }

    char handle[32];
    sprintf(handle, "%llx", (long long)value);
    msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_WORKSPACE, 0,
            PCRDR_OPERATION_DESTROYPLAINWINDOW, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLE, handle, NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    if (msg == NULL) {
        LOG_ERROR("Failed to make request message for %s\n", op_name);
        goto failed;
    }

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, purc_variant_ref(result_key),
                plainwin_destroyed_handler) < 0) {
        LOG_ERROR("Failed to send request message\n");
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` for window %s sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation), element);
    pcrdr_release_message(msg);
    return 0;

failed:
    if (msg) {
        pcrdr_release_message(msg);
    }

    return -1;
}

static int page_created_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    purc_variant_t result_key = (purc_variant_t)context;
    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        goto done;
    }

    LOG_INFO("Got a response for request (%s) to create page (%s): %d\n",
            purc_variant_get_string_const(response_msg->requestId),
            purc_variant_get_string_const(result_key),
            response_msg->retCode);

    if (response_msg->retCode == PCRDR_SC_OK) {
        info->nr_pages_created++;

        purc_variant_t handle;
        handle = purc_variant_make_ulongint(response_msg->resultValue);
        purc_variant_object_set(info->handles, result_key, handle);

        issue_next_batch_operation(conn);
    }
    else {
        LOG_ERROR("failed to create the desired page: %s\n",
            purc_variant_get_string_const(result_key));
    }

done:
    purc_variant_unref(result_key);
    return 0;
}

static int
create_page(pcrdr_conn* conn, const char *op_name, purc_variant_t op)
{
    pcrdr_msg *msg;
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    purc_variant_t result_key = make_result_key(op, "page/");
    if (result_key == PURC_VARIANT_INVALID) {
        LOG_ERROR("No valid `resultKey` defined for %s\n", op_name);
        goto failed;
    }

    if (purc_variant_object_get(info->handles, result_key)) {
        LOG_ERROR("Duplicated `resultKey`\n");
        goto failed;
    }

    msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_WORKSPACE, 0,
            PCRDR_OPERATION_CREATEPAGE, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    if (msg == NULL) {
        LOG_ERROR("Failed to make request message for %s\n", op_name);
        goto failed;
    }

    purc_variant_t tmp;
    if ((tmp = purc_variant_object_get_by_ckey(op, "element"))) {

        const char *str = purc_variant_get_string_const(tmp);
        if (str == NULL) {
            LOG_ERROR("Bad group value type: %s\n",
                    purc_variant_typename(purc_variant_get_type(tmp)));
            goto failed;
        }

        const char *value;
        char type[PURC_LEN_IDENTIFIER + 1];
        value = split_element(str, type);
        if (value == NULL) {
            LOG_ERROR("Bad page group value: %s\n", str);
            goto failed;
        }

        if (strcmp(type, "id")) {
            LOG_ERROR("Bad page group type: %s\n", type);
            goto failed;
        }

        msg->elementType = PCRDR_MSG_ELEMENT_TYPE_ID;
        msg->elementValue = purc_variant_make_string(value, false);
    }

    purc_variant_t data = purc_variant_make_object_0();
    if ((tmp = purc_variant_object_get_by_ckey(op, "name"))) {
        purc_variant_object_set_by_static_ckey(data, "name", tmp);
    }
    else {
        LOG_ERROR("No page name defined for %s\n", op_name);
        goto failed;
    }

    if ((tmp = purc_variant_object_get_by_ckey(op, "class"))) {
        purc_variant_object_set_by_static_ckey(data, "class", tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(op, "title"))) {
        purc_variant_object_set_by_static_ckey(data, "title", tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(op, "layoutStyle"))) {
        purc_variant_object_set_by_static_ckey(data, "layoutStyle", tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(op, "toolkitStyle"))) {
        purc_variant_object_set_by_static_ckey(data, "toolkitStyle", tmp);
    }

    msg->dataType = PCRDR_MSG_DATA_TYPE_JSON;
    msg->data = data;

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, purc_variant_ref(result_key),
                page_created_handler) < 0) {
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` for page %s sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation),
            purc_variant_get_string_const(result_key));
    pcrdr_release_message(msg);
    return 0;

failed:
    if (result_key)
        purc_variant_unref(result_key);

    if (msg) {
        pcrdr_release_message(msg);
    }
    else if (data) {
        purc_variant_unref(data);
    }

    return -1;
}

static int
update_page(pcrdr_conn* conn, const char *op_name, purc_variant_t op)
{
    pcrdr_msg *msg;
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    const char *element = NULL;
    purc_variant_t trace_key;
    if ((trace_key = purc_variant_object_get_by_ckey(op, "element"))) {
        element = purc_variant_get_string_const(trace_key);
    }

    if (element == NULL) {
        LOG_ERROR("No page given in %s\n", op_name);
        goto failed;
    }

    uint64_t value;
    char element_name[PURC_LEN_IDENTIFIER + 1];
    value = split_target(info->handles, element, element_name);
    if (strcmp(element_name, "page")) {
        LOG_ERROR("Bad page given: %s\n", element);
        goto failed;
    }
    char handle[32];
    sprintf(handle, "%llx", (long long)value);

    const char *property = NULL;
    purc_variant_t tmp;
    if ((tmp = purc_variant_object_get_by_ckey(op, "property"))) {
        property = purc_variant_get_string_const(tmp);
    }

    if (property == NULL) {
        LOG_ERROR("No property given: %s\n", op_name);
        goto failed;
    }

    purc_variant_t prop_value;
    if (!(prop_value = purc_variant_object_get_by_ckey(op, "value"))) {
        LOG_ERROR("No property value given: %s\n", op_name);
        goto failed;
    }

    msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_WORKSPACE, 0,
            PCRDR_OPERATION_UPDATEPAGE, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLE, handle, property,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    if (msg == NULL) {
        LOG_ERROR("Failed to make request message for %s\n", op_name);
        goto failed;
    }

    if (purc_variant_get_string_const(prop_value)) {
        msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    }
    else {
        msg->dataType = PCRDR_MSG_DATA_TYPE_JSON;
    }
    msg->data = purc_variant_ref(prop_value);

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, purc_variant_ref(trace_key),
                plainwin_page_updated_handler) < 0) {
        LOG_ERROR("Failed to send request message for %s\n", op_name);
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` for window %s sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation), element);
    pcrdr_release_message(msg);
    return 0;

failed:
    if (msg) {
        pcrdr_release_message(msg);
    }

    return -1;
}

static int page_destroyed_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    purc_variant_t result_key = (purc_variant_t)context;
    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        goto done;
    }

    LOG_INFO("Got a response for request (%s) to destroy page (%s): %d\n",
            purc_variant_get_string_const(response_msg->requestId),
            purc_variant_get_string_const(result_key),
            response_msg->retCode);

    if (response_msg->retCode == PCRDR_SC_OK) {
        if (!purc_variant_object_remove(info->handles, result_key, true)) {
            LOG_ERROR("Failed to remove the page handle: %s\n",
                purc_variant_get_string_const(result_key));
        }

        info->nr_pages_created--;
        assert(info->nr_pages_created >= 0);

        issue_next_batch_operation(conn);
    }
    else {
        LOG_ERROR("failed to destroy a plain window\n");
    }

done:
    purc_variant_unref(result_key);
    return 0;
}

static int
destroy_page(pcrdr_conn* conn, const char *op_name, purc_variant_t op)
{
    pcrdr_msg *msg;
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    const char *element = NULL;
    purc_variant_t result_key;
    if ((result_key = purc_variant_object_get_by_ckey(op, "element"))) {
        element = purc_variant_get_string_const(result_key);
    }

    if (element == NULL) {
        LOG_ERROR("No page given in %s\n", op_name);
        goto failed;
    }

    uint64_t value;
    char element_name[PURC_LEN_IDENTIFIER + 1];
    value = split_target(info->handles, element, element_name);
    if (strcmp(element_name, "page")) {
        LOG_ERROR("Bad page given: %s\n", element);
        goto failed;
    }

    char handle[32];
    sprintf(handle, "%llx", (long long)value);
    msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_WORKSPACE, 0,
            PCRDR_OPERATION_DESTROYPAGE, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_HANDLE, handle, NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    if (msg == NULL) {
        LOG_ERROR("Failed to make request message for %s\n", op_name);
        goto failed;
    }

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, purc_variant_ref(result_key),
                page_destroyed_handler) < 0) {
        LOG_ERROR("Failed to send request message for %s\n", op_name);
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` for window %s sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation), element);
    pcrdr_release_message(msg);
    return 0;

failed:
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

    purc_variant_t result_key = (purc_variant_t)context;
    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        goto done;
    }

    LOG_INFO("Got a response for request (%s) to load content (%s): %d\n",
            purc_variant_get_string_const(response_msg->requestId),
            purc_variant_get_string_const(result_key),
            response_msg->retCode);

    if (response_msg->retCode == PCRDR_SC_OK) {
        if (!purc_variant_object_remove(info->doc_contents, result_key, true)) {
            LOG_ERROR("Failed to remove the document content for %s\n",
                purc_variant_get_string_const(result_key));
            goto done;
        }

        if (!purc_variant_object_remove(info->doc_wrotten_len,
                    result_key, true)) {
            LOG_ERROR("Failed to remove the document wrotten length for %s\n",
                purc_variant_get_string_const(result_key));
            goto done;
        }

        purc_variant_t handle;
        handle = purc_variant_make_ulongint(response_msg->resultValue);
        purc_variant_object_set(info->handles, result_key, handle);
        purc_variant_unref(handle);

        issue_next_batch_operation(conn);
    }
    else {
        LOG_ERROR("failed to load document\n");
        // info->running = false;
    }

done:
    purc_variant_unref(result_key);
    return 0;
}

static int write_more_document(pcrdr_conn* conn, purc_variant_t result_key);

static int wrotten_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    assert(info);

    purc_variant_t result_key = (purc_variant_t)context;
    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        goto done;
    }

    LOG_INFO("Got a response for request (%s) to write content (%s): %d\n",
            purc_variant_get_string_const(response_msg->requestId),
            purc_variant_get_string_const(result_key),
            response_msg->retCode);

    if (response_msg->retCode == PCRDR_SC_OK) {
        uint64_t len_wrotten, len_content;

        purc_variant_t tmp;
        tmp = purc_variant_object_get(info->doc_wrotten_len, result_key);
        if (tmp == NULL ||
                !purc_variant_cast_to_ulongint(tmp, &len_wrotten, false)) {
            LOG_ERROR("No document wrotten length for %s\n",
                purc_variant_get_string_const(result_key));
            goto done;
        }

        tmp = purc_variant_object_get(info->doc_contents, result_key);
        if (tmp == NULL ||
                !purc_variant_get_string_const_ex(tmp, &len_content)) {
            LOG_ERROR("No document contents for %s\n",
                purc_variant_get_string_const(result_key));
            goto done;
        }

        if (len_wrotten == len_content) {
            purc_variant_t handle;
            handle = purc_variant_make_ulongint(response_msg->resultValue);
            purc_variant_object_set(info->handles, result_key, handle);
            purc_variant_unref(handle);

            if (!purc_variant_object_remove(info->doc_contents,
                        result_key, true)) {
                LOG_ERROR("Failed to remove the document content for %s\n",
                    purc_variant_get_string_const(result_key));
                goto done;
            }

            if (!purc_variant_object_remove(info->doc_wrotten_len,
                        result_key, true)) {
                LOG_ERROR("Failed to remove the document wrotten length for %s\n",
                    purc_variant_get_string_const(result_key));
                goto done;
            }

            issue_next_batch_operation(conn);
        }
        else {
            write_more_document(conn, purc_variant_ref(result_key));
        }
    }
    else {
        LOG_ERROR("failed to write content\n");
        // info->running = false;
    }

done:
    purc_variant_unref(result_key);
    return 0;
}

static int write_more_document(pcrdr_conn* conn, purc_variant_t result_key)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);
    const char *doc_content;
    uint64_t len_wrotten, len_content, win_handle;

    purc_variant_t tmp;
    tmp = purc_variant_object_get(info->doc_wrotten_len, result_key);
    if (tmp == NULL ||
            !purc_variant_cast_to_ulongint(tmp, &len_wrotten, false)) {
        LOG_ERROR("No document wrotten length for %s\n",
            purc_variant_get_string_const(result_key));
        goto failed;
    }

    tmp = purc_variant_object_get(info->doc_contents, result_key);
    if (tmp == NULL || !(doc_content =
                purc_variant_get_string_const_ex(tmp, &len_content))) {
        LOG_ERROR("No document contents for %s\n",
            purc_variant_get_string_const(result_key));
        goto failed;
    }

    tmp = purc_variant_object_get(info->handles, result_key);
    if (tmp == NULL ||
            !purc_variant_cast_to_ulongint(tmp, &win_handle, false)) {
        LOG_ERROR("No window/page handle for %s\n",
            purc_variant_get_string_const(result_key));
        goto failed;
    }

    pcrdr_msg *msg = NULL;
    purc_variant_t data = PURC_VARIANT_INVALID;

    pcrdr_response_handler handler;
    size_t len_to_write = 0;
    if (len_wrotten + DEF_LEN_ONE_WRITE > len_content) {
        // writeEnd
        msg = pcrdr_make_request_message(info->last_target,
                win_handle,
                PCRDR_OPERATION_WRITEEND, NULL, NULL,
                PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
                PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);


        tmp = purc_variant_make_ulongint(len_content);
        purc_variant_object_set(info->doc_wrotten_len, result_key, tmp);
        purc_variant_unref(tmp);

        data = purc_variant_make_string_static(doc_content + len_wrotten,
                false);
        handler = loaded_handler;
    }
    else {
        // writeMore
        msg = pcrdr_make_request_message(info->last_target,
                win_handle,
                PCRDR_OPERATION_WRITEMORE, NULL, NULL,
                PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
                PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);

        const char *start = doc_content + len_wrotten;
        const char *end;
        pcutils_string_check_utf8_len(start, DEF_LEN_ONE_WRITE, NULL, &end);
        if (end > start) {

            len_to_write = end - start;

            len_wrotten += len_to_write;
            tmp = purc_variant_make_ulongint(len_wrotten);
            purc_variant_object_set(info->doc_wrotten_len, result_key, tmp);
            purc_variant_unref(tmp);

            data = purc_variant_make_string_static(start, false);
        }
        else {
            LOG_WARN("no valid character for window %s\n",
                purc_variant_get_string_const(result_key));
            goto failed;
        }
        handler = wrotten_handler;
    }

    if (msg == NULL || data == PURC_VARIANT_INVALID) {
        goto failed;
    }

    msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    msg->data = data;
    msg->textLen = len_to_write;
    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, purc_variant_ref(result_key),
                handler) < 0) {
        LOG_ERROR("Failed to send request message for %s\n",
                purc_variant_get_string_const(result_key));
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` for window %s sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation),
            purc_variant_get_string_const(result_key));
    pcrdr_release_message(msg);
    return 0;

failed:

    if (msg) {
        pcrdr_release_message(msg);
    }
    else if (data) {
        purc_variant_unref(data);
    }

    return -1;
}

static int load_or_write_document(pcrdr_conn* conn, purc_variant_t op)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    purc_variant_t result_key;
    result_key = make_result_key(op, "dom/");
    if (result_key == PURC_VARIANT_INVALID) {
        LOG_ERROR("No valid `resultKey` defined\n");
        goto failed;
    }

    const char *target = NULL;
    purc_variant_t tmp;
    if ((tmp = purc_variant_object_get_by_ckey(op, "target"))) {
        target = purc_variant_get_string_const(tmp);
    }
    if (target == NULL) {
        LOG_ERROR("No target defined\n");
        goto failed;
    }

    char target_name[PURC_LEN_IDENTIFIER + 1];
    uint64_t win_handle = 0;
    win_handle = split_target(info->handles, target, target_name);
    if (strcmp(target_name, "plainwindow") == 0) {
        info->last_target = PCRDR_MSG_TARGET_PLAINWINDOW;
    }
    else if (strcmp(target_name, "page") == 0) {
        info->last_target = PCRDR_MSG_TARGET_PAGE;
    }
    else {
        LOG_ERROR("Bad target name: %s\n", target);
        goto failed;
    }

    const char *doc_content = NULL;
    size_t len_content;
    tmp = purc_variant_object_get(info->doc_contents, result_key);
    if (tmp) {
        doc_content = purc_variant_get_string_const_ex(tmp, &len_content);
    }

    if (doc_content == NULL) {
        const char *file;
        if ((tmp = purc_variant_object_get_by_ckey(op, "content"))) {
            file = purc_variant_get_string_const(tmp);
        }

        char *loaded;
        if (file) {
            loaded = load_file_content(file, &len_content);
        }

        if (loaded == NULL) {
            LOG_ERROR("Failed to load document content from %s\n", file);
            goto failed;
        }

        doc_content = loaded;
        tmp = purc_variant_make_string_reuse_buff(loaded, len_content, true);
        purc_variant_object_set(info->doc_contents, result_key, tmp);
        purc_variant_unref(tmp);

        tmp = purc_variant_make_ulongint(0);
        purc_variant_object_set(info->doc_wrotten_len, result_key, tmp);
        purc_variant_unref(tmp);
    }

    pcrdr_msg *msg = NULL;
    purc_variant_t data = PURC_VARIANT_INVALID;

    size_t len_wrotten;
    size_t len_to_write = 0;
    pcrdr_response_handler handler;
    if (len_content > DEF_LEN_ONE_WRITE) {
        // use writeBegin
        msg = pcrdr_make_request_message(info->last_target,
                win_handle,
                PCRDR_OPERATION_WRITEBEGIN, NULL, NULL,
                PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
                PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);

        const char *start = doc_content;
        const char *end;
        pcutils_string_check_utf8_len(start, DEF_LEN_ONE_WRITE, NULL, &end);
        if (end > start) {
            len_to_write = end - start;

            data = purc_variant_make_string_static(start, false);
            len_wrotten = len_to_write;
        }
        else {
            LOG_ERROR("No valid character in document content\n");
            goto failed;
        }

        handler = wrotten_handler;
    }
    else {
        // use load
        msg = pcrdr_make_request_message(info->last_target,
                win_handle,
                PCRDR_OPERATION_LOAD, NULL, NULL,
                PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
                PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);

        data = purc_variant_make_string_static(doc_content, false);
        len_wrotten = len_content;
        handler = loaded_handler;
    }

    if (msg == NULL || data == PURC_VARIANT_INVALID) {
        LOG_ERROR("Failed to initialize the request message\n");
        goto failed;
    }

    /* We use the resultKey (`dom/xxx`) to store
     * the window/page handle temporarily */
    tmp = purc_variant_make_ulongint(win_handle);
    purc_variant_object_set(info->handles, result_key, tmp);
    purc_variant_unref(tmp);

    tmp = purc_variant_make_ulongint(len_wrotten);
    purc_variant_object_set(info->doc_wrotten_len, result_key, tmp);
    purc_variant_unref(tmp);

    msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    msg->data = data;
    msg->textLen = len_to_write;

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, purc_variant_ref(result_key),
                handler) < 0) {
        purc_variant_ref(result_key);
        LOG_ERROR("Failed to send the request message\n");
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` for window %s sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation),
            purc_variant_get_string_const(result_key));
    pcrdr_release_message(msg);
    return 0;

failed:
    if (result_key) {
        purc_variant_ref(result_key);
    }

    if (msg) {
        pcrdr_release_message(msg);
    }
    else if (data) {
        purc_variant_unref(data);
    }

    return -1;
}

static pcrdr_msg *make_change_message(struct client_info *info,
        int op_id, const char *operation, purc_variant_t op,
        uint64_t dom_handle)
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
            PCRDR_MSG_TARGET_DOM, dom_handle,
            operation, NULL, NULL,
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
    uint64_t dom_handle = (uint64_t)(uintptr_t)context;

    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        return 0;
    }

    LOG_INFO("Got a response for request (%s) to change DOM (%llx): %d\n",
            purc_variant_get_string_const(response_msg->requestId),
            (long long)dom_handle,
            response_msg->retCode);

    if (response_msg->retCode == PCRDR_SC_OK) {
        issue_next_batch_operation(conn);
    }
    else {
        LOG_ERROR("failed to change document\n");
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
    uint64_t dom_handle;
    dom_handle = split_target(info->handles, target, target_name);
    if (strcmp(target_name, "dom")) {
        goto failed;
    }

    pcrdr_msg *msg = make_change_message(info, op_id, operation, op,
            dom_handle);
    if (msg == NULL) {
        LOG_ERROR("Failed to make request message\n");
        return -1;
    }

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, (void *)(uintptr_t)dom_handle,
                changed_handler) < 0) {
        LOG_ERROR("Failed to send request message\n");
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` (%s) for DOM %llx sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation),
            msg->property?purc_variant_get_string_const(msg->property):"N/A",
            (long long)dom_handle);
    pcrdr_release_message(msg);
    return 0;

failed:
    pcrdr_release_message(msg);
    return -1;
}

/* Common handler for `setPageGroups`, `addPageGroups`, and `removePageGroup`.
 */
static int page_group_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    uint64_t ws_handle = (uint64_t)(uintptr_t)context;

    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        return 0;
    }

    LOG_INFO("Got a response for request (%s) to change workspace (%llx): %d\n",
            purc_variant_get_string_const(response_msg->requestId),
            (long long)ws_handle,
            response_msg->retCode);

    if (response_msg->retCode == PCRDR_SC_OK) {
        issue_next_batch_operation(conn);
    }
    else {
        LOG_ERROR("failed to change workspace\n");
    }

    return 0;
}

static int
set_page_groups(pcrdr_conn* conn, const char *op_name, purc_variant_t op)
{
    pcrdr_msg *msg;
    //struct client_info *info = pcrdr_conn_get_user_data(conn);

    purc_variant_t data = load_operation_content(op);
    if (data == PURC_VARIANT_INVALID) {
        goto failed;
    }

    msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_WORKSPACE, 0,
            PCRDR_OPERATION_SETPAGEGROUPS, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    if (msg == NULL) {
        LOG_ERROR("Failed to make request message\n");
        goto failed;
    }

    msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    msg->data = data;

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, NULL,
                page_group_handler) < 0) {
        LOG_ERROR("Failed to send request message (%s)\n", op_name);
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` for workspace/0 sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation));
    pcrdr_release_message(msg);
    return 0;

failed:

    if (msg) {
        pcrdr_release_message(msg);
    }
    else if (data) {
        purc_variant_unref(data);
    }

    return -1;
}

static int
add_page_groups(pcrdr_conn* conn, const char *op_name, purc_variant_t op)
{
    pcrdr_msg *msg;
    //struct client_info *info = pcrdr_conn_get_user_data(conn);

    purc_variant_t data = load_operation_content(op);
    if (data == PURC_VARIANT_INVALID) {
        goto failed;
    }

    msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_WORKSPACE, 0,
            PCRDR_OPERATION_ADDPAGEGROUPS, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    if (msg == NULL) {
        LOG_ERROR("Failed to make request message\n");
        goto failed;
    }

    msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    msg->data = data;

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, NULL,
                page_group_handler) < 0) {
        LOG_ERROR("Failed to send request message (%s)\n", op_name);
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` for workspace/0 sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation));
    pcrdr_release_message(msg);
    return 0;

failed:

    if (msg) {
        pcrdr_release_message(msg);
    }
    else if (data) {
        purc_variant_unref(data);
    }

    return -1;
}

static int
remove_page_group(pcrdr_conn* conn, const char *op_name, purc_variant_t op)
{
    pcrdr_msg *msg;
    //struct client_info *info = pcrdr_conn_get_user_data(conn);

    const char *element = NULL;
    purc_variant_t tmp;
    if ((tmp = purc_variant_object_get_by_ckey(op, "element"))) {
        element = purc_variant_get_string_const(tmp);
    }

    if (element == NULL) {
        LOG_ERROR("No group identifier given: %s\n", op_name);
        goto failed;
    }

    char type[PURC_LEN_IDENTIFIER + 1];
    const char *gid = split_element(element, type);
    if (gid == NULL) {
        LOG_ERROR("Invalid element value for %s\n", op_name);
        goto failed;
    }

    if (strcmp(type, "id")) {
        LOG_ERROR("Must be an identifier for %s\n", op_name);
        goto failed;
    }

    msg = pcrdr_make_request_message(PCRDR_MSG_TARGET_WORKSPACE, 0,
            PCRDR_OPERATION_REMOVEPAGEGROUP, NULL, NULL,
            PCRDR_MSG_ELEMENT_TYPE_ID, gid, NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    if (msg == NULL) {
        LOG_ERROR("Failed to make request message for %s\n", op_name);
        goto failed;
    }

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, NULL,
                page_group_handler) < 0) {
        LOG_ERROR("Failed to send request message (%s)\n", op_name);
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` for workspace/0 sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation));
    pcrdr_release_message(msg);
    return 0;

failed:

    if (msg) {
        pcrdr_release_message(msg);
    }

    return -1;
}

static int default_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    if (state == PCRDR_RESPONSE_CANCELLED || response_msg == NULL) {
        return 0;
    }

    LOG_INFO("Got a response for request (%s) on context (%p): %d\n",
            purc_variant_get_string_const(response_msg->requestId),
            context, response_msg->retCode);

    if (response_msg->retCode == PCRDR_SC_OK) {
        issue_next_batch_operation(conn);
    }
    else {
        LOG_ERROR("failed to change workspace\n");
    }

    return 0;
}

static int
get_property(pcrdr_conn* conn, const char *op_name, purc_variant_t op)
{
    pcrdr_msg *msg;
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    purc_variant_t tmp;
    const char *target = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "target"))) {
        target = purc_variant_get_string_const(tmp);
    }

    if (target == NULL) {
        LOG_ERROR("No `target` defined in %s\n", op_name);
        goto failed;
    }

    char target_name[PURC_LEN_IDENTIFIER + 1];
    uint64_t handle;
    handle = split_target(info->handles, target, target_name);

    pcrdr_msg_target target_type;
    if (strcmp(target_name, "session") == 0) {
        target_type = PCRDR_MSG_TARGET_SESSION;
        handle = 0;
    }
    else if (strcmp(target_name, "workspace") == 0) {
        target_type = PCRDR_MSG_TARGET_WORKSPACE;
        handle = 0;
    }
    else if (strcmp(target_name, "plainwindow") == 0) {
        target_type = PCRDR_MSG_TARGET_PLAINWINDOW;
    }
    else if (strcmp(target_name, "page") == 0) {
        target_type = PCRDR_MSG_TARGET_PAGE;
    }
    else if (strcmp(target_name, "dom") == 0) {
        target_type = PCRDR_MSG_TARGET_DOM;
    }
    else {
        LOG_ERROR("Not supported element type: %s\n", target_name);
        goto failed;
    }

    const char *element = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "element"))) {
        element = purc_variant_get_string_const(tmp);
    }

    if (element == NULL) {
        LOG_ERROR("No `element` given in %s\n", op_name);
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
    else if (strcmp(element_type_str, "css") == 0) {
        element_type = PCRDR_MSG_ELEMENT_TYPE_CSS;
    }
    else {
        LOG_ERROR("Not supported element type: %s\n", element_type_str);
        goto failed;
    }

    const char *property = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "property"))) {
        property = purc_variant_get_string_const(tmp);
    }

    if (property == NULL) {
        LOG_ERROR("No `property` given in %s\n", op_name);
        goto failed;
    }

    pcrdr_response_handler handler = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "handler"))) {
        const char *handler_name = purc_variant_get_string_const(tmp);

        if (handler_name) {
            handler = dlsym(info->sample_handle, handler_name);
        }
    }

    if (handler == NULL) {
        LOG_ERROR("No valid `handler` given in %s\n", op_name);
        goto failed;
    }

    msg = pcrdr_make_request_message(target_type, handle,
            PCRDR_OPERATION_GETPROPERTY, NULL, NULL,
            element_type, element_value, property,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    if (msg == NULL) {
        LOG_ERROR("Failed to make request message for %s\n", op_name);
        goto failed;
    }

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, 0,
                handler) < 0) {
        LOG_ERROR("Failed to send request message for %s\n", op_name);
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` for %s.%s sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation), element, property);
    pcrdr_release_message(msg);
    return 0;

failed:
    if (msg) {
        pcrdr_release_message(msg);
    }

    return -1;
}

static int
set_property(pcrdr_conn* conn, const char *op_name, purc_variant_t op)
{
    pcrdr_msg *msg;
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    purc_variant_t tmp;
    const char *target = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "target"))) {
        target = purc_variant_get_string_const(tmp);
    }

    if (target == NULL) {
        LOG_ERROR("No `target` defined in %s\n", op_name);
        goto failed;
    }

    char target_name[PURC_LEN_IDENTIFIER + 1];
    uint64_t handle;
    handle = split_target(info->handles, target, target_name);

    pcrdr_msg_target target_type;
    if (strcmp(target_name, "session") == 0) {
        target_type = PCRDR_MSG_TARGET_SESSION;
        handle = 0;
    }
    else if (strcmp(target_name, "workspace") == 0) {
        target_type = PCRDR_MSG_TARGET_WORKSPACE;
        handle = 0;
    }
    else if (strcmp(target_name, "plainwindow") == 0) {
        target_type = PCRDR_MSG_TARGET_PLAINWINDOW;
    }
    else if (strcmp(target_name, "page") == 0) {
        target_type = PCRDR_MSG_TARGET_PAGE;
    }
    else if (strcmp(target_name, "dom") == 0) {
        target_type = PCRDR_MSG_TARGET_DOM;
    }
    else {
        LOG_ERROR("Not supported element type: %s\n", target_name);
        goto failed;
    }

    const char *element = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "element"))) {
        element = purc_variant_get_string_const(tmp);
    }

    if (element == NULL) {
        LOG_ERROR("No `element` given in %s\n", op_name);
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
    else if (strcmp(element_type_str, "css") == 0) {
        element_type = PCRDR_MSG_ELEMENT_TYPE_CSS;
    }
    else {
        LOG_ERROR("Not supported element type: %s\n", element_type_str);
        goto failed;
    }

    const char *property = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "property"))) {
        property = purc_variant_get_string_const(tmp);
    }

    if (property == NULL) {
        LOG_ERROR("No `property` given in %s\n", op_name);
        goto failed;
    }

    purc_variant_t data;
    if (!(data = purc_variant_object_get_by_ckey(op, "value"))) {
        LOG_ERROR("No `value` given in %s\n", op_name);
        goto failed;
    }

    pcrdr_response_handler handler = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "handler"))) {
        const char *handler_name = purc_variant_get_string_const(tmp);

        if (handler_name) {
            handler = dlsym(info->sample_handle, handler_name);
        }
    }

    msg = pcrdr_make_request_message(target_type, handle,
            PCRDR_OPERATION_SETPROPERTY, NULL, NULL,
            element_type, element_value, property,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    if (msg == NULL) {
        LOG_ERROR("Failed to make request message for %s\n", op_name);
        goto failed;
    }

    msg->dataType = PCRDR_MSG_DATA_TYPE_JSON;
    msg->data = purc_variant_ref(data);

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, 0,
                (handler != NULL) ? handler : default_handler) < 0) {
        LOG_ERROR("Failed to send request message for %s\n", op_name);
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` for %s.%s sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation), element, property);
    pcrdr_release_message(msg);
    return 0;

failed:
    if (msg) {
        pcrdr_release_message(msg);
    }

    return -1;
}

static int
call_method(pcrdr_conn* conn, const char *op_name, purc_variant_t op)
{
    pcrdr_msg *msg;
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    purc_variant_t tmp;
    const char *target = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "target"))) {
        target = purc_variant_get_string_const(tmp);
    }

    if (target == NULL) {
        LOG_ERROR("No `target` defined in %s\n", op_name);
        goto failed;
    }

    char target_name[PURC_LEN_IDENTIFIER + 1];
    uint64_t handle;
    handle = split_target(info->handles, target, target_name);

    pcrdr_msg_target target_type;
    if (strcmp(target_name, "session") == 0) {
        target_type = PCRDR_MSG_TARGET_SESSION;
        handle = 0;
    }
    else if (strcmp(target_name, "workspace") == 0) {
        target_type = PCRDR_MSG_TARGET_WORKSPACE;
        handle = 0;
    }
    else if (strcmp(target_name, "plainwindow") == 0) {
        target_type = PCRDR_MSG_TARGET_PLAINWINDOW;
    }
    else if (strcmp(target_name, "page") == 0) {
        target_type = PCRDR_MSG_TARGET_PAGE;
    }
    else if (strcmp(target_name, "dom") == 0) {
        target_type = PCRDR_MSG_TARGET_DOM;
    }
    else {
        LOG_ERROR("Not supported target: %s\n", target_name);
        goto failed;
    }

    const char *element = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "element"))) {
        element = purc_variant_get_string_const(tmp);
    }

    if (element == NULL) {
        LOG_ERROR("No `element` given in %s\n", op_name);
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
    else if (strcmp(element_type_str, "css") == 0) {
        element_type = PCRDR_MSG_ELEMENT_TYPE_CSS;
    }
    else {
        LOG_ERROR("Not supported element type: %s\n", element_type_str);
        goto failed;
    }

    purc_variant_t data = purc_variant_make_object_0();
    if ((tmp = purc_variant_object_get_by_ckey(op, "method")) &&
            purc_variant_get_string_const(tmp)) {
        purc_variant_object_set_by_static_ckey(data, "method", tmp);
    }
    else {
        LOG_ERROR("Not `method` specified for %s\n", op_name);
        goto failed;
    }

    if ((tmp = purc_variant_object_get_by_ckey(op, "arg")) &&
            purc_variant_get_string_const(tmp)) {
        purc_variant_object_set_by_static_ckey(data, "arg", tmp);
    }

    pcrdr_response_handler handler = NULL;
    if ((tmp = purc_variant_object_get_by_ckey(op, "handler"))) {
        const char *handler_name = purc_variant_get_string_const(tmp);

        if (handler_name) {
            handler = dlsym(info->sample_handle, handler_name);
        }
    }

    if (handler == NULL) {
        LOG_ERROR("Not valid `handler` specified for %s\n", op_name);
        goto failed;
    }

    msg = pcrdr_make_request_message(target_type, handle,
            PCRDR_OPERATION_CALLMETHOD, NULL, NULL,
            element_type, element_value, NULL,
            PCRDR_MSG_DATA_TYPE_VOID, NULL, 0);
    if (msg == NULL) {
        LOG_ERROR("Failed to make request message for %s\n", op_name);
        goto failed;
    }

    msg->dataType = PCRDR_MSG_DATA_TYPE_JSON;
    msg->data = data;

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, 0, handler) < 0) {
        LOG_ERROR("Failed to send request message for %s\n", op_name);
        goto failed;
    }

    LOG_INFO("Request (%s) `%s` for %s sent\n",
            purc_variant_get_string_const(msg->requestId),
            purc_variant_get_string_const(msg->operation), element);
    pcrdr_release_message(msg);
    return 0;

failed:
    if (msg) {
        pcrdr_release_message(msg);
    }

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
        retv = create_plainwin(conn, operation, op);
        break;

    case PCRDR_K_OPERATION_UPDATEPLAINWINDOW:
        retv = update_plainwin(conn, operation, op);
        break;

    case PCRDR_K_OPERATION_DESTROYPLAINWINDOW:
        retv = destroy_plainwin(conn, operation, op);
        break;

    case PCRDR_K_OPERATION_LOAD:
        retv = load_or_write_document(conn, op);
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

    case PCRDR_K_OPERATION_SETPAGEGROUPS:
        retv = set_page_groups(conn, operation, op);
        break;

    case PCRDR_K_OPERATION_ADDPAGEGROUPS:
        retv = add_page_groups(conn, operation, op);
        break;

    case PCRDR_K_OPERATION_REMOVEPAGEGROUP:
        retv = remove_page_group(conn, operation, op);
        break;

    case PCRDR_K_OPERATION_CREATEPAGE:
        retv = create_page(conn, operation, op);
        break;

    case PCRDR_K_OPERATION_UPDATEPAGE:
        retv = update_page(conn, operation, op);
        break;

    case PCRDR_K_OPERATION_DESTROYPAGE:
        retv = destroy_page(conn, operation, op);
        break;

    case PCRDR_K_OPERATION_GETPROPERTY:
        retv = get_property(conn, operation, op);
        break;

    case PCRDR_K_OPERATION_SETPROPERTY:
        retv = set_property(conn, operation, op);
        break;

    case PCRDR_K_OPERATION_CALLMETHOD:
        retv = call_method(conn, operation, op);
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
    const char *event_name = NULL;
    const char *target = NULL;
    const char *element = NULL;
    const char *op_name = NULL;

    purc_variant_t tmp;
    if ((tmp = purc_variant_object_get_by_ckey(evt_vrt, "eventName"))) {
        event_name = purc_variant_get_string_const(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(evt_vrt, "target"))) {
        target = purc_variant_get_string_const(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(evt_vrt, "element"))) {
        element = purc_variant_get_string_const(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(evt_vrt, "namedOp"))) {
        op_name = purc_variant_get_string_const(tmp);
    }

    if (event_name == NULL || target == NULL || op_name == NULL) {
        goto failed;
    }

    if (strcmp(event_name, purc_variant_get_string_const(evt_msg->eventName)))
        goto failed;

    struct client_info *info = pcrdr_conn_get_user_data(conn);

    int target_type;
    uint64_t target_value;
    target_type = transfer_target_info(info, target, &target_value);
    if (target_type != evt_msg->target ||
            target_value != evt_msg->targetValue) {
        goto failed;
    }

    if (strcmp("destroy", event_name) == 0) {
        // remove target from info->handles
        purc_variant_object_remove_by_static_ckey(info->handles, target, true);
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
                    element, purc_variant_get_string_const(evt_msg->elementValue));
            goto failed;
        }
    }

    return op_name;

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
            if (op == PURC_VARIANT_INVALID) {
                LOG_ERROR("No named operation defined: %s\n", op_name);
            }
            else if (purc_variant_is_object(op)) {
                LOG_INFO("Queue a named operation: %s\n", op_name);
                queue_operations(conn, op);
            }
            else if (purc_variant_is_array(op)) {
                LOG_INFO("Queue a named batch operations: %s\n", op_name);
                queue_operations(conn, op);
            }
            else {
                LOG_ERROR("Not a valid named operation: %s\n", op_name);
            }
        }
    }
    else {
        LOG_INFO("Got an event not intrested in (target: %d/%p): %s (%s)\n",
                msg->target, (void *)(uintptr_t)msg->targetValue,
                purc_variant_get_string_const(msg->eventName),
                purc_variant_get_string_const(msg->sourceURI));

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

    client.doc_contents = purc_variant_make_object_0();
    client.doc_wrotten_len = purc_variant_make_object_0();
    client.handles = purc_variant_make_object_0();

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

    queue_operations(conn, client.initialOps);

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

