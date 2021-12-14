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
#include <assert.h>

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

pcrdr_msg *pcrdr_make_request_message(
        pcrdr_msg_target target, uintptr_t target_value,
        const char *operation,
        pcrdr_msg_element_type element_type, void *element,
        const char *property,
        pcrdr_msg_data_type data_type, const char* data)
{
    pcrdr_msg *msg = calloc(1, sizeof(pcrdr_msg));
    if (msg == NULL)
        return NULL;

    msg->type = PCRDR_MSG_TYPE_REQUEST;
    msg->target = target;
    msg->targetValue = target_value;

    assert(operation);
    msg->operation = strdup(operation);
    if (msg->operation == NULL)
        goto failed;

    msg->elementType = element_type;
    if (element_type == PCRDR_MSG_ELEMENT_TYPE_VOID) {
        msg->element = NULL;
    }
    else if (element_type == PCRDR_MSG_ELEMENT_TYPE_CSS ||
            element_type == PCRDR_MSG_ELEMENT_TYPE_XPATH) {
        assert(element);
        msg->element = strdup(element);
        if (msg->element == NULL)
            goto failed;
    }
    else { /* PCRDR_MSG_ELEMENT_TYPE_HANDLES */
        assert(element);

        uintptr_t *handles = element;
        size_t nr_handles = 0;
        while (*handles) {
            nr_handles++;
            handles++;
        }

        msg->element = calloc(nr_handles + 1, sizeof(uintptr_t));
        if (msg->element == NULL)
            goto failed;

        memcpy(msg->element, element, sizeof(uintptr_t) * (nr_handles + 1));
    }

    if (property) {
        msg->property = strdup(property);
        if (msg->property == NULL)
            goto failed;
    }

    char request_id[PURCRDR_LEN_UNIQUE_ID + 1];
    pcrdr_generate_unique_id(request_id, "REQ");
    msg->requestId = strdup(request_id);
    if (msg->requestId == NULL)
        goto failed;

    msg->dataType = data_type;
    if (data_type != PCRDR_MSG_DATA_TYPE_VOID) {
        assert(data);
        msg->dataLen = strlen(data);
        msg->data = strdup(data);
        if (msg->data == NULL) {
            goto failed;
        }
    }

    return msg;

failed:
    pcrdr_release_message(msg);
    return NULL;
}

pcrdr_msg *pcrdr_make_response_message(
        const pcrdr_msg *request_msg,
        int ret_code, uintptr_t result_value,
        const char *extra_info)
{
    pcrdr_msg *msg = calloc(1, sizeof(pcrdr_msg));
    if (msg == NULL)
        return NULL;

    assert(request_msg->type == PCRDR_MSG_TYPE_REQUEST);
    assert(request_msg->requestId);

    msg->type = PCRDR_MSG_TYPE_RESPONSE;
    msg->requestId = strdup(request_msg->requestId);
    if (msg->requestId == NULL)
        goto failed;

    if (extra_info) {
        msg->dataType = PCRDR_MSG_DATA_TYPE_RESULT_EXTRA;

        msg->dataLen = strlen(extra_info);
        msg->data = strdup(extra_info);
        if (msg->data == NULL) {
            goto failed;
        }
    }
    else {
        msg->dataType = PCRDR_MSG_DATA_TYPE_RESULT;
    }

    msg->retCode = ret_code;
    msg->resultValue = result_value;

    return msg;

failed:
    pcrdr_release_message(msg);
    return NULL;
}

pcrdr_msg *pcrdr_make_event_message(
        pcrdr_msg_target target, uintptr_t target_value,
        const char *event_name,
        pcrdr_msg_element_type element_type, void *element,
        const char *property,
        pcrdr_msg_data_type data_type, const char* data)
{
    pcrdr_msg *msg = calloc(1, sizeof(pcrdr_msg));
    if (msg == NULL)
        return NULL;

    msg->type = PCRDR_MSG_TYPE_EVENT;
    msg->target = target;
    msg->targetValue = target_value;

    assert(event_name);
    msg->eventName = strdup(event_name);
    if (msg->eventName == NULL)
        goto failed;

    msg->elementType = element_type;
    if (element_type == PCRDR_MSG_ELEMENT_TYPE_VOID) {
        msg->element = NULL;
    }
    else if (element_type == PCRDR_MSG_ELEMENT_TYPE_CSS ||
            element_type == PCRDR_MSG_ELEMENT_TYPE_XPATH) {
        assert(element);
        msg->element = strdup(element);
        if (msg->element == NULL)
            goto failed;
    }
    else { /* PCRDR_MSG_ELEMENT_TYPE_HANDLES */
        assert(element);

        uintptr_t *handles = element;
        size_t nr_handles = 0;
        while (*handles) {
            nr_handles++;
            handles++;
        }

        msg->element = calloc(nr_handles + 1, sizeof(uintptr_t));
        if (msg->element == NULL)
            goto failed;

        memcpy(msg->element, element, sizeof(uintptr_t) * (nr_handles + 1));
    }

    if (property) {
        msg->property = strdup(property);
        if (msg->property == NULL)
            goto failed;
    }

    msg->dataType = data_type;
    if (data_type != PCRDR_MSG_DATA_TYPE_VOID) {
        assert(data);
        msg->dataLen = strlen(data);
        msg->data = strdup(data);
        if (msg->data == NULL) {
            goto failed;
        }
    }

    return msg;

failed:
    pcrdr_release_message(msg);
    return NULL;
}


void pcrdr_release_message(pcrdr_msg *msg)
{
    if (msg->operation)
        free(msg->operation);

    if (msg->element)
        free(msg->element);

    if (msg->property)
        free(msg->property);

    if (msg->eventName)
        free(msg->eventName);

    if (msg->requestId)
        free(msg->requestId);

    if (msg->data)
        free(msg->data);

    free(msg);
}

int pcrdr_parse_packet(char *packet, size_t sz_packet, pcrdr_msg **msg)
{
    return 0;
}

