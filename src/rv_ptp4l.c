/**
 * @file rv_ptp4l.c
 * @note Copyright (C) 2013, ALC NetworX GmbH
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

#include "rv_init.h"
#include "uv.h"

#include <stdlib.h>
#include <string.h>

extern int rv_get_clock_status(struct rv_ptpclock_t *rv_clock, struct clock *clock);

// On each published change, we update rv_clock_old as well. This way
// we won't miss a slowly drifting offset/path delay.
static void check_for_state_changes(LinuxPtpClock *linuxptp) {
    struct rv_ptpclock_t rv_clock_old = {0};
    bool clock_state_change = false;

    // save old status for later comparisons
    memcpy(&rv_clock_old, &linuxptp->clock_state, sizeof(struct rv_ptpclock_t));

    // get new status from clock_handle
    rv_get_clock_status(&linuxptp->clock_state, linuxptp->clock_handle);

    // check for clock state changes
    clock_state_change =   (rv_clock_old.port_count != linuxptp->clock_state.port_count)
                        || (rv_clock_old.domain     != linuxptp->clock_state.domain)
                        || (rv_clock_old.slave_only != linuxptp->clock_state.slave_only)
                        || (rv_clock_old.priority1  != linuxptp->clock_state.priority1)
                        || (rv_clock_old.priority2  != linuxptp->clock_state.priority2)
                        || (rv_clock_old.event_priority   != linuxptp->clock_state.event_priority)
                        || (rv_clock_old.general_priority != linuxptp->clock_state.general_priority);
    
    if(clock_state_change) {
        publish_ptp_clock_state(&linuxptp->mqtt_handle, &linuxptp->clock_state);
    }
    
    // check for port status changes
    for(unsigned idx = 0; idx < linuxptp->clock_state.port_count; ++idx) {
        bool port_state_change = false;
        struct rv_ptpport_t *old_port = &rv_clock_old.port[idx];
        struct rv_ptpport_t *new_port = &linuxptp->clock_state.port[idx];
        
        port_state_change =    (old_port->state != new_port->state)
                            || (old_port->ttl   != new_port->ttl)
                            || (old_port->delay_mechanism       != new_port->delay_mechanism)
                            || (old_port->log_announce_interval != new_port->log_announce_interval)
                            || (old_port->log_sync_interval     != new_port->log_sync_interval);
                            
        if(strncmp(old_port->grandmaster_id, new_port->grandmaster_id, RV_PTP_CLOCK_ID_STRING_SIZE)) {
            port_state_change = true;
        }
        
        if(strncmp(old_port->master_id, new_port->master_id, RV_PTP_CLOCK_ID_STRING_SIZE)) {
            port_state_change = true;
        }        

        if(port_state_change) {
            publish_ptp_port_state(&linuxptp->mqtt_handle, &linuxptp->clock_state.port[idx], &linuxptp->clock_state.port_last[idx]);
        }

        // publish path delay changes in 1us steps
        if(old_port->path_delay/1000 != new_port->path_delay/1000) {
            publish_ptp_path_delay(&linuxptp->mqtt_handle, &linuxptp->clock_state.port[idx]);
        }
    }
    
    // at last check for offset changes > 10us
    // this must be done at last, to be able to check port state changes before overwriting them
    if(linuxptp->clock_state.offset / 10000 != rv_clock_old.offset/10000) {
        // publish new status regardless of other changes
        publish_ptp_offset(&linuxptp->mqtt_handle, &linuxptp->clock_state);
    }
    
    return;
}

int main(int argc, char *argv[]) {
    LinuxPtpClock linuxptp;
    char process_name[RV_NAME_MAX];

    memset(&linuxptp, 0, sizeof(LinuxPtpClock));
    memset(process_name, 0, RV_NAME_MAX);
    
    uv_setup_args(argc, argv);
    rv_get_process_name(process_name, RV_NAME_MAX);
    
    linuxptp.clock_handle = ptp4l_init(argc, argv, false);
    if(!linuxptp.clock_handle) {
        return EXIT_FAILURE;
    }

    if(rv_ptp_mqtt_init(&linuxptp, process_name) < 0) {
        ptp4l_exit(linuxptp.clock_handle);

        return EXIT_FAILURE;
    }

    while(is_running()) {
        if (clock_poll(linuxptp.clock_handle)) {
            break;
        }
        
        check_for_state_changes(&linuxptp);
    }

    rv_ptp_mqtt_exit(&linuxptp);

    ptp4l_exit(linuxptp.clock_handle);

    return EXIT_SUCCESS;
}
