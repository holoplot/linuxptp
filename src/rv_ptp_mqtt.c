/**
 * @file rv_ptp_mqtt.c
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
#include <uv.h>

#include "rv_init.h"

#include "rv_ptp_ifc.h"

#include "print.h"

#include <errno.h>
#include <string.h>

int publish_ptp_offset(RvMQTTHandle *mqtt_handle, struct rv_ptpclock_t *ptp_clock) {
    char mqtt_topic[RV_NAME_MAX] = {0};
    bool is_slave = false;
    
    for(unsigned idx = 0; idx < ptp_clock->port_count; ++idx) {
        if(ptp_clock->port[idx].state == eRvPtpSlave) {
            is_slave = true;
            break;
        }
    }
    
    if(is_slave) {
        json_t *offset_data = json_object();
        json_object_set_new(offset_data, "offset_nsec", json_integer(ptp_clock->offset)); 
        
        snprintf(mqtt_topic, RV_NAME_MAX, "ptp/clock/offset");
        rv_mqtt_publish_jsonrpc(mqtt_handle, mqtt_topic, "offsetToMaster", offset_data, NULL);
        json_decref(offset_data);

        json_t *health = json_object();
        json_object_set_new(health, "value", json_integer(ptp_clock->offset)); 
        json_object_set_new(health, "unit", json_string("ns"));
        rv_mqtt_publish_health(mqtt_handle, "nodesys/health/ptp/clock/offset", health);
        json_decref(health);
    }
    
    return 0;
}

int publish_ptp_clock_state(RvMQTTHandle *mqtt_handle, struct rv_ptpclock_t *ptp_clock) {
    char mqtt_topic[RV_NAME_MAX] = {0};

    json_t *clockstate_data = json_object();
    json_object_set_new(clockstate_data, "clock_id", json_string(ptp_clock->clk_id));
    json_object_set_new(clockstate_data, "clock_accuracy", json_integer(ptp_clock->clk_accuracy));
    json_object_set_new(clockstate_data, "clock_class", json_integer(ptp_clock->clk_class));
    json_object_set_new(clockstate_data, "clock_traceable", json_boolean(ptp_clock->traceable));
    
    json_t *port_array = json_array();
    for(unsigned idx = 0; idx < ptp_clock->port_count; ++idx) {
        json_array_append_new(port_array, json_string(ptp_clock->port[idx].ifc_name));
    }
    json_object_set_new(clockstate_data, "port_array", port_array);
    
    snprintf(mqtt_topic, RV_NAME_MAX, "ptp/clock/status");
    rv_mqtt_publish_jsonrpc(mqtt_handle, mqtt_topic, "clockStatus", clockstate_data, NULL);
    json_decref(clockstate_data);
    
    return 0;
}

int publish_ptp_path_delay(RvMQTTHandle *mqtt_handle, struct rv_ptpport_t *ptp_port) {
    char mqtt_topic[RV_NAME_MAX] = {0};

    // only of PASSIVE or slave_only
    if((ptp_port->state == eRvPtpPassive) || (ptp_port->state == eRvPtpSlave)) {
        json_t *pathdelay_data = json_object();
        json_object_set_new(pathdelay_data, "port_name", json_string(ptp_port->ifc_name));
        json_object_set_new(pathdelay_data, "path_delay_nsec", json_integer(ptp_port->path_delay));
        
        snprintf(mqtt_topic, RV_NAME_MAX, "ptp/port/%s/path_delay", ptp_port->ifc_name);
        rv_mqtt_publish_jsonrpc(mqtt_handle, mqtt_topic, "pathDelay", pathdelay_data, NULL);
        json_decref(pathdelay_data);
    }
    
    return 0;
}

int publish_ptp_port_state(RvMQTTHandle *mqtt_handle, struct rv_ptpport_t *ptp_port, struct rv_ptpport_t *ptp_port_last) {
    char mqtt_topic[RV_NAME_MAX] = {0};
    json_t *health;

    json_t *portstate_data = json_object();
    json_object_set_new(portstate_data, "port_name", json_string(ptp_port->ifc_name));
    json_object_set_new(portstate_data, "port_state", json_integer(ptp_port->state));
    
    json_object_set_new(portstate_data, "master_id", json_string(ptp_port->master_id));
    json_object_set_new(portstate_data, "master_ip", json_string(ptp_port->master_ip));

    json_object_set_new(portstate_data, "grandmaster_id", json_string(ptp_port->grandmaster_id));

    snprintf(mqtt_topic, RV_NAME_MAX, "ptp/port/%s/status", ptp_port->ifc_name);
    rv_mqtt_publish_jsonrpc(mqtt_handle, mqtt_topic, "portStatus", portstate_data, NULL);
    json_decref(portstate_data);

    health = json_object();
    json_object_set_new(health, "value", json_integer(ptp_port->state)); 
    snprintf(mqtt_topic, RV_NAME_MAX, "nodesys/health/ptp/%s/state", ptp_port->ifc_name);
    rv_mqtt_publish_health(mqtt_handle, mqtt_topic, health);
    json_decref(health);

    health = json_object();
    if(!strncmp(ptp_port->master_id, ptp_port_last->master_id, RV_PTP_CLOCK_ID_STRING_SIZE)) {
        json_object_set_new(health, "value", json_integer(0)); 
    } else {
        json_object_set_new(health, "value", json_integer(1)); 
    }
    snprintf(mqtt_topic, RV_NAME_MAX, "nodesys/health/ptp/%s/master", ptp_port->ifc_name);
    rv_mqtt_publish_health(mqtt_handle, mqtt_topic, health);
    json_decref(health);
    
    health = json_object();
    if(!strncmp(ptp_port->grandmaster_id, ptp_port_last->grandmaster_id, RV_PTP_CLOCK_ID_STRING_SIZE)) {
        json_object_set_new(health, "value", json_integer(0)); 
    } else {
        json_object_set_new(health, "value", json_integer(1)); 
    }
    snprintf(mqtt_topic, RV_NAME_MAX, "nodesys/health/ptp/%s/grandmaster", ptp_port->ifc_name);
    rv_mqtt_publish_health(mqtt_handle, mqtt_topic, health);
    json_decref(health);

    memcpy(ptp_port_last, ptp_port, sizeof(struct rv_ptpport_t));

    return 0;
}

static void regular_publisher(uv_timer_t *timer) {
    LinuxPtpClock *linuxptp = (LinuxPtpClock*)timer->data;
    
    publish_ptp_offset(&linuxptp->mqtt_handle, &linuxptp->clock_state);
    
    // run through ports and publish path path_delay
    for(unsigned idx = 0; idx < linuxptp->clock_state.port_count; ++idx) {
        publish_ptp_path_delay(&linuxptp->mqtt_handle, &linuxptp->clock_state.port[idx]);
    }
}

static void timer_thread_fn(void *arg) {
    LinuxPtpClock *linuxptp = (LinuxPtpClock*)arg;

    uv_loop_t timer_thread_loop;
    
    uv_loop_init(&timer_thread_loop);
    
    uv_timer_init(&timer_thread_loop, &linuxptp->publish_timer);
    uv_timer_start(&linuxptp->publish_timer, regular_publisher, 1000, 1000);
    linuxptp->publish_timer.data = linuxptp;
    
    uv_run(&timer_thread_loop, UV_RUN_DEFAULT);

    uv_loop_close(&timer_thread_loop);
}

void rv_ptp_mqtt_exit(LinuxPtpClock *linuxptp) {
    // stop thread for frequent offset and path_delay messages here
    uv_timer_stop(&linuxptp->publish_timer);
    uv_close((uv_handle_t*)&linuxptp->publish_timer, 0);
    
    uv_thread_join(&linuxptp->timer_thread);
    
    memset(&linuxptp->clock_state, 0, sizeof(struct rv_ptpclock_t));

    // dtor() disconnects implicit if necessary
    rv_mqtt_dtor(&linuxptp->mqtt_handle);
}

int rv_ptp_mqtt_init(LinuxPtpClock *linuxptp, char *client_id) {
    memset(&linuxptp->clock_state, 0, sizeof(struct rv_ptpclock_t));
    
    if(!rv_mqtt_ctor(&linuxptp->mqtt_handle, client_id, "1.8.0", RV_MQTT_DEFAULT_BROKER_ADDR, RV_MQTT_DEFAULT_BROKER_PORT)) {
        return -1;
    }
    
    // start thread for frequent offset and path_delay messages here (1 per second)
    if(uv_thread_create(&linuxptp->timer_thread, timer_thread_fn, linuxptp) < 0) {
        // thread start failed
        rv_ptp_mqtt_exit(linuxptp);
        
        return -1;
    }
        
    return 0;
}
