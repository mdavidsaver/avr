/** Modbus RTU server for AVR8
 * Copyright (C) 2013 Michael Davidsaver <mdavidsaver@gmail.com>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "mbus.h"

#include <string.h>

uint16_t bswap16(uint16_t in)
{
    return (in&0x00ff)<<8 | (in&0xff00)>>8;
}

#ifndef __AVR__
#  include <arpa/inet.h>
uint16_t _crc16_update(uint16_t, uint8_t);
#else
#  include <util/crc16.h>
#  define htons(X) ((uint16_t)bswap16(X))
#  define ntohs(X) ((uint16_t)bswap16(X))
#endif

#if MAX_BUFFER>=256
#  error MAX_BUFFER must be <256
#endif

struct mbus_single_reg {
    uint16_t addr;
    uint16_t data;
    uint16_t crc;
};

struct mbus_multi_reply {
    uint8_t count;
    uint16_t data[1+MAX_BUFFER/2]; // extra entry for crc
} __attribute__((packed));

struct mbus_except {
    uint8_t code;
    uint8_t lrc;
};

struct mbus_message {
    uint8_t node;
    uint8_t function;
    union {
        struct mbus_single_reg mb_s;
        struct mbus_multi_reply mb_m;
        struct mbus_except mb_e;
    };
};

static union {
    struct mbus_message b_p;
    uint8_t b_b[sizeof(struct mbus_message)];
} buf;

static uint8_t buf_cnt = 8, buf_pos;

static uint8_t err_cnt;

#define STATE_REPLY 1
static uint8_t mb_state = 0;

volatile uint8_t mbus_in_byte;
volatile uint8_t mbus_out_byte;
volatile uint8_t mbus_status;

void mbus_reset(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        mbus_rx_clear();
        mbus_status = 0;
        mbus_in_byte = mbus_out_byte = 0;
        err_cnt = 0;
        memset(buf.b_b, 0, sizeof(buf));
    }
}


void mbus_rx_clear(void)
{
    buf_cnt = 8;
    buf_pos = 0;
}

static
uint16_t calculate_crc(const uint8_t* d, uint8_t c)
{
    uint16_t r=0xffff;

    while(c--)
        r=_crc16_update(r, *d++);
    return r;
}

void mbus_exception(uint8_t code)
{
    uint8_t sum= buf.b_p.node;

    buf.b_p.function |= 0x80;
    sum += buf.b_p.function;

    buf.b_p.mb_e.code = code;
    sum += code;

    buf.b_p.mb_e.lrc = (~sum)+1;

    buf_cnt = 4;
    mb_state |= STATE_REPLY;

    err_cnt++;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        mbus_status |= MBUS_RX_ERROR;
    }
}

static void mbus_dispatch(void)
{
    uint16_t crc=calculate_crc(buf.b_b, buf_cnt-2);

    if(ntohs(buf.b_p.mb_s.crc)!=crc) {
        mbus_exception(4);

    } else if(buf.b_p.function==3) {
        uint16_t count = ntohs(buf.b_p.mb_s.data);
        uint8_t cnt = count; // valid counts will always be <256
        // read
        if(count>MAX_BUFFER/2)
            mbus_exception(3);
        else
            mbus_read_holding(ntohs(buf.b_p.mb_s.addr),
                              cnt,
                              buf.b_p.mb_m.data);

        {
            size_t i;
            for(i=0; i<cnt; i++)
                buf.b_p.mb_m.data[i] = htons(buf.b_p.mb_m.data[i]);
        }

        if(!(mb_state&STATE_REPLY)) {
            // no exception, send reply
            // node, function are the same.
            buf.b_p.mb_m.count = 2*cnt;
            buf.b_p.mb_m.data[cnt] = htons(calculate_crc(buf.b_b, 3+2*cnt));
            buf_cnt = 5+2*cnt;
        }
    } else { // function==6
        // write
        mbus_write_holding(ntohs(buf.b_p.mb_s.addr),
                           ntohs(buf.b_p.mb_s.data));

        // reply is to echo back request, or exception signaled by user
    }

    mb_state |= STATE_REPLY;
}

static void mbus_recieve(void)
{
    // recving request
    uint8_t next, sts;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        sts = mbus_status;
        mbus_status = sts & ~MBUS_RX_READY;
        next = mbus_in_byte;
    }

    if(!(sts&MBUS_RX_READY)) {
        // RX timeout, reset buffer
        mbus_rx_clear();
        return;
    }

    uint8_t bpos = buf_pos;

    // store byte
    buf.b_b[bpos++] = next;

    if(bpos==buf_cnt) {
        // complete message received
        mbus_dispatch();

    } else if(bpos==2) {
        // early check of function code
        if(buf.b_p.function!=3 && buf.b_p.function!=6) {
            mbus_exception(1); // illegal function
        }
    }

    if(mb_state&STATE_REPLY) {
        buf_pos = 1;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            mbus_status |= MBUS_TX_READY;
            mbus_out_byte = buf.b_b[0];
        }
    }else
        buf_pos = bpos;
}

static void mbus_transmit(void)
{
    uint8_t sts, bpos = buf_pos, next=buf.b_b[bpos];

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        sts = mbus_status;
        if(!(sts&MBUS_TX_READY)) {
            mbus_status = sts|MBUS_TX_READY;
            mbus_out_byte = next;
        }
    }

    if(!(sts&MBUS_TX_READY)) {
        bpos++;
        buf_pos=bpos;
    }

    if(buf_pos==buf_cnt) {
        // done with send. setup for next recv
        mb_state &= ~STATE_REPLY;
        buf_pos = 0;
        buf_cnt = 8;
    }
}

void mbus_process(void)
{
    if(mb_state&STATE_REPLY)
        mbus_transmit();
    else
        mbus_recieve();
}
