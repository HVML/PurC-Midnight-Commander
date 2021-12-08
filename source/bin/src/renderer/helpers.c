/*
** helpers.c -- The helpers for the renderer server.
**
** Copyright (c) 2020, 2021 FMSoft (http://www.fmsoft.cn)
**
** Author: Vincent Wei (https://github.com/VincentWei)
**
** This file is part of PurC Midnight Commander.
**
** PurcMC is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** PurcMC is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see http://www.gnu.org/licenses/.
*/

#include <ctype.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "lib/md5.h"

#include "ulog.h"
#include "server.h"

/* Return Codes and Messages */
#define UNKNOWN_RET_CODE    "Unknown Return Code"

static struct  {
    int ret_code;
    const char* ret_msg;
} ret_code_2_messages[] = {
    { SERVER_SC_IOERR,               /* 1 */
        "I/O Error" },
    { SERVER_SC_OK,                  /* 200 */
        "Ok" },
    { SERVER_SC_CREATED,             /* 201 */
        "Created" },
    { SERVER_SC_ACCEPTED,            /* 202 */
        "Accepted" },
    { SERVER_SC_NO_CONTENT,          /* 204 */
        "No Content" },
    { SERVER_SC_RESET_CONTENT,       /* 205 */
        "Reset Content" },
    { SERVER_SC_PARTIAL_CONTENT,     /* 206 */
        "Partial Content" },
    { SERVER_SC_BAD_REQUEST,         /* 400 */
        "Bad Request" },
    { SERVER_SC_UNAUTHORIZED,        /* 401 */
        "Unauthorized" },
    { SERVER_SC_FORBIDDEN,           /* 403 */
        "Forbidden" },
    { SERVER_SC_NOT_FOUND,           /* 404 */
        "Not Found" },
    { SERVER_SC_METHOD_NOT_ALLOWED,  /* 405 */
        "Method Not Allowed" },
    { SERVER_SC_NOT_ACCEPTABLE,      /* 406 */
        "Not Acceptable" },
    { SERVER_SC_CONFLICT,            /* 409 */
        "Conflict" },
    { SERVER_SC_GONE,                /* 410 */
        "Gone" },
    { SERVER_SC_PRECONDITION_FAILED, /* 412 */
        "Precondition Failed" },
    { SERVER_SC_PACKET_TOO_LARGE,    /* 413 */
        "Packet Too Large" },
    { SERVER_SC_EXPECTATION_FAILED,  /* 417 */
        "Expectation Failed" },
    { SERVER_SC_IM_A_TEAPOT,         /* 418 */
        "I'm a teapot" },
    { SERVER_SC_UNPROCESSABLE_PACKET,    /* 422 */
        "Unprocessable Packet" },
    { SERVER_SC_LOCKED,              /* 423 */
        "Locked" },
    { SERVER_SC_FAILED_DEPENDENCY,   /* 424 */
        "Failed Dependency" },
    { SERVER_SC_FAILED_DEPENDENCY,   /* 425 */
        "Failed Dependency" },
    { SERVER_SC_UPGRADE_REQUIRED,    /* 426 */
        "Upgrade Required" },
    { SERVER_SC_RETRY_WITH,          /* 449 */
        "Retry With" },
    { SERVER_SC_UNAVAILABLE_FOR_LEGAL_REASONS,   /* 451 */
        "Unavailable For Legal Reasons" },
    { SERVER_SC_INTERNAL_SERVER_ERROR,   /* 500 */
        "Internal Server Error" },
    { SERVER_SC_NOT_IMPLEMENTED,     /* 501 */
        "Not Implemented" },
    { SERVER_SC_BAD_CALLEE,          /* 502 */
        "Bad Callee" },
    { SERVER_SC_SERVICE_UNAVAILABLE, /* 503 */
        "Service Unavailable" },
    { SERVER_SC_CALLEE_TIMEOUT,      /* 504 */
        "Callee Timeout" },
    { SERVER_SC_INSUFFICIENT_STORAGE,    /* 507 */
        "Insufficient Storage" },
};

#define TABLESIZE(table)    (sizeof(table)/sizeof(table[0]))

const char* server_get_ret_message (int ret_code)
{
    unsigned int lower = 0;
    unsigned int upper = TABLESIZE (ret_code_2_messages) - 1;
    int mid = TABLESIZE (ret_code_2_messages) / 2;

    if (ret_code < ret_code_2_messages[lower].ret_code ||
            ret_code > ret_code_2_messages[upper].ret_code)
        return UNKNOWN_RET_CODE;

    do {
        if (ret_code < ret_code_2_messages[mid].ret_code)
            upper = mid - 1;
        else if (ret_code > ret_code_2_messages[mid].ret_code)
            lower = mid + 1;
        else
            return ret_code_2_messages [mid].ret_msg;

        mid = (lower + upper) / 2;

    } while (lower <= upper);

    return UNKNOWN_RET_CODE;
}

