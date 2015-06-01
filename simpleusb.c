/* A "simple" control only USB device for
 * AVR atmega8u2, atmega16u2, or atmega32u2
 *
 * In addition to the standard control requests,
 * two more are defined to set/get a 16-bit 'userval'.
 * See simpleusb-client.py
 *
 * Doesn't require any peripherals
 *
 * Reference documents:
 *  USB spec. 2.0
 *  Atmel datasheet 7799E-AVR-09/2012
 *
 * And using as examples the AVR USB framework libraries:
 *  http://www.lufa-lib.org
 *  http://www.contiki-os.org/
 *
 * Author: Michael Davidsaver <mdavidsaver@gmail.com>
 */
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdlib.h>

#include "usb.h"

//#define HANDLE_SUSPEND
#if 0
#define oops(BOOL, MSG) do{}while(0)
#else
#define oops(BOOL, MSG) if(!(BOOL)) {put_str(MSG);abort();}
#endif

#define NELM(V) (sizeof(V)/sizeof(V[0]))

#define EP0_SIZE 64

#define set_bit(REG, BIT) REG |= _BV(BIT)
#define clear_bit(REG, BIT) REG &= ~_BV(BIT)
#define toggle_bit(REG, BIT) REG ^= _BV(BIT)
#define assign_bit(REG, BIT, VAL) do{if(VAL) set_bit(REG,BIT) else clear_bit(REG,BIT);}while(0)

#define EP_select(N) do{UENUM = (N)&0x07;}while(0)

#define EP_read8() (UEDATX)
#define EP_read16_le() ({uint16_t L, H; L=UEDATX; H=UEDATX; (H<<8)|L;})

#define EP_write8(V) do{UEDATX = (V);}while(0)
#define EP_write16_le(V) do{UEDATX=(V)&0xff;UEDATX=((V)>>8)&0xff;}while(0)


static
uint8_t ctrl_write_PM(const void *addr, uint16_t len);

/* UART for debugging */

static inline void setupuart(void)
{
#define BAUD_TOL 2
#define BAUD 19200
#include <util/setbaud.h>
    UBRR1H = UBRRH_VALUE;
    UBRR1L = UBRRL_VALUE;
#if USE_2X
    UCSR1A = _BV(U2X1);
#else
    UCSR1A = 0;
#endif
    /* 8 N 1 */
    UCSR1C = _BV(UCSZ10)|_BV(UCSZ11);
    /* Enable Tx/Rx */
    UCSR1B = _BV(TXEN1)|_BV(RXEN1);
#undef BAUD
#undef BAUD_TOL
#ifdef USE_2X
#  undef USE_2X
#endif
}

static
void put_char(uint8_t c)
{
    loop_until_bit_is_set(UCSR1A, UDRE1);
    UDR1 = c;
}

static const char hexchr[16] = "0123456789ABCDEF";

static __attribute__((unused))
void put_hex(char c)
{
    uint8_t b = c;
    put_char(hexchr[(b>>4)&0xf]);
    put_char(hexchr[b&0xf]);
}

static
void put_eol(void)
{
    put_char('\r');
    put_char('\n');
}

static __attribute__((unused))
void put_str(const char *m)
{
    char c;
    while((c=*m)!='\0')
        put_char(c);
    put_eol();
}

/* The control request currently being processed */

static usb_header head;

/* USB descriptors, stored in flash */

static const usb_std_device_desc PROGMEM devdesc = {
    .bLength = sizeof(devdesc),
    .bDescType = usb_desc_device,
    .bcdUSB = 0x0200,
    .bDevClass = 0xff, /* vender specific */
    .bDevSubClass = 0xff, /* vender specific */
    .bDevProto = 0xff, /* vender specific */
    .bMaxPacketSize = EP0_SIZE, /* EP 0 size 64 bytes */
    .idVendor = 0x1234,
    .idProd = 0x1234,
    .bcdDevice = 0x0100,
    .iProd = 1,
    .iSerial = 2,
    .bNumConfig = 1
};

