/**
 * @file purcrdr.h
 * @author Vincent Wei (https://github.com/VincentWei)
 * @date 2021/01/12
 * @brief This file declares API for the simple markup generator of
 *        a PurC Renderer.
 *
 * Copyright (c) 2021 FMSoft (http://www.fmsoft.cn)
 *
 * This file is part of PurC Midnight Commander.
 *
 * PurCSMG is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PurCRDR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/.
 */

#ifndef _PURCRDR_H_
#define _PURCRDR_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>

/* Constants */
#define PCRDR_PROTOCOL_NAME             "PURCRDR"
#define PCRDR_PROTOCOL_VERSION_STRING   "100"
#define PCRDR_PROTOCOL_VERSION          100
#define PCRDR_MINIMAL_PROTOCOL_VERSION  100

#define PCRDR_US_PATH                   "/var/tmp/purcrdr.sock"
#define PCRDR_WS_PORT                   "7702"
#define PCRDR_WS_PORT_RESERVED          "7703"

#define PCRDR_LOCALHOST                 "localhost"
#define PCRDR_APP_PURCSMG               "cn.fmsoft.hybridos.purcsmg"
#define PCRDR_RUNNER_CMDLINE            "cmdline"

#define PCRDR_NOT_AVAILABLE             "<N/A>"

/* Status Codes */
#define PCRDR_SC_IOERR                  1
#define PCRDR_SC_OK                     200
#define PCRDR_SC_CREATED                201
#define PCRDR_SC_ACCEPTED               202
#define PCRDR_SC_NO_CONTENT             204
#define PCRDR_SC_RESET_CONTENT          205
#define PCRDR_SC_PARTIAL_CONTENT        206
#define PCRDR_SC_BAD_REQUEST            400
#define PCRDR_SC_UNAUTHORIZED           401
#define PCRDR_SC_FORBIDDEN              403
#define PCRDR_SC_NOT_FOUND              404
#define PCRDR_SC_METHOD_NOT_ALLOWED     405
#define PCRDR_SC_NOT_ACCEPTABLE         406
#define PCRDR_SC_CONFLICT               409
#define PCRDR_SC_GONE                   410
#define PCRDR_SC_PRECONDITION_FAILED    412
#define PCRDR_SC_PACKET_TOO_LARGE       413
#define PCRDR_SC_EXPECTATION_FAILED     417
#define PCRDR_SC_IM_A_TEAPOT            418
#define PCRDR_SC_UNPROCESSABLE_PACKET   422
#define PCRDR_SC_LOCKED                 423
#define PCRDR_SC_FAILED_DEPENDENCY      424
#define PCRDR_SC_TOO_EARLY              425
#define PCRDR_SC_UPGRADE_REQUIRED       426
#define PCRDR_SC_RETRY_WITH             449
#define PCRDR_SC_UNAVAILABLE_FOR_LEGAL_REASONS             451
#define PCRDR_SC_INTERNAL_SERVER_ERROR  500
#define PCRDR_SC_NOT_IMPLEMENTED        501
#define PCRDR_SC_BAD_CALLEE             502
#define PCRDR_SC_SERVICE_UNAVAILABLE    503
#define PCRDR_SC_CALLEE_TIMEOUT         504
#define PCRDR_SC_INSUFFICIENT_STORAGE   507