/* Error Codes and Messages */
#define UNKNOWN_ERR_CODE    "Unknown Error Code"

static const char* err_messages [] = {
    /* 0 */
    "Everything Ok",
    /* SERVER_EC_IO (-1) */
    "IO Error",
    /* SERVER_EC_CLOSED (-2) */
    "Peer Closed",
    /* SERVER_EC_NOMEM (-3) */
    "No Enough Memory",
    /* SERVER_EC_TOO_LARGE (-4) */
    "Too Large",
    /* SERVER_EC_PROTOCOL (-5) */
    "Protocol",
    /* SERVER_EC_UPPER (-6) */
    "Upper",
    /* SERVER_EC_NOT_IMPLEMENTED (-7) */
    "Not Implemented",
    /* SERVER_EC_INVALID_VALUE (-8) */
    "Invalid Value",
    /* SERVER_EC_DUPLICATED (-9) */
    "Duplicated",
    /* SERVER_EC_TOO_SMALL_BUFF (-10) */
    "Too Small Buffer",
    /* SERVER_EC_BAD_SYSTEM_CALL (-11) */
    "Bad System Call",
    /* SERVER_EC_AUTH_FAILED (-12) */
    "Authentication Failed",
    /* SERVER_EC_SERVER_ERROR (-13) */
    "Server Error",
    /* SERVER_EC_TIMEOUT (-14) */
    "Timeout",
    /* SERVER_EC_UNKNOWN_EVENT (-15) */
    "Unknown Event",
    /* SERVER_EC_UNKNOWN_RESULT (-16) */
    "Unknown Result",
    /* SERVER_EC_UNKNOWN_METHOD (-17) */
    "Unknown Method",
    /* SERVER_EC_UNEXPECTED (-18) */
    "Unexpected",
    /* SERVER_EC_SERVER_REFUSED (-19) */
    "Server Refused",
    /* SERVER_EC_BAD_PACKET (-20) */
    "Bad Packet",
    /* SERVER_EC_BAD_CONNECTION (-21) */
    "Bad Connection",
    /* SERVER_EC_CANT_LOAD (-22) */
    "Cannot Load Resource",
    /* SERVER_EC_BAD_KEY (-23) */
    "Bad Key",
};

const char* server_get_err_message (int err_code)
{
    if (err_code > 0)
        return UNKNOWN_ERR_CODE;

    err_code = -err_code;
    if (err_code > (int)TABLESIZE (err_messages))
        return UNKNOWN_ERR_CODE;

    return err_messages [err_code];
}

int server_errcode_to_retcode (int err_code)
{
    switch (err_code) {
        case 0:
            return SERVER_SC_OK;
        case SERVER_EC_IO:
            return SERVER_SC_IOERR;
        case SERVER_EC_CLOSED:
            return SERVER_SC_SERVICE_UNAVAILABLE;
        case SERVER_EC_NOMEM:
            return SERVER_SC_INSUFFICIENT_STORAGE;
        case SERVER_EC_TOO_LARGE:
            return SERVER_SC_PACKET_TOO_LARGE;
        case SERVER_EC_PROTOCOL:
            return SERVER_SC_UNPROCESSABLE_PACKET;
        case SERVER_EC_UPPER:
            return SERVER_SC_INTERNAL_SERVER_ERROR;
        case SERVER_EC_NOT_IMPLEMENTED:
            return SERVER_SC_NOT_IMPLEMENTED;
        case SERVER_EC_INVALID_VALUE:
            return SERVER_SC_BAD_REQUEST;
        case SERVER_EC_DUPLICATED:
            return SERVER_SC_CONFLICT;
        case SERVER_EC_TOO_SMALL_BUFF:
            return SERVER_SC_INSUFFICIENT_STORAGE;
        case SERVER_EC_BAD_SYSTEM_CALL:
            return SERVER_SC_INTERNAL_SERVER_ERROR;
        case SERVER_EC_AUTH_FAILED:
            return SERVER_SC_UNAUTHORIZED;
        case SERVER_EC_SERVER_ERROR:
            return SERVER_SC_INTERNAL_SERVER_ERROR;
        case SERVER_EC_TIMEOUT:
            return SERVER_SC_CALLEE_TIMEOUT;
        case SERVER_EC_UNKNOWN_EVENT:
            return SERVER_SC_NOT_FOUND;
        case SERVER_EC_UNKNOWN_RESULT:
            return SERVER_SC_NOT_FOUND;
        case SERVER_EC_UNKNOWN_METHOD:
            return SERVER_SC_NOT_FOUND;
        default:
            break;
    }

    return SERVER_SC_INTERNAL_SERVER_ERROR;
}

