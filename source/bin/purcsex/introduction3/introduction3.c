/*
** introduction3.c - event handlers for introduction3
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
    unsigned nr_windows;
};

struct sample_data *sample_initializer(const char *name)
{
    struct sample_data *data;

    data = calloc(1, sizeof(struct sample_data));
    data->nr_windows = 3;

    return data;
}

void sample_terminator(const char *name, struct sample_data *data)
{
    if (data)
        free(data);
}

void on_intro_window_closed(pcrdr_conn* conn,
        purc_variant_t event_desired, const pcrdr_msg *event_msg)
{
    struct client_info *info = pcrdr_conn_get_user_data(conn);

    info->sample_data->nr_windows--;
    LOG_INFO("# windows left: %u\n", info->sample_data->nr_windows);
    if (info->sample_data->nr_windows == 0)
        info->running = false;
}

