/**
 * @file rv_random.c
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
#include "rv_random.h"
#include "rv_init.h"

#include "md5.h"

#include <time.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/syscall.h>

extern char* getlogin();

#ifndef MD5_DIGEST_LEN
#define MD5_DIGEST_LEN 16
#else
#error MD5_DIGEST_LEN already defined elsewhere
#endif

static const size_t md5_digest_len = MD5_DIGEST_LEN;

static int rv_gettid(void) {
    return syscall(SYS_gettid);
}

/*
 * Return random unsigned 32-bit quantity. Use 'type' argument if you
 * need to generate several different values in close succession.
 * 
 * According to RFC 3550, page 73f.
 * 
 */
uint32_t rv_random32(int type) {
    md5_state_t md5;
    md5_byte_t md5_digest[MD5_DIGEST_LEN];

    unsigned long r = 0;
    
    struct {
        int     type;
        time_t  sec;
        clock_t cpu;

        uint32_t pid;
        uint32_t tid;
        size_t   namesize;
        char     name[RV_NAME_MAX];
    } s;

    const char *login_name = getlogin();
    
    s.type = type;
    time(&s.sec);
    s.cpu  = clock();

    s.pid  = rv_getpid();
    s.tid  = rv_gettid();

    s.namesize = RV_NAME_MAX;
    memset(s.name, 0, s.namesize);
    if(login_name) {
        strncpy(s.name, login_name, RV_NAME_MAX - 1);
    } else {
        strncpy(s.name, "system", RV_NAME_MAX - 1);
    }
    
    memset(&md5, 0, sizeof(md5_state_t));
    memset(md5_digest, 0, md5_digest_len);
    
    md5_init(&md5);
    md5_append(&md5, (md5_byte_t*)&s, sizeof(s));
    md5_finish(&md5, md5_digest);
    
    for(size_t idx = 0; idx < 3; ++idx) {
        r ^= md5.abcd[idx]; // from digest buffer
    }
    
    return r;
}

static char b64_encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                    'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                    '4', '5', '6', '7', '8', '9', '+', '/'};
static char local_cname[RV_CNAME_LEN + 1] = {0};

static char *b64_encode(const uint8_t *data, size_t input_length) {
    if(input_length > 12) {
        input_length = 12; // we use 96bit from src data only (according to RFC 7022)
    }
  
    for (size_t src_idx = 0, tgt_idx = 0; src_idx < input_length;) {
        uint32_t octet_a = src_idx < input_length ? (uint8_t)data[src_idx++] : 0;
        uint32_t octet_b = src_idx < input_length ? (uint8_t)data[src_idx++] : 0;
        uint32_t octet_c = src_idx < input_length ? (uint8_t)data[src_idx++] : 0;
 
        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
 
        local_cname[tgt_idx++] = b64_encoding_table[(triple >> 3 * 6) & 0x3F];
        local_cname[tgt_idx++] = b64_encoding_table[(triple >> 2 * 6) & 0x3F];
        local_cname[tgt_idx++] = b64_encoding_table[(triple >> 1 * 6) & 0x3F];
        local_cname[tgt_idx++] = b64_encoding_table[(triple >> 0 * 6) & 0x3F];
    }
 
    return local_cname;
}

char *rv_random_hash(char *cname, size_t cname_len) {
    uint8_t tmp_cname[12] = {0};
    
    if(!cname || (cname_len < RV_CNAME_LEN)) {
        errno = EINVAL;
        
        return NULL;
    }
    memset(cname, 0, cname_len);
    
    if(!memcmp(local_cname, tmp_cname, 12)) {
        // cname not initialized, so we have to do here
        for(size_t idx = 0; idx < 12; idx += sizeof(uint32_t)) {
            uint32_t prng = rv_random32((idx * 4) - 1);
            memcpy(tmp_cname + idx, &prng, sizeof(uint32_t));
        }
        
        b64_encode(tmp_cname, 12);
    }
    
    // now return local_cname
    memcpy(cname, local_cname, RV_CNAME_LEN);
    
    return cname;
}