#define PCRDR_EC_IO                     (-1)
#define PCRDR_EC_CLOSED                 (-2)
#define PCRDR_EC_NOMEM                  (-3)
#define PCRDR_EC_TOO_LARGE              (-4)
#define PCRDR_EC_PROTOCOL               (-5)
#define PCRDR_EC_UPPER                  (-6)
#define PCRDR_EC_NOT_IMPLEMENTED        (-7)
#define PCRDR_EC_INVALID_VALUE          (-8)
#define PCRDR_EC_DUPLICATED             (-9)
#define PCRDR_EC_TOO_SMALL_BUFF         (-10)
#define PCRDR_EC_BAD_SYSTEM_CALL        (-11)
#define PCRDR_EC_AUTH_FAILED            (-12)
#define PCRDR_EC_SERVER_ERROR           (-13)
#define PCRDR_EC_TIMEOUT                (-14)
#define PCRDR_EC_UNKNOWN_EVENT          (-15)
#define PCRDR_EC_UNKNOWN_RESULT         (-16)
#define PCRDR_EC_UNKNOWN_METHOD         (-17)
#define PCRDR_EC_UNEXPECTED             (-18)
#define PCRDR_EC_SERVER_REFUSED         (-19)
#define PCRDR_EC_BAD_PACKET             (-20)
#define PCRDR_EC_BAD_CONNECTION         (-21)
#define PCRDR_EC_CANT_LOAD              (-22)
#define PCRDR_EC_BAD_KEY                (-23)

#define PCRDR_LEN_HOST_NAME             127
#define PCRDR_LEN_APP_NAME              127
#define PCRDR_LEN_RUNNER_NAME           63
#define PCRDR_LEN_METHOD_NAME           63
#define PCRDR_LEN_BUBBLE_NAME           63
#define PCRDR_LEN_ENDPOINT_NAME         \
    (PCRDR_LEN_HOST_NAME + PCRDR_LEN_APP_NAME + PCRDR_LEN_RUNNER_NAME + 3)
#define PCRDR_LEN_UNIQUE_ID             63

#define PCRDR_MIN_PACKET_BUFF_SIZE      512
#define PCRDR_DEF_PACKET_BUFF_SIZE      1024
#define PCRDR_DEF_TIME_EXPECTED         5   /* 5 seconds */

/* the maximal size of a payload in a frame (4KiB) */
#define PCRDR_MAX_FRAME_PAYLOAD_SIZE    4096

/* the maximal size of a payload which will be held in memory (40KiB) */
#define PCRDR_MAX_INMEM_PAYLOAD_SIZE    40960

/* the maximal time to ping client (60 seconds) */
#define PCRDR_MAX_PING_TIME             60

/* the maximal no responding time (90 seconds) */
#define PCRDR_MAX_NO_RESPONDING_TIME    90

/* Connection types */
enum {
    CT_UNIX_SOCKET = 1,
    CT_WEB_SOCKET,
};

/* The frame operation codes for UnixSocket */
typedef enum USOpcode_ {
    US_OPCODE_CONTINUATION = 0x00,
    US_OPCODE_TEXT = 0x01,
    US_OPCODE_BIN = 0x02,
    US_OPCODE_END = 0x03,
    US_OPCODE_CLOSE = 0x08,
    US_OPCODE_PING = 0x09,
    US_OPCODE_PONG = 0x0A,
} USOpcode;

/* The frame header for UnixSocket */
typedef struct USFrameHeader_ {
    int op;
    unsigned int fragmented;
    unsigned int sz_payload;
    unsigned char payload[0];
} USFrameHeader;

/* packet body types */
enum {
    PT_TEXT = 0,
    PT_BINARY,
};

struct _pcrdr_msg;
typedef struct _pcrdr_msg pcrdr_msg;

