/*
** cmdline.c -- The simple markup generator for PurCMC renderer.
**
** Copyright (c) 2021 FMSoft (http://www.fmsoft.cn)
**
** Author: Vincent Wei (https://github.com/VincentWei)
**
** This file is part of PurC Midnight Commander.
**
** PurCSMG is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** PurCSMG is distributed in the hope that it will be useful,
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
#include <errno.h>
#include <assert.h>

#include <termio.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "purcmc_version.h"
#include "purcsmg.h"

#define ULOG_INFO(fmt, ...) printf(fmt, ## __VA_ARGS__)
#define ULOG_NOTE(fmt, ...) printf(fmt, ## __VA_ARGS__)
#define ULOG_WARN(fmt, ...) printf(fmt, ## __VA_ARGS__)
#define ULOG_ERR(fmt, ...) fprintf(stderr, fmt, ## __VA_ARGS__)

struct run_info the_client;

/* command identifiers */
enum {
    CMD_HELP = 0,
    CMD_EXIT,
    CMD_RESET,
    CMD_WRITE,
    CMD_LOAD,
    CMD_UPDATE,
    CMD_APPEND,
    CMD_PREPEND,
    CMD_INSERT_BEFORE,
    CMD_INSERT_AFTER,
    CMD_CLEAR,
    CMD_ERASE,
    CMD_SHOW,
};

/* argument type */
enum {
    AT_NONE = 0,
    AT_INTEGER,
    AT_STRING,
    AT_EJSON,
    AT_MAX_NR_ARGS = AT_EJSON,
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
    { CMD_RESET,
        "reset", "r",
        "reset",
        AT_NONE, AT_NONE, AT_NONE, AT_STRING, },
    { CMD_WRITE,
        "write", "w",
        "write <p>Hello, world!</p>",
        AT_NONE, AT_NONE, AT_NONE, AT_STRING, },
    { CMD_LOAD,
        "load", "l",
        "load test.html",
        AT_NONE, AT_NONE, AT_NONE, AT_STRING, },
    { CMD_UPDATE,
        "update", "u",
        "update 3456 textContent Hello, world",
        AT_NONE, AT_STRING, AT_STRING, AT_STRING, },
    { CMD_APPEND,
        "append", "a",
        "append 3456 <li>an item</li>",
        AT_NONE, AT_NONE, AT_STRING, AT_STRING, },
    { CMD_PREPEND,
        "prepend", "p",
        "prepend 3456 <li>an item</li>",
        AT_NONE, AT_NONE, AT_STRING, AT_STRING, },
    { CMD_INSERT_BEFORE,
        "insertBefore", "ib",
        "insertBefore 3456 <li>an item</li>",
        AT_NONE, AT_NONE, AT_STRING, AT_STRING, },
    { CMD_INSERT_AFTER,
        "insertAfter", "ib",
        "insertAfter 3456 <li>an item</li>",
        AT_NONE, AT_NONE, AT_STRING, AT_STRING, },
    { CMD_CLEAR,
        "clear", "c",
        "clear 3456",
        AT_NONE, AT_NONE, AT_NONE, AT_STRING, },
    { CMD_ERASE,
        "erase", "e",
        "erase 3456",
        AT_NONE, AT_NONE, AT_NONE, AT_STRING, },
    { CMD_SHOW,
        "show", "s",
        "show 3456",
        AT_NONE, AT_NONE, AT_NONE, AT_STRING, },
};

