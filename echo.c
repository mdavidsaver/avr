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

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "mbus.h"

static uint16_t data[4];

void mbus_read_holding(uint16_t addr, uint8_t count, uint16_t * restrict result)
{
    if(addr>=4)
        mbus_exception(2);
    if(count>4 || addr+count>4)
        mbus_exception(3);

    memcpy(result, data+addr, 2*count);
}

void mbus_write_holding(uint16_t addr, uint16_t value)
{
    if(addr>=4)
        mbus_exception(2);

    data[addr] = value;
}
