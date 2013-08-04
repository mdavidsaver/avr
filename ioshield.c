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
#include <avr/wdt.h>
#include <avr/eeprom.h>

#include "mbus.h"

#define NELEMENTS(X) (sizeof(X)/sizeof(*(X)))

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
 *
 * Rev 1 boards have port 1,2 and 3,4 swapped.
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

#define NREG 10

static uint16_t reg[NREG];

/* each block can hold a copy of registers and a checksum */
static uint16_t eereg[NREG+1] EEMEM;

/* sequence to restore registers from eeprom */
static uint8_t eereg_restore_seq[] = {3,5,2,4,1,0};

void user_init(void)
{
    uint16_t initreg[NREG+1], isum;

    reg[0] = MCUSR; // save reset source

    // Enable Tx/Rx control drivers
    PORTD = _BV(PD2)|_BV(PD3); // enable internal pull-ups
    DDRD = _BV(DDD2)|_BV(DDD3); // Set to outputs (level high)

    // Turn on Rx buffers
    PORTD &= ~_BV(PD3);

    // Setup output pins
    DDRB |= _BV(DDB2)|_BV(DDB3);
    DDRD |= _BV(DDD5)|_BV(DDD6);

    reg[6] = reg[8] = 10<<8; // outputs 3,4 only allow divider /1024


    eeprom_read_block(initreg, eereg, sizeof(initreg));
    isum = calculate_crc((uint8_t*)initreg, sizeof(reg));
    if(isum==initreg[NREG]) {
        uint8_t i;

        for(i=1; i<NELEMENTS(eereg_restore_seq); i++) {
            uint8_t reg = eereg_restore_seq[i];
            mbus_write_holding(reg, initreg[reg]);
        }
    }
}

struct pinmap {
    uint8_t iomask;
    uint8_t valmask;
};
static struct pinmap pinb[] = {
    {_BV(PINB0), 0x20},
    {_BV(PINB1), 0x10},
    {_BV(PINB2), 0x02},
    {_BV(PINB3), 0x01},
};
static struct pinmap pind[] = {
    {_BV(PIND4), 0x80},
    {_BV(PIND7), 0x40},
    {_BV(PIND5), 0x08},
    {_BV(PIND6), 0x04},
};

void user_tick(void)
{
    uint8_t i;
    uint8_t * const breg=(uint8_t*)reg;
    uint8_t IB = PINB, ID = PIND;
    uint8_t state = 0;

    // High byte is:
    // PIND4, PIND7, PINB0, PINB1, PIND5, PIND6, PINB2, PINB3
    for(i=0; i<NELEMENTS(pinb); i++)
        state |= (IB&pinb[i].iomask) ? pinb[i].valmask : 0;
    for(i=0; i<NELEMENTS(pind); i++)
        state |= (ID&pind[i].iomask) ? pind[i].valmask : 0;

    breg[3] = state;
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

static void mbus_write_csr(uint16_t faddr, uint16_t value)
{
    value &= 0x0301; //mask out unused bits

    if(value&1) {
        // reset
        cli();
        wdt_enable(WDTO_30MS);
        while(1) {}; // wait...
    }

    if(value&0x0100) {
        PORTD &= ~_BV(PD2); // enable Tx buffers
    } else {
        PORTD |= _BV(PD2); // tri-state Tx buffers
    }

    reg[0] = value&0x0100; // only write those which are safe to save

    if(value&0x0200) {
        uint16_t savereg[NREG+1];
        /* write eeprom */

        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            memcpy(savereg, reg, sizeof(reg));
        }
        savereg[NREG] = calculate_crc((uint8_t*)savereg, sizeof(reg));
        eeprom_write_block(savereg, eereg, sizeof(savereg));
    }
}

static void mbus_write_outputs(uint16_t faddr, uint16_t rvalue)
{
    uint8_t value=rvalue&0x0F;
    uint8_t *breg=(uint8_t*)reg;

    uint8_t SB=PORTB, SD=PORTD;

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

    breg[2] = value;
}

static void mbus_write_config_out1(uint16_t faddr, uint16_t value)
{
    uint8_t div=value>>8;
    uint8_t mode=value&0x3;

    // disable counter in preparation to change mode
    TCCR2A = 0;
    TCCR2B = 0;

    if(mode!=0) {
        if(div>=sizeof(divtbl))
            div=0;
        div = divtbl[div];

        if(mode==1) { // freq (CTC mode)
            TCCR2A = _BV(WGM21)|_BV(COM2A0);
        } else { // PWM (fast)
            TCCR2A = _BV(WGM21)|_BV(WGM20)|_BV(COM2A0);
        }
        TCCR2B = div;
    }

    value = rdivtbl[div]<<8 | mode;

    reg[2] = value;
}

static void mbus_write_param_out1(uint16_t faddr, uint16_t value)
{
    OCR2A = value;
}

static void mbus_write_config_out2(uint16_t faddr, uint16_t value)
{
    uint8_t div=value>>8;
    uint8_t mode=value&0x3;

    // disable counter in preparation to change mode
    TCCR1A = 0;
    TCCR1B = 0;
    OCR1A = OCR1B = 0;

    if(mode!=0) {
        if(div>=sizeof(divtbl))
            div=0;
        div = divtbl[div];

        if(mode==1) { // freq (CTC mode)
            OCR1A = reg[5];
            TCCR1B = _BV(WGM12);
        } else { // PWM (phase + freq correct)
            TCCR1B = _BV(WGM13);
            ICR1 = 0x7fff;
            OCR1B = reg[5];
        }
        TCCR1B |= div;
        TCCR1A |= _BV(COM1B0);
    }

    reg[4] = rdivtbl[div]<<8 | mode;
}

static void mbus_write_param_out2(uint16_t faddr, uint16_t value)
{
    switch(reg[4]&3) {
    case 1: OCR1A = value; break;
    case 2: OCR1B = value; break;
    default: break;
    }
}

typedef void (*mbus_write_op_t)(uint16_t,uint16_t);

static mbus_write_op_t mbus_ops[] = {
    mbus_write_csr,
    mbus_write_outputs,
    mbus_write_config_out1,
    mbus_write_param_out1,
    mbus_write_config_out2,
    mbus_write_param_out2,
};

void mbus_write_holding(uint16_t faddr, uint16_t value)
{
    uint8_t addr=faddr;
    if(faddr>=NELEMENTS(mbus_ops))
        return;

    mbus_ops[addr](faddr,value);
}
