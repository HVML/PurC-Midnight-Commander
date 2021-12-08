/*
** cmdline.c -- The code for simple markup generator cmdline.
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termio.h>
#include <signal.h>
#include <getopt.h>
#include <assert.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <hibox/ulog.h>
#include <hibox/json.h>

#include "purcrdr.h"
#include "cmdline.h"

/* original terminal modes */
static struct run_info the_client;

/* command identifiers */
enum {
    CMD_HELP = 0,
    CMD_EXIT,
    CMD_PLAY,
    CMD_REGISTER_METHOD,
    CMD_REVOKE_METHOD,
    CMD_SET_RETURN_VALUE,
    CMD_CALL,
    CMD_REGISTER_EVENT,
    CMD_REVOKE_EVENT,
    CMD_FIRE,
    CMD_SUBSCRIBE,
    CMD_UNSUBSCRIBE,
    CMD_LIST_ENDPOINTS,
    CMD_LIST_PROCEDURES,
    CMD_LIST_EVENTS,
    CMD_LIST_SUBSCRIBERS,
};

/* argument type */
enum {
    AT_NONE = 0,
    AT_ENDPOINT,
    AT_METHOD,
    AT_BUBBLE,
    AT_GAME,
    AT_INTEGER,
    AT_STRING,
    AT_JSON,
    AT_MAX_NR_ARGS = AT_JSON,
};

static struct cmd_info {
    int cmd;
    const char* long_name;
    const char* short_name;
    const char* sample;
    int type_1st_arg;
    int type_2nd_arg;
    int type_3rd_arg;
    int type_4th_arg;
} sg_cmd_info [] = {
    { CMD_HELP,
        "help", "h",
        "help",
        AT_NONE, AT_NONE, AT_NONE, AT_NONE },
    { CMD_EXIT,
        "exit", "x",
        "exit",
        AT_NONE, AT_NONE, AT_NONE, AT_NONE },
    { CMD_PLAY,
        "play", "p",
        "play drum 10 This is a secret",
        AT_GAME, AT_INTEGER, AT_NONE, AT_STRING, },
    { CMD_REGISTER_METHOD,
        "registerMethod", "rgm", 
        "registermethod whoAmI *",
        AT_NONE, AT_BUBBLE, AT_NONE, AT_STRING, },
    { CMD_REVOKE_METHOD,
        "revokeMethod", "rvm", 
        "revokemethod whoAmI",
        AT_NONE, AT_BUBBLE, AT_NONE, AT_NONE, },
    { CMD_SET_RETURN_VALUE,
        "setretvalue", "srv", 
        "setretvalue whoAmI Tom Jerry",
        AT_NONE, AT_BUBBLE, AT_NONE, AT_STRING, },
    { CMD_CALL,
        "call", "c", 
        "call @localhost/cn.fmsoft.hybridos.hibus/builtin echo Hi, there",
        AT_ENDPOINT, AT_METHOD, AT_NONE, AT_STRING, },
    { CMD_REGISTER_EVENT,
        "registerEvent", "rge", 
        "registerevent MYEVENT *",
        AT_NONE, AT_BUBBLE, AT_NONE, AT_STRING, },
    { CMD_REVOKE_EVENT,
        "revokeEvent", "rve", 
        "revokeevent MYEVENT",
        AT_NONE, AT_BUBBLE, AT_NONE, AT_NONE, },
    { CMD_FIRE,
        "fire", "f", 
        "fire CLOCK 14:00",
        AT_NONE, AT_BUBBLE, AT_NONE, AT_STRING, },
    { CMD_SUBSCRIBE,
        "subscribe", "sub",
        "sub @localhost/cn.fmsoft.hybridos.hibus/builtin NEWENDPOINT",
        AT_ENDPOINT, AT_BUBBLE, AT_NONE, AT_NONE, },
    { CMD_UNSUBSCRIBE,
        "unsubscribe", "unsub",
        "unsub @localhost/cn.fmsoft.hybridos.hibus/builtin NEWENDPOINT" ,
        AT_ENDPOINT, AT_BUBBLE, AT_NONE, AT_NONE, },
    { CMD_LIST_ENDPOINTS,
        "listendpoints", "lep", 
        "lep",
        AT_NONE, AT_NONE, AT_NONE, AT_NONE, },
    { CMD_LIST_PROCEDURES,
        "listprocedures", "lp",
        "lp @localhost/cn.fmsoft.hybridos.hibus/builtin",
        AT_ENDPOINT, AT_NONE, AT_NONE, AT_NONE, },
    { CMD_LIST_EVENTS,
        "listevents", "le",
        "le @localhost/cn.fmsoft.hybridos.hibus/builtin",
        AT_ENDPOINT, AT_NONE, AT_NONE, AT_NONE, },
    { CMD_LIST_SUBSCRIBERS,
        "listsubscribers", "ls",
        "ls @localhost/cn.fmsoft.hybridos.hibus/builtin NEWENDPOINT",
        AT_ENDPOINT, AT_BUBBLE, AT_NONE, AT_NONE, },
};

static int setup_tty (void)
{
    int ttyfd;
    struct termios my_termios;

    ttyfd = open ("/dev/tty", O_RDONLY);
    if (ttyfd < 0) {
        ULOG_ERR ("Failed to open /dev/tty: %s.", strerror (errno));
        return -1;
    }

    if (tcgetattr (ttyfd, &the_client.startup_termios) < 0) {
        ULOG_ERR ("Failed to call tcgetattr: %s.", strerror (errno));
        goto error;
    }

    memcpy (&my_termios, &the_client.startup_termios, sizeof ( struct termios));
#if 0
    my_termios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    my_termios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
            | INLCR | IGNCR | ICRNL | IXON);
    my_termios.c_oflag &= ~OPOST;
    my_termios.c_cflag &= ~(CSIZE | PARENB);
    my_termios.c_cflag |= CS8;
#else
    my_termios.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN);
    my_termios.c_iflag &= ~(ICRNL | INLCR);
    my_termios.c_iflag |= ICRNL;
    my_termios.c_cc[VMIN] = 0;
    my_termios.c_cc[VTIME] = 0;
#endif

    if (tcsetattr (ttyfd, TCSAFLUSH, &my_termios) < 0) {
        ULOG_ERR ("Failed to call tcsetattr: %s.", strerror (errno));
        goto error;
    }

    if (fcntl (ttyfd, F_SETFL, fcntl (ttyfd, F_GETFL, 0) | O_NONBLOCK) == -1) {
        ULOG_ERR ("Failed to set TTY as non-blocking: %s.", strerror (errno));
        return -1;
    }

    return ttyfd;

error:
    close (ttyfd);
    return -1;
}

static int restore_tty (int ttyfd)
{
    if (tcsetattr (ttyfd, TCSAFLUSH, &the_client.startup_termios) < 0)
        return -1;

    close (ttyfd);
    return 0;
}

