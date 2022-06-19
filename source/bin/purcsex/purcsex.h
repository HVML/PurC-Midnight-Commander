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

struct sample_data;
struct client_info {
    bool running;
    bool interact;
    bool batch_finished;

    uint32_t nr_created_windows;
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
    size_t nr_events;

    purc_variant_t batchOps;
    size_t nr_ops;
    size_t issued_ops;

    size_t nr_windows_created;
    size_t nr_pages_created;

    /*
     * contents for windows or pages.
     *
     * an object variant; the key is in one of the following patterns:
     *
     *  - `plainwindow/<resultKey>`
     *  - `page/<resultKey>`
     *
     * and the value is the content string loaded from the specified file.
     */
    purc_variant_t doc_contents;

    /*
     * length wrotten to the renderer for windows or pages.
     *
     * an object variant; the key is in one of the following patterns:
     *
     *  - `plainwindow/<resultKey>`
     *  - `page/<resultKey>`
     *
     * and the value is the content length (an ulongint variant) has been
     * wrotten to the renderer.
     */
    purc_variant_t doc_wrotten_len;

    /*
     * handles for windows, pages, and DOMs:
     *
     * an object variant; the key is in one of the following patterns:
     *
     *  - `plainwindow/<resultKey>`
     *  - `tabbedwindow/<resultKey>`
     *  - `page/<resultKey>`
     *  - `dom/<resultKey>`
     *
     * and the value is the result value (an ulongint variant) returned
     * from the renderer.
     */
    purc_variant_t handles;

    void *sample_handle;
    struct sample_data *sample_data;

    char buff[32];
};

typedef void (*sample_event_handler_t)(pcrdr_conn* conn,
        purc_variant_t evt_vrt, const pcrdr_msg *evt_msg);

typedef struct sample_data *(*sample_initializer_t)(const char *sample_name);
typedef void (*sample_terminator_t)(const char *sample_name,
        struct sample_data *data);

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