struct _pcrdr_conn;
typedef struct _pcrdr_conn pcrdr_conn;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Helpers Helper functions
 *  implemented in helpers.c, for both server and clients.
 * @{
 */

/**
 * Get the return message of a return code.
 * 
 * @param ret_code: the return code.
 *
 * Returns the pointer to the message string of the specific return code.
 *
 * Returns: a pointer to the message string.
 *
 * Since: 1.0
 */
const char *pcrdr_get_ret_message(int ret_code);

/**
 * Get the error message of an error code.
 *
 * pcrdr_get_err_message:
 * @param err_code: the error code.
 *
 * Returns the pointer to the message string of the specific error code.
 *
 * Returns: a pointer to the message string.
 *
 * Since: 1.0
 */
const char *pcrdr_get_err_message(int err_code);

/**
 * Convert an error code to a return code.
 *
 * pcrdr_errcode_to_retcode:
 * @param err_code: the internal error code of PurCRDR.
 *
 * Returns the return code of the PurCRDR protocol according to
 * the internal error code.
 *
 * Returns: the return code of PurCRDR protocol.
 *
 * Since: 1.0
 */
int pcrdr_errcode_to_retcode(int err_code);

/**
 * Check whether a string is a valid token.
 * 
 * @param token: the pointer to the token string.
 * @param max_len: The maximal possible length of the token string.
 *
 * Checks whether a token string is valid. According to PurCRDR protocal,
 * the runner name, method name, bubble name should be a valid token.
 *
 * Note that a string with a length longer than \a max_len will
 * be considered as an invalid token.
 *
 * Returns: true for a valid token, otherwise false.
 *
 * Since: 1.0
 */
bool pcrdr_is_valid_token(const char *token, int max_len);

/**
 * Generate an unique identifier.
 *
 * @param id_buff: the buffer to save the identifier.
 * @param prefix: the prefix used for the identifier.
 *
 * Generates a unique id; the size of \a id_buff should be at least 64 long.
 *
 * Returns: none.
 *
 * Since: 1.0
 */
void pcrdr_generate_unique_id(char *id_buff, const char *prefix);

/**
 * Generate an unique MD5 identifier.
 *
 * @param id_buff: the buffer to save the identifier.
 * @param prefix: the prefix used for the identifier.
 *
 * Generates a unique id by using MD5 digest algorithm.
 * The size of \a id_buff should be at least 33 bytes long.
 *
 * Returns: none.
 *
 * Since: 1.0
 */
void pcrdr_generate_md5_id(char *id_buff, const char *prefix);

/**
 * Check whether a string is a valid unique identifier.
 *
 * @param id: the unique identifier.
 *
 * Checks whether a unique id is valid.
 *
 * Returns: none.
 *
 * Since: 1.0
 */
bool pcrdr_is_valid_unique_id(const char *id);

/**
 * Check whether a string is a valid MD5 identifier.
 *
 * @param id: the unique identifier.
 *
 * Checks whether a unique identifier is valid.
 *
 * Returns: none.
 *
 * Since: 1.0
 */
bool pcrdr_is_valid_md5_id(const char *id);

/**
 * Get the elapsed seconds.
 *
 * @param ts1: the earlier time.
 * @param ts2 (nullable): the later time.
 *
 * Calculates the elapsed seconds between two times.
 * If \a ts2 is NULL, the function uses the current time.
 *
 * Returns: the elapsed time in seconds (a double).
 *
 * Since: 1.0
 */
double pcrdr_get_elapsed_seconds(const struct timespec *ts1,
        const struct timespec *ts2);

/**@}*/

/**
 * @defgroup Connection Connection functions
 *
 * The connection functions are implemented in libhibus.c, only for clients.
 * @{
 */

/**
 * Connect to the server via UnixSocket.
 *
 * @param path_to_socket: the path to the unix socket.
 * @param app_name: the app name.
 * @param runner_name: the runner name.
 * @param conn: the pointer to a pcrdr_conn* to return the purcrdr connection.
 *
 * Connects to a PurCRDR server via WebSocket.
 *
 * Returns: the error code; zero means everything is ok.
 *
 * Since: 1.0
 */
int pcrdr_connect_via_unix_socket(const char *path_to_socket,
        const char *app_name, const char *runner_name, pcrdr_conn** conn);

/**
 * Connect to the server via WebSocket.
 *
 * @param srv_host_name: the host name of the server.
 * @param port: the port.
 * @param app_name: the app name.
 * @param runner_name: the runner name.
 * @param conn: the pointer to a pcrdr_conn* to return the purcrdr connection.
 *
 * Connects to a PurCRDR server via WebSocket.
 *
 * Returns: the error code; zero means everything is ok.
 *
 * Note that this function is not implemented so far.
 */
int pcrdr_connect_via_web_socket(const char *srv_host_name, int port,
        const char *app_name, const char *runner_name, pcrdr_conn** conn);

/**
 * Disconnect to the server.
 *
 * @param conn: the pointer to the purcrdr connection.
 *
 * Disconnects the purcrdr connection.
 *
 * Returns: the error code; zero means everything is ok.
 *
 * Since: 1.0
 */
int pcrdr_disconnect(pcrdr_conn* conn);

/**
 * Free a connection.
 *
 * @param conn: the pointer to the purcrdr connection.
 *
 * Frees the space used by the connection, including the connection itself.
 *
 * Returns: the error code; zero means everything is ok.
 *
 * Since: 1.0
 */
int pcrdr_free_connection(pcrdr_conn* conn);

/**
 * The prototype of an event handler.
 *
 * @param conn: the pointer to the purcrdr connection.
 * @param msg: the event message object.
 *
 * Since: 1.0
 */
typedef void (*pcrdr_event_handler)(pcrdr_conn* conn, const pcrdr_msg *msg);

/**
 * pcrdr_conn_get_event_handler:
 * @param conn: the pointer to the purcrdr connection.
 *
 * Returns the current error handler of the purcrdr connection.
 *
 * Since: 1.0
 */
pcrdr_event_handler pcrdr_conn_get_event_handler(pcrdr_conn* conn);

/**
 * Set the error handler of the connection.
 *
 * @param conn: the pointer to the purcrdr connection.
 * @param event_handler: the new error handler.
 *
 * Sets the error handler of the purcrdr connection, and returns the old one.
 *
 * Since: 1.0
 */
pcrdr_event_handler pcrdr_conn_set_event_handler(pcrdr_conn* conn,
        pcrdr_event_handler event_handler);

/**
 * Get the user data associated with the connection.
 *
 * @param conn: the pointer to the purcrdr connection.
 *
 * Returns the current user data (a pointer) bound with the purcrdr connection.
 *
 * Since: 1.0
 */
void *pcrdr_conn_get_user_data(pcrdr_conn* conn);

/**
 * Set the user data associated with the connection.
 *
 * @param conn: the pointer to the purcrdr connection.
 * @param user_data: the new user data (a pointer).
 *
 * Sets the user data of the purcrdr connection, and returns the old one.
 *
 * Since: 1.0
 */
void *pcrdr_conn_set_user_data(pcrdr_conn* conn, void* user_data);

/**
 * Get the last return code from the server.
 *
 * @param conn: the pointer to the purcrdr connection.
 *
 * Returns the last return code of PurCRDR result or error message.
 *
 * Since: 1.0
 */
int pcrdr_conn_get_last_ret_code(pcrdr_conn* conn);

/**
 * Get the server host name of a connection.
 *
 * @param conn: the pointer to the purcrdr connection.
 *
 * Returns the host name of the PurCRDR server.
 *
 * Since: 1.0
 */
const char *pcrdr_conn_srv_host_name(pcrdr_conn* conn);

/**
 * Get the own host name of a connection.
 *
 * @param conn: the pointer to the purcrdr connection.
 *
 * Returns the host name of the current PurCRDR client.
 *
 * Since: 1.0
 */
const char *pcrdr_conn_own_host_name(pcrdr_conn* conn);

/**
 * Get the app name of a connection.
 *
 * @param conn: the pointer to the purcrdr connection.
 *
 * Returns the app name of the current PurCRDR client.
 *
 * Since: 1.0
 */
const char *pcrdr_conn_app_name(pcrdr_conn* conn);

/**
 * Get the runner name of a connection.
 *
 * @param conn: the pointer to the purcrdr connection.
 *
 * Returns the runner name of the current PurCRDR client.
 *
 * Since: 1.0
 */
const char *pcrdr_conn_runner_name(pcrdr_conn* conn);

/**
 * Get the file descriptor of the connection.
 *
 * @param conn: the pointer to the purcrdr connection.
 *
 * Returns the file descriptor of the purcrdr connection socket.
 *
 * Returns: the file descriptor.
 *
 * Since: 1.0
 */
int pcrdr_conn_socket_fd(pcrdr_conn* conn);

/**
 * Get the connnection socket type.
 *
 * @param conn: the pointer to the purcrdr connection.
 *
 * Returns the socket type of the purcrdr connection.
 *
 * Returns: \a CT_UNIX_SOCKET for UnixSocket, and \a CT_WEB_SOCKET for WebSocket.
 *
 * Since: 1.0
 */
int pcrdr_conn_socket_type(pcrdr_conn* conn);

/**
 * Read a packet (pre-allocation version).
 *
 * @param conn: the pointer to the purcrdr connection.
 * @param packet_buf: the pointer to a buffer for saving the contents
 *      of the packet.
 * @param sz_packet: the pointer to a unsigned integer for returning
 *      the length of the packet.
 *
 * Reads a packet and saves the contents of the packet and returns
 * the length of the packet.
 *
 * Returns: the error code; zero means everything is ok.
 *
 * Note that use this function only if you know the length of
 * the next packet, and have a long enough buffer to save the
 * contents of the packet.
 *
 * Also note that if the length of the packet is 0, there is no data in
 * the packet. You should ignore the packet in this case.
 *
 * Since: 1.0
 */
int pcrdr_read_packet(pcrdr_conn* conn, char* packet_buf, size_t *sz_packet);

/**
 * Read a packet (allocation version).
 *
 * @param conn: the pointer to the purcrdr connection.
 * @param packet: the pointer to a pointer to a buffer for returning
 *      the contents of the packet.
 * @param sz_packet: the pointer to a unsigned integer for returning
 *      the length of the packet.
 *
 * Reads a packet and allocates a buffer for the contents of the packet
 * and returns the contents and the length.
 *
 * Returns: the error code; zero means everything is ok.
 *
 * Note that the caller is responsible for releasing the buffer.
 *
 * Also note that if the length of the packet is 0, there is no data in the packet.
 * You should ignore the packet in this case.
 *
 * Since: 1.0
 */
int pcrdr_read_packet_alloc(pcrdr_conn* conn, void **packet, size_t *sz_packet);

/**
 * Send a text packet to the server.
 *
 * @param conn: the pointer to the purcrdr connection.
 * @param text: the pointer to the text to send.
 * @param txt_len: the length to send.
 *
 * Sends a text packet to the PurCRDR server.
 *
 * Returns: the error code; zero means everything is ok.
 *
 * Since: 1.0
 */
int pcrdr_send_text_packet(pcrdr_conn* conn, const char *text, size_t txt_len);

/**
 * Ping the server.
 *
 * @param conn: the pointer to the purcrdr connection.
 *
 * Pings the PurCRDR server. The client should ping the server
 * about every 30 seconds to tell the server "I am alive".
 * According to the PurCRDR protocol, the server may consider
 * a client died if there was no any data from the client
 * for 90 seconds.
 *
 * Returns: the error code; zero means everything is ok.
 *
 * Since: 1.0
 */
int pcrdr_ping_server(pcrdr_conn* conn);

typedef enum {
    PCRDR_MSG_TYPE_REQUEST = 0,
    PCRDR_MSG_TYPE_RESPONSE,
    PCRDR_MSG_TYPE_EVENT,
} pcrdr_msg_type;

typedef enum {
    PCRDR_MSG_TARGET_SESSION = 0,
    PCRDR_MSG_TARGET_WINDOW,
    PCRDR_MSG_TARGET_TAB,
    PCRDR_MSG_TARGET_DOM,
} pcrdr_msg_target;

typedef enum {
    PCRDR_MSG_ELEMENT_TYPE_VOID = 0,
    PCRDR_MSG_ELEMENT_TYPE_CSS,
    PCRDR_MSG_ELEMENT_TYPE_XPATH,
    PCRDR_MSG_ELEMENT_TYPE_HANDLE,
} pcrdr_msg_element_type;

pcrdr_msg_type pcrdr_message_get_type(const pcrdr_msg *msg);

typedef enum {
    PCRDR_MSG_DATA_TYPE_VOID = 0,
    PCRDR_MSG_DATA_TYPE_EJSON,
    PCRDR_MSG_DATA_TYPE_TEXT,
} pcrdr_msg_data_type;

struct _pcrdr_msg {
    pcrdr_msg_type          type;
    pcrdr_msg_target        target;
    pcrdr_msg_element_type  elementType;
    pcrdr_msg_data_type     dataType;
    unsigned int            retCode;

    uintptr_t       targetValue;
    char *          operation;
    char *          element;
    char *          property;
    char *          event;

    char *          requestId;

    uintptr_t       resultValue;

    size_t          dataLen;
    char *          data;
};

/**
 * Make a request message.
 *
 * @param target: the target of the message.
 * @param target_value: the value of the target object
 * @param operation: the request operation.
 * @param element_type: the element type of the request
 * @param element: the pointer to the element(s) (nullable).
 * @param property: the property (nullable).
 * @param data_type: the data type of the request.
 * @param data: the pointer to the data (nullable)
 *
 * Returns: The pointer to message object; NULL on error.
 *
 * Since: 1.0
 */
pcrdr_msg *pcrdr_make_request_message(
        pcrdr_msg_target target, uintptr_t target_value,
        const char *operation,
        const char *request_id,
        pcrdr_msg_element_type element_type, const char *element,
        const char *property,
        pcrdr_msg_data_type data_type, const char* data, size_t data_len);

/**
 * Make a response message for a request message.
 *
 * @param request_msg: the request message.
 * @param ret_code: the return code.
 * @param result_value: the result value.
 * @param extra_info: the extra information (nullable).
 *
 * Returns: The pointer to the response message object; NULL on error.
 *
 * Since: 1.0
 */
pcrdr_msg *pcrdr_make_response_message(
        const char *request_id,
        unsigned int ret_code, uintptr_t result_value,
        pcrdr_msg_data_type data_type, const char* data, size_t data_len);

/**
 * Make an event message.
 *
 * @param target: the target of the message.
 * @param target_value: the value of the target object
 * @param event: the event name.
 * @param element_type: the element type.
 * @param element: the pointer to the element(s) (nullable).
 * @param property: the property (nullable)
 * @param data_type: the data type of the event.
 * @param data: the pointer to the data (nullable)
 *
 * Returns: The pointer to the event message object; NULL on error.
 *
 * Since: 1.0
 */
pcrdr_msg *pcrdr_make_event_message(
        pcrdr_msg_target target, uintptr_t target_value,
        const char *event,
        pcrdr_msg_element_type element_type, const char *element,
        const char *property,
        pcrdr_msg_data_type data_type, const char* data, size_t data_len);

/**
 * Parse a packet and make a corresponding message.
 *
 * @param packet_text: the pointer to the packet text buffer.
 * @param sz_packet: the size of the packet.
 * @param msg: The pointer to a pointer to return the parsed message object.
 *
 * Returns: the error code; zero means everything is ok.
 *
 * Note that this function may change the content in \a packet.
 *
 * Since: 1.0
 */
int pcrdr_parse_packet(char *packet, size_t sz_packet, pcrdr_msg **msg);

typedef ssize_t (*cb_write)(void *ctxt, const void *buf, size_t count);

/**
 * Serialize a message.
 *
 * @param msg: the poiter to the message to serialize.
 * @param fn: the callback to write characters.
 * @param ctxt: the context will be passed to fn.
 *
 * Returns: the error code; zero means everything is ok.
 *
 * Since: 1.0
 */
int pcrdr_serialize_message(const pcrdr_msg *msg, cb_write fn, void *ctxt);

/**
 * Serialize a message to buffer.
 *
 * @param msg: the poiter to the message to serialize.
 * @param buff: the pointer to the buffer.
 * @param sz: the size of the buffer.
 *
 * Returns: the number of characters should be written to the buffer.
 * A return value more than @sz means that the output was truncated.
 *
 * Since: 1.0
 */
size_t pcrdr_serialize_message_to_buffer(const pcrdr_msg *msg,
        void *buff, size_t sz);

/**
 * Compare two messages.
 *
 * @param msga: the poiter to the first message.
 * @param msga: the poiter to the second message.
 *
 * Returns: zero when the messages are identical.
 *
 * Since: 1.0
 */
int pcrdr_compare_messages(const pcrdr_msg *msg_a, const pcrdr_msg *msg_b);

/**
 * Release a message.
 *
 * @param msg: the poiter to the message to release.
 *
 * Returns: None.
 *
 * Since: 1.0
 */
void pcrdr_release_message(pcrdr_msg *msg);

/**
 * The prototype of a result handler.
 *
 * @param conn: the pointer to the purcrdr connection.
 * @param request_msg: the original request message.
 * @param response_msg: the response message.
 *
 * Returns: 0 for finished the handle of the result; otherwise -1.
 *
 * Note that after calling the result handler, the request message object
 * and the response message object will be released.
 * Since: 1.0
 */
typedef int (*pcrdr_result_handler)(pcrdr_conn* conn,
        const pcrdr_msg *request_msg,
        const pcrdr_msg *response_msg);

/**
 * Send a request and handle the result in a callback.
 *
 * @param conn: the pointer to the purcrdr connection.
 * @param request_msg: the pointer to the request message.
 * @param time_expected: the expected return time in seconds.
 * @param result_handler: the result handler.
 * @param request_id (nullable): the buffer to store the request identifier.
 *
 * This function emits a request to the purcrdr server and
 * returns immediately. The result handler will be called
 * in subsequent calls of \a pcrdr_read_and_dispatch_packet().
 *
 * Returns: the error code; zero means everything is ok.
 *
 * Since: 1.0
 */
int pcrdr_send_request(pcrdr_conn* conn, pcrdr_msg *request_msg,
        int time_expected, pcrdr_result_handler result_handler);

/**
 * Send a request and wait the response.
 *
 * @param conn: the pointer to the purcrdr connection.
 * @param request_msg: the pointer to the request message.
 * @param time_expected: the expected return time in seconds.
 * @param response_msg: the pointer to a pointer to return the response message.
 *
 * This function send a request to the server and wait for the result.
 *
 * Returns: the error code; zero means everything is ok.
 *
 * Since: 1.0
 */
int pcrdr_send_request_and_wait(pcrdr_conn* conn, const pcrdr_msg *request_msg,
        int time_expected, pcrdr_msg **response_msg);

/**
 * Read and dispatch the packet from the server.
 *
 * @param conn: the pointer to the purcrdr connection.
 *
 * This function read a PurCRDR packet and dispatches the packet to
 * a event handler, method handler, or result handler.
 *
 * Returns: the error code; zero means everything is ok.
 *
 * Since: 1.0
 */
int pcrdr_read_and_dispatch_packet(pcrdr_conn* conn);

/**
 * Wait and dispatch the packet from the server.
 *
 * @param conn: the pointer to the purcrdr connection.
 * @param timeout_ms (not nullable): the timeout value in milliseconds.
 *
 * This function waits for a PurCRDR packet by calling select()
 * and dispatches the packet to event handlers, method handlers,
 * or result handlers.
 *
 * Returns: the error code; zero means everything is ok.
 *
 * Note that if you need watching multiple file descriptors, you'd
 * better user \a pcrdr_read_and_dispatch_packet.
 *
 * Since: 1.0
 */
int pcrdr_wait_and_dispatch_packet(pcrdr_conn* conn, int timeout_ms);

/**@}*/

#ifdef __cplusplus
}
#endif

