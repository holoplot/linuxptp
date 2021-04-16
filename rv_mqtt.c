/**
 * @file rv_mqtt.c
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
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "rv_mqtt.h"
#include "rv_random.h"
#include "rv_jsonrpc.h"

#include <MQTTAsync.h>

const uint32_t timeout = 10000L;

static void process_message(RvMQTTHandle *mqtt_handle, struct mqtt_topic_t *topic, char *wildcard_name, char *message, unsigned message_len) {
    json_t        *json_message;
    json_error_t  json_error;

    RvJsonRpcRequest jsonrpc_request;
    RvJsonRpcResponse jsonrpc_response;
    RvJsonRpcError jsonrpc_error;
    
    memset(&jsonrpc_request, 0, sizeof(RvJsonRpcRequest));
    memset(&jsonrpc_response, 0, sizeof(RvJsonRpcResponse));
    memset(&jsonrpc_error, 0, sizeof(RvJsonRpcError));
    
    if(!message) {
        return;
    }
    
    if(message_len == 0) {
        // empty object
        json_message = json_object();
        
        topic->process_message(topic->context, wildcard_name, "delete", json_message);
        
        json_decref(json_message);
        return;
    }
    
    // load json message structure
    memset(&json_error, 0, sizeof(json_error_t));
    json_message = json_loads(message, JSON_DISABLE_EOF_CHECK, &json_error);
    if(!json_message) {
        pr_err("Could not load mqtt json message: %s", json_error.text);
        return;
    }

    // first of all: find out what message we have to deal with
    if(json_object_get(json_message, "jsonrpc") && json_object_get(json_message, "method")) {
        // it's a json rpc request
        if(rv_jsonrpc_request_decode(&jsonrpc_request, json_message, &jsonrpc_error) < 0) {
            // invalid message, check if it was a notification, or if a response is required
            if(jsonrpc_request.id && (strlen(topic->response_topic) > 0)) {
                json_t *json_err = rv_jsonrpc_error_encode(&jsonrpc_error);
                rv_mqtt_publish_jsonrpc(mqtt_handle, topic->response_topic, NULL, json_err, jsonrpc_request.id);
                json_decref(json_err);
            }
            // now log and ignore it then
            pr_err("Invalid JSON RPC request message received: %s", message); 

            rv_jsonrpc_request_dtor(&jsonrpc_request);
            rv_jsonrpc_error_dtor(&jsonrpc_error);
            return;
        }
        
        // finally call topic callback
        int result = topic->process_message(topic->context, wildcard_name, jsonrpc_request.method, jsonrpc_request.data); 
        
        // check if we shall send a response
        if(jsonrpc_request.id && (strlen(topic->response_topic) > 0)) {
            json_t *json_result;
            // check for error
            if(result >= 0) {
                // all is fine
                json_result = json_object();
                json_object_set_new(json_result, "result", json_integer(result));
            } else {
                // something was wrong
                rv_jsonrpc_error_ctor(&jsonrpc_error, result, NULL, NULL);
                json_result = rv_jsonrpc_error_encode(&jsonrpc_error);
            }
            rv_mqtt_publish_jsonrpc(mqtt_handle, topic->response_topic, NULL, json_result, jsonrpc_request.id);

            json_decref(json_result);
        }
        
        rv_jsonrpc_request_dtor(&jsonrpc_request);
        rv_jsonrpc_error_dtor(&jsonrpc_error);
        
        json_decref(json_message);
        return; // request is handled, nothing more to do
    } 
    
    if(json_object_get(json_message, "jsonrpc") && (json_object_get(json_message, "result") || json_object_get(json_message, "error"))) {
        // it's a json rpc response
        if(rv_jsonrpc_response_decode(&jsonrpc_response, json_message, &jsonrpc_error) < 0) {
            // we could not decode jsonrpc_response
            pr_err("Invalid JSON RPC response message received: %s", message); 

            rv_jsonrpc_response_dtor(&jsonrpc_response);
            rv_jsonrpc_error_dtor(&jsonrpc_error);
            return;
        }

        // TODO: compare request id
        
        // finally call topic callback
        topic->process_message(topic->context, wildcard_name, NULL, jsonrpc_response.result);

        rv_jsonrpc_response_dtor(&jsonrpc_response);
        rv_jsonrpc_error_dtor(&jsonrpc_error);
        
        json_decref(json_message);
        return; // response is handled, nothing more to do
    }
    
    // it's probably flat json
    // try to call topic callback with json object received, use "invalid RPC" as marker
    topic->process_message(topic->context, wildcard_name, "invalid RPC", json_message); 
    
    json_decref(json_message);
}

static bool topic_cmp(const char *tgt_name, const char *src_name, char* wildcard_name) {
    // check level by level
    char *tgt_level, *src_level;

    // default wildcard_name: complete topic name
    memset(wildcard_name, 0, RV_NAME_MAX);
    strncpy(wildcard_name, src_name, strnlen(src_name, RV_NAME_MAX));
    
    while(tgt_name && src_name) {
        tgt_level = strchr(tgt_name, '/');
        src_level = strchr(src_name, '/');
        
        if(!tgt_level && !src_level) {
            // both are null, compare the last layer
            if(!strncmp(tgt_name, src_name, strnlen(tgt_name, RV_NAME_MAX))) {
                break;
            }
            
            // take into account possible wildcards here
            if(!strncmp(tgt_name, "+", 1)) {
                memset(wildcard_name, 0, RV_NAME_MAX);
                strncpy(wildcard_name, src_name, strnlen(src_name, RV_NAME_MAX));
                break;
            }

            return false;
        }
        
        if(!tgt_level || !src_level) {
            // one of them is at the end, while the other is not
            // this is allowed only, if tgt_name points to '#'
            if(!strncmp(tgt_name, "#", 1)) {
                break;
            } 
            return false;
        }
        
        // 'src_level' and 'tgt_level' are for sure != NULL here (see check above)
        // skip the '+' wildcard here
        if(!strncmp(tgt_name, "+", 1)) {
            // save wildcard ID for later use here, src_level already points to the next '/'
            memset(wildcard_name, 0, RV_NAME_MAX);
            strncpy(wildcard_name, src_name, strnlen(src_name, src_level - src_name));
        } else {
            // compare current layer
            unsigned len = tgt_level - tgt_name;
            if(strncmp(tgt_name, src_name, len)) {
                // not equal
                return false;
            }
        }

        // + 1 is at least the Null-byte, so it is safe to start a new loop
        tgt_name = tgt_level + 1;
        src_name = src_level + 1;
    }
    
    return true;
}

static int msgArrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message) {
    RvMQTTHandle *self = (RvMQTTHandle*)context;

    // make message->payload a C style string
    char *msg = (char*)calloc(message->payloadlen + 1, sizeof(char));
    memcpy(msg, message->payload, message->payloadlen);
    
    //rv_printf("Message arrived, topic: %s, message_len: %i, message: %s", topicName, message->payloadlen, (message->payloadlen > 0) ? msg : "");
    
    // search for message handler to be called, depending on topic
    for(unsigned idx = 0; idx < MQTT_MAX_MESSAGE_HANDLER; ++idx) {
        struct mqtt_topic_t *topic = &self->topic[idx];
        char wildcard_name[RV_NAME_MAX];

        memset(wildcard_name, 0, RV_NAME_MAX);
        if(topic_cmp(topic->name, topicName, wildcard_name) && topic->process_message) {
            pr_debug("Message handler %s for topic %s found.", topic->name, topicName);
            process_message(self, topic, wildcard_name, msg, message->payloadlen);
            
            break;
        }
    }

    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    free(msg);
    
    // MQTTCLIENT_SUCCESS is 0, but this function return value is boolean, so 0 means false
    return 1;
}

/*
static void onConnectFailure(void *context, MQTTAsync_failureData *data) {
    RvMQTTHandle *self = (RvMQTTHandle*)context;
}
*/