bool server_is_valid_token (const char* token, int max_len)
{
    int i;

    if (!isalpha (token [0]))
        return false;

    i = 1;
    while (token [i]) {

        if (max_len > 0 && i > max_len)
            return false;

        if (!isalnum (token [i]) && token [i] != '_')
            return false;

        i++;
    }

    return true;
}

bool server_is_valid_endpoint_name (const char* endpoint_name)
{
    char host_name [SERVER_LEN_HOST_NAME + 1];
    char app_name [SERVER_LEN_APP_NAME + 1];
    char runner_name [SERVER_LEN_RUNNER_NAME + 1];

    if (server_extract_host_name (endpoint_name, host_name) <= 0)
        return false;

    if (server_extract_app_name (endpoint_name, app_name) <= 0)
        return false;

    if (server_extract_runner_name (endpoint_name, runner_name) <= 0)
        return false;

    return server_is_valid_host_name (host_name) &&
        server_is_valid_app_name (app_name) &&
        server_is_valid_runner_name (runner_name);
}

/* @<host_name>/<app_name>/<runner_name> */
int server_extract_host_name (const char* endpoint, char* host_name)
{
    int len;
    char* slash;

    if (endpoint [0] != '@' || (slash = strchr (endpoint, '/')) == NULL)
        return 0;

    endpoint++;
    len = (uintptr_t)slash - (uintptr_t)endpoint;
    if (len <= 0 || len > SERVER_LEN_APP_NAME)
        return 0;

    strncpy (host_name, endpoint, len);
    host_name [len] = '\0';

    return len;
}

char* server_extract_host_name_alloc (const char* endpoint)
{
    char* host_name;
    if ((host_name = malloc (SERVER_LEN_HOST_NAME + 1)) == NULL)
        return NULL;

    if (server_extract_host_name (endpoint, host_name) > 0)
        return host_name;

    free (host_name);
    return NULL;
}

/* @<host_name>/<app_name>/<runner_name> */
int server_extract_app_name (const char* endpoint, char* app_name)
{
    int len;
    char *first_slash, *second_slash;

    if (endpoint [0] != '@' || (first_slash = strchr (endpoint, '/')) == 0 ||
            (second_slash = strrchr (endpoint, '/')) == 0 ||
            first_slash == second_slash)
        return 0;

    first_slash++;
    len = (uintptr_t)second_slash - (uintptr_t)first_slash;
    if (len <= 0 || len > SERVER_LEN_APP_NAME)
        return 0;

    strncpy (app_name, first_slash, len);
    app_name [len] = '\0';

    return len;
}

char* server_extract_app_name_alloc (const char* endpoint)
{
    char* app_name;

    if ((app_name = malloc (SERVER_LEN_APP_NAME + 1)) == NULL)
        return NULL;

    if (server_extract_app_name (endpoint, app_name) > 0)
        return app_name;

    free (app_name);
    return NULL;
}

int server_extract_runner_name (const char* endpoint, char* runner_name)
{
    int len;
    char *second_slash;

    if (endpoint [0] != '@' ||
            (second_slash = strrchr (endpoint, '/')) == 0)
        return 0;

    second_slash++;
    len = strlen (second_slash);
    if (len > SERVER_LEN_RUNNER_NAME)
        return 0;

    strcpy (runner_name, second_slash);

    return len;
}

char* server_extract_runner_name_alloc (const char* endpoint)
{
    char* runner_name;

    if ((runner_name = malloc (SERVER_LEN_RUNNER_NAME + 1)) == NULL)
        return NULL;

    if (server_extract_runner_name (endpoint, runner_name) > 0)
        return runner_name;

    free (runner_name);
    return NULL;
}

int server_assemble_endpoint_name (const char* host_name, const char* app_name,
        const char* runner_name, char* buff)
{
    int host_len, app_len, runner_len;

    if ((host_len = strlen (host_name)) > SERVER_LEN_HOST_NAME)
        return 0;

    if ((app_len = strlen (app_name)) > SERVER_LEN_APP_NAME)
        return 0;

    if ((runner_len = strlen (runner_name)) > SERVER_LEN_RUNNER_NAME)
        return 0;

    buff [0] = '@';
    buff [1] = '\0';
    strcat (buff, host_name);
    buff [host_len + 1] = '/';
    buff [host_len + 2] = '\0';

    strcat (buff, app_name);
    buff [host_len + app_len + 2] = '/';
    buff [host_len + app_len + 3] = '\0';

    strcat (buff, runner_name);

    return host_len + app_len + runner_len + 3;
}

