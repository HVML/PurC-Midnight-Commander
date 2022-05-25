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
#define LEN_NORMAL_ARG      PURC_LEN_ENDPOINT_NAME
#define LEN_LAST_ARG        1023
#define LEN_GAME_NAME       31

#define LEN_EDIT_BUFF       1023

#define LEN_HISTORY_BUF     128

#define TABLESIZE(table)    (sizeof(table)/sizeof(table[0]))

#define MAX_NR_WINDOWS      8
#define MAX_CHANGES         128

/* 0 is not a valid handle */
enum {
    HANDLE_TEXTCONTENT_CLOCK1 = 1,
    HANDLE_TEXTCONTENT_CLOCK2,
    HANDLE_HTMLCONTENT,
    HANDLE_ATTR_VALUE1,
    HANDLE_ATTR_VALUE2,
    HANDLE_TEXTCONTENT_TITLE,
};

enum {
    STATE_INITIAL = 0,
    STATE_WINDOW_CREATED,
    STATE_DOCUMENT_WROTTEN,
    STATE_DOCUMENT_LOADED,
    STATE_DOCUMENT_TESTING,
    STATE_DOCUMENT_RESET,
    STATE_WINDOW_DESTROYED,
    STATE_FATAL,
};

/* original terminal modes */
struct run_info {
    int ttyfd;
    bool running;
    bool use_cmdline;

    time_t last_sigint_time;

    struct termios startup_termios;

    char app_name [PURC_LEN_APP_NAME + 1];
    char runner_name [PURC_LEN_RUNNER_NAME + 1];
    char builtin_endpoint [PURC_LEN_ENDPOINT_NAME + 1];
    char self_endpoint [PURC_LEN_ENDPOINT_NAME + 1];

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

    /* fields for autotest */
    char* doc_content;
    size_t len_content;
    size_t nr_chars;
    int    nr_windows;
    int    test_method;

    int nr_destroyed_wins;
    int state[MAX_NR_WINDOWS];
    bool wait[MAX_NR_WINDOWS];

    size_t len_wrotten[MAX_NR_WINDOWS];
    int max_changes[MAX_NR_WINDOWS];
    int changes[MAX_NR_WINDOWS];

    // handles of windows.
    uint64_t win_handles[MAX_NR_WINDOWS];
    // handles of DOM.
    uint64_t dom_handles[MAX_NR_WINDOWS];
};

extern struct run_info the_client;

int setup_tty (void);
int restore_tty (int ttyfd);
void handle_tty_input (pcrdr_conn *conn);
void cmdline_print_prompt (pcrdr_conn *conn, bool reset_history);

#endif /* _PURCMC_PURCSMG_H */

