/**
 ** purcsmg.h: Common definitions for simple markup generator.
 **
 ** Copyright (C) 2021, 2022 FMSoft (http://www.fmsoft.cn)
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

#ifndef _PURCMC_PURCSMG_H
#define _PURCMC_PURCSMG_H

#include <termio.h>
#include <purc/purc.h>

#include "lib/kvlist.h"

#define NR_CMD_ARGS         4

#define LEN_COMMAND         31
#define LEN_NORMAL_ARG      PCRDR_LEN_ENDPOINT_NAME
#define LEN_LAST_ARG        1023
#define LEN_GAME_NAME       31

#define LEN_EDIT_BUFF       1023

#define LEN_HISTORY_BUF     128

#define TABLESIZE(table)    (sizeof(table)/sizeof(table[0]))

/* original terminal modes */
struct run_info {
    int ttyfd;
    bool running;
    bool use_cmdline;

    time_t last_sigint_time;

    struct termios startup_termios;

    char app_name [PCRDR_LEN_APP_NAME + 1];
    char runner_name [PCRDR_LEN_RUNNER_NAME + 1];
    char builtin_endpoint [PCRDR_LEN_ENDPOINT_NAME + 1];
    char self_endpoint [PCRDR_LEN_ENDPOINT_NAME + 1];

    char* initial_file;

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

extern struct run_info the_client;

int setup_tty (void);
int restore_tty (int ttyfd);
void handle_tty_input (pcrdr_conn *conn);
void cmdline_print_prompt (pcrdr_conn *conn, bool reset_history);

#endif /* _PURCMC_PURCSMG_H */
