/**
 * @file rv_mqtt.h
 * @note Copyright (C) 2017, ALC NetworX GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#pragma once

#include "print.h"

#include <jansson.h>
#include <uv.h>
#include <MQTTAsync.h>

#define RV_NAME_MAX 127

#define RV_MQTT_DEFAULT_BROKER_ADDR "127.0.0.1"
#define RV_MQTT_DEFAULT_BROKER_PORT 1883

#define RV_MQTT_DEFAULT_CLIENT_ID "ALCNetworX"
#define RV_MQTT_DEFAULT_CLIENT_VERSION "0.0.0"

#define MQTT_MAX_MESSAGE_HANDLER 512

#define RV_FAILURE_INT -1
#define RV_FAILURE_PTR NULL
#define RV_FAILURE_BOOL false
#define RV_FAILURE_VOID

#define CONCAT(a, b) a ## b
#define CONCAT2(a, b) CONCAT(a, b)

#define RV_PARAMETER_CHECK_INT(x, y)  {if(!x) {pr_err("%s()", __func__); return y;}}
#define RV_PARAMETER_CHECK_VOID(x, y) {if(!x) {return y;}}
#define RV_PARAMETER_CHECK_PTR(x, y)  RV_PARAMETER_CHECK_INT(x, y)
#define RV_PARAMETER_CHECK_BOOL(x, y) RV_PARAMETER_CHECK_VOID(x, y)
#define RV_PARAMETER_CHECK(x, y) CONCAT2(RV_PARAMETER_CHECK_, y)((x), CONCAT2(RV_FAILURE_, y))

typedef int (*rv_mqtt_message_cb)(void* context, const char *wildcard_name, const char *method, struct json_t *data);

struct mqtt_topic_t {
    char name[RV_NAME_MAX];
    
    rv_mqtt_message_cb process_message;
    void *context;
    
    // JSON RPC request id for current request to a specific topic (according to the subscription)
    // request id must be locked while waiting for a response, to be able to check responses to it
    // (but with maximum timeout, in case there is no response at all)
    uv_mutex_t request_id_guard;
    uv_cond_t request_id_waiter;  
    uint32_t request_id;         // current id we are waiting for
    
    char response_topic[RV_NAME_MAX];
};

typedef struct mqtt_handle_t RvMQTTHandle;
struct mqtt_handle_t {
    MQTTAsync mqtt_client;
    uv_mutex_t guard;
    
    char broker_addr[RV_NAME_MAX];
    
    char client_id[RV_NAME_MAX/2];
    char client_version[RV_NAME_MAX/2];
    
    struct mqtt_topic_t topic[MQTT_MAX_MESSAGE_HANDLER];
};

struct json_t;

enum jsonrpc_error_e {
    eRvInvalidObject  = -32700,  // parse error, not well formed
    eRvUnsupportedEnc = -32701,  // parse error, unsupported encoding
    eRvInvalidEnc     = -32702,  // parse error, invalid character for encoding
    
    eRvInvalidMessage = -32600,  // server error, invalid JSONRPC, not conforming to spec
    eRvUnknownCommand = -32601,  // server error, requested method not found
    eRvInvalidValue   = -32602,  // server error, invalid method parameters
    
    eRvApplicationErr = -32500,  // general application error
    
    eRvSystemErr      = -32400,  // general system error
    
    eRvTransportErr   = -32300,  // general transport error
};
typedef enum jsonrpc_error_e JsonRpcError;


extern RvMQTTHandle *rv_mqtt_ctor(RvMQTTHandle *self, const char *client_id, const char *client_version, const char *broker_addr, uint16_t port);
extern void rv_mqtt_dtor(RvMQTTHandle *self);

extern int rv_mqtt_subscribe(RvMQTTHandle *self, char *topic_name, rv_mqtt_message_cb topic_msg_handler, void *context, char *response_topic);
extern void rv_mqtt_unsubscribe(RvMQTTHandle *self, char *topic_name);

extern int rv_mqtt_publish_health(RvMQTTHandle *self, const char *topic, json_t *data);
extern int rv_mqtt_publish_jsonrpc(RvMQTTHandle *self, const char *topic, const char *method, json_t *data, json_t *id);
extern int rv_mqtt_publish(RvMQTTHandle *self, const char *topic_name, int retain, char *message, unsigned message_len);

extern json_t *rv_mqtt_request_id(RvMQTTHandle *self, const char *topic_name);
