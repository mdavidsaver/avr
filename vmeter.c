/* Arduino Uno as a volt meter
 */
#include <avr/io.h>
#include <avr/wdt.h>
#include <util/delay.h>

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

static
void put_char(uint8_t c)
{
    loop_until_bit_is_set(UCSR0A, UDRE0);
    UDR0 = c;
}

static uint8_t hexc[16] = "0123456789ABCDEF";

int main (void) __attribute__ ((OS_main));
int main (void)
{
    wdt_disable();
    MCUSR &= ~_BV(WDRF);

    // setup LED
    PORTB = _BV(PB5);
    DDRB  = _BV(DDB5);

    // Setup ADC
    // Ref=AVcc, Ch=ADC0
    ADMUX = _BV(REFS0);
    // Enable, Clock /128
    ADCSRA = _BV(ADEN)|_BV(ADPS0)|_BV(ADPS1)|_BV(ADPS2);

    setupuart();

    while(1) {
        uint16_t val;
        uint8_t cv, i;
        _delay_ms(500);
        PORTB ^= _BV(PB5);

        // Start converson
        ADCSRA |= _BV(ADSC);

        loop_until_bit_is_set(ADCSRA, ADIF);

        val = ADC;
        for(i=0; i<4; i++) {
            cv = (val&0xf000)>>12;
            val<<=4;
            put_char(hexc[cv]);
        }
        put_char('\r');
        put_char('\n');
    }

    return 0;
}