/**
 * @addtogroup Helpers
 *  @{
 */

/**
 * Convert a string to uppercases in place.
 *
 * @param name: the pointer to a name string (not nullable).
 *
 * Converts a name string uppercase in place.
 *
 * Returns: the length of the name string.
 *
 * Since: 1.0
 */
static inline int
pcrdr_name_toupper(char *name)
{
    int i = 0;

    while (name [i]) {
        name [i] = toupper(name[i]);
        i++;
    }

    return i;
}

/**
 * Convert a string to lowercases and copy to another buffer.
 *
 * @param name: the pointer to a name string (not nullable).
 * @param buff: the buffer used to return the converted name string (not nullable).
 * @param max_len: The maximal length of the name string to convert.
 *
 * Converts a name string lowercase and copies the letters to
 * the specified buffer.
 *
 * Note that if \a max_len <= 0, the argument will be ignored.
 *
 * Returns: the total number of letters converted.
 *
 * Since: 1.0
 */
static inline int
pcrdr_name_tolower_copy(const char *name, char *buff, int max_len)
{
    int n = 0;

    while (*name) {
        buff [n] = tolower(*name);
        name++;
        n++;

        if (max_len > 0 && n == max_len)
            break;
    }

    buff [n] = '\0';
    return n;
}

/**
 * Convert a string to uppercases and copy to another buffer.
 *
 * @param name: the pointer to a name string (not nullable).
 * @param buff: the buffer used to return the converted name string (not nullable).
 * @param max_len: The maximal length of the name string to convert.
 *
 * Converts a name string uppercase and copies the letters to
 * the specified buffer.
 *
 * Note that if \a max_len <= 0, the argument will be ignored.
 *
 * Returns: the total number of letters converted.
 *
 * Since: 1.0
 */