static void handle_signal_action (int sig_number)
{
    if (sig_number == SIGINT) {
        if (the_client.last_sigint_time == 0) {
            fprintf (stderr, "\n");
            fprintf (stderr, "SIGINT caught, press <CTRL+C> again in 5 seconds to quit.\n");
            the_client.last_sigint_time = pcrdr_get_monotoic_time ();
        }
        else if (pcrdr_get_monotoic_time () < the_client.last_sigint_time + 5) {
            fprintf (stderr, "SIGINT caught, quit...\n");
            the_client.running = false;
        }
        else {
            fprintf (stderr, "\n");
            fprintf (stderr, "SIGINT caught, press <CTRL+C> again in 5 seconds to quit.\n");
            the_client.running = true;
            the_client.last_sigint_time = pcrdr_get_monotoic_time ();
        }
    }
    else if (sig_number == SIGPIPE) {
        fprintf (stderr, "SIGPIPE caught; the server might have quitted!\n");
    }
    else if (sig_number == SIGCHLD) {
        pid_t pid;
        int status;

        while ((pid = waitpid (-1, &status, WNOHANG)) > 0) {
            if (WIFEXITED (status)) {
                if (WEXITSTATUS(status))
                    fprintf (stderr, "Player (%d) exited: return value: %d\n", 
                            pid, WEXITSTATUS(status));
            }
            else if (WIFSIGNALED(status)) {
                fprintf (stderr, "Player (%d) exited because of signal %d\n",
                        pid, WTERMSIG (status));
            }
        }
    }
}

static int setup_signals (void)
{
    struct sigaction sa;
    memset (&sa, 0, sizeof (sa));
    sa.sa_handler = handle_signal_action;

    if (sigaction (SIGINT, &sa, 0) != 0) {
        ULOG_ERR ("Failed to call sigaction for SIGINT: %s\n", strerror (errno));
        return -1;
    }

    if (sigaction (SIGPIPE, &sa, 0) != 0) {
        ULOG_ERR ("Failed to call sigaction for SIGPIPE: %s\n", strerror (errno));
        return -1;
    }

    if (sigaction (SIGCHLD, &sa, 0) != 0) {
        ULOG_ERR ("Failed to call sigaction for SIGCHLD: %s\n", strerror (errno));
        return -1;
    }

    return 0;
}

static void print_copying (void)
{
    fprintf (stdout,
            "\n"
            "PurCRDR - the data bus system for HybridOS.\n"
            "\n"
            "Copyright (C) 2020 FMSoft <https://www.fmsoft.cn>\n"
            "\n"
            "PurCRDR is free software: you can redistribute it and/or modify\n"
            "it under the terms of the GNU General Public License as published by\n"
            "the Free Software Foundation, either version 3 of the License, or\n"
            "(at your option) any later version.\n"
            "\n"
            "PurCRDR is distributed in the hope that it will be useful,\n"
            "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
            "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
            "GNU General Public License for more details.\n"
            "You should have received a copy of the GNU General Public License\n"
            "along with this program.  If not, see http://www.gnu.org/licenses/.\n"
            );
    fprintf (stdout, "\n");
}

// move cursor to the start of the current line and erase whole line
static inline void console_reset_line (void)
{
    fputs ("\x1B[0G\x1B[2K", stderr);
}

static inline void console_beep (void)
{
    putc (0x07, stderr);
}

static void console_print_prompt (pcrdr_conn *conn, bool reset_history)
{
    struct run_info *info = pcrdr_conn_get_user_data (conn);

    assert (info);

    console_reset_line ();
    fputs ("PurCRDRCL >> ", stderr);

    // reset the command line buffers
#if 0
    info->cmd [0] = '\0';
    info->arg_1st [0] = '\0';
    info->arg_2nd [0] = '\0';
    info->arg_3rd [0] = '\0';
    info->arg_lst [0] = '\0';
#endif
    info->edit_buff [0] = '\0';
    info->curr_edit_pos = 0;

    if (reset_history) {
        info->curr_history_idx = -1;
        if (info->saved_buff) {
            free (info->saved_buff);
            info->saved_buff = NULL;
        }
        info->edited = false;
    }
}

static void on_cmd_help (pcrdr_conn *conn)
{
    fprintf (stderr, "Commands:\n\n");
    fprintf (stderr, "  <help | h>\n");
    fprintf (stderr, "    print this help message.\n");
    fprintf (stderr, "  <exit | x>\n");
    fprintf (stderr, "    exit this PurCRDR command line program.\n");
    fprintf (stderr, "  <play | p> <game> <number of players> <parameters>\n");
    fprintf (stderr, "    play a game\n");
    fprintf (stderr, "  <registerMethod | rgm> <method> <app access pattern list>\n");
    fprintf (stderr, "    register a method\n");
    fprintf (stderr, "  <revokeMethod | rvm> <method>\n");
    fprintf (stderr, "    revoke a method\n");
    fprintf (stderr, "  <setReturnValue | srv> <method> <return value>\n");
    fprintf (stderr, "    set the return value of the specified method\n");
    fprintf (stderr, "  <call | c> <endpoint> <method> <parameters>\n");
    fprintf (stderr, "    call a procedure\n");
    fprintf (stderr, "  <registerEvent | rge> <BUBBLE> <app access pattern list>\n");
    fprintf (stderr, "    register an event\n");
    fprintf (stderr, "  <revokeEvent | rve> <BUBBLE>\n");
    fprintf (stderr, "    revoke an event\n");
    fprintf (stderr, "  <fire | f> <BUBBLE> <parameters>\n");
    fprintf (stderr, "    fire an event\n");
    fprintf (stderr, "  <subscribe | sub> <endpoint> <BUBBLE>\n");
    fprintf (stderr, "    suscribe an event.\n");
    fprintf (stderr, "  <unsubscribe | unsub> <endpoint> <BUBBLE>\n");
    fprintf (stderr, "    unsuscribe an event.\n");
    fprintf (stderr, "  <listEndpoints | lep>\n");
    fprintf (stderr, "    list all endpoints.\n");
    fprintf (stderr, "  <listProcedures | lp> <endpoint>\n");
    fprintf (stderr, "    list all procedures can be called by the specified endpoint.\n");
    fprintf (stderr, "  <listEvents | le> <endpoint>\n");
    fprintf (stderr, "    list all events can be subscribed by the specified endpoint.\n");
    fprintf (stderr, "  <listSubscribers | ls> <endpoint> <BUBBLE>\n");
    fprintf (stderr, "    list all subscribers of the specified endpint bubble.\n");
    fprintf (stderr, "\n");
    fprintf (stderr, "Shortcuts:\n\n");
    fprintf (stderr, "  <F1>\n    print this help message.\n");
    fprintf (stderr, "  <F2>\n    list all endpoints.\n");
    fprintf (stderr, "  <F3>\n    show history command.\n");
    fprintf (stderr, "  <ESC>\n    exit this PurCRDR command line program.\n");
    //fprintf (stderr, "\t<TAB>\n\t\tauto complete the command.\n");
    fprintf (stderr, "  <UP>/<DOWN>\n   switch among history.\n");
    fprintf (stderr, "\n");
}

