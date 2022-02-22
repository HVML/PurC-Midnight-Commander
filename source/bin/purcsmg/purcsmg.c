/*
** purcsmg.c - The simple markup generator for PurCMC renderer.
**
** Copyright (C) 2021, 2022 FMSoft (http://www.fmsoft.cn)
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
#include <getopt.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termio.h>
#include <signal.h>
#include <assert.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "purcmc_version.h"
#include "purcsmg.h"

struct run_info the_client;

static void print_copying (void)
{
    fprintf (stdout,
        "\n"
        "PurCSMG - a simple markup generator interacting with PurCMC renderer.\n"
        "\n"
        "Copyright (C) 2021, 2022 FMSoft <https://www.fmsoft.cn>\n"
        "\n"
        "PurCSMG is free software: you can redistribute it and/or modify\n"
        "it under the terms of the GNU General Public License as published by\n"
        "the Free Software Foundation, either version 3 of the License, or\n"
        "(at your option) any later version.\n"
        "\n"
        "PurCSMG is distributed in the hope that it will be useful,\n"
        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
        "GNU General Public License for more details.\n"
        "You should have received a copy of the GNU General Public License\n"
        "along with this program.  If not, see http://www.gnu.org/licenses/.\n"
        );
    fprintf (stdout, "\n");
}

static void format_current_time (char* buff, size_t sz)
{
    struct tm tm;
    time_t curr_time = time (NULL);

    localtime_r (&curr_time, &tm);
    strftime (buff, sz, "%H:%M", &tm);
}

static char buffer_a[4096];
static char buffer_b[4096];

struct buff_info {
    char *  buf;
    size_t  size;
    off_t   pos;
};

static ssize_t write_to_buf (void *ctxt, const void *buf, size_t count)
{
    struct buff_info *info = (struct buff_info *)ctxt;

    if (info->pos + count <= info->size) {
        memcpy (info->buf + info->pos, buf, count);
        info->pos += count;
        return count;
    }
    else {
        ssize_t n = info->size - info->pos;

        if (n > 0) {
            memcpy (info->buf + info->pos, buf, n);
            info->pos += n;
            return n;
        }

        return 0;
    }

    return -1;
}

static int serialize_and_parse_again(const pcrdr_msg *msg)
{
    int ret;
    pcrdr_msg *msg_parsed;
    struct buff_info info_a = { buffer_a, sizeof (buffer_a), 0 };
    struct buff_info info_b = { buffer_b, sizeof (buffer_b), 0 };

    pcrdr_serialize_message (msg, write_to_buf, &info_a);
    buffer_a[info_a.pos] = '\0';
    puts ("Serialized original message: \n");
    puts (buffer_a);
    puts ("<<<<<<<<\n");

    if ((ret = pcrdr_parse_packet (buffer_a, info_a.pos, &msg_parsed))) {
        printf ("Failed pcrdr_parse_packet: %s\n",
                purc_get_error_message (ret));
        return ret;
    }

    pcrdr_serialize_message (msg_parsed, write_to_buf, &info_b);
    buffer_b[info_b.pos] = '\0';
    puts ("Serialized parsed message: \n");
    puts (buffer_b);
    puts ("<<<<<<<<\n");

    ret = pcrdr_compare_messages(msg, msg_parsed);
    pcrdr_release_message(msg_parsed);

    return ret;
}

static int test_basic_functions (void)
{
    int ret;
    pcrdr_msg *msg;

    msg = pcrdr_make_request_message (PCRDR_MSG_TARGET_SESSION,
            random(), "to_do_something", NULL,
            PCRDR_MSG_ELEMENT_TYPE_VOID, NULL, NULL,
            PCRDR_MSG_DATA_TYPE_TEXT, "The data", 0);

    ret = serialize_and_parse_again(msg);
    pcrdr_release_message(msg);

    if (ret) {
        puts ("Failed serialize_and_parse_again\n");
        return ret;
    }

    return 0;
}

/* Command line help. */
static void print_usage (void)
{
    printf ("PurCSMG (%s) - "
            "a simple markup generator interacting with PurCMC renderer\n\n",
            MC_CURRENT_VERSION);

    printf (
            "Usage: "
            "purcsmg [ options ... ]\n\n"
            ""
            "The following options can be supplied to the command:\n\n"
            ""
            "  -a --app=<app_name>          - Connect to PurcMC renderer with the specified app name.\n"
            "  -r --runner=<runner_name>    - Connect to PurcMC renderer with the specified runner name.\n"
            "  -f --file=<html_file>        - The initial HTML file to load.\n"
            "  -c --cmdline                 - Use command line.\n"
            "  -v --version                 - Display version information and exit.\n"
            "  -h --help                    - This help.\n"
            "\n"
            );
}