static inline int
pcrdr_name_toupper_copy(const char *name, char *buff, int max_len)
{
    int n = 0;

    while (*name) {
        buff [n] = toupper(*name);
        name++;
        n++;

        if (max_len > 0 && n == max_len)
            break;
    }

    buff [n] = '\0';
    return n;
}

/**
 * Get monotonic time in seconds
 *
 * Gets the monotoic time in seconds.
 *
 * Returns: the the monotoic time in seconds.
 *
 * Since: 1.0
 */
static inline time_t pcrdr_get_monotoic_time(void)
{
    struct timespec tp;

    clock_gettime(CLOCK_MONOTONIC, &tp);
    return tp.tv_sec;
}

bool pcrdr_is_valid_host_name(const char *host_name);
bool pcrdr_is_valid_app_name(const char *app_name);
bool pcrdr_is_valid_endpoint_name(const char *endpoint_name);
int pcrdr_extract_host_name(const char *endpoint, char *buff);
int pcrdr_extract_app_name(const char *endpoint, char *buff);
int pcrdr_extract_runner_name(const char *endpoint, char *buff);
char *pcrdr_extract_host_name_alloc(const char *endpoint);
char *pcrdr_extract_app_name_alloc(const char *endpoint);
char *pcrdr_extract_runner_name_alloc(const char *endpoint);
int pcrdr_assemble_endpoint_name(const char *host_name, const char *app_name,
        const char *runner_name, char *buff);
char *pcrdr_assemble_endpoint_name_alloc(const char *host_name,
        const char *app_name, const char *runner_name);

static inline bool
pcrdr_is_valid_runner_name(const char *runner_name)
{
    return pcrdr_is_valid_token(runner_name, PCRDR_LEN_RUNNER_NAME);
}

/**@}*/

#endif /* !_PURCRDR_H_ */