static void on_cmd_exit (pcrdr_conn *conn)
{
    struct run_info *info = pcrdr_conn_get_user_data (conn);

    assert (info);

    fputs ("Exiting...\n", stderr);
    info->running = false;
}

static void on_cmd_play (pcrdr_conn *conn,
        const char* game, long nr_players, const char* param)
{
    if (strcasecmp (game, "drum") == 0) {
        start_drum_game (conn, nr_players, param);
    }
    else {
        fprintf (stderr, "Unknown game: %s\n", game);
    }
}

static void on_cmd_call (pcrdr_conn *conn,
        const char* endpoint, const char* method, const char* param)
{
    int ret_code;
    char* ret_value;
    int err_code;

    err_code = pcrdr_call_procedure_and_wait (conn,
            endpoint,
            method,
            param,
            PURCRDR_DEF_TIME_EXPECTED,
            &ret_code, &ret_value);

    if (err_code) {
        fprintf (stderr, "Failed to call procedure %s/%s with parameter %s: %s\n",
                endpoint, method, param, pcrdr_get_err_message (err_code));
        if (err_code == PURCRDR_EC_SERVER_ERROR) {
            int ret_code = pcrdr_conn_get_last_ret_code (conn);
            fprintf (stderr, "Server returned message: %s (%d)\n",
                    pcrdr_get_ret_message (ret_code), ret_code);
        }
    }
    else {
        fprintf (stderr, "Got result from procedure %s/%s with parameter %s: \n%s\n",
                endpoint, method, param, ret_value);

        free (ret_value);
    }
}

static const char* my_method_handler (pcrdr_conn* conn,
        const char* from_endpoint, const char* to_method,
        const char* method_param, int *err_code)
{
    char normalized_name [PURCRDR_LEN_METHOD_NAME + 1];
    struct run_info *info = pcrdr_conn_get_user_data (conn);
    void* data;
    char* value;

    pcrdr_name_tolower_copy (to_method, normalized_name, PURCRDR_LEN_METHOD_NAME);
    if ((data = kvlist_get (&info->ret_value_list, normalized_name)) == NULL) {
        return "NULL";
    }

    value = *(char **)data;
    return value;
}

static void on_cmd_register_method (pcrdr_conn *conn,
        const char* method, const char* param)
{
    int err_code;

    err_code = pcrdr_register_procedure_const (conn, method,
            "localhost", param, my_method_handler);
    if (err_code) {
        struct run_info *info = pcrdr_conn_get_user_data (conn);

        fprintf (stderr, "Failed to register method %s/%s with app access patterns %s: %s\n",
                info->self_endpoint, method, param, pcrdr_get_err_message (err_code));
        if (err_code == PURCRDR_EC_SERVER_ERROR) {
            int ret_code = pcrdr_conn_get_last_ret_code (conn);

            fprintf (stderr, "Server returned message: %s (%d)\n",
                    pcrdr_get_ret_message (ret_code), ret_code);
        }
    }
    else {
        fprintf (stderr, "Method registered.\n");
    }
}

static void on_cmd_revoke_method (pcrdr_conn *conn,
        const char* method)
{
    int err_code;

    err_code = pcrdr_revoke_procedure (conn, method);
    if (err_code) {
        struct run_info *info = pcrdr_conn_get_user_data (conn);

        fprintf (stderr, "Failed to revoke method %s/%s: %s\n",
                info->self_endpoint, method, pcrdr_get_err_message (err_code));
        if (err_code == PURCRDR_EC_SERVER_ERROR) {
            int ret_code = pcrdr_conn_get_last_ret_code (conn);

            fprintf (stderr, "Server returned message: %s (%d)\n",
                    pcrdr_get_ret_message (ret_code), ret_code);
        }
    }
    else {
        fprintf (stderr, "Method revoked.\n");
    }
}

static void on_cmd_set_return_value (pcrdr_conn *conn,
        const char* method, const char* ret_value)
{
    char normalized_name [PURCRDR_LEN_METHOD_NAME + 1];
    struct run_info *info = pcrdr_conn_get_user_data (conn);
    char* value;

    if (!pcrdr_is_valid_method_name (method)) {
        fprintf (stderr, "Bad method name: %s\n", method);
        return;
    }

    pcrdr_name_tolower_copy (method, normalized_name, PURCRDR_LEN_METHOD_NAME);
    value = strdup (ret_value);
    if (kvlist_set (&info->ret_value_list, normalized_name, &value)) {
        fprintf (stderr, "Value store for method: %s\n", normalized_name);
    }
    else {
        fprintf (stderr, "Failed to store value for method: %s\n", normalized_name);
    }
}

static void on_cmd_register_event (pcrdr_conn *conn,
        const char* bubble, const char* param)
{
    int err_code;

    err_code = pcrdr_register_event (conn, bubble, "localhost", param);
    if (err_code) {
        struct run_info *info = pcrdr_conn_get_user_data (conn);

        fprintf (stderr, "Failed to register event %s/%s with app access patterns %s: %s\n",
                info->self_endpoint, bubble, param, pcrdr_get_err_message (err_code));
        if (err_code == PURCRDR_EC_SERVER_ERROR) {
            int ret_code = pcrdr_conn_get_last_ret_code (conn);

            fprintf (stderr, "Server returned message: %s (%d)\n",
                    pcrdr_get_ret_message (ret_code), ret_code);
        }
    }
    else {
        fprintf (stderr, "Event registered.\n");
    }
}

static void on_cmd_revoke_event (pcrdr_conn *conn,
        const char* bubble)
{
    int err_code;

    err_code = pcrdr_revoke_event (conn, bubble);
    if (err_code) {
        struct run_info *info = pcrdr_conn_get_user_data (conn);

        fprintf (stderr, "Failed to revoke event %s/%s: %s\n",
                info->self_endpoint, bubble, pcrdr_get_err_message (err_code));
        if (err_code == PURCRDR_EC_SERVER_ERROR) {
            int ret_code = pcrdr_conn_get_last_ret_code (conn);

            fprintf (stderr, "Server returned message: %s (%d)\n",
                    pcrdr_get_ret_message (ret_code), ret_code);
        }
    }
    else {
        fprintf (stderr, "Event revoked.\n");
    }
}

static void on_cmd_fire (pcrdr_conn *conn,
        const char* bubble, const char* param)
{
    int err_code;

    err_code = pcrdr_fire_event (conn, bubble, param);
    if (err_code) {
        struct run_info *info = pcrdr_conn_get_user_data (conn);

        fprintf (stderr, "Failed to fire event %s/%s with parameter %s: %s\n",
                info->self_endpoint, bubble, param, pcrdr_get_err_message (err_code));
        if (err_code == PURCRDR_EC_SERVER_ERROR) {
            int ret_code = pcrdr_conn_get_last_ret_code (conn);
            fprintf (stderr, "Server returned message: %s (%d)\n",
                    pcrdr_get_ret_message (ret_code), ret_code);
        }
    }
    else {
        fprintf (stderr, "Event fired.\n");
    }
}