static void onConnectSuccess(void *context, MQTTAsync_successData *data) {
    RvMQTTHandle *self = (RvMQTTHandle*)context;

    RvJsonRpcRequest jsonrpc_request; 
    char version_topic[RV_NAME_MAX] = {0};
    
    char process_hash[RV_CNAME_LEN+1] = {0};
    rv_random_hash(process_hash, RV_CNAME_LEN + 1);
    
    json_t *version = json_object();
    json_object_set_new(version, "protocol_version", json_string("1.0"));
    json_object_set_new(version, "process_version", json_string(self->client_version));
    json_object_set_new(version, "process_hash", json_string(process_hash));
    
    rv_jsonrpc_request_ctor(&jsonrpc_request, "version", version, NULL);
    json_t *new_request = rv_jsonrpc_request_encode(&jsonrpc_request);        
    if(!new_request) {
        return;
    }
    
    char *message = json_dumps(new_request, 0);
    // send version into "process_version" topic
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    pubmsg.payload = message;
    pubmsg.payloadlen = strlen(message);

    pubmsg.qos = 0;
    pubmsg.retained = 1;
    
    snprintf(version_topic, RV_NAME_MAX, "%s/version", self->client_id);
    
    int rc = MQTTAsync_sendMessage(self->mqtt_client, version_topic, &pubmsg, NULL);
    if (rc != MQTTASYNC_SUCCESS) {
        pr_err("MQTTHandle %s failed to publish version to topic %s, return code %d", self->client_id, version_topic, rc);
    }
    
    // cleanup
    free(message);
    json_decref(new_request);
    rv_jsonrpc_request_dtor(&jsonrpc_request);

    json_decref(version);
}

