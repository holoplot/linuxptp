/**
 * @file rv_jsonrpc_request.c
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

#include "rv_jsonrpc.h"
#include "rv_random.h"

#include "print.h"

#include <stdlib.h>
#include <string.h>

// note: 'data' steals json reference!
RvJsonRpcRequest *rv_jsonrpc_request_ctor(void *mem, const char *method, json_t *data, json_t *id) {
    RvJsonRpcRequest *self = (RvJsonRpcRequest*)mem;
    
    if(!self) {
        // no memory given, we must allocate it
        self = (RvJsonRpcRequest*)calloc(1, sizeof(RvJsonRpcRequest));
        if(!self) {
            pr_emerg("Could not allocate memory for JSON RPC request!");
            return NULL;
        }
    }
    memset(self, 0, sizeof(RvJsonRpcRequest));
    
    strncpy(self->method, method, RV_NAME_MAX - 1);
    self->data = json_deep_copy(data);    
    self->id = id ? json_deep_copy(id) : id;

    return self;
}

RvJsonRpcRequest *rv_jsonrpc_request_dtor(RvJsonRpcRequest *self) {
    if(!self) {
        return NULL;
    }
    
    // json_decref checks for NULL pointer
    json_decref(self->data);
    json_decref(self->id);
    
    memset(self, 0, sizeof(RvJsonRpcRequest));
    
    return self;
}

json_t* rv_jsonrpc_request_encode(RvJsonRpcRequest *self) {
    json_t *jsonrpc_request, *parameter;
    RvTimeUtc time_utc;
    
    char process_hash[RV_CNAME_LEN+1] = {0};
    
    if(!self) {
        // nothing to do
        return NULL;
    }
    
    // prepare additional values to be added automatically
    time_utc = rv_gettimeofday();
    
    rv_random_hash(process_hash, RV_CNAME_LEN + 1);
    
    parameter = json_deep_copy(self->data);
    json_object_set_new(parameter, "timestamp", json_integer(time_utc.seconds));
    json_object_set_new(parameter, "protocol_version", json_string(JSONRPC_PROTOCOL_VERSION));
    json_object_set_new(parameter, "process_hash", json_string(process_hash));
    
    jsonrpc_request = json_object();
    json_object_set_new(jsonrpc_request, "jsonrpc", json_string("2.0"));
    json_object_set_new(jsonrpc_request, "method", json_string(self->method));
    json_object_set_new(jsonrpc_request, "params", parameter);
    
    if(self->id) {
        json_object_set_new(jsonrpc_request, "id", json_deep_copy(self->id));
    }
    
    return jsonrpc_request;
}

int rv_jsonrpc_request_decode(RvJsonRpcRequest *self, json_t *object, RvJsonRpcError *error) {
    json_t *value;
    
    if(!self || !object) {
        error = rv_jsonrpc_error_ctor(error, eRvInvalidObject, NULL, NULL);
        return -1;
    }

    // preinitialize request
    memset(self, 0, sizeof(RvJsonRpcRequest));
    
    value = json_object_get(object, "id");
    if(value) {
        self->id = json_deep_copy(value);
    }
    
    // check key jsonrpc
    value = json_object_get(object, "jsonrpc");
    if(!value) {
        error = rv_jsonrpc_error_ctor(error, eRvInvalidMessage, NULL, NULL);
        return -1;
    }
    if(strncmp(json_string_value(value), "2.0", 4)) {
        error = rv_jsonrpc_error_ctor(error, eRvUnsupportedEnc, NULL, NULL);
        return -1;
    }
    
    value = json_object_get(object, "method");
    if(!value) {
        error = rv_jsonrpc_error_ctor(error, eRvInvalidMessage, NULL, NULL);
        return -1;
    }
    strncpy(self->method, json_string_value(value), RV_NAME_MAX - 1);
    
    value = json_object_get(object, "params");
    if(!value) {
        rv_jsonrpc_request_dtor(self);
        return -1;
    }
    // we ignore management values for now!
    json_object_del(value, "process_hash");
    json_object_del(value, "protocol_version");
    json_object_del(value, "timestamp");
    
    self->data = json_deep_copy(value);
    
    return 0;
}