static void cb_generic_event (pcrdr_conn* conn,
        const char* from_endpoint, const char* from_bubble,
        const char* bubble_data)
{
    fprintf (stderr, "\nGot an event from (%s/%s):\n%s\n",
            from_endpoint, from_bubble, bubble_data);
}

static void on_cmd_subscribe (pcrdr_conn *conn,
        const char* endpoint, const char* bubble)
{
    int err_code;

    err_code = pcrdr_subscribe_event (conn, endpoint, bubble, cb_generic_event);

    if (err_code) {
        int ret_code = pcrdr_conn_get_last_ret_code (conn);
        fprintf (stderr, "Failed to subscribe event: %s/%s: %s (%d)\n",
                endpoint, bubble,
                pcrdr_get_err_message (err_code), ret_code);
    }
    else {
        fprintf (stderr, "Subscribed event: %s/%s\n",
                endpoint, bubble);
    }
}

static void on_cmd_unsubscribe (pcrdr_conn *conn,
        const char* endpoint, const char* bubble)
{
    int err_code;

    err_code = pcrdr_unsubscribe_event (conn, endpoint, bubble);

    if (err_code) {
        int ret_code = pcrdr_conn_get_last_ret_code (conn);
        fprintf (stderr, "Failed to unsubscribe event: %s/%s: %s (%d)\n",
                endpoint, bubble,
                pcrdr_get_err_message (err_code), ret_code);
    }
    else {
        fprintf (stderr, "Unsubscribed event: %s/%s\n",
                endpoint, bubble);
    }
}

static int on_result_list_procedures (pcrdr_conn* conn,
        const char* from_endpoint, const char* from_method,
        const char* call_id, int ret_code, const char* ret_value)
{
    if (ret_code == PURCRDR_SC_OK) {
        struct run_info *info = pcrdr_conn_get_user_data (conn);
        bool first_time = true;

        if (info->jo_endpoints) {
            first_time = false;
            json_object_put (info->jo_endpoints);
        }
        else {
        }

        info->jo_endpoints = pcrdr_json_object_from_string (ret_value,
                strlen (ret_value), 5);
        if (info->jo_endpoints == NULL) {
            ULOG_ERR ("Failed to build JSON object for endpoints:\n%s\n", ret_value);
        }
        else if (first_time) {
            json_object_to_fd (2, info->jo_endpoints,
                    JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);
            fputs ("\n", stderr);
        }

        return 0;
    }
    else if (ret_code == PURCRDR_SC_ACCEPTED) {
        ULOG_WARN ("The server accepted the call\n");
    }
    else {
        ULOG_WARN ("Unexpected return code: %d\n", ret_code);
    }

    return -1;
}

static void on_cmd_list_endpoints (pcrdr_conn* conn)
{
    struct run_info *info = pcrdr_conn_get_user_data (conn);

    if (info->jo_endpoints) {
        fputs ("ENDPOINTS:\n", stderr);
        json_object_to_fd (2, info->jo_endpoints,
                JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);
        fputs ("\n", stderr);
    }
    else {
        fputs ("WAIT A MOMENT...\n", stderr);
    }

    pcrdr_call_procedure (conn,
            info->builtin_endpoint,
            "listEndpoints",
            "",
            PURCRDR_DEF_TIME_EXPECTED,
            on_result_list_procedures, NULL);
}

static void on_cmd_show_history (pcrdr_conn* conn)
{
    int i;
    struct run_info *info = pcrdr_conn_get_user_data (conn);

    fputs ("History commands:\n", stderr);

    for (i = 0; i < LEN_HISTORY_BUF; i++) {
        if (info->history_cmds [i]) {
            fprintf (stderr, "%d) %s\n", i, info->history_cmds [i]);
        }
        else
            break;
    }
}

static void on_cmd_list_procedures (pcrdr_conn *conn,
        const char* endpoint)
{
    int err_code;
    int ret_code;
    char *ret_value;
    struct run_info *info = pcrdr_conn_get_user_data (conn);

    err_code = pcrdr_call_procedure_and_wait (conn,
            info->builtin_endpoint,
            "listProcedures",
            endpoint,
            PURCRDR_DEF_TIME_EXPECTED,
            &ret_code, &ret_value);

    if (err_code) {
        fprintf (stderr, "Failed to call listProcedures on endpoint %s: %s\n",
                endpoint, pcrdr_get_ret_message (ret_code));
    }
    else {
        pcrdr_json *jo;

        fprintf (stderr, "Procedures can be called by %s:\n", endpoint);

        jo = pcrdr_json_object_from_string (ret_value,
                strlen (ret_value), 5);
        if (jo == NULL) {
            fprintf (stderr, "Bad result:\n%s\n", ret_value);
        }
        else {
            json_object_to_fd (2, jo,
                    JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);
            fputs ("\n", stderr);
        }

        free (ret_value);
    }
}

static void on_cmd_list_events (pcrdr_conn *conn,
        const char* endpoint)
{
    int err_code;
    int ret_code;
    char *ret_value;
    struct run_info *info = pcrdr_conn_get_user_data (conn);

    err_code = pcrdr_call_procedure_and_wait (conn,
            info->builtin_endpoint,
            "listEvents",
            endpoint,
            PURCRDR_DEF_TIME_EXPECTED,
            &ret_code, &ret_value);

    if (err_code) {
        fprintf (stderr, "Failed to call listEvents for endpoint %s: %s\n",
                endpoint, pcrdr_get_ret_message (ret_code));
    }
    else {
        pcrdr_json *jo;

        fprintf (stderr, "Events can be subscribed by %s:\n", endpoint);

        jo = pcrdr_json_object_from_string (ret_value,
                strlen (ret_value), 5);
        if (jo == NULL) {
            fprintf (stderr, "Bad result:\n%s\n", ret_value);
        }
        else {
            json_object_to_fd (2, jo,
                    JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);
            fputs ("\n", stderr);
        }

        free (ret_value);
    }
}

static void on_cmd_list_subscribers (pcrdr_conn *conn,
        const char* endpoint, const char* bubble)
{
    int n, err_code;
    int ret_code;
    char *ret_value;
    struct run_info *info = pcrdr_conn_get_user_data (conn);
    char param_buff [PURCRDR_MIN_PACKET_BUFF_SIZE];

    n = snprintf (param_buff, sizeof (param_buff), 
            "{"
            "\"endpointName\": \"%s\","
            "\"bubbleName\": \"%s\""
            "}",
            endpoint,
            bubble);

    if (n < 0 || (size_t)n >= sizeof (param_buff)) {
        fprintf (stderr, "Too small parameter buffer.\n");
        return;
    }

    err_code = pcrdr_call_procedure_and_wait (conn,
            info->builtin_endpoint,
            "listEventSubscribers",
            param_buff,
            PURCRDR_DEF_TIME_EXPECTED,
            &ret_code, &ret_value);

    if (err_code) {
        fprintf (stderr, "Failed to call listEventSubscribers for endpoint bubble %s/%s: %s\n",
                endpoint, bubble, pcrdr_get_ret_message (ret_code));
    }
    else {
        pcrdr_json *jo;

        fprintf (stderr, "Subscribers of %s/%s:\n", endpoint, bubble);

        jo = pcrdr_json_object_from_string (ret_value,
                strlen (ret_value), 5);
        if (jo == NULL) {
            fprintf (stderr, "Bad result:\n%s\n", ret_value);
        }
        else {
            json_object_to_fd (2, jo,
                    JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);
            fputs ("\n", stderr);
        }

        free (ret_value);
    }
}