static const struct {
    usb_std_config_desc conf;
    usb_std_iface_desc iface;
} PROGMEM devconf = {
    .conf = {
        .bLength = sizeof(usb_std_config_desc),
        .bDescType = usb_desc_config,
        .bTotalLengh = sizeof(devconf),
        .bNumIFaces = 1,
        .bConfValue = 1,
        .bmAttribs  = 0x80, /* bus powered */
        .bMaxPower  = 20/2  /* 20 mA */
    },
    .iface = {
        .bLength = sizeof(usb_std_iface_desc),
        .bDescType = usb_desc_iface,
        .bNumIFace = 0,
        .bAltSetting = 0,
        .bNumEP = 0,
        .bIfaceClass = 0xff, /* vender specific */
        .bIfaceSubClass = 0xff, /* vender specific */
        .bIfaceProto = 0xff, /* vender specific */
    }
};

static const usb_std_string_desc iLang PROGMEM = {
    .bLength = sizeof(usb_std_string_desc) + 2,
    .bDescType = usb_desc_string,
    .bString = {0x0409}
};

#define DESCSTR(STR) { \
.bLength = sizeof(usb_std_string_desc) + sizeof(STR)-2, \
.bDescType = usb_desc_string, \
.bString = STR \
}

static const usb_std_string_desc iProd PROGMEM = DESCSTR(L"simpleusb");
static const usb_std_string_desc iSerial PROGMEM = DESCSTR(L"42");

#undef DESCSTR

/* Handle the standard Get Descriptor request.
 * Return 1 on success
 */
static
uint8_t USB_get_desc(void)
{
    const void *addr;
    uint8_t len, idx = head.wValue;
    switch(head.wValue>>8)
    {
    case usb_desc_device:
        if(idx!=0) return 0;
        addr = &devdesc; len = sizeof(devdesc);
        break;
    case usb_desc_config:
        if(idx!=0) return 0;
        addr = &devconf; len = sizeof(devconf);
        break;
    case usb_desc_string:
        switch(idx)
        {
        case 0: addr = &iLang; break;
        case 1: addr = &iProd; break;
        case 2: addr = &iSerial; break;
        default: return 0;
        }
        /* the first byte of any descriptor is it's length in bytes */
        len = pgm_read_byte(addr);
        break;
    default:
        return 0;
    }

    if(len>head.wLength)
        len = head.wLength;

    return !ctrl_write_PM(addr, len);
}

static void setupEP0(void);

static uint16_t userval; /* user register */

static inline void setupusb(void)
{
    /* disable USB interrupts and clear any active */
    UDIEN = 0;
    UDINT = 0;

    set_bit(UDCON, DETACH); /* redundant? */

    /* toggle USB reset */
    clear_bit(USBCON, USBE);
    set_bit(USBCON, USBE);

    /* No required.
     * Gives some time to start reprograming
     * if previous program gets stuck right away
     */
    _delay_ms(1000);
    put_char('.');

    /* Unfreeze */
    clear_bit(USBCON, FRZCLK);

    /* setup PLL for 8 MHz system clock */
    PLLCSR = 0;
    set_bit(PLLCSR, PLLE);
    loop_until_bit_is_set(PLLCSR, PLOCK);
    put_char('.');

    setupEP0(); /* configure control EP */
    put_char('.');

#ifdef HANDLE_SUSPEND
    set_bit(UDIEN, SUSPE);
#endif
    set_bit(UDIEN, EORSTE);

    /* allow host to un-stick us.
     * Warning: Don't use w/ DETACH on CPU start
     *          or a reset loop will result
     */
    //set_bit(UDCON, RSTCPU);
    clear_bit(UDCON, DETACH);
}

