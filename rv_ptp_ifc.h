/**
 * @file rv_ptp_ifc.h
 * @note Copyright (C) 2012, ALC NetworX GmbH
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

/**
 * \addtogroup rvutil
 * @{
 * \file rv_ptp_ifc.h
 * \brief PTP definitions and interface
 */

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

#define RV_IFC_NAME_LEN                   64
#define RV_PTP_MAX_PORTS                  8
#define RV_PTP_CLOCK_ID_STRING_SIZE       32 // 6 byte long hex numbers of the GM's MAC address plus FF FE, dashes in between

struct json_t;

typedef uint8_t RvPtpDomain;    //!< default domain is 0

//! PTP port state according to IEEE 1588-2008 Table 8 and Table 10
typedef enum rv_ptp_portstate_t {
    eRvPtpInvalid = 0,      //!< This state is not defined by IEEE 1588
    eRvPtpInitializing,     //!< The port is initializing its hw and sw structures
    eRvPtpFaulty,           //!< The port has detected a fault
    eRvPtpDisabled,         //!< The port is disabled for PTP
    eRvPtpListening,        //!< The port waits for an announce message from the master
    eRvPtpPreMaster,        //!< The port is in master state, but does not (yet) send announce or sync messages
    eRvPtpMaster,           //!< The port is in master state
    eRvPtpPassive,          //!< The port only handles delay request/response messages
    eRvPtpUncalibrated,     //!< The port is in slave state, but its clock servo has not yet stabilized
    eRvPtpSlave,            //!< The port is in slave state, with a locked clock servo
    eRvPtpGrandmaster,      //!< Non standard extension of PTP4L
    eRvPtpUnknown
} RvPtpPortstate;

extern char *portstate2str[eRvPtpUnknown];

typedef enum rv_ptp_delay_mechanism_t {
    eRvPtpDelayMechanismAuto = 0,
    eRvPtpDelayMechanismE2E,
    eRvPtpDelayMechanismP2P
} RvPtpDelayMechanism;

#pragma pack(push, 1)
struct rv_ptpport_t {
    char ifc_name[RV_IFC_NAME_LEN]; 
    
    uint8_t state;
    uint8_t delay_mechanism;
    int8_t  log_announce_interval;
    int8_t  log_sync_interval;

    uint8_t ttl;

    char  master_id[RV_PTP_CLOCK_ID_STRING_SIZE];
    char  master_ip[INET6_ADDRSTRLEN];

    char grandmaster_id[RV_PTP_CLOCK_ID_STRING_SIZE];

    uint32_t path_delay;

    uint8_t version_number;
    
    // flag that handles port state changes from unsynced->synced
    bool new_slave_or_master;
};

typedef struct rv_ptpclock_t {
    char clk_id[RV_PTP_CLOCK_ID_STRING_SIZE];

    bool    slave_only;

    uint8_t priority1;
    uint8_t priority2;

    uint8_t clk_class;
    uint8_t clk_accuracy;

    bool traceable;
    
    uint8_t domain;

    uint8_t offset_sign;
    int64_t offset;

    uint8_t event_priority;
    uint8_t general_priority;

    uint8_t port_count;
    struct rv_ptpport_t port[RV_PTP_MAX_PORTS];
    struct rv_ptpport_t port_last[RV_PTP_MAX_PORTS];
} RvPtpClockState;
#pragma pack(pop)

typedef struct rv_ptp_properties_t {
    // clock specific
    RvPtpDomain             domain;
    uint8_t                 prio1;
    uint8_t                 prio2;
    uint8_t                 slave_only;          // used as a bool
    uint8_t                 dscp_event;
    uint8_t                 dscp_general;    

    // port specific
    int8_t                  ttl;
    int8_t                  log_announce;
    int8_t                  log_sync;
    RvPtpDelayMechanism     delay_mechanism;
} RvPtpProperties;

/** @} */