static void history_save_command (struct run_info *info, const char* cmd)
{
    int pos;

    if (cmd [0] == '\0')
        return;

    if (info->nr_history_cmds > 0) {
        pos = (info->nr_history_cmds - 1) % LEN_HISTORY_BUF;
        if (cmd [0] && strcasecmp (info->history_cmds [pos], cmd) == 0)
            return;
    }

    pos = info->nr_history_cmds % LEN_HISTORY_BUF;
    info->nr_history_cmds++;

    if (info->history_cmds [pos]) {
        free (info->history_cmds [pos]);
    }
    else  {
        info->history_cmds [pos] = strdup (cmd);
    }

    info->curr_history_idx = -1;
}

static void history_clear (struct run_info *info)
{
    int i;

    for (i = 0; i < LEN_HISTORY_BUF; i++) {
        if (info->history_cmds [i]) {
            free (info->history_cmds [i]);
            info->history_cmds [i] = NULL;
        }
    }

    if (info->saved_buff) {
        free (info->saved_buff);
        info->saved_buff = NULL;
    }

    info->curr_history_idx = -1;
}

static const char* history_get_next (struct run_info *info)
{
    int pos;

    if (info->nr_history_cmds <= 0)
        return NULL;

    if (info->curr_history_idx < 0) {
        info->curr_history_idx = 0;
    }
    else if (info->curr_history_idx < info->nr_history_cmds - 1) {
        info->curr_history_idx++;
    }
    else {
        info->curr_history_idx = -1;
        return NULL;
    }

    pos = info->curr_history_idx % LEN_HISTORY_BUF;
    return info->history_cmds [pos];
}

static const char* history_get_prev (struct run_info *info)
{
    int pos;

    if (info->nr_history_cmds <= 0)
        return NULL;

    if (info->curr_history_idx < 0) {
        info->curr_history_idx = info->nr_history_cmds - 1;
    }
    else if (info->curr_history_idx > 0) {
        info->curr_history_idx--;
    }
    else {
        info->curr_history_idx = -1;
        return NULL;
    }

    pos = info->curr_history_idx % LEN_HISTORY_BUF;
    return info->history_cmds [pos];
}

static void use_history_command (pcrdr_conn* conn, bool prev)
{
    struct run_info *info = pcrdr_conn_get_user_data (conn);
    const char* cmd;
   
    if (info->edited) {
        if (info->saved_buff)
            free (info->saved_buff);

        if (info->edit_buff[0]) {
            info->saved_buff = strdup (info->edit_buff);
        }
        else {
            info->saved_buff = strdup ("");
        }
    }

    if (prev)
        cmd = history_get_prev (info);
    else
        cmd = history_get_next (info);

    if (cmd == NULL) {
        if (info->saved_buff)
            cmd = info->saved_buff;
        else
            cmd = "";
        console_beep ();
    }

    if (cmd) {
        int len = strlen (cmd);

        assert (len < LEN_EDIT_BUFF);

        console_print_prompt (conn, false);
        fputs (cmd, stderr);
        strcpy (info->edit_buff, cmd);
        info->edited = false;
        info->curr_edit_pos = len;
    }
}

static void on_confirm_command (pcrdr_conn *conn)
{
    int i;
    struct run_info *info = pcrdr_conn_get_user_data (conn);
    struct cmd_info* curr_cmd = NULL;
    const char* cmd;
    const char* args [NR_CMD_ARGS] = { NULL };
    int *arg_types;
    char *saveptr;

    fputs ("\n", stderr);

    if (info->edit_buff [0] == '\0')
        goto done;

    history_save_command (info, info->edit_buff);

    cmd = strtok_r (info->edit_buff, " ", &saveptr);
    if (cmd == NULL) {
        goto bad_cmd_line;
    }

    for (i = 0; i < (int)TABLESIZE (sg_cmd_info); i++) {
        if (strcasecmp (cmd, sg_cmd_info[i].short_name) == 0
                || strcasecmp (cmd, sg_cmd_info[i].long_name) == 0) {

            curr_cmd = sg_cmd_info + i;
            break;
        }
    }

    if (curr_cmd == NULL) {
        goto bad_cmd_line;
    }

    // check arguments
    arg_types = &curr_cmd->type_1st_arg;
    for (i = 0; i < NR_CMD_ARGS; i++) {
        if (arg_types [i] == AT_NONE) {
            args [i] = NULL;
        }
        else if (i < (NR_CMD_ARGS - 1)) {
            args [i] = strtok_r (NULL, " ", &saveptr);
            if (args [i] == NULL)
                goto bad_cmd_line;
        }
        else {
            args [i] = strtok_r (NULL, "", &saveptr);
            if (args [i] == NULL)
                goto bad_cmd_line;
        }

        switch (arg_types [i]) {
            case AT_NONE:
                break;

            case AT_ENDPOINT:
                if (!pcrdr_is_valid_endpoint_name (args [i]))
                    goto bad_cmd_line;
                break;

            case AT_METHOD:
                if (!pcrdr_is_valid_method_name (args [i]))
                    goto bad_cmd_line;
                break;

            case AT_BUBBLE:
                if (!pcrdr_is_valid_bubble_name (args [i]))
                    goto bad_cmd_line;
                break;

            case AT_GAME:
                if (!pcrdr_is_valid_token (args [i], LEN_GAME_NAME))
                    goto bad_cmd_line;
                break;

            case AT_INTEGER:
                {
                    char* endptr;
                    long l = strtol (args [i], &endptr, 0);
                    if (l == 0 && endptr == args [i])
                        goto bad_cmd_line;

                    break;
                }

            case AT_STRING:
                break;

            case AT_JSON:
                break;
        }
    }

    switch (curr_cmd->cmd) {
        case CMD_HELP:
            on_cmd_help (conn);
            break;

        case CMD_EXIT:
            on_cmd_exit (conn);
            return;

        case CMD_PLAY:
            on_cmd_play (conn, args[0], strtol (args [1], NULL, 0), args [3]);
            break;

        case CMD_REGISTER_METHOD:
            on_cmd_register_method (conn, args[1], args [3]);
            break;

        case CMD_REVOKE_METHOD:
            on_cmd_revoke_method (conn, args[1]);
            break;

        case CMD_SET_RETURN_VALUE:
            on_cmd_set_return_value (conn, args[1], args [3]);
            break;

        case CMD_CALL:
            on_cmd_call (conn, args[0], args [1], args [3]);
            break;

        case CMD_REGISTER_EVENT:
            on_cmd_register_event (conn, args[1], args [3]);
            break;

        case CMD_REVOKE_EVENT:
            on_cmd_revoke_event (conn, args[1]);
            break;

        case CMD_FIRE:
            on_cmd_fire (conn, args[1], args [3]);
            break;

        case CMD_SUBSCRIBE:
            on_cmd_subscribe (conn, args[0], args [1]);
            break;

        case CMD_UNSUBSCRIBE:
            on_cmd_unsubscribe (conn, args[0], args [1]);
            break;

        case CMD_LIST_ENDPOINTS:
            on_cmd_list_endpoints (conn);
            break;

        case CMD_LIST_PROCEDURES:
            on_cmd_list_procedures (conn, args[0]);
            break;

        case CMD_LIST_EVENTS:
            on_cmd_list_events (conn, args[0]);
            break;

        case CMD_LIST_SUBSCRIBERS:
            on_cmd_list_subscribers (conn, args[0], args[1]);
            break;

        default:
            break;
    }

done:
    console_print_prompt (conn, true);
    return;

bad_cmd_line:
    if (curr_cmd) {
        fputs ("Bad arguments; sample:\n", stderr);
        fputs (curr_cmd->sample, stderr);
        fputs ("\n", stderr);
    }
    else {
        on_cmd_help (conn);
    }

    console_print_prompt (conn, true);
}

