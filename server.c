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

#include <avr/io.h>
#include <avr/interrupt.h>

#include "mbus.h"
#include "server.h"

#ifndef F_CPU
#  error F_CPU must be defined
#endif

static inline void setupuart(void)
{
#define BAUD_TOL 3
#define BAUD 115200
#include <util/setbaud.h>
    UBRR0H = UBRRH_VALUE;
    UBRR0L = UBRRL_VALUE;
#if USE_2X
    UCSR0A = _BV(U2X0);
#else
    UCSR0A = 0;
#endif
    /* 8 N 1 */
    UCSR0C = _BV(UCSZ00)|_BV(UCSZ01);
    /* Enable Tx/Rx */
    UCSR0B = _BV(TXEN0)|_BV(RXEN0);
#undef BAUD
#undef BAUD_TOL
#ifdef USE_2X
#  undef USE_2X
#endif
}

uint8_t timo_active;

int main(void) __attribute__ ((OS_main));
int main(void)
{
    setupuart();

    // enable blinking led on arduino uno
    PORTB = _BV(PB5);
    DDRB  = _BV(DDB5);
    PORTB &= ~_BV(PB5);

    // setup timer0
    TCCR0B = _BV(CS00)|_BV(CS02); // /1024

    user_init();
    sei();

    while(1) {
        uint8_t do_proc = 0;
        uint8_t usts;

        user_loop();

        usts = UCSR0A;

        // if mbus has data to send, and
        // UART can accept.
        if(mbus_status&MBUS_TX_READY &&
                (usts&_BV(UDRE0)))
        {
            UDR0 = mbus_out_byte;
            mbus_status &= ~MBUS_TX_READY;
            do_proc = 1;
        }

        if(timo_active)
            continue;

        if(usts&_BV(RXC0) && !(mbus_status&MBUS_RX_READY)) {
            uint8_t data = UDR0;
            // UART received data
            if((usts&(_BV(FE0)|_BV(DOR0)|_BV(UPE0))) ||
               mbus_status&MBUS_RX_ERROR)
            {
                // RX error or overflow
                // clear any partially received input
                mbus_status&=~MBUS_RX_ERROR;
                mbus_rx_clear();
                // setup timeout
                timo_active=0xff;
                PORTB |= _BV(PB5); // turn on LED
            } else {
                mbus_in_byte = data;
                mbus_status |= MBUS_RX_READY;
                do_proc = 1;
            }
        }

        if(do_proc)
            mbus_process();
    }
}

ISR(TIMER0_OVF_vect)
{
    uint8_t ta = timo_active;
    if(ta) {
        ta--;
        timo_active=ta;
        if(!ta) {
            PORTB &= ~_BV(PB5); // turn off LED
        }
    }

    user_tick();
}
