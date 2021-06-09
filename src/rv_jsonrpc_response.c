/**
 * @file rv_jsonrpc_response.c
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

#include "print.h"

#include <stdlib.h>
#include <string.h>

// note: 'data' steals json reference!
RvJsonRpcResponse *rv_jsonrpc_response_ctor(void *mem, json_t *data, json_t *id) {
    RvJsonRpcResponse *self = (RvJsonRpcResponse*)mem;
    
    if(!self) {
        // no memory given, we must allocate it
        self = (RvJsonRpcResponse*)calloc(1, sizeof(RvJsonRpcResponse));
        if(!self) {
            pr_emerg("Could not allocate memory for JSON RPC request!");
            return NULL;
        }
    }
    memset(self, 0, sizeof(RvJsonRpcResponse));
    
    self->result = json_deep_copy(data);
    self->id     = json_deep_copy(id);
    
    return self;
}

extern RvJsonRpcResponse *rv_jsonrpc_response_dtor(RvJsonRpcResponse *self) {
    if(!self) {
        return NULL;
    }
    
    json_decref(self->result);
    json_decref(self->id);
    
    memset(self, 0, sizeof(RvJsonRpcResponse));
    
    return self;
}

json_t *rv_jsonrpc_response_encode(RvJsonRpcResponse *self) {
    json_t *jsonrpc_response;
    
    if(!self) {
        return NULL;
    }

    if(!json_object_get(self->result, "result") && !json_object_get(self->result, "error")) {
        return NULL;
    }
    
    if(!self->id) {
        return NULL;
    }
    
    jsonrpc_response = json_object();
    json_object_set_new(jsonrpc_response, "jsonrpc", json_string("2.0"));
    json_object_update(jsonrpc_response, self->result);
    json_object_set_new(jsonrpc_response, "id", json_incref(self->id));
    
    return jsonrpc_response;
}

int rv_jsonrpc_response_decode(RvJsonRpcResponse *self, json_t *object, RvJsonRpcError *error) {
    json_t *value;

    if(!self || !object) {
        error = rv_jsonrpc_error_ctor(error, eRvInvalidObject, NULL, NULL);
        return -1;
    }
    memset(self, 0, sizeof(RvJsonRpcResponse));

    json_t *id = json_object_get(object, "id");
    if(id) {
        self->id = json_deep_copy(id);
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

    value = json_object_get(object, "result");
    if(!value) {
        value = json_object_get(object, "error");
        if(!value) {
            error = rv_jsonrpc_error_ctor(error, eRvInvalidMessage, NULL, NULL);
            return -1;
        }
    }
    self->result = json_deep_copy(value);
    
    return 0;
}
