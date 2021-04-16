/**
 * @file rv_jsonrpc_error.c
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

static char *rv_jsonrpc_strerror(JsonRpcError error, char *buffer, unsigned buffer_len) {
    if(!buffer || (buffer_len == 0)) {
        return NULL;
    }
    
    switch(error) {
        case eRvInvalidObject:
            strncpy(buffer, "Invalid JSON object", buffer_len);
            break;
        case eRvUnsupportedEnc: 
            strncpy(buffer, "Unsupported JSONRPC encoding", buffer_len);
            break;
        case eRvInvalidEnc: 
            strncpy(buffer, "Invalid JSONRPC encoding", buffer_len);
            break;
            
        case eRvInvalidMessage: 
            strncpy(buffer, "Invalid JSONRPC message", buffer_len);
            break;
        case eRvUnknownCommand: 
            strncpy(buffer, "Unknown command", buffer_len);
            break;
        case eRvInvalidValue: 
            strncpy(buffer, "Invalid value", buffer_len);
            break;
            
        case eRvApplicationErr: 
            strncpy(buffer, "General application error", buffer_len);
            break;
        
        case eRvSystemErr: 
            strncpy(buffer, "General system error", buffer_len);
            break;
            
        case eRvTransportErr: 
            strncpy(buffer, "General transport error", buffer_len);
            break;
            
        default: 
            strncpy(buffer, "Unknown error", buffer_len);
    }
    
    return buffer;
}

// steals reference of json objects
RvJsonRpcError* rv_jsonrpc_error_ctor(void *mem, int code, const char *message, json_t *data) {
    RvJsonRpcError *self = (RvJsonRpcError*)mem;
    
    if(!self) {
        // no memory given, we must allocate it
        self = (RvJsonRpcError*)calloc(1, sizeof(RvJsonRpcError));
        if(!self) {
            pr_emerg("Could not allocate memory for JSON RPC request!");
            return NULL;
        }
    }
    memset(self, 0, sizeof(RvJsonRpcError));

    self->code = code;
    if(!message) {
        rv_jsonrpc_strerror(self->code, self->message, JSONRPC_ERRORMSG_MAXLEN);
    } else {
        strncpy(self->message, message, strnlen(message, JSONRPC_ERRORMSG_MAXLEN));
    }

    if(data) {
        self->data = json_deep_copy(data);
    }

    return self;
}

RvJsonRpcError* rv_jsonrpc_error_dtor(RvJsonRpcError *self) {
    if(!self) {
        return NULL;
    }
    
    json_decref(self->data);
    
    memset(self, 0, sizeof(RvJsonRpcError));
    
    return self;
}

json_t *rv_jsonrpc_error_encode(RvJsonRpcError *self) {
    json_t *result = json_object();
    
    json_object_set_new(result, "code", json_integer(self->code));
    json_object_set_new(result, "message", json_string(self->message));

    if(self->data) {
        json_object_set_new(result, "data", json_incref(self->data));
    }

    return result;
}

int *rv_jsonrpc_error_decode(RvJsonRpcError *self, json_t *object) {
    return 0;
}