char* server_assemble_endpoint_name_alloc (const char* host_name, const char* app_name,
        const char* runner_name)
{
    char* endpoint;
    int host_len, app_len, runner_len;

    if ((host_len = strlen (host_name)) > SERVER_LEN_HOST_NAME)
        return NULL;

    if ((app_len = strlen (app_name)) > SERVER_LEN_APP_NAME)
        return NULL;

    if ((runner_len = strlen (runner_name)) > SERVER_LEN_RUNNER_NAME)
        return NULL;

    if ((endpoint = malloc (host_len + app_len + runner_len + 4)) == NULL)
        return NULL;

    endpoint [0] = '@';
    endpoint [1] = '\0';
    strcat (endpoint, host_name);
    endpoint [host_len + 1] = '/';
    endpoint [host_len + 2] = '\0';

    strcat (endpoint, app_name);
    endpoint [host_len + app_len + 2] = '/';
    endpoint [host_len + app_len + 3] = '\0';

    strcat (endpoint, runner_name);

    return endpoint;
}

bool server_is_valid_host_name (const char* host_name)
{
    // TODO
    (void)host_name;
    return true;
}

/* cn.fmsoft.hybridos.aaa */
bool server_is_valid_app_name (const char* app_name)
{
    int len, max_len = SERVER_LEN_APP_NAME;
    const char *start;
    char *end;

    start = app_name;
    while (*start) {
        char saved;
        end = strchr (start, '.');
        if (end == NULL) {
            saved = 0;
            end += strlen (start);
        }
        else {
            saved = '.';
            *end = 0;
        }

        if (end == start)
            return false;

        if ((len = server_is_valid_token (start, max_len)) <= 0)
            return false;

        max_len -= len;
        if (saved) {
            start = end + 1;
            *end = saved;
            max_len--;
        }
        else {
            break;
        }
    }

    return true;
}

void server_generate_unique_id (char* id_buff, const char* prefix)
{
    static unsigned long accumulator;
    struct timespec tp;
    int i, n = strlen (prefix);
    char my_prefix [9];

    for (i = 0; i < 8; i++) {
        if (i < n) {
            my_prefix [i] = toupper (prefix [i]);
        }
        else
            my_prefix [i] = 'X';
    }
    my_prefix [8] = '\0';

    clock_gettime (CLOCK_REALTIME, &tp);
    snprintf (id_buff, SERVER_LEN_UNIQUE_ID + 1,
            "%s-%016lX-%016lX-%016lX",
            my_prefix, tp.tv_sec, tp.tv_nsec, accumulator);
    accumulator++;
}

void server_generate_md5_id (char* id_buff, const char* prefix)
{
    int n;
    char key [256];
    unsigned char md5_digest [MD5_DIGEST_SIZE];
    struct timespec tp;

    clock_gettime (CLOCK_REALTIME, &tp);
    n = snprintf (key, sizeof (key), "%s-%ld-%ld-%ld", prefix,
            tp.tv_sec, tp.tv_nsec, random ());

    if (n < 0) {
        ULOG_WARN ("Unexpected call to snprintf.\n");
    }
    else if ((size_t)n >= sizeof (key))
        ULOG_WARN ("The buffer is too small for resultId.\n");

    md5digest (key, md5_digest);
    bin2hex (md5_digest, MD5_DIGEST_SIZE, id_buff);
}

bool server_is_valid_unique_id (const char* id)
{
    int n = 0;

    while (id [n]) {
        if (n > SERVER_LEN_UNIQUE_ID)
            return false;

        if (!isalnum (id [n]) && id [n] != '-')
            return false;

        n++;
    }

    return true;
}

bool server_is_valid_md5_id (const char* id)
{
    int n = 0;

    while (id [n]) {
        if (n > (MD5_DIGEST_SIZE << 1))
            return false;

        if (!isalnum (id [n]))
            return false;

        n++;
    }

    return true;
}

double server_get_elapsed_seconds (const struct timespec *ts1, const struct timespec *ts2)
{
    struct timespec ts_curr;
    time_t ds;
    long dns;

    if (ts2 == NULL) {
        clock_gettime (CLOCK_MONOTONIC, &ts_curr);
        ts2 = &ts_curr;
    }

    ds = ts2->tv_sec - ts1->tv_sec;
    dns = ts2->tv_nsec - ts1->tv_nsec;
    return ds + dns * 1.0E-9;
}