int rv_mqtt_connect(RvMQTTHandle *self) {
    RV_PARAMETER_CHECK(self && self->mqtt_client, INT);
    
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    conn_opts.MQTTVersion = MQTTVERSION_DEFAULT;

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.will = NULL;
    
    conn_opts.context   = self;
    //conn_opts.onFailure = onConnectFailure;
    conn_opts.onSuccess = onConnectSuccess;
    conn_opts.connectTimeout = timeout;
    conn_opts.automaticReconnect = 1;
    
    pr_info("Connecting MQTTHandle %s to MQTT broker %s", self->client_id, self->broker_addr);
    
    int rc = MQTTAsync_connect(self->mqtt_client, &conn_opts);
    if (rc != MQTTASYNC_SUCCESS) {
        pr_err("MQTTHandle %s could not connected to MQTT broker %s: %i", self->client_id, self->broker_addr, rc);
        
        return -1;
    }

    while(!MQTTAsync_isConnected(self->mqtt_client)) {
        usleep(timeout);
    }
    
    return 0;
}

static void connectionLost(void *context, char *cause) {
    RvMQTTHandle *self = (RvMQTTHandle*)context;

    pr_warning("MQTT Handle %s lost connection to MQTT broker %s, cause %s, trying to reconnect.", self->client_id, self->broker_addr, cause);
    
    // try to reconnect
    if(rv_mqtt_connect(self)  < 0) {
        return;
    }
    
    for(unsigned idx = 0; idx < MQTT_MAX_MESSAGE_HANDLER; ++idx) {
        struct mqtt_topic_t *topic = &self->topic[idx];
        
        if(!topic->context) {
            continue;
        }
        
        rv_mqtt_subscribe(self, topic->name, topic->process_message, topic->context, topic->response_topic);
    }
}

#define MQTTAsync_disconnectOptions_initializer_mod { {'M', 'Q', 'T', 'D'}, 1, 0, NULL, NULL, NULL, MQTTProperties_initializer, MQTTREASONCODE_SUCCESS, NULL, NULL }
static void rv_mqtt_disconnect(RvMQTTHandle *self) {
    RV_PARAMETER_CHECK(self && self->mqtt_client, VOID);
    MQTTAsync_disconnectOptions opt = MQTTAsync_disconnectOptions_initializer_mod;

    //opt.context   = self;
    //opt.onFailure = onDisconnectFailure;
    //opt.onSuccess = onDisconnectSuccess;
    opt.timeout   = timeout; 
    
    int rc = MQTTAsync_disconnect(self->mqtt_client, &opt);
    if (rc != MQTTASYNC_SUCCESS) {
        pr_err("Failed to send disconnect to MQTT broker at %s: %i", self->broker_addr, rc);
    }
    
    while(MQTTAsync_isConnected(self->mqtt_client)) {
        usleep(timeout);
    }

    self->mqtt_client = NULL;
}

void rv_mqtt_dtor(RvMQTTHandle *self) {
    RV_PARAMETER_CHECK(self, VOID);
    
    if(MQTTAsync_isConnected(self->mqtt_client)) {
        rv_mqtt_disconnect(self);
    }
    
    MQTTAsync_destroy(&self->mqtt_client);
    
    uv_mutex_destroy(&self->guard);
    
    memset(self, 0, sizeof(RvMQTTHandle));
}