static void on_append_char (pcrdr_conn *conn, int ch)
{
    struct run_info *info = pcrdr_conn_get_user_data (conn);

    if (info->curr_edit_pos < LEN_EDIT_BUFF) {
        info->edit_buff [info->curr_edit_pos++] = ch;
        info->edit_buff [info->curr_edit_pos] = '\0';
        info->edited = true;

        putc (ch, stderr);
    }
    else {
        putc (0x07, stderr);    // beep
    }
}

static void on_delete_char (pcrdr_conn *conn)
{
    struct run_info *info = pcrdr_conn_get_user_data (conn);

    if (info->curr_edit_pos > 0) {
        info->edit_buff [--info->curr_edit_pos] = '\0';
        info->edited = true;

        fputs ("\x1B[1D\x1B[1X", stderr);
    }
    else {
        putc (0x07, stderr);    // beep
    }
}

static void handle_tty_input (pcrdr_conn *conn)
{
    struct run_info *info = pcrdr_conn_get_user_data (conn);
    ssize_t n;
    char buff [256];

    assert (info);
    while ((n = read (info->ttyfd, buff, 256)) > 0) {
        ssize_t i;

        buff [n] = '\0';
        for (i = 0; i < n; i++) {
            if (buff [i] == '\r') {
                // confirm user's input
                // fputs ("CR", stderr);
                on_confirm_command (conn);
            }
            else if (buff [i] == '\n') {
                // confirm user's input
                // fputs ("NL", stderr);
                on_confirm_command (conn);
            }
            else if (buff [i] == '\t') {
                // confirm user's input
                // fputs ("HT", stderr);
            }
            else if (buff [i] == '\b') {
                // backspace
                // fputs ("BS", stderr);
            }
            else if (buff [i] == 0x7f) {
                // backspace
                // fputs ("DEL", stderr);
                on_delete_char (conn);
            }
            else if (buff [i] == 0x1B) {
                // an escape sequence.
                if (buff [i + 1] == 0) {
                    fputs ("ESC", stderr);
                    i += 1;
                    on_cmd_exit (conn);
                }
                else if (strncmp (buff + i, "\x1b\x5b\x41", 3) == 0) {
                    //fputs ("UP", stderr);
                    use_history_command (conn, true);
                    i += 3;
                }
                else if (strncmp (buff + i, "\x1b\x5b\x42", 3) == 0) {
                    //fputs ("DOWN", stderr);
                    use_history_command (conn, false);
                    i += 3;
                }
                else if (strncmp (buff + i, "\x1b\x5b\x43", 3) == 0) {
                    // fputs ("RIGHT", stderr);
                    i += 3;
                }
                else if (strncmp (buff + i, "\x1b\x5b\x44", 3) == 0) {
                    // fputs ("LEFT", stderr);
                    i += 3;
                }
                else if (strncmp (buff + i, "\x1B\x5B\x33\x7E", 4) == 0) {
                    // fputs ("Del", stderr);
                    i += 4;
                }
                else if (strncmp (buff + i, "\x1B\x5B\x32\x7E", 4) == 0) {
                    // fputs ("Ins", stderr);
                    i += 4;
                }
                else if (strncmp (buff + i, "\x1B\x5B\x48", 3) == 0) {
                    // fputs ("Home", stderr);
                    i += 3;
                }
                else if (strncmp (buff + i, "\x1B\x5B\x46", 3) == 0) {
                    // fputs ("End", stderr);
                    i += 3;
                }
                else if (strncmp (buff + i, "\x1B\x5B\x35\x7E", 4) == 0) {
                    // fputs ("PgUp", stderr);
                    i += 4;
                }
                else if (strncmp (buff + i, "\x1B\x5B\x36\x7E", 4) == 0) {
                    // fputs ("PgDn", stderr);
                    i += 4;
                }
                else if (strncmp (buff + i, "\x1B\x4F\x50", 3) == 0) {
                    fputs ("F1\n", stderr);
                    i += 3;
                    on_cmd_help (conn);
                    console_print_prompt (conn, true);
                }
                else if (strncmp (buff + i, "\x1B\x4F\x51", 3) == 0) {
                    fputs ("F2\n", stderr);
                    i += 3;
                    on_cmd_list_endpoints (conn);
                    console_print_prompt (conn, true);
                }
                else if (strncmp (buff + i, "\x1B\x4F\x52", 3) == 0) {
                    fputs ("F3\n", stderr);
                    i += 3;
                    on_cmd_show_history (conn);
                    console_print_prompt (conn, true);
                }
                else if (strncmp (buff + i, "\x1B\x4F\x53", 3) == 0) {
                    //fputs ("F4", stderr);
                    i += 4;
                }
                else if (strncmp (buff + i, "\x1B\x5B\x31\x35\x7E", 5) == 0) {
                    //fputs ("F5", stderr);
                    i += 5;
                }
                else if (strncmp (buff + i, "\x1B\x5B\x31\x37\x7E", 5) == 0) {
                    //fputs ("F6", stderr);
                    i += 5;
                }
                else if (strncmp (buff + i, "\x1B\x5B\x31\x38\x7E", 5) == 0) {
                    //fputs ("F7", stderr);
                    i += 5;
                }
                else if (strncmp (buff + i, "\x1B\x5B\x31\x39\x7E", 5) == 0) {
                    //fputs ("F8", stderr);
                    i += 5;
                }
                else {
                    while (buff [i]) {
                        // fprintf (stderr, "\\x%X", buff[i]);
                        i++;
                    }
                }
            }
            else if (buff [i]) {
                on_append_char (conn, buff[i]);
            }
        }
    }
}

