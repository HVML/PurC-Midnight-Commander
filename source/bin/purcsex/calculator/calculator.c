/*
** calculator.c - event handlers for calculator
**
** Copyright (C) 2022 FMSoft (http://www.fmsoft.cn)
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

#include "calculator.h"
#include "../purcsex.h"
#include "../log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>

#define LEN_EXPRESSION  1024

struct sample_data {
    unsigned fraction;
    unsigned length;
    char expression[LEN_EXPRESSION + 4];
};

struct sample_data *sample_initializer(const char *name)
{
    struct sample_data *data;

    LOG_DEBUG("%s is allocating buffer for expression\n", name);
    data = calloc(1, sizeof(struct sample_data));
    data->fraction = 10;

    return data;
}

void sample_terminator(const char *name, struct sample_data *data)
{
    LOG_DEBUG("%s is freeing buffer for expression\n", name);
    if (data)
        free(data);
}

static int noreturn_handler(pcrdr_conn* conn,
        const char *request_id, int state,
        void *context, const pcrdr_msg *response_msg)
{
    return 0;
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

        if (purc_variant_cast_to_ulongint(v, &handle, false))
            return handle;
    }

failed:
    target_name[0] = 0; /* target_name is an empty string when failed */
    return 0;
}

static uint64_t
get_handle(struct client_info *info, purc_variant_t event_desired)
{
    uint64_t handle = 0;

    purc_variant_t tmp;
    if ((tmp = purc_variant_object_get_by_ckey(event_desired, "target"))) {
        const char *target = purc_variant_get_string_const(tmp);
        if (target == NULL) {
            LOG_ERROR("No valid target in catched event\n");
            goto done;
        }

        char target_name[PURC_LEN_IDENTIFIER + 1];
        handle = split_target(info->handles, target, target_name);
        if (strcasecmp(target_name, "dom")) {
            LOG_ERROR("No valid target value in catched event\n");
            goto done;
        }
    }

done:
    return handle;
}

void calc_change_fraction(pcrdr_conn* conn,
        purc_variant_t event_desired, const pcrdr_msg *event_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    uint64_t dom_handle = get_handle(info, event_desired);
    if (dom_handle == 0)
        return;

    purc_variant_t value;
    value = purc_variant_object_get_by_ckey(event_msg->data, "targetValue");

    size_t value_length;
    const char *value_text;
    value_text = purc_variant_get_string_const_ex(value, &value_length);
    if (value_text == NULL) {
        LOG_ERROR("Failed to get value: %s\n",
                purc_get_error_message(purc_get_last_error()));
        return;
    }

    info->sample_data->fraction = (unsigned)atoi(value_text);

    pcrdr_msg *msg;
    msg = pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, dom_handle,
            "setProperty", PCRDR_REQUESTID_NORETURN, NULL,
            PCRDR_MSG_ELEMENT_TYPE_ID, "theFraction",
            "textContent",
            PCRDR_MSG_DATA_TYPE_TEXT, value_text, value_length);

    if (msg == NULL) {
        LOG_ERROR("Failed to make request message: %s\n",
                purc_get_error_message(purc_get_last_error()));
        return;
    }

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, 0,
                noreturn_handler) < 0) {
        LOG_ERROR("Failed to send request: %s\n",
                purc_get_error_message(purc_get_last_error()));
    }
    else {
        LOG_DEBUG("Request (%s) sent\n",
                purc_variant_get_string_const(msg->operation));
    }

    pcrdr_release_message(msg);
}

#define IDPREFIX_DIGIT      "theDigit"
#define IDPREFIX_SIGN       "theSign"

static char get_digit_sign(const char *id)
{
    if (strncmp(id, IDPREFIX_DIGIT, sizeof(IDPREFIX_DIGIT) - 1) == 0 &&
            strlen(id) == sizeof(IDPREFIX_DIGIT)) {
        return id[sizeof(IDPREFIX_DIGIT) - 1];
    }
    else if (strncmp(id, IDPREFIX_SIGN, sizeof(IDPREFIX_SIGN) - 1) == 0 &&
            strlen(id) >= sizeof(IDPREFIX_SIGN)) {
        const char *sign = id + sizeof(IDPREFIX_SIGN) - 1;
        if (strcmp(sign, "Dot") == 0) {
            return '.';
        }
        else if (strcmp(sign, "Plus") == 0) {
            return '+';
        }
        else if (strcmp(sign, "Minus") == 0) {
            return '-';
        }
        else if (strcmp(sign, "Times") == 0) {
            return '*';
        }
        else if (strcmp(sign, "Division") == 0) {
            return '/';
        }
    }

    LOG_ERROR("Invalid identifier for digit button: %s\n", id);
    return 0;
}

static void set_expression(pcrdr_conn* conn,
        struct client_info *info, uint64_t dom_handle)
{
    const char *text;
    size_t length;

    if (info->sample_data->length > 0) {
        text =info->sample_data->expression;
        length = info->sample_data->length;
    }
    else {
        text = "0";
        length = 1;
    }

    pcrdr_msg *msg;
    msg = pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, dom_handle,
            "setProperty", PCRDR_REQUESTID_NORETURN, NULL,
            PCRDR_MSG_ELEMENT_TYPE_ID, "theExpression",
            "textContent",
            PCRDR_MSG_DATA_TYPE_TEXT,
            text, length);

    if (msg == NULL) {
        LOG_ERROR("Failed to make request message: %s\n",
                purc_get_error_message(purc_get_last_error()));
        return;
    }

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, 0,
                noreturn_handler) < 0) {
        LOG_ERROR("Failed to send request: %s\n",
                purc_get_error_message(purc_get_last_error()));
    }
    else {
        LOG_DEBUG("Request (%s) sent\n",
                purc_variant_get_string_const(msg->operation));
    }

    if (strcmp(text, "ERROR") == 0) {
        info->sample_data->length = 0;
    }

    pcrdr_release_message(msg);
}