RvMQTTHandle *rv_mqtt_ctor(RvMQTTHandle *self, const char *client_id, const char *client_version, const char *broker_addr, uint16_t broker_port) {
    RV_PARAMETER_CHECK(self, PTR);
    
    memset(self, 0, sizeof(RvMQTTHandle));

    uv_mutex_init(&self->guard);
    
    strncpy(self->client_id, client_id ? client_id : RV_MQTT_DEFAULT_CLIENT_ID, RV_NAME_MAX/2 - 1);
    strncpy(self->client_version, client_version ? client_version : RV_MQTT_DEFAULT_CLIENT_VERSION, RV_NAME_MAX/2 - 1);
    
    snprintf(self->broker_addr, RV_NAME_MAX - 1, "%s:%i", (broker_addr ? broker_addr : RV_MQTT_DEFAULT_BROKER_ADDR), (broker_port ? broker_port : RV_MQTT_DEFAULT_BROKER_PORT));
    
    MQTTAsync_createOptions create_opts = MQTTAsync_createOptions_initializer;
    int rc = MQTTAsync_createWithOptions(&self->mqtt_client, self->broker_addr, self->client_id, MQTTCLIENT_PERSISTENCE_NONE, NULL, &create_opts);
    if(rc != MQTTASYNC_SUCCESS) {
        pr_err("Could not create MQTT client: %i", rc);
        
        return NULL;
    }
    
    rc = MQTTAsync_setCallbacks(self->mqtt_client, self, connectionLost, msgArrived, NULL);
    if(rc != MQTTASYNC_SUCCESS) {
        pr_err("Failed to set callbacks for MQTTHandle %s: %i", self->broker_addr, rc);
        
        rv_mqtt_dtor(self);
        return NULL;
    }
    
    if(rv_mqtt_connect(self) < 0) {
        rv_mqtt_dtor(self);
        return NULL;
    }
    
    return self;
}

/*
static void onDisconnectFailure(void *context, MQTTAsync_failureData *data) {
    RvMQTTHandle *self = (RvMQTTHandle*)context;
}

static void onDisconnectSuccess(void *context, MQTTAsync_successData *data) {
    RvMQTTHandle *self = (RvMQTTHandle*)context;

    //QUESTION: do we have to cleanup LWT manually here?
}
*/ 

/*
static void onSubscribeFailure(void *context, MQTTAsync_failureData *data) {
    RvMQTTHandle *self = (RvMQTTHandle*)context;
}

static void onSubscribeSuccess(void *context, MQTTAsync_successData *data) {
    RvMQTTHandle *self = (RvMQTTHandle*)context;
}
*/

int rv_mqtt_subscribe(RvMQTTHandle *self, char *topic_name, rv_mqtt_message_cb topic_msg_handler, void *context, char *response_topic) {
    RV_PARAMETER_CHECK(self && self->mqtt_client && topic_name && topic_msg_handler, INT);
    MQTTAsync_responseOptions opt = MQTTAsync_responseOptions_initializer;
    
    pr_debug("Subscribing to topic %s for client %s", topic_name, self->client_id);
    for(unsigned idx = 0; idx < MQTT_MAX_MESSAGE_HANDLER; ++idx) {
        struct mqtt_topic_t *topic = &self->topic[idx];

        if(topic->process_message == NULL) {
            continue;
        }
        
        // to avoid topic doubles, we overwrite possible existing topics here
        if(!strncmp(topic->name, topic_name, RV_NAME_MAX)) {
            topic->process_message = topic_msg_handler;
            topic->context = context;
            
            memset(response_topic, 0, RV_NAME_MAX);
            if(response_topic) {
                strncpy(topic->response_topic, response_topic, RV_NAME_MAX - 1);
            }
            
            goto subscribe;
        }
    }
    
    // nothing to overwrite, let's run through again, and look for a free place
    for(unsigned idx = 0; (idx < MQTT_MAX_MESSAGE_HANDLER); ++idx) {
        struct mqtt_topic_t *topic = &self->topic[idx];
    
        if(topic->process_message == NULL) {
            strncpy(topic->name, topic_name, RV_NAME_MAX - 1);
            topic->process_message = topic_msg_handler;
            topic->context = context;
            
            memset(topic->response_topic, 0, RV_NAME_MAX);
            if(response_topic) {
                strncpy(topic->response_topic, response_topic, RV_NAME_MAX - 1);
            }
        
            break;
        }
    }

subscribe:
    opt.context = self;
    //opt.onFailure = onSubscribeFailure;
    //opt.onSuccess = onSubscribeSuccess;

    int rc = MQTTAsync_subscribe(self->mqtt_client, topic_name, 0, &opt);
    if (rc != MQTTASYNC_SUCCESS) {
        pr_err("Failed to subscribe to TOPIC %s, return code %d", topic_name, rc);
        
        rv_mqtt_unsubscribe(self, topic_name);
        return -1;
    }
    
    // all places are in use by other topics, return failure here
    return 0;
}