/* Setup the control endpoint. (may be called from ISR) */
static void setupEP0(void)
{
    /* EPs assumed to be configured in increasing order */

    EP_select(0);

    /* un-configure EP 0 */
    clear_bit(UECONX, EPEN);
    clear_bit(UECFG1X, ALLOC);

    /* configure EP 0 */
    set_bit(UECONX, EPEN);
    UECFG0X = 0; /* CONTROL */
    UECFG1X = 0b00110010; /* EPSIZE=64B, 1 bank, ALLOC */
#if EP0_SIZE!=64
#  error EP0 size mismatch
#endif

    if(bit_is_clear(UESTA0X, CFGOK)) {
        put_char('!');
        while(1) {} /* oops */
    }
}

ISR(USB_GEN_vect, ISR_BLOCK)
{
    uint8_t status = UDINT, ack = 0;
    put_char('I');
#ifdef HANDLE_SUSPEND
    if(bit_is_set(status, SUSPI))
    {
        ack |= _BV(SUSPI);
        /* USB Suspend */

        /* prepare for wakeup */
        clear_bit(UDIEN, SUSPE);
        set_bit(UDIEN, WAKEUPE);

        set_bit(USBCON, FRZCLK); /* freeze */
    }
    if(bit_is_set(status, WAKEUPI))
    {
        ack |= _BV(WAKEUPI);
        /* USB wakeup */
        clear_bit(USBCON, FRZCLK); /* freeze */

        clear_bit(UDIEN, WAKEUPE);
        set_bit(UDIEN, SUSPE);
    }
#endif
    if(bit_is_set(status, EORSTI))
    {
        ack |= _BV(EORSTI);
        /* coming out of USB reset */

#ifdef HANDLE_SUSPEND
        clear_bit(UDIEN, SUSPE);
        set_bit(UDIEN, WAKEUPE);
#endif

        put_char('E');
        setupEP0();
    }
    /* ack. all active interrupts (write 0)
     * (write 1 has no effect)
     */
    UDINT = ~ack;
}

/* write value from flash to EP0 */
static
uint8_t ctrl_write_PM(const void *addr, uint16_t len)
{
    while(len) {
        uint8_t ntx = EP0_SIZE,
                bsize = UEBCLX,
                epintreg = UEINTX;

        oops(ntx>=bsize, "EP"); /* EP0_SIZE is wrong */

        ntx -= bsize;
        if(ntx>len)
            ntx = len;

        if(bit_is_set(epintreg, RXSTPI))
            return 1; /* another SETUP has started, abort this one */
        if(bit_is_set(epintreg, RXOUTI))
            break; /* stop early? (len computed improperly?) */

        /* Retry until can send */
        if(bit_is_clear(epintreg, TXINI))
            continue;
        oops(ntx>0, "Ep"); /* EP0_SIZE is wrong (or logic error?) */

        len -= ntx;

        while(ntx) {
            uint8_t val = pgm_read_byte(addr);
            EP_write8(val);
            addr++;
            ntx--;
        }

        clear_bit(UEINTX, TXINI);
    }
    return 0;
}

/* Handle standard Set Address request */
static
void USB_set_address(void)
{
    UDADDR = head.wValue&0x7f;

    clear_bit(UEINTX, TXINI); /* send 0 length reply */

    loop_until_bit_is_set(UEINTX, TXINI); /* wait until sent */

    UDADDR = _BV(ADDEN) | (head.wValue&0x7f);

    clear_bit(UEINTX, TXINI); /* magic packet? */
}

static
uint8_t USB_config;

