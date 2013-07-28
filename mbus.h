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
#ifndef MBUS_H
#define MBUS_H

#if !defined(__STDC_VERSION__) || __STDC_VERSION__< 199901L
#  define restrict
#endif

#include <inttypes.h>

#ifndef __AVR__
#  define ATOMIC_BLOCK(X)
#  define ATOMIC_RESTORESTATE
#else
#  include <util/atomic.h>
#endif

#ifndef MAX_BUFFER
#  define MAX_BUFFER 20
#endif

extern volatile uint8_t mbus_in_byte;
extern volatile uint8_t mbus_out_byte;

//! Set by the mbus_process when mbus_out_byte has
//! data to be sent.  User code should clear after
//! reading mbus_out_byte
#define MBUS_TX_READY 0x01
//! Set by user code after writing mbus_in_byte with
//! newly received data.  Cleared by mbus_process().
#define MBUS_RX_READY 0x02
//! Set by mbus_process() when a protocol error is
//! detected.  User program should ignore received
//! data for ~1 second.
#define MBUS_RX_ERROR 0x04
extern volatile uint8_t mbus_status;

/** @brief Reset modbus server internal state
 * Return to startup state
 */
void mbus_reset(void);

void mbus_rx_clear(void);

/** @brief Process modbus data
 * Call periodically complete processing
 * of received modbus requests.
 *
 * Call after clearing MBUS_TX_READY,
 * or setting MBUS_RX_READY.
 * Calling in other circumstates acts
 * as an RX timeout and clears the
 * incoming buffer.
 */
void mbus_process(void);

void mbus_exception(uint8_t code);

// User program must implement these functions

void mbus_read_holding(uint16_t addr, uint8_t count, uint16_t * restrict result);

void mbus_write_holding(uint16_t addr, uint16_t value);

#endif // MBUS_H