static char short_options[] = "a:r:f:cvh";
static struct option long_opts[] = {
    {"app"            , required_argument , NULL , 'a' } ,
    {"runner"         , required_argument , NULL , 'r' } ,
    {"file"           , required_argument , NULL , 'f' } ,
    {"cmdline"        , no_argument       , NULL , 'c' } ,
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
            fprintf (stdout, "PurCSMG: %s\n", MC_CURRENT_VERSION);
            return -1;
        case 'a':
            if (strlen (optarg) < PCRDR_LEN_APP_NAME)
                strcpy (the_client.app_name, optarg);
            break;
        case 'r':
            if (strlen (optarg) < PCRDR_LEN_RUNNER_NAME)
                strcpy (the_client.runner_name, optarg);
            break;
        case 'f':
            the_client.initial_file = strdup (optarg);
            break;
        case 'c':
            the_client.use_cmdline = true;
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
    int ret, cnnfd = -1, ttyfd = -1, maxfd;
    pcrdr_conn* conn;
    fd_set rfds;
    struct timeval tv;
    char curr_time [16];

    purc_instance_extra_info extra_info = {
        .renderer_prot = PURC_RDRPROT_PURCMC,
        .renderer_uri = "unix://" PCRDR_PURCMC_US_PATH,
        .enable_remote_fetcher = false,
    };

    print_copying ();

    if (read_option_args (argc, argv)) {
        return EXIT_FAILURE;
    }

    if (!the_client.app_name[0])
        strcpy (the_client.app_name, "cn.fmsoft.app.purcsmg");
    if (!the_client.runner_name[0])
        strcpy (the_client.runner_name, "cmdline");

    ret = purc_init_ex (PURC_MODULE_PCRDR, the_client.app_name,
            the_client.runner_name, &extra_info);
    if (ret != PURC_ERROR_OK) {
        fprintf (stderr, "Failed to initialize the PurC instance: %s\n",
                purc_get_error_message (ret));
        return EXIT_FAILURE;
    }

    if (test_basic_functions ()) {
        return EXIT_FAILURE;
    }

    kvlist_init (&the_client.ret_value_list, NULL);
    the_client.running = true;
    the_client.last_sigint_time = 0;

    if (the_client.use_cmdline && (ttyfd = setup_tty ()) < 0)
        goto failed;

    conn = purc_get_conn_to_renderer();
    assert(conn != NULL);
    cnnfd = pcrdr_conn_socket_fd(conn);
    assert(cnnfd >= 0);

    the_client.ttyfd = ttyfd;
    the_client.curr_history_idx = -1;
    pcrdr_conn_set_user_data (conn, &the_client);

    format_current_time (curr_time, sizeof (curr_time) - 1);

    if (ttyfd >= 0) cmdline_print_prompt (conn, true);
    maxfd = cnnfd > ttyfd ? cnnfd : ttyfd;

    do {
        int retval;
        char _new_clock [16];

        FD_ZERO (&rfds);
        FD_SET (cnnfd, &rfds);
        if (ttyfd >= 0)
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
                int err_code = pcrdr_read_and_dispatch_message (conn);
                if (err_code < 0) {
                    fprintf (stderr, "Failed to read and dispatch message: %s\n",
                            purc_get_error_message (err_code));
                    if (err_code == PCRDR_ERROR_IO)
                        break;
                }

                if (ttyfd >= 0) cmdline_print_prompt (conn, true);
            }
            else if (ttyfd >= 0 && FD_ISSET (ttyfd, &rfds)) {
                handle_tty_input (conn);
            }
        }
        else {
            format_current_time (_new_clock, sizeof (_new_clock) - 1);
            if (strcmp (_new_clock, curr_time)) {
                strcpy (curr_time, _new_clock);
                pcrdr_ping_renderer (conn);
            }
        }

        if (pcrdr_get_monotoic_time () > the_client.last_sigint_time + 5) {
            // cancel quit
            the_client.last_sigint_time = 0;
        }

    } while (the_client.running);

    fputs ("\n", stderr);

failed:
    if (the_client.initial_file)
        free (the_client.initial_file);

    if (ttyfd >= 0)
        restore_tty (ttyfd);

    purc_cleanup ();

    return EXIT_SUCCESS;
}

