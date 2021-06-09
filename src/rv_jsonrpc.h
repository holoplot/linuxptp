/**
 * @file rv_jsonrpc.h
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

#include "jansson.h"

#include "rv_init.h"
#include "rv_mqtt.h" // for JsonRpcError

#define JSONRPC_ERRORMSG_MAXLEN 1024

#define JSONRPC_PROTOCOL_VERSION "1.0"
#define JSONRPC_PROCESS_VERSION "1.17"

struct json_t;

typedef struct rv_jsonrpc_request_t {
    char method[RV_NAME_MAX];
    json_t *data;
    json_t *id;
} RvJsonRpcRequest;

typedef struct rv_jsonrpc_response_t {
    json_t *result; // might contain error object as well
    json_t *id;
} RvJsonRpcResponse;

typedef struct rv_jsonrpc_error_t {
    JsonRpcError code;
    char message[JSONRPC_ERRORMSG_MAXLEN];
    
    json_t *data;
} RvJsonRpcError;

extern RvJsonRpcRequest *rv_jsonrpc_request_ctor(void *self, const char *method, json_t *data, json_t *id);
extern RvJsonRpcRequest *rv_jsonrpc_request_dtor(RvJsonRpcRequest *self);
extern json_t* rv_jsonrpc_request_encode(RvJsonRpcRequest *self);
extern int rv_jsonrpc_request_decode(RvJsonRpcRequest *self, json_t *object, RvJsonRpcError *error);

extern RvJsonRpcResponse *rv_jsonrpc_response_ctor(void *self, json_t *result, json_t *id);
extern RvJsonRpcResponse *rv_jsonrpc_response_dtor(RvJsonRpcResponse *self);
extern json_t* rv_jsonrpc_response_encode(RvJsonRpcResponse *self);
extern int rv_jsonrpc_response_decode(RvJsonRpcResponse *self, json_t *object, RvJsonRpcError *error);

extern RvJsonRpcError* rv_jsonrpc_error_ctor(void *self, int code, const char *message, json_t *data);
extern RvJsonRpcError* rv_jsonrpc_error_dtor(RvJsonRpcError *self);
extern json_t *rv_jsonrpc_error_encode(RvJsonRpcError *self);
extern int *rv_jsonrpc_error_decode(RvJsonRpcError *self, json_t *object);

/** @} */