static
void handle_CONTROL(void)
{
    uint8_t ok = 0;
    /* SETUP message */
    head.bmReqType = EP_read8();
    head.bReq = EP_read8();
    head.wValue = EP_read16_le();
    head.wIndex = EP_read16_le();
    head.wLength = EP_read16_le();

    /* ack. first stage of CONTROL.
     * Clears buffer for IN/OUT data
     */
    clear_bit(UEINTX, RXSTPI);

    /* despite what the figure in
     * 21.12.2 (Control Read) would suggest,
     * SW should not clear TXINI here
     * as doing so will send a zero length
     * response.
     */

    switch(head.bReq)
    {
    case usb_req_set_feature:
    case usb_req_clear_feature:
        /* No features to handle.
         * We ignore Remote wakeup,
         * and EP0 will never be Halted
         */
        ok = 1;
        break;
    case usb_req_get_status:
        switch(head.bmReqType) {
        case 0b10000000:
        case 0b10000001:
        case 0b10000010:
            /* alway status 0 */
            loop_until_bit_is_set(UEINTX, TXINI);
            EP_write16_le(0);
            clear_bit(UEINTX, TXINI);
            ok = 1;
        }
        break;
    case usb_req_set_address:
        if(head.bmReqType==0) {
            USB_set_address();

            put_char('A');
            return;
        }
        break;
    case usb_req_get_desc:
        if(head.bmReqType==0x80) {
            ok = USB_get_desc();
        }
        break;
    case usb_req_set_config:
        if(head.bmReqType==0) {
            USB_config = head.wValue;
            ok = 1;
        }
        break;
    case usb_req_get_config:
        if(head.bmReqType==0x80) {
            loop_until_bit_is_set(UEINTX, TXINI);
            EP_write8(USB_config);
            clear_bit(UEINTX, TXINI);
            ok = 1;
        }
        break;
    case usb_req_set_iface:
    case usb_req_get_iface:
    case usb_req_set_desc:
    case usb_req_synch_frame:
        break;

    /* our (vendor specific) operations */
    case 0x7f:
        if(head.bmReqType==0b01000010 && head.wLength>=2) {
            /* Control Write H2D */
            loop_until_bit_is_set(UEINTX, RXOUTI);
            userval = EP_read16_le();
            clear_bit(UEINTX, RXOUTI);
            ok = 1;
        } else if(head.bmReqType==0b11000010 && head.wLength>=2) {
            /* Control Read D2H */
            loop_until_bit_is_set(UEINTX, TXINI);
            EP_write16_le(userval);
            clear_bit(UEINTX, TXINI);
            ok = 1;
        }
        break;
    default:
        put_char('?');
        put_hex(head.bmReqType);
        put_hex(head.bReq);
        put_hex(head.wLength>>8);
        put_hex(head.wLength);
    }

    if(ok) {
        if(head.bmReqType&ReqType_DirD2H) {
            /* Control read.
             * Wait for, and complete, status
             */
            uint8_t sts;
            while(!((sts=UEINTX)&(_BV(RXSTPI)|_BV(RXOUTI)))) {}
            //loop_until_bit_is_set(UEINTX, RXOUTI);
            ok = (sts & _BV(RXOUTI));
            if(!ok) {
                set_bit(UECONX, STALLRQ);
                put_char('S');
            } else {
                clear_bit(UEINTX, RXOUTI);
                clear_bit(UEINTX, TXINI);
            }
        } else {
            /* Control write.
             * indicate completion
             */
            clear_bit(UEINTX, TXINI);
        }
        put_char('C');

    } else {
        /* fail un-handled SETUP */
        set_bit(UECONX, STALLRQ);
        put_char('F');
    }
}

int main (void) __attribute__ ((OS_main));
int main (void)
{
    wdt_disable();
    MCUSR &= ~_BV(WDRF);

    CLKPR = _BV(CLKPCE); /* prepare for divider change */
    CLKPR = 0; /* clock divider to /1 (no divider) */

    setupuart();
    put_char('[');
    setupusb();

    put_hex(UDADDR);

    put_char(']');
    put_eol();

    sei(); /* enable interrupts */

    while(1) {
        EP_select(0);
        if(bit_is_set(UEINTX, RXSTPI))
            handle_CONTROL();
    }
}
