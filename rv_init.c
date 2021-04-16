/**
 * @file rv_init.c
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

#include "rv_init.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <sys/time.h>

int rv_get_process_name(char *buffer, size_t buffer_len) {
    char *name, process_title[RV_NAME_MAX];
    
    if(!buffer || !buffer_len) {
        return -1;
    }
    
    memset(process_title, 0, RV_NAME_MAX);
    uv_get_process_title(process_title, RV_NAME_MAX);
    
    // find 'basename' of process by removing leading slashes
    name = strrchr(process_title, '/');
    if(!name) {
        name = process_title;
    } else {
        name += sizeof(char);
    }
    
    memset(buffer, 0, buffer_len);
    strncpy(buffer, name, buffer_len - 1);
    
    return rv_getpid();
}

RvTimeUtc rv_gettimeofday(void) {
    RvTimeUtc result;
    struct timeval now;
    
    gettimeofday(&now, NULL);
    result.seconds = now.tv_sec;
    result.nanoseconds = now.tv_usec * RV_USEC_NSEC;
    
    return result;
}
