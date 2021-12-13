/*
** message.c -- The implementation of API to make or release a message.
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
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/un.h>
#include <sys/time.h>

#include "lib/hiboxcompat.h"
#include "lib/md5.h"

#include "purcrdr.h"

#if 0
static const char *target_names [] = {
    "session",
    "window",
    "tab",
    "dom",
};

static const char *data_type_names [] = {
    "void",
    "ejson",
    "text",
    "result",
    "result_extra",
};
#endif

#define PCRDR_RESVAL_INITIAL    "_initial"
#define PCRDR_RESVAL_DEFAULT    "_default"
#define PCRDR_RESVAL_VOID       "_void"


struct _pcrdr_msg {
    pcrdr_msg_type      type;
    pcrdr_msg_data_type dataType;

    const char*     target;
    const char*     operation;
    const char*     element;
    const char*     property;
    const char*     event;

    char*           requestId;

    size_t          dataLen;
    char*           data;
};

pcrdr_msg *pcrdr_make_request_message(
        pcrdr_msg_target target, uintptr_t target_value,
        const char *operation,
        pcrdr_msg_element_type element_type, void *element,
        const char *property,
        pcrdr_msg_data_type data_type, const char* data)
{
    return NULL;
}

pcrdr_msg *pcrdr_make_response_message(
        const pcrdr_msg *request_msg,
        int ret_code, uintptr_t result_value,
        const char *extra_info)
{
    return NULL;
}

pcrdr_msg *pcrdr_make_event_message(
        pcrdr_msg_target target, uintptr_t target_value,
        const char *event,
        pcrdr_msg_element_type element_type, void *element,
        const char *property,
        pcrdr_msg_data_type data_type, const char* data)
{
    return NULL;
}


void pcrdr_release_message(pcrdr_msg *msg)
{
    if (msg->requestId)
        free(msg->requestId);

    if (msg->dataLen && msg->data)
        free(msg->data);

    free(msg);
}

int pcrdr_parse_packet(char *packet, size_t sz_packet, pcrdr_msg **msg)
{
    return 0;
}