void calc_click_digit_sign(pcrdr_conn* conn,
        purc_variant_t event_desired, const pcrdr_msg *event_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    uint64_t dom_handle = get_handle(info, event_desired);
    if (dom_handle == 0)
        return;

    purc_variant_t target_id;
    target_id = purc_variant_object_get_by_ckey(event_msg->data, "targetId");

    const char *element_id;
    element_id = purc_variant_get_string_const(target_id);
    if (element_id == NULL) {
        LOG_ERROR("Failed to get element Id: %s\n",
                purc_get_error_message(purc_get_last_error()));
        return;
    }

    char digit = get_digit_sign(element_id);
    if (digit == 0)
        return;

    if (info->sample_data->length < LEN_EXPRESSION) {
        info->sample_data->expression[info->sample_data->length] = digit;
        info->sample_data->length++;
    }
    else {
        LOG_WARN("The buffer for expression is full.\n");
        return;
    }

    set_expression(conn, info, dom_handle);
}

void calc_click_back(pcrdr_conn* conn,
        purc_variant_t event_desired, const pcrdr_msg *event_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    uint64_t dom_handle = get_handle(info, event_desired);
    if (dom_handle == 0)
        return;

    if (info->sample_data->length > 0) {
        info->sample_data->length--;
    }
    else {
        LOG_WARN("The buffer for expression is empty.\n");
        return;
    }

    set_expression(conn, info, dom_handle);
}

void calc_click_clear(pcrdr_conn* conn,
        purc_variant_t event_desired, const pcrdr_msg *event_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    uint64_t dom_handle = get_handle(info, event_desired);
    if (dom_handle == 0)
        return;

    if (info->sample_data->length > 0) {
        info->sample_data->length = 0;
    }
    else {
        LOG_WARN("The buffer for expression is empty.\n");
        return;
    }

    set_expression(conn, info, dom_handle);
}

#define OP_PERCENT "()/100"

void calc_click_op_percent(pcrdr_conn* conn,
        purc_variant_t event_desired, const pcrdr_msg *event_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    uint64_t dom_handle = get_handle(info, event_desired);
    if (dom_handle == 0)
        return;

    if (info->sample_data->length > 0 &&
            info->sample_data->length <= LEN_EXPRESSION - sizeof(OP_PERCENT)) {
        memmove(info->sample_data->expression + 1,
                info->sample_data->expression,
                info->sample_data->length);

        info->sample_data->expression[0] = '(';
        info->sample_data->expression[info->sample_data->length + 1] = ')';
        info->sample_data->expression[info->sample_data->length + 2] = '/';
        info->sample_data->expression[info->sample_data->length + 3] = '1';
        info->sample_data->expression[info->sample_data->length + 4] = '0';
        info->sample_data->expression[info->sample_data->length + 5] = '0';

        info->sample_data->length += sizeof(OP_PERCENT) - 1;

    }
    else
        return;

    set_expression(conn, info, dom_handle);
}

#undef OP_PERCENT

#define OP_TOGGLE_SIGN "-()"

void calc_click_op_toggle_sign(pcrdr_conn* conn,
        purc_variant_t event_desired, const pcrdr_msg *event_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    uint64_t dom_handle = get_handle(info, event_desired);
    if (dom_handle == 0)
        return;

    if (info->sample_data->length > 0 &&
            info->sample_data->length <= LEN_EXPRESSION - sizeof(OP_TOGGLE_SIGN)) {
        memmove(info->sample_data->expression + 2,
                info->sample_data->expression,
                info->sample_data->length);

        info->sample_data->expression[0] = '-';
        info->sample_data->expression[1] = '(';
        info->sample_data->length += sizeof(OP_TOGGLE_SIGN) - 1;

        info->sample_data->expression[info->sample_data->length - 1] = ')';
    }
    else
        return;

    set_expression(conn, info, dom_handle);
}
#undef OP_TOGGLE_SIGN

static void
trim_tail_spaces(char *dest, size_t n)
{
    while (n>1) {
        if (!isspace(dest[n-1]))
            break;
        dest[--n] = '\0';
    }
}

static size_t
fetch_cmd_output(const char *cmd, char *dest, size_t sz)
{
    FILE *fin = NULL;
    size_t n = 0;

    fin = popen(cmd, "r");
    if (!fin)
        return 0;

    n = fread(dest, 1, sz - 1, fin);
    if (n == 0)
        return 0;

    n--;
    dest[n] = '\0';

    if (pclose(fin)) {
        return 0;
    }

    trim_tail_spaces(dest, n);
    return n;
}

void calc_click_equal(pcrdr_conn* conn,
        purc_variant_t event_desired, const pcrdr_msg *event_msg)
{
    char cmd[2048];

    struct client_info *info = pcrdr_conn_get_user_data(conn);

    uint64_t dom_handle = get_handle(info, event_desired);
    if (dom_handle == 0)
        return;

    info->sample_data->expression[info->sample_data->length] = 0;
    snprintf(cmd, sizeof(cmd), "(echo 'scale=%d; %s') | bc",
            info->sample_data->fraction,
            info->sample_data->expression);

    info->sample_data->length = fetch_cmd_output(cmd,
            info->sample_data->expression, LEN_EXPRESSION);
    if (info->sample_data->length == 0) {
        strcpy(info->sample_data->expression, "ERROR");
        info->sample_data->length = 5;
    }

    LOG_DEBUG("result: %s (%u)\n",
            info->sample_data->expression, info->sample_data->length);
    set_expression(conn, info, dom_handle);
}