static void handle_signal_action (int sig_number)
{
    if (sig_number == SIGINT) {
        if (the_client.last_sigint_time == 0) {
            fprintf (stderr, "\n");
            fprintf (stderr, "SIGINT caught, press <CTRL+C> again in 5 seconds to quit.\n");
            the_client.last_sigint_time = purc_get_monotoic_time ();
        }
        else if (purc_get_monotoic_time () < the_client.last_sigint_time + 5) {
            fprintf (stderr, "SIGINT caught, quit...\n");
            the_client.running = false;
        }
        else {
            fprintf (stderr, "\n");
            fprintf (stderr, "SIGINT caught, press <CTRL+C> again in 5 seconds to quit.\n");
            the_client.running = true;
            the_client.last_sigint_time = purc_get_monotoic_time ();
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

// move cursor to the start of the current line and erase whole line
static inline void cmdline_reset_line (void)
{
    fputs ("\x1B[0G\x1B[2K", stderr);
}

static inline void cmdline_beep (void)
{
    putc (0x07, stderr);
}

static void on_cmd_help (pcrdr_conn *conn)
{
    fprintf (stderr, "Commands:\n\n");
    fprintf (stderr, "  < help | h >\n");
    fprintf (stderr, "    print this help message.\n");
    fprintf (stderr, "  < exit | x >\n");
    fprintf (stderr, "    exit this PurCSMG command line program.\n");
    fprintf (stderr, "  < reset | r >\n");
    fprintf (stderr, "    reset page content.\n");
    fprintf (stderr, "  < write | w > <content>\n");
    fprintf (stderr, "    write HTML content\n");
    fprintf (stderr, "  < load | l > <file>\n");
    fprintf (stderr, "    load HTML content from a file.\n");
    fprintf (stderr, "  < update | u > <element handle> <property> <content>\n");
    fprintf (stderr, "    update a property of an element.\n");
    fprintf (stderr, "  < append | a > <element handle> <content>\n");
    fprintf (stderr, "    append content in an element.\n");
    fprintf (stderr, "  < prepend | p > <element handle> <content>\n");
    fprintf (stderr, "    prepend content in an element.\n");
    fprintf (stderr, "  < insertBefore | ib > <element handle> <content>\n");
    fprintf (stderr, "    insert content before an element.\n");
    fprintf (stderr, "  < insertAfter | ia > <element handle> <content>\n");
    fprintf (stderr, "    insert content after an element.\n");
    fprintf (stderr, "  < displace | d > <element handle> <content>\n");
    fprintf (stderr, "    displace content of an element.\n");
    fprintf (stderr, "  < clear | c > <element handle>\n");
    fprintf (stderr, "    clear content of an element.\n");
    fprintf (stderr, "  < erase | e > <element handle>\n");
    fprintf (stderr, "    erase an element and its content.\n");
    fprintf (stderr, "  < show | s > <element handle>\n");
    fprintf (stderr, "    show an element and its content.\n");
    fprintf (stderr, "\n");
    fprintf (stderr, "Shortcuts:\n\n");
    fprintf (stderr, "  <F1>\n    print this help message.\n");
    fprintf (stderr, "  <F3>\n    show history command.\n");
    fprintf (stderr, "  <ESC>\n    exit this PurCSMG command line program.\n");
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
        cmdline_beep ();
    }

    if (cmd) {
        int len = strlen (cmd);

        assert (len < LEN_EDIT_BUFF);

        cmdline_print_prompt (conn, false);
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

            case AT_EJSON:
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

        default:
            break;
    }

done:
    cmdline_print_prompt (conn, true);
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

    cmdline_print_prompt (conn, true);
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

void handle_tty_input (pcrdr_conn *conn)
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
                    cmdline_print_prompt (conn, true);
                }
                else if (strncmp (buff + i, "\x1B\x4F\x51", 3) == 0) {
                    fputs ("F2\n", stderr);
                    i += 3;
                    cmdline_print_prompt (conn, true);
                }
                else if (strncmp (buff + i, "\x1B\x4F\x52", 3) == 0) {
                    fputs ("F3\n", stderr);
                    i += 3;
                    on_cmd_show_history (conn);
                    cmdline_print_prompt (conn, true);
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

int setup_tty (void)
{
    int ttyfd;
    struct termios my_termios;

    if (setup_signals () < 0)
        return -1;

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

int restore_tty (int ttyfd)
{
    history_clear (&the_client);

    if (tcsetattr (ttyfd, TCSAFLUSH, &the_client.startup_termios) < 0)
        return -1;

    close (ttyfd);
    return 0;
}

void cmdline_print_prompt (pcrdr_conn *conn, bool reset_history)
{
    struct run_info *info = pcrdr_conn_get_user_data (conn);

    assert (info);

    cmdline_reset_line ();
    fputs ("PurCSMG >> ", stderr);

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

