#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <util/atomic.h>
#include <avr/wdt.h>
#include <util/delay.h>

/* LEDS */
#define PORTLED PORTD
#define DDRLED DDRD
#define PINLED PIND
#define LED1 _BV(PD5)
#define LED2 _BV(PD6)
#define LED3 _BV(PD3)
#define OCRLED1 OCR0B
#define OCRLED2 OCR0A
#define OCRLED3 OCR2B
#define LEDALL (LED1|LED2|LED3)

/* Relay/FET drive */
#define PORTDRV PORTB
#define DDRDRV DDRB
#define PINDRV PINB
#define OCRDRV OCR1A
#define DRIVE _BV(PB1)

/* Motion input */
#define PORTMOTION PORTC
#define DDRMOTION DDRC
#define PINMOTION PINC
#define MOTION _BV(PC0)
#define PCINTMOTION PCINT8

#define THRESHOLD 255

static inline void setupuart(void);
static void put_char(uint8_t c);
static void put_hex(uint8_t n);

static volatile uint8_t edge_count;

ISR(PCINT1_vect)
{
    uint8_t ecnt = edge_count;
    if(ecnt!=0xff) {
        ecnt++;
        edge_count=ecnt;
    }
}

int main (void) __attribute__ ((OS_main));
int main (void)
{
    uint8_t holdoff = 0, ontime = 0;

    wdt_disable();
    MCUSR &= ~_BV(WDRF);

    DDRLED = LEDALL; /* direction out (initially low) */

    /* leave disable internal pullup disabled (external pull *down*) */
    DDRDRV = DRIVE; /* direction out (initially low) */

    /* setup pin change interrupt for motion input */
    PCMSK1 = _BV(PCINTMOTION);
    PCICR = _BV(PCIE1);

    power_all_disable();
    power_usart0_enable();
    power_timer0_enable();
    power_timer2_enable();

    /* counters 0 and 2 used to dim LEDs
     * Use mode 3 (Fast PWM w/ TOP=FF)
     * Setup OCR to non-inverting (roll-over=0)
     */
    TCCR0A = _BV(WGM00)|_BV(WGM01)|_BV(COM0A1)|_BV(COM0B1);
    TCCR0B = _BV(CS00); /* clk/1 */

    TCCR2A = _BV(WGM20)|_BV(WGM21)|_BV(COM2B1);
    TCCR2B = _BV(CS20); /* clk/1 */

    setupuart();

    sei(); /* enable interrupts */

    while(1) {
        uint8_t cnt;
        ATOMIC_BLOCK(ATOMIC_FORCEON) {
            cnt = edge_count;
            edge_count = 0;
        }

        if(cnt>100) { /* something wierd, reset */
            holdoff = 0;
        } else if(cnt>1) {
            if(holdoff<10)
                holdoff+=2;
        } else {
            if(holdoff)
                holdoff-=1;
        }

        if(holdoff>=6) {
            ontime = 20;
        } else if(ontime) {
            ontime--;
        }
        OCRLED3 = ontime*2;

        if(ontime)
            PORTDRV |= DRIVE;
        else
            PORTDRV &= ~DRIVE;

        put_hex(cnt);
        put_char(' ');
        put_hex(holdoff);
        put_char(' ');
        put_hex(ontime);
        put_char('\r');
        put_char('\n');

        if(cnt>0x1f)
            cnt = 0x1f;
        OCRLED1 = cnt;
        _delay_ms(500);
    }

    return 0; /* never gets here */
}

static inline void setupuart(void)
{
#define BAUD_TOL 3
#define BAUD 9600
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
    /* Enable Tx only */
    UCSR0B = _BV(TXEN0);
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

static
void put_hex(uint8_t n)
{
    put_char(hexc[n>>4]);
    put_char(hexc[n&0xf]);
}
