/*
 * @file purcsex.h
 * @author Vincent Wei
 * @date 2022/05/19
 * @brief The common definitions.
 *
 * Copyright (C) 2022 FMSoft <https://www.fmsoft.cn>
 *
 * This file is a part of PurC Midnight Commander.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdbool.h>
#include <purc/purc.h>

#define MAX_NR_WINDOWS      8
struct client_info {
    bool running;
    bool interact;

    uint32_t nr_windows;
    uint32_t nr_destroyed_wins;

    time_t last_sigint_time;
    size_t run_times;

    char app_name[PURC_LEN_APP_NAME + 1];
    char runner_name[PURC_LEN_RUNNER_NAME + 1];
    char sample_name[PURC_LEN_IDENTIFIER + 1];

    purc_variant_t sample;
    purc_variant_t initialOps;
    purc_variant_t namedOps;
    purc_variant_t events;

    size_t nr_ops;
    size_t nr_events;

    size_t ops_issued;
    size_t nr_windows_created;

    char *doc_content[MAX_NR_WINDOWS];
    size_t len_content[MAX_NR_WINDOWS];
    size_t len_wrotten[MAX_NR_WINDOWS];

    // handles of windows.
    uint64_t win_handles[MAX_NR_WINDOWS];

    // handles of DOM.
    uint64_t dom_handles[MAX_NR_WINDOWS];

    void *module_handle;

    char buff[32];
};

typedef void (*ext_event_handler)(pcrdr_conn* conn,
        purc_variant_t evt_vrt, const pcrdr_msg *evt_msg);

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