static const char *a_json =
"{"
    "\"packetType\": \"result\","
    "\"resultId\": \"RESULTXX-000000005FDAC261-000000001BED7939-0000000000000001\","
    "\"callId\": \"CALLXXXX-000000005FDAC261-000000001BEC6766-0000000000000000\","
    "\"fromEndpoint\": \"@localhost/cn.fmsoft.hybridos.hibus/builtin\","
    "\"fromMethod\": \"echo\","
    "\"timeDiff\": 0.000047,"
    "\"timeConsumed\": 0.000000,"
    "\"retCode\": 200,"
    "\"retMsg\": \"Ok\","
    "\"retValue\": \"I am here\""
"}";

static const char* my_echo_method (pcrdr_conn* conn,
        const char* from_endpoint, const char* to_method,
        const char* method_param, int *err_code)
{
    *err_code = 0;
    return method_param;

#if 0
    char *ret_value = calloc (1, 9001);
    memset(ret_value, 'c', 9000);
    return ret_value;
#endif
}

static int my_echo_result (pcrdr_conn* conn,
        const char* from_endpoint, const char* from_method,
        const char* call_id,
        int ret_code, const char* ret_value)
{
    if (ret_code == PURCRDR_SC_OK) {
        ULOG_INFO ("Got the result: %s\n", ret_value);
        return 0;
    }
    else if (ret_code == PURCRDR_SC_ACCEPTED) {
        ULOG_WARN ("The server accepted the call\n");
    }
    else {
        ULOG_WARN ("Unexpected return code: %d\n", ret_code);
    }

    return -1;
}

static void format_current_time (char* buff, size_t sz)
{
    struct tm tm;
    time_t curr_time = time (NULL);

    localtime_r (&curr_time, &tm);
    strftime (buff, sz, "%H:%M", &tm);
}

static int test_basic_functions (pcrdr_conn *conn)
{
    pcrdr_json *jo;

    int err_code, ret_code;
    char *ret_value;
    struct run_info *info = pcrdr_conn_get_user_data (conn);

    pcrdr_json_packet_to_object (a_json, strlen (a_json), &jo);
    if (jo == NULL) {
        ULOG_ERR ("Bad JSON: \n%s\n", a_json);
    }
    else {
        ULOG_INFO ("pcrdr_json_packet_to_object passed\n");
        json_object_put (jo);
    }

    /* call echo method of the builtin endpoint */
    err_code = pcrdr_call_procedure_and_wait (conn,
            info->builtin_endpoint,
            "echo",
            "I am here",
            PURCRDR_DEF_TIME_EXPECTED,
            &ret_code, &ret_value);

    if (err_code) {
        ULOG_ERR ("Failed to call pcrdr_call_procedure_and_wait: %s\n",
                pcrdr_get_err_message (err_code));
    }
    else {
        ULOG_INFO ("Got the result for `echo` method: %s (%d)\n",
                ret_value ? ret_value : "(null)", ret_code);
    }

    err_code = pcrdr_register_event (conn, "alarm", "*", "*");
    ULOG_INFO ("error message for pcrdr_register_event: %s (%d)\n",
            pcrdr_get_err_message (err_code), err_code);

    err_code = pcrdr_fire_event (conn, "alarm", "12:00");
    ULOG_INFO ("error message for pcrdr_fire_event: %s (%d)\n",
            pcrdr_get_err_message (err_code), err_code);

    err_code = pcrdr_revoke_event (conn, "alarm");
    ULOG_INFO ("error message for pcrdr_revoke_event: %s (%d)\n",
            pcrdr_get_err_message (err_code), err_code);

    err_code = pcrdr_register_procedure_const (conn, "echo", NULL, NULL, my_echo_method);
    ULOG_INFO ("error message for pcrdr_register_procedure: %s (%d)\n",
            pcrdr_get_err_message (err_code), err_code);

    /* call echo method of myself */
    err_code = pcrdr_call_procedure_and_wait (conn,
            info->self_endpoint,
            "echo",
            "I AM HERE",
            PURCRDR_DEF_TIME_EXPECTED,
            &ret_code, &ret_value);

    if (err_code) {
        ULOG_ERR ("Failed to call pcrdr_call_procedure_and_wait: %s\n",
                pcrdr_get_err_message (err_code));
    }
    else {
        ULOG_INFO ("Got the result for `echo` method: %s (%d)\n",
                ret_value ? ret_value : "(null)", ret_code);
    }

    err_code = pcrdr_revoke_procedure (conn, "echo");
    ULOG_INFO ("error message for pcrdr_revoke_procedure: %s (%d)\n",
            pcrdr_get_err_message (err_code), err_code);

    if (err_code == PURCRDR_EC_SERVER_ERROR) {
        int ret_code = pcrdr_conn_get_last_ret_code (conn);
        ULOG_INFO ("last return code: %d (%s)\n",
                ret_code, pcrdr_get_ret_message (ret_code));
    }

    return err_code;
}

static void on_new_broken_endpoint (pcrdr_conn* conn,
        const char* from_endpoint, const char* from_bubble,
        const char* bubble_data)
{
    pcrdr_json *jo = pcrdr_json_object_from_string (bubble_data,
            strlen (bubble_data), 2);
    if (jo == NULL) {
        ULOG_ERR ("Failed to parse bubbleData:\n%s\n", bubble_data);
        return;
    }

    if (strcasecmp (from_bubble, "NEWENDPOINT") == 0) {
        fputs ("NEW ENDPOINT:\n", stderr);
        json_object_to_fd (2, jo,
                    JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);
    }
    else if (strcasecmp (from_bubble, "BROKENENDPOINT") == 0) {
        fputs ("LOST ENDPOINT:\n", stderr);
        json_object_to_fd (2, jo,
                    JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);
    }

    json_object_put (jo);
}

/* Command line help. */
static void print_usage (void)
{
    printf ("PurCRDRCL (%s) - the command line of data bus system for HybridOS\n\n", PURCRDR_VERSION);

    printf (
            "Usage: "
            "hibuscl [ options ... ]\n\n"
            ""
            "The following options can be supplied to the command:\n\n"
            ""
            "  -a --app=<app_name>          - Connect to PurCRDR with the specified app name.\n"
            "  -r --runner=<runner_name>    - Connect to PurCRDR with the specified runner name.\n"
            "  -h --help                    - This help.\n"
            "  -v --version                 - Display version information and exit.\n"
            "\n"
            );
}

static char short_options[] = "a:r:vh";
static struct option long_opts[] = {
    {"app"            , required_argument , NULL , 'a' } ,
    {"runner"         , required_argument , NULL , 'r' } ,
    {"version"        , no_argument       , NULL , 'v' } ,
    {"help"           , no_argument       , NULL , 'h' } ,
    {0, 0, 0, 0}
};