void rv_mqtt_unsubscribe(RvMQTTHandle *self, char *topic_name) {
    RV_PARAMETER_CHECK(self && self->mqtt_client && topic_name, VOID);
    
    MQTTAsync_unsubscribe(self->mqtt_client, topic_name, NULL);

    // remove topic specific message handlers
    for(unsigned idx = 0; (idx < MQTT_MAX_MESSAGE_HANDLER); ++idx) {
        struct mqtt_topic_t *topic = &self->topic[idx];
       
        if(!strncmp(topic->name, topic_name, RV_NAME_MAX)) {
            memset(topic, 0, sizeof(struct mqtt_topic_t));
            break;
        }
    }
    
    pr_debug("Unsubscribing topic %s for client %s.", topic_name, self->client_id);
}

int rv_mqtt_publish(RvMQTTHandle *self, const char *topic_name, int retain, char *message, unsigned message_len) {
    RV_PARAMETER_CHECK(self && self->mqtt_client && topic_name, INT);
    
    uv_mutex_lock(&self->guard);
    
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer; 
    pubmsg.payload    = message ? message     : "";
    pubmsg.payloadlen = message ? message_len : 0 ;
    
    pubmsg.qos      = 0;
    pubmsg.retained = retain;
    
    int rc = MQTTAsync_sendMessage(self->mqtt_client, topic_name, &pubmsg, NULL);
    if (rc != MQTTASYNC_SUCCESS) {
        pr_err("MQTTHandler %s failed to publish status to topic %s, return code %d", self->client_id, topic_name, rc);
        uv_mutex_unlock(&self->guard);
        return -1;
    }
    uv_mutex_unlock(&self->guard);
    
    return 0;
}

int rv_mqtt_publish_health(RvMQTTHandle *self, const char *topic, json_t *data) 
{
    RV_PARAMETER_CHECK(self && topic && data && json_is_object(data), INT);

    // add timestamp to data
    RvTimeUtc time_utc = rv_gettimeofday();
    json_object_set_new(data, "timestamp", json_integer(time_utc.seconds));
    
    // publish message
    char *message = json_dumps(data, 0);
    rv_mqtt_publish(self, topic, 1, message, strlen(message));

    // release and return
    free(message);

    return 0;
}

int rv_mqtt_publish_jsonrpc(RvMQTTHandle *self, const char *topic, const char *method, json_t *data, json_t *id) {
    RV_PARAMETER_CHECK(self && self->mqtt_client && topic, INT);
    
    if(method) {
        // encode as JSON RPC request (or notification if request_id == 0)
        RvJsonRpcRequest jsonrpc_request;
        
        rv_jsonrpc_request_ctor(&jsonrpc_request, method, data, id);
        json_t *new_request = rv_jsonrpc_request_encode(&jsonrpc_request);
        if(!new_request) {
            return -1;
        }
        
        // only notifications are retained (for now)
        int retained = id ? 0 : 1;
        
        char *message = json_dumps(new_request, 0);
        rv_mqtt_publish(self, topic, retained, message, strlen(message));
        
        // cleanup
        free(message);
        json_decref(new_request);
        rv_jsonrpc_request_dtor(&jsonrpc_request);
        
        return 0;
    }
    
    //encode as JSON RPC response
    RvJsonRpcResponse jsonrpc_response;
    
    rv_jsonrpc_response_ctor(&jsonrpc_response, data, id);
    json_t *new_response = rv_jsonrpc_response_encode(&jsonrpc_response);
    if(!new_response) {
        return -1;
    }
    
    char *message = json_dumps(new_response, 0);
    rv_mqtt_publish(self, topic, 0, message, strlen(message));
    
    // cleanup
    free(message);
    json_decref(new_response);
    rv_jsonrpc_response_dtor(&jsonrpc_response);
        
    return 0;
}

json_t *rv_mqtt_request_id(RvMQTTHandle *self, const char *topic_name) {
    json_t *request_id = json_object();

    char process_name[RV_NAME_MAX];
        
    rv_get_process_name(process_name, RV_NAME_MAX);
    
    json_object_set_new(request_id, "request_name", json_string(process_name));
    
    for(unsigned idx = 0; (idx < MQTT_MAX_MESSAGE_HANDLER); ++idx) {
        struct mqtt_topic_t *topic = &self->topic[idx];
        
        if(!strncmp(topic->name, topic_name, RV_NAME_MAX)) {
            ++topic->request_id;
            json_object_set_new(request_id, "request_id", json_integer(topic->request_id));
            
            return request_id;
        }
    }

    // could not find topic in subscribe list, sending as notification instead
    json_decref(request_id);
    return NULL;
}
