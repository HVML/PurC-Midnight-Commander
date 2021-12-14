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
#include <errno.h>
#include <assert.h>

#include "lib/hiboxcompat.h"
#include "lib/md5.h"

#include "purcrdr.h"

pcrdr_msg *pcrdr_make_request_message(
        pcrdr_msg_target target, uintptr_t target_value,
        const char *operation,
        pcrdr_msg_element_type element_type, const char *element,
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
    else {
        assert(element);
        msg->element = strdup(element);
        if (msg->element == NULL)
            goto failed;
    }

#if 0
    if (element_type == PCRDR_MSG_ELEMENT_TYPE_CSS ||
            element_type == PCRDR_MSG_ELEMENT_TYPE_XPATH) {
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
#endif

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
        const char *request_id,
        unsigned int ret_code, uintptr_t result_value,
        pcrdr_msg_data_type data_type, const char* data)
{
    pcrdr_msg *msg = calloc(1, sizeof(pcrdr_msg));
    if (msg == NULL)
        return NULL;

    assert(request_id);
    msg->type = PCRDR_MSG_TYPE_RESPONSE;
    msg->requestId = strdup(request_id);
    if (msg->requestId == NULL)
        goto failed;

    msg->dataType = data_type;
    if (data_type != PCRDR_MSG_DATA_TYPE_VOID) {
        msg->dataLen = strlen(data);
        msg->data = strdup(data);
        if (msg->data == NULL) {
            goto failed;
        }
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
        const char *event,
        pcrdr_msg_element_type element_type, const char *element,
        const char *property,
        pcrdr_msg_data_type data_type, const char* data)
{
    pcrdr_msg *msg = calloc(1, sizeof(pcrdr_msg));
    if (msg == NULL)
        return NULL;

    msg->type = PCRDR_MSG_TYPE_EVENT;
    msg->target = target;
    msg->targetValue = target_value;

    assert(event);
    msg->event = strdup(event);
    if (msg->event == NULL)
        goto failed;

    msg->elementType = element_type;
    if (element_type == PCRDR_MSG_ELEMENT_TYPE_VOID) {
        msg->element = NULL;
    }
    else {
        assert(element);
        msg->element = strdup(element);
        if (msg->element == NULL)
            goto failed;
    }

#if 0
    if (element_type == PCRDR_MSG_ELEMENT_TYPE_CSS ||
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
#endif

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

    if (msg->event)
        free(msg->event);

    if (msg->requestId)
        free(msg->requestId);

    if (msg->data)
        free(msg->data);

    free(msg);
}

static inline bool is_blank_line(const char *line)
{
    while (*line) {
        if (*line == ' ' || *line == '\t')
            continue;
        else if (*line == '\n')
            return true;
        else
            return false;

        line++;
    }

    return true;
}

static inline char *skip_left_spaces(char *str)
{
    while (*str == ' ' || *str == '\t') {
        str++;
    }

    return str;
}

static bool on_type(pcrdr_msg *msg, char *value)
{
    if (strcasecmp(value, "request")) {
        msg->type = PCRDR_MSG_TYPE_REQUEST;
    }
    else if (strcasecmp(value, "response")) {
        msg->type = PCRDR_MSG_TYPE_RESPONSE;
    }
    else if (strcasecmp(value, "event")) {
        msg->type = PCRDR_MSG_TYPE_EVENT;
    }
    else {
        return false;
    }

    return true;
}

static bool on_target(pcrdr_msg *msg, char *value)
{
    char *target, *target_value;
    char *saveptr;

    target = strtok_r(value, "/", &saveptr);
    if (target == NULL)
        return false;

    target_value = strtok_r(NULL, "/", &saveptr);
    if (target_value == NULL)
        return false;

    if (strcasecmp(target, "session")) {
        msg->target = PCRDR_MSG_TARGET_SESSION;
    }
    else if (strcasecmp(target, "window")) {
        msg->target = PCRDR_MSG_TARGET_WINDOW;
    }
    else if (strcasecmp(target, "tab")) {
        msg->target = PCRDR_MSG_TARGET_TAB;
    }
    else if (strcasecmp(target, "dom")) {
        msg->target = PCRDR_MSG_TARGET_DOM;
    }
    else {
        return false;
    }

    errno = 0;
    if (sizeof(uintptr_t) == sizeof(unsigned long int))
        msg->targetValue = strtoul(target_value, NULL, 16);
    else if (sizeof(uintptr_t) == sizeof(unsigned long long int))
        msg->targetValue = strtoull(target_value, NULL, 16);
    else
        assert(0);

    if (errno)
        return false;

    return true;
}

static bool on_operation(pcrdr_msg *msg, char *value)
{
    msg->operation = value;
    return true;
}

static bool on_event(pcrdr_msg *msg, char *value)
{
    msg->event = value;
    return true;
}

static bool on_element(pcrdr_msg *msg, char *value)
{
    char *type;
    char *saveptr;

    type = strtok_r(value, "/", &saveptr);
    if (type == NULL)
        return false;

    if (strcasecmp(type, "css")) {
        msg->elementType = PCRDR_MSG_ELEMENT_TYPE_CSS;
    }
    else if (strcasecmp(type, "xpath")) {
        msg->elementType = PCRDR_MSG_ELEMENT_TYPE_XPATH;
    }
    else if (strcasecmp(type, "handle")) {
        msg->elementType = PCRDR_MSG_ELEMENT_TYPE_HANDLE;
    }
    else {
        return false;
    }

    msg->element = strtok_r(NULL, "/", &saveptr);
    if (msg->element == NULL)
        return false;

    return true;
}

static bool on_property(pcrdr_msg *msg, char *value)
{
    msg->property = value;
    return true;
}

static bool on_request_id(pcrdr_msg *msg, char *value)
{
    msg->requestId = value;
    return true;
}

static bool on_result(pcrdr_msg *msg, char *value)
{
    char *subtoken;
    char *saveptr;

    subtoken = strtok_r(value, "/", &saveptr);
    if (subtoken == NULL)
        return false;

    errno = 0;
    msg->retCode = (unsigned int)strtoul(subtoken, NULL, 10);
    if (errno)
        return false;

    subtoken = strtok_r(NULL, "/", &saveptr);
    if (subtoken == NULL)
        return false;

    errno = 0;
    msg->resultValue = (uintptr_t)strtoull(subtoken, NULL, 16);
    if (errno)
        return false;

    return true;
}

static bool on_data_type(pcrdr_msg *msg, char *value)
{
    if (strcasecmp(value, "void")) {
        msg->dataType = PCRDR_MSG_DATA_TYPE_VOID;
    }
    else if (strcasecmp(value, "ejson")) {
        msg->dataType = PCRDR_MSG_DATA_TYPE_EJSON;
    }
    else if (strcasecmp(value, "text")) {
        msg->dataType = PCRDR_MSG_DATA_TYPE_TEXT;
    }
    else {
        return false;
    }

    return true;
}

static bool on_data_len(pcrdr_msg *msg, char *value)
{
    errno = 0;
    msg->dataLen = strtoul(value, NULL, 16);

    if (errno)
        return false;

    return true;
}

typedef bool (*key_op)(pcrdr_msg *msg, char *value);

#define STR_KEY_TYPE        "type"
#define STR_KEY_TARGET      "target"
#define STR_KEY_OPERATION   "operation"
#define STR_KEY_ELEMENT     "element"
#define STR_KEY_PROPERTY    "property"
#define STR_KEY_EVENT       "event"
#define STR_KEY_REQUEST_ID  "requestId"
#define STR_KEY_RESULT      "result"
#define STR_KEY_DATA_TYPE   "dataType"
#define STR_KEY_DATA_LEN    "dataLen"

static struct key_op_pair {
    const char *key;
    key_op      op;
} key_ops[] = {
    { STR_KEY_TYPE,         on_type },
    { STR_KEY_TARGET,       on_target },
    { STR_KEY_OPERATION,    on_operation },
    { STR_KEY_ELEMENT, on_element },
    { STR_KEY_PROPERTY, on_property },
    { STR_KEY_EVENT, on_event },
    { STR_KEY_REQUEST_ID, on_request_id },
    { STR_KEY_RESULT, on_result },
    { STR_KEY_DATA_TYPE, on_data_type },
    { STR_KEY_DATA_LEN, on_data_len },
};

static key_op find_key_op(const char* key)
{
    for (int i = 0; i < sizeof(key_ops)/sizeof(key_ops[0]); i++) {
        if (strcasecmp(key, key_ops[i].key) == 0) {
            return key_ops[i].op;
        }
    }

    return NULL;
}

int pcrdr_parse_packet(char *packet, size_t sz_packet, pcrdr_msg **msg_out)
{
    pcrdr_msg msg;

    char *str1;
    char *line;
    char *saveptr1;

    memset(&msg, 0, sizeof(msg));

    for (str1 = packet; ; str1 = NULL) {
        line = strtok_r(str1, "\n", &saveptr1);
        if (line == NULL) {
            goto failed;
        }

        if (is_blank_line(line)) {
            msg.data = strtok_r(NULL, "\n", &saveptr1);
            break;
        }

        char *key, *value;
        char *saveptr2;
        key = strtok_r(line, ":", &saveptr2);
        if (key == NULL) {
            goto failed;
        }

        value = strtok_r(NULL, ":", &saveptr2);
        if (value == NULL) {
            goto failed;
        }

        key_op op = find_key_op(key);
        if (op == NULL) {
            goto failed;
        }

        if (!op(&msg, skip_left_spaces(value))) {
            goto failed;
        }
    }

    pcrdr_msg *_msg;
    if (msg.type == PCRDR_MSG_TYPE_REQUEST) {
        _msg = pcrdr_make_request_message(msg.target, msg.targetValue,
                msg.operation,
                msg.elementType,
                msg.element,
                msg.property,
                msg.dataType,
                msg.data);
    }
    else if (msg.type == PCRDR_MSG_TYPE_RESPONSE) {
#if 0
        if (msg.dataType == PCRDR_MSG_DATA_TYPE_RESULT ||
                msg.dataType == PCRDR_MSG_DATA_TYPE_RESULT_EXTRA) {

            msg.data = saveptr;
        }
#endif

        _msg = pcrdr_make_response_message(
                msg.requestId,
                msg.retCode, msg.resultValue,
                msg.dataType, msg.data);
    }
    else if (msg.type == PCRDR_MSG_TYPE_EVENT) {
        _msg = pcrdr_make_event_message(msg.target, msg.targetValue,
                msg.event,
                msg.elementType,
                msg.element,
                msg.property,
                msg.dataType,
                msg.data);
    }

    if (_msg == NULL) {
        goto failed;
    }

    *msg_out = _msg;
    return 0;

failed:
    return PURCRDR_EC_BAD_PACKET;
}

#define STR_KV_SEPARATOR    ":"
#define STR_LINE_SEPARATOR  "\n"
#define STR_TOKEN_SEPARATOR "/"

static const char *type_names [] = {
    "request",      /* PCRDR_MSG_TYPE_REQUEST */
    "response",     /* PCRDR_MSG_TYPE_RESPONSE */
    "event",        /* PCRDR_MSG_TYPE_EVENT */
};

static const char *target_names [] = {
    "session",
    "window",
    "tab",
    "dom",
};

static const char *element_type_names [] = {
    "void",
    "css",
    "xpath",
    "handle",
};

static const char *data_type_names [] = {
    "void",
    "ejson",
    "text",
};

int pcrdr_serialize_message(const pcrdr_msg *msg, cb_write fn, void *ctxt)
{
    int n;
    char buff[128];

    /* type: <request | response | event> */
    fn(ctxt, STR_KEY_TYPE, sizeof(STR_KEY_TYPE) - 1);
    fn(ctxt, STR_KV_SEPARATOR, sizeof(STR_KV_SEPARATOR) - 1);
    fn(ctxt, type_names[msg->type], strlen(type_names[msg->type]));
    fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);

    if (msg->type == PCRDR_MSG_TYPE_REQUEST) {
        /* target: <session | window | tab | dom>/<handle> */
        fn(ctxt, STR_KEY_TARGET, sizeof(STR_KEY_TARGET) - 1);
        fn(ctxt, STR_KV_SEPARATOR, sizeof(STR_KV_SEPARATOR) - 1);
        fn(ctxt, target_names[msg->target], strlen(target_names[msg->target]));
        fn(ctxt, STR_TOKEN_SEPARATOR, sizeof(STR_TOKEN_SEPARATOR) - 1);

        n = snprintf(buff, sizeof(buff),
                "%llx", (unsigned long long int)msg->targetValue);
        if (n < 0)
            return PURCRDR_EC_UNEXPECTED;
        else if ((size_t)n >= sizeof (buff)) {
            ULOG_ERR ("Too small buffer for serialize message.\n");
            return PURCRDR_EC_TOO_SMALL_BUFF;
        }
        fn(ctxt, buff, n);
        fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);

        /* operation: <operation> */
        fn(ctxt, STR_KEY_OPERATION, sizeof(STR_KEY_OPERATION) - 1);
        fn(ctxt, STR_KV_SEPARATOR, sizeof(STR_KV_SEPARATOR) - 1);
        fn(ctxt, msg->operation, strlen(msg->operation));
        fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);

        if (msg->elementType != PCRDR_MSG_ELEMENT_TYPE_VOID) {
            /* element: <css | xpath | handle>/<element> */
            fn(ctxt, STR_KEY_ELEMENT, sizeof(STR_KEY_ELEMENT) - 1);
            fn(ctxt, STR_KV_SEPARATOR, sizeof(STR_KV_SEPARATOR) - 1);
            fn(ctxt, element_type_names[msg->target],
                    strlen(element_type_names[msg->target]));
            fn(ctxt, STR_TOKEN_SEPARATOR, sizeof(STR_TOKEN_SEPARATOR) - 1);
            fn(ctxt, msg->element, strlen(msg->element));
            fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);
        }

        if (msg->property) {
            /* property: <property> */
            fn(ctxt, STR_KEY_PROPERTY, sizeof(STR_KEY_PROPERTY) - 1);
            fn(ctxt, STR_KV_SEPARATOR, sizeof(STR_KV_SEPARATOR) - 1);
            fn(ctxt, msg->property, strlen(msg->property));
            fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);
        }

        /* requestId: <requestId> */
        fn(ctxt, STR_KEY_REQUEST_ID, sizeof(STR_KEY_REQUEST_ID) - 1);
        fn(ctxt, STR_KV_SEPARATOR, sizeof(STR_KV_SEPARATOR) - 1);
        fn(ctxt, msg->requestId, strlen(msg->requestId));
        fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);

        /* dataType: <void | ejson | text> */
        fn(ctxt, STR_KEY_DATA_TYPE, sizeof(STR_KEY_DATA_TYPE) - 1);
        fn(ctxt, STR_KV_SEPARATOR, sizeof(STR_KV_SEPARATOR) - 1);
        fn(ctxt, data_type_names[msg->target],
                strlen(data_type_names[msg->target]));
        fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);

        /* dataLen: <data_length> */
        fn(ctxt, STR_KEY_DATA_LEN, sizeof(STR_KEY_DATA_LEN) - 1);
        fn(ctxt, STR_KV_SEPARATOR, sizeof(STR_KV_SEPARATOR) - 1);
        n = snprintf(buff, sizeof(buff), "%lu", (unsigned long int)msg->dataLen);
        if (n < 0)
            return PURCRDR_EC_UNEXPECTED;
        else if ((size_t)n >= sizeof (buff)) {
            ULOG_ERR ("Too small buffer for serialize message.\n");
            return PURCRDR_EC_TOO_SMALL_BUFF;
        }
        fn(ctxt, buff, n);
        fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);

        /* a blank line */
        fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);

        /* the extra data */
        fn(ctxt, msg->data, msg->dataLen);
    }
    else if (msg->type == PCRDR_MSG_TYPE_RESPONSE) {
        /* requestId: <requestId> */
        fn(ctxt, STR_KEY_REQUEST_ID, sizeof(STR_KEY_REQUEST_ID) - 1);
        fn(ctxt, STR_KV_SEPARATOR, sizeof(STR_KV_SEPARATOR) - 1);
        fn(ctxt, msg->requestId, strlen(msg->requestId));
        fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);

        /* dataType: <void | ejson | text> */
        fn(ctxt, STR_KEY_DATA_TYPE, sizeof(STR_KEY_DATA_TYPE) - 1);
        fn(ctxt, STR_KV_SEPARATOR, sizeof(STR_KV_SEPARATOR) - 1);
        fn(ctxt, data_type_names[msg->target],
                strlen(data_type_names[msg->target]));
        fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);

        /* dataLen: <data_length> */
        fn(ctxt, STR_KEY_DATA_LEN, sizeof(STR_KEY_DATA_LEN) - 1);
        fn(ctxt, STR_KV_SEPARATOR, sizeof(STR_KV_SEPARATOR) - 1);
        n = snprintf(buff, sizeof(buff), "%lu", (unsigned long int)msg->dataLen);
        if (n < 0)
            return PURCRDR_EC_UNEXPECTED;
        else if ((size_t)n >= sizeof (buff)) {
            ULOG_ERR ("Too small buffer for serialize message.\n");
            return PURCRDR_EC_TOO_SMALL_BUFF;
        }
        fn(ctxt, buff, n);
        fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);

        /* a blank line */
        fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);

        /* <retCode>/<resultValue> */
        n = snprintf(buff, sizeof(buff), "%u", msg->retCode);
        if (n < 0)
            return PURCRDR_EC_UNEXPECTED;
        else if ((size_t)n >= sizeof (buff)) {
            ULOG_ERR ("Too small buffer for serialize message.\n");
            return PURCRDR_EC_TOO_SMALL_BUFF;
        }
        fn(ctxt, buff, n);
        fn(ctxt, STR_TOKEN_SEPARATOR, sizeof(STR_TOKEN_SEPARATOR) - 1);
        n = snprintf(buff, sizeof(buff),
                "%llx", (unsigned long long int)msg->resultValue);
        if (n < 0)
            return PURCRDR_EC_UNEXPECTED;
        else if ((size_t)n >= sizeof (buff)) {
            ULOG_ERR ("Too small buffer for serialize message.\n");
            return PURCRDR_EC_TOO_SMALL_BUFF;
        }
        fn(ctxt, buff, n);
        fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);

        /* the extra data */
        fn(ctxt, msg->data, strlen(msg->data));
    }
    else if (msg->type == PCRDR_MSG_TYPE_EVENT) {
        /* target: <session | window | tab | dom>/<handle> */
        fn(ctxt, STR_KEY_TARGET, sizeof(STR_KEY_TARGET) - 1);
        fn(ctxt, STR_KV_SEPARATOR, sizeof(STR_KV_SEPARATOR) - 1);
        fn(ctxt, target_names[msg->target], strlen(target_names[msg->target]));
        fn(ctxt, STR_TOKEN_SEPARATOR, sizeof(STR_TOKEN_SEPARATOR) - 1);

        n = snprintf(buff, sizeof(buff),
                "%llx", (unsigned long long int)msg->targetValue);
        if (n < 0)
            return PURCRDR_EC_UNEXPECTED;
        else if ((size_t)n >= sizeof (buff)) {
            ULOG_ERR ("Too small buffer for serialize message.\n");
            return PURCRDR_EC_TOO_SMALL_BUFF;
        }
        fn(ctxt, buff, n);
        fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);

        /* event: <event> */
        fn(ctxt, STR_KEY_EVENT, sizeof(STR_KEY_EVENT) - 1);
        fn(ctxt, STR_KV_SEPARATOR, sizeof(STR_KV_SEPARATOR) - 1);
        fn(ctxt, msg->event, strlen(msg->event));
        fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);

        if (msg->elementType != PCRDR_MSG_ELEMENT_TYPE_VOID) {
            /* element: <css | xpath | handle>/<element> */
            fn(ctxt, STR_KEY_ELEMENT, sizeof(STR_KEY_ELEMENT) - 1);
            fn(ctxt, STR_KV_SEPARATOR, sizeof(STR_KV_SEPARATOR) - 1);
            fn(ctxt, element_type_names[msg->target],
                    strlen(element_type_names[msg->target]));
            fn(ctxt, STR_TOKEN_SEPARATOR, sizeof(STR_TOKEN_SEPARATOR) - 1);
            fn(ctxt, msg->element, strlen(msg->element));
            fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);
        }

        if (msg->property) {
            /* property: <property> */
            fn(ctxt, STR_KEY_PROPERTY, sizeof(STR_KEY_PROPERTY) - 1);
            fn(ctxt, STR_KV_SEPARATOR, sizeof(STR_KV_SEPARATOR) - 1);
            fn(ctxt, msg->property, strlen(msg->property));
            fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);
        }

        /* dataType: <void | ejson | text> */
        fn(ctxt, STR_KEY_DATA_TYPE, sizeof(STR_KEY_DATA_TYPE) - 1);
        fn(ctxt, STR_KV_SEPARATOR, sizeof(STR_KV_SEPARATOR) - 1);
        fn(ctxt, data_type_names[msg->target],
                strlen(data_type_names[msg->target]));
        fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);

        /* dataLen: <data_length> */
        fn(ctxt, STR_KEY_DATA_LEN, sizeof(STR_KEY_DATA_LEN) - 1);
        fn(ctxt, STR_KV_SEPARATOR, sizeof(STR_KV_SEPARATOR) - 1);
        n = snprintf(buff, sizeof(buff), "%lu", (unsigned long int)msg->dataLen);
        if (n < 0)
            return PURCRDR_EC_UNEXPECTED;
        else if ((size_t)n >= sizeof (buff)) {
            ULOG_ERR ("Too small buffer for serialize message.\n");
            return PURCRDR_EC_TOO_SMALL_BUFF;
        }
        fn(ctxt, buff, n);
        fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);

        /* a blank line */
        fn(ctxt, STR_LINE_SEPARATOR, sizeof(STR_LINE_SEPARATOR) - 1);

        /* the data */
        fn(ctxt, msg->data, msg->dataLen);
    }
    else {
        assert(0);
    }

    return 0;
}