static int read_option_args (int argc, char **argv)
{
    int o, idx = 0;

    while ((o = getopt_long (argc, argv, short_options, long_opts, &idx)) >= 0) {
        if (-1 == o || EOF == o)
            break;
        switch (o) {
            case 'h':
                print_usage ();
                return -1;
            case 'v':
                fprintf (stdout, "PurCRDRCL: %s\n", PURCRDR_VERSION);
                return -1;
            case 'a':
                if (strlen (optarg) < PURCRDR_LEN_APP_NAME)
                    strcpy (the_client.app_name, optarg);
                break;
            case 'r':
                if (strlen (optarg) < PURCRDR_LEN_RUNNER_NAME)
                    strcpy (the_client.runner_name, optarg);
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

int main (int argc, char **argv)
{
    int cnnfd = -1, ttyfd = -1, maxfd;
    pcrdr_conn* conn;
    fd_set rfds;
    struct timeval tv;
    char curr_time [16];

    print_copying ();

    if (read_option_args (argc, argv)) {
        return EXIT_SUCCESS;
    }

    if (!the_client.app_name[0] ||
            !pcrdr_is_valid_app_name (the_client.app_name)) {
        strcpy (the_client.app_name, PURCRDR_APP_HIBUS);
    }

    if (!the_client.runner_name[0] ||
            !pcrdr_is_valid_runner_name (the_client.runner_name)) {
        strcpy (the_client.runner_name, PURCRDR_RUNNER_CMDLINE);
    }

    ulog_open (-1, -1, "PurCRDRCL");

    kvlist_init (&the_client.ret_value_list, NULL);
    the_client.running = true;
    the_client.last_sigint_time = 0;
    if (setup_signals () < 0)
        goto failed;

    if ((ttyfd = setup_tty ()) < 0)
        goto failed;

    cnnfd = pcrdr_connect_via_unix_socket (PURCRDR_US_PATH,
            the_client.app_name, the_client.runner_name, &conn);

    if (cnnfd < 0) {
        fprintf (stderr, "Failed to connect to PurCRDR server: %s\n",
                pcrdr_get_err_message (cnnfd));
        goto failed;
    }

    pcrdr_assemble_endpoint_name (
            pcrdr_conn_srv_host_name (conn),
            PURCRDR_APP_HIBUS, PURCRDR_RUNNER_BUILITIN,
            the_client.builtin_endpoint);

    pcrdr_assemble_endpoint_name (
            pcrdr_conn_own_host_name (conn),
            the_client.app_name, the_client.runner_name,
            the_client.self_endpoint);

    the_client.ttyfd = ttyfd;
    the_client.curr_history_idx = -1;
    pcrdr_conn_set_user_data (conn, &the_client);

    if (test_basic_functions (conn))
        goto failed;

    format_current_time (curr_time, sizeof (curr_time) - 1);

    int err_code;
    err_code = pcrdr_register_procedure_const (conn, "echo", NULL, NULL, my_echo_method);
    ULOG_INFO ("error message for pcrdr_register_procedure: %s (%d)\n",
            pcrdr_get_err_message (err_code), err_code);

    err_code = pcrdr_call_procedure (conn,
            the_client.self_endpoint,
            "echo",
            "I AM HERE AGAIN",
            PURCRDR_DEF_TIME_EXPECTED,
            my_echo_result, NULL);
    ULOG_INFO ("error message for pcrdr_call_procedure: %s (%d)\n",
            pcrdr_get_err_message (err_code), err_code);

    err_code = pcrdr_register_event (conn, "clock", NULL, NULL);
    ULOG_INFO ("error message for pcrdr_register_event: %s (%d)\n",
            pcrdr_get_err_message (err_code), err_code);

    err_code = pcrdr_subscribe_event (conn, the_client.self_endpoint, "clock",
            cb_generic_event);
    ULOG_INFO ("error message for pcrdr_subscribe_event: %s (%d)\n",
            pcrdr_get_err_message (err_code), err_code);

    err_code = pcrdr_fire_event (conn, "clock", curr_time);
    ULOG_INFO ("error message for pcrdr_fire_event: %s (%d)\n",
            pcrdr_get_err_message (err_code), err_code);

    if ((err_code = pcrdr_subscribe_event (conn,
                    the_client.builtin_endpoint, "NEWENDPOINT",
                    on_new_broken_endpoint))) {
        fprintf (stderr, "Failed to subscribe builtin event `NEWENDPOINT` (%d): %s\n",
                err_code, pcrdr_get_err_message (err_code));
    }

    if ((err_code = pcrdr_subscribe_event (conn,
                    the_client.builtin_endpoint, "BROKENENDPOINT",
                    on_new_broken_endpoint))) {
        fprintf (stderr, "Failed to subscribe builtin event `BROKENENDPOINT` (%d): %s\n",
                err_code, pcrdr_get_err_message (err_code));
    }

    console_print_prompt (conn, true);
    maxfd = cnnfd > ttyfd ? cnnfd : ttyfd;
    do {
        int retval;
        char _new_clock [16];

        FD_ZERO (&rfds);
        FD_SET (cnnfd, &rfds);
        FD_SET (ttyfd, &rfds);

        tv.tv_sec = 0;
        tv.tv_usec = 200 * 1000;
        retval = select (maxfd + 1, &rfds, NULL, NULL, &tv);

        if (retval == -1) {
            if (errno == EINTR)
                continue;
            else
                break;
        }
        else if (retval) {
            if (FD_ISSET (cnnfd, &rfds)) {
                int err_code = pcrdr_read_and_dispatch_packet (conn);
                if (err_code) {
                    fprintf (stderr, "Failed to read and dispatch packet: %s\n",
                            pcrdr_get_err_message (err_code));
                    if (err_code == PURCRDR_EC_IO)
                        break;
                }

                console_print_prompt (conn, true);
            }
            else if (FD_ISSET (ttyfd, &rfds)) {
                handle_tty_input (conn);
            }
        }
        else {
            format_current_time (_new_clock, sizeof (_new_clock) - 1);
            if (strcmp (_new_clock, curr_time)) {
                pcrdr_fire_event (conn, "clock", _new_clock);
                strcpy (curr_time, _new_clock);
            }
        }

        if (pcrdr_get_monotoic_time () > the_client.last_sigint_time + 5) {
            // cancel quit
            the_client.last_sigint_time = 0;
        }

    } while (the_client.running);

    if ((err_code = pcrdr_unsubscribe_event (conn, the_client.builtin_endpoint,
                    "NEWENDPOINT"))) {
        fprintf (stderr, "Failed to unsubscribe builtin event `NEWENDPOINT` (%d): %s\n",
                err_code, pcrdr_get_err_message (err_code));
    }

    if ((err_code = pcrdr_unsubscribe_event (conn, the_client.builtin_endpoint,
                    "BROKENENDPOINT"))) {
        fprintf (stderr, "Failed to unsubscribe builtin event `BROKENENDPOINT` (%d): %s\n",
                err_code, pcrdr_get_err_message (err_code));
    }

    // cleanup
    json_object_put (the_client.jo_endpoints);

    {
        const char* name;
        void* data;

        kvlist_for_each (&the_client.ret_value_list, name, data) {
            char* value = *(char **)data;

            free (value);
        }

        kvlist_free (&the_client.ret_value_list);
    }

    history_clear (&the_client);

    fputs ("\n", stderr);

failed:
    if (ttyfd >= 0)
        restore_tty (ttyfd);

    if (cnnfd >= 0)
        pcrdr_disconnect (conn);

    ulog_close ();
    return 0;
}

