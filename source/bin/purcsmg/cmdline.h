/**
 ** cmdline.h: Common definitions for simple markup generator.
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

#ifndef _PURCRDR_CMDLIEN_H
#define _PURCRDR_CMDLIEN_H

#include <termio.h>
#include "lib/kvlist.h"

#define NR_CMD_ARGS         4

#define LEN_COMMAND         31
#define LEN_NORMAL_ARG      PURCRDR_LEN_ENDPOINT_NAME
#define LEN_LAST_ARG        1023
#define LEN_GAME_NAME       31

#define LEN_EDIT_BUFF       1023

#define LEN_HISTORY_BUF     128

#define TABLESIZE(table)    (sizeof(table)/sizeof(table[0]))

/* original terminal modes */
struct run_info {
    int ttyfd;
    bool running;
    time_t last_sigint_time;

    struct termios startup_termios;

    char app_name [PURCRDR_LEN_APP_NAME + 1];
    char runner_name [PURCRDR_LEN_RUNNER_NAME + 1];
    char builtin_endpoint [PURCRDR_LEN_ENDPOINT_NAME + 1];
    char self_endpoint [PURCRDR_LEN_ENDPOINT_NAME + 1];

    pcrdr_json *jo_endpoints;
    struct kvlist ret_value_list;

    char edit_buff [LEN_EDIT_BUFF + 1];
    int curr_edit_pos;
    bool edited;

    int nr_history_cmds;
    int curr_history_idx;
    char* history_cmds [LEN_HISTORY_BUF];
    char* saved_buff;

    /* fields for drum-game */
    int nr_players;
    char* ball_content;
};

int start_drum_game (pcrdr_conn* conn, int nr_players, const char *ball_content);

#endif /* _PURCRDR_CMDLIEN_H */

