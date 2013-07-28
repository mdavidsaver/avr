/* clock out with delays
 */
#include <avr/io.h>
#include <util/delay.h>

int
main (void)
{
    PORTB = _BV(PB5);
    DDRB  = _BV(DDB5);


    while(1) {
        _delay_ms(500);
        PORTB ^= _BV(PB5);
    }

    return 0;
}
