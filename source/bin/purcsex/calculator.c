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

#include "purcsex.h"
#include "calculator.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define LEN_EXPRESSION  1024

struct sample_data {
    unsigned fraction;
    char expression[LEN_EXPRESSION];
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

void calc_change_fraction(pcrdr_conn* conn,
        purc_variant_t event_desired, const pcrdr_msg *event_msg)
{
    LOG_DEBUG("called\n");

    int win = 0; /* TODO */
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    purc_variant_t details;
    details = purc_variant_object_get_by_ckey(event_msg->data, "details");

    purc_variant_t value;
    value = purc_variant_object_get_by_ckey(details, "targetValue");

    size_t value_length;
    const char *value_text;
    value_text = purc_variant_get_string_const_ex(value, &value_length);

    pcrdr_msg *msg;
    msg = pcrdr_make_request_message(
            PCRDR_MSG_TARGET_DOM, info->dom_handles[win],
            "setProperty", PCRDR_REQUESTID_NORETURN,
            PCRDR_MSG_ELEMENT_TYPE_ID, "theFraction",
            "textContent",
            PCRDR_MSG_DATA_TYPE_TEXT, value_text, value_length);

    if (pcrdr_send_request(conn, msg,
                PCRDR_DEF_TIME_EXPECTED, 0,
                noreturn_handler) < 0) {
        LOG_ERROR("Failed to send request (%s)\n",
                purc_variant_get_string_const(msg->operation));
    }
    else {
        LOG_INFO("Request (%s) sent\n",
                purc_variant_get_string_const(msg->operation));
    }

    pcrdr_release_message(msg);
}

void calc_click_digit(pcrdr_conn* conn,
        purc_variant_t evt_vrt, const pcrdr_msg *evt_msg)
{
    LOG_INFO("called\n");
}

void calc_click_sign(pcrdr_conn* conn,
        purc_variant_t evt_vrt, const pcrdr_msg *evt_msg)
{
    LOG_INFO("called\n");
}

void calc_click_clear(pcrdr_conn* conn,
        purc_variant_t evt_vrt, const pcrdr_msg *evt_msg)
{
    LOG_INFO("called\n");
}

void calc_click_equal(pcrdr_conn* conn,
        purc_variant_t evt_vrt, const pcrdr_msg *evt_msg)
{
    LOG_INFO("called\n");
}

