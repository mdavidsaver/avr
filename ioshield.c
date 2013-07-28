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

#include <util/delay.h>
#include <avr/io.h>

#include "mbus.h"

/* BNC I/O Shield rev 1 pinout
 * with arduino uno
 *
 * Out 1 - Arduino Dig 11 (PB3 - OC2A)
 * Out 2 - Arduino Dig 10 (PB2 - OC1B)
 * Out 3 - Arduino Dig 06 (PD6 - OC0A)
 * Out 4 - Arduino Dig 05 (PD5 - OC0B)
 *
 * In 1 - Arduino Dig 09 (PB1)
 * In 2 - Arduino Dig 08 (PB0)
 * In 3 - Arduino Dig 07 (PD7)
 * In 4 - Arduino Dig 04 (PD4)
 *
 * Rx enable - Arduino Dig 3 (PD3)
 * Tx enable - Arduino Dig 2 (PD2)
 */

/** BNC I/O Shield register map.
 *
 * 0x0000 - Control/Status register
 *
 *  Reading:
 *   0x00FF - Reset source flags (AVR8 MCUSR at boot)
 *   0x0100 - Output enable status.  Outputs are tri-state when clear.
 *
 *  Writing:
 *   0x0001 - Reset.  Cause the Board to reset.  Not reply will be sent.
 *   0x0100 - Enable outputs.  Outputs are tri-state when clear.
 *   0x0200 - Save to eeprom.
 *
 * 0x0001 - input/output register
 *
 *  Reading:
 *   0x000F - Last output command
 *   0x0F00 - Current output pin state
 *   0xF000 - Current input pin state
 *
 *  Writing:
 *   0x000F - Set output command when in immediate mode.
 *
 * 0x0002,0x0004,0x0006,0x0008 - Output config registers
 *
 *   Each of the 4 output pins is configured here
 *
 *  Writing:
 *  0x0003 - Output Mode (0b00 - immediate, 0b01 - Freq, 0b10 - PWM, 0b11 - Invalid)
 *  0x0F00 - Output clock Prescaler (2 to the power of N). (outputs 1 and 2 only)
 *
 * 0x0003,0x0005,0x0007,0x0009 - Output parameter register
 *
 *  0xFFFF - For Output 2
 *  0xFF00 - For Outputs 1, 3, and 4
 *
 * When in Frequency mode, this is a divider.  When in PWM mode this is a phase.
 */

uint16_t reg[10];

void user_init(void)
{
    reg[0] = MCUSR; // save reset source

    // Enable Tx/Rx control drivers
    PORTD = _BV(PD2)|_BV(PD3); // enable internal pull-ups
    DDRD = _BV(DDD2)|_BV(DDD3); // Set to outputs (level high)

    // Turn on Rx buffers
    PORTD &= ~_BV(PD3);

    // Setup output pins
    DDRB = _BV(DDB2)|_BV(DDB3);
    DDRD |= _BV(DDD5)|_BV(DDD6);

    ICR1 = 0xffff;

    reg[6] = reg[8] = 10<<8; // outputs 3,4 only allow divider /1024
}

void user_tick(void)
{
    uint8_t * const breg=(uint8_t*)reg;
    uint8_t IB = PINB, ID = PIND;
    uint8_t state = 0;

    // High byte is:
    // PIND4, PIND7, PINB0, PINB1, PIND5, PIND6, PINB2, PINB3
    state |= (ID&PIND4) ? 0x80 : 0;
    state |= (ID&PIND7) ? 0x40 : 0;
    state |= (IB&PINB0) ? 0x20 : 0;
    state |= (IB&PINB1) ? 0x10 : 0;
    state |= (ID&PIND5) ? 0x08 : 0;
    state |= (ID&PIND6) ? 0x04 : 0;
    state |= (IB&PINB2) ? 0x02 : 0;
    state |= (IB&PINB3) ? 0x01 : 0;

    breg[2] = state;
}

void mbus_read_holding(uint16_t addr, uint8_t count, uint16_t * restrict result)
{
    if(addr>=sizeof(reg))
        mbus_exception(2);
    if(count>sizeof(reg) || addr+count>sizeof(reg))
        mbus_exception(3);

    memcpy(result, reg+addr, 2*count);
}

// map power of 2 to clock divider selection.  Round to lower frequency
static uint8_t divtbl[] = {0, 1, 2, 2, 2, 3, 3, 4, 5, 5};
// map clock divider to power of 2
static uint8_t rdivtbl[] = {0, 1, 4, 6, 8, 10};

void mbus_write_holding(uint16_t faddr, uint16_t value)
{
    uint8_t addr=faddr;
    if(faddr>=sizeof(reg))
        mbus_exception(2);

    if(addr==0) { // CSR
        value &= 0x0101; //mask out unused bits

        if(value&1) {
            // reset
            WDTCSR = WDE; // enable watchdog with shortest timeout
            while(1) {}; // wait...
        }

        if(value&0x0100) {
            PORTD &= ~_BV(PD2); // enable Tx buffers
        } else {
            PORTD |= _BV(PD2); // tri-state Tx buffers
        }

    } else if(addr==1) { // set outputs
        uint8_t SB=PORTB, SD=PORTD;
        value &= 0x000F;
        reg[1] &= ~0x000F;
        value |= reg[1];

        SB &= ~(_BV(PB2)|_BV(PB3));
        SD &= ~(_BV(PD5)|_BV(PD6));

#define ACT(MASK, SET, BIT) \
    if(value&MASK) {SET |= _BV(BIT);}

        ACT(0x1, SB, PB3);
        ACT(0x2, SB, PB2);
        ACT(0x4, SD, PD6);
        ACT(0x8, SD, PD5);
#undef ACT

        PORTB=SB;
        PORTD=SD;

    } else if(addr==2) { // config out #1 (pin OC2A)
        uint8_t div=value>>8;
        uint8_t mode=value&0x3;

        // disable counter in preparation to change mode
        TCCR2A = 0;
        TCCR2B = 0;

        if(mode!=0) {
            if(div>sizeof(divtbl))
                div=0;
            div = divtbl[div];

            if(mode==1) { // freq (CTC mode)
                TCCR2A = WGM21|COM2A0;
            } else { // PWM (fast)
                TCCR2A = WGM21|WGM20|COM2A0;
            }
            TCCR2B = div;
        }

        value = rdivtbl[div]<<8 | mode;

    } else if(addr==3) {
        OCR2A = value;

    } else if(addr==4) { // config out #2 (pin OC1B)
        uint8_t div=value>>8;
        uint8_t mode=value&0x3;

        // disable counter in preparation to change mode
        TCCR1A = 0;
        TCCR1B = 0;

        if(mode!=0) {
            if(div>sizeof(divtbl))
                div=0;
            div = divtbl[div];

            if(mode==1) { // freq (CTC mode)
                div |= WGM12;
            } else { // PWM (phase + freq correct)
                div |= WGM13;
            }
            TCCR1B = div;
            TCCR1A = COM1B0;
        }

        value = rdivtbl[div]<<8 | mode;

    } else if(addr==5) {
        OCR1A = value;

    } else
        return;

    reg[addr] = value;
}

