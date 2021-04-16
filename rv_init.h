/**
 * @file rv_init.h
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

#include "uv.h"

#include "rv_ptp_ifc.h"
#include "rv_mqtt.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#define RV_NAME_MAX 127  // _POSIX_MAX_PATH is not defined in Windows 
#define RV_CNAME_LEN 16

struct clock;

typedef struct linuxptp_clock_t {
    struct clock *clock_handle;
    
    RvPtpClockState clock_state;
    
    RvMQTTHandle mqtt_handle;

    uv_thread_t timer_thread;
    uv_timer_t publish_timer;
} LinuxPtpClock;

extern int is_running();

extern int rv_ptp_mqtt_init(LinuxPtpClock *linuxptp, char *client_id);
extern void rv_ptp_mqtt_exit(LinuxPtpClock *linuxptp);

extern int publish_ptp_clock_state(RvMQTTHandle *mqtt_handle, struct rv_ptpclock_t *ptp_clock);
extern int publish_ptp_offset(RvMQTTHandle *mqtt_handle, struct rv_ptpclock_t *ptp_clock);

extern int publish_ptp_port_state(RvMQTTHandle *mqtt_handle, struct rv_ptpport_t *ptp_port, struct rv_ptpport_t *ptp_port_last);
extern int publish_ptp_path_delay(RvMQTTHandle *mqtt_handle, struct rv_ptpport_t *ptp_port);

extern void* ptp4l_init(int argc, char *argv[], int force_slave_only);
extern void ptp4l_exit(struct clock* clock_handle);
extern int clock_poll(struct clock* clock_handle);

extern int hwstamp_ctl_main(int argc, char *argv[]);
extern int phc2sys_main(int argc, char *argv[]);
extern int pmc_main(int argc, char *argv[]);

#define RV_USEC_NSEC 1000

//!< UTC in nanoseconds since 1970/01/01 (used for low precision non-TAI times)
typedef struct rv_time_utc_t {
    uint32_t seconds;
    uint32_t nanoseconds;
} RvTimeUtc;

/**
 * Portable gettimeofday()
 *
 * The assumed timescale is based on UTC; more precisely the time is in seconds since the
 * Unix Epoch (for a more detailed treatment of the relationship between the Unix Epoch and
 * UTC see the Wikipedia entry on Unix Time (or Posix time). The time of day may or may not
 * be synchronous to an external reference, and it may or may not be synchronous to a PTP
 * clock, should one be available in the system.
 *
 * The time resolution is system dependent, and it is quite possible that the time values
 * jump forward or even backward when someone sets the system time manually, or on a leap
 * second, or when a daemon synchronizes the clock to a server.
 */
extern RvTimeUtc rv_gettimeofday(void);

extern int rv_get_process_name(char *buffer, size_t buffer_len);

static __inline int rv_getpid(void) {
    return getpid();
}
