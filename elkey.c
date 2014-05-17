//#define F_CPU   9.6MHZ
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

// For ATtiny13

/* Port definitions, free to customize */
#define DOT_PORT     PORTB
#define DOT_PIN      PINB
#define DOT_DDR      DDRB
#define DOT_MASK     _BV(3)

#define DASH_PORT    PORTB
#define DASH_PIN     PINB
#define DASH_DDR     DDRB
#define DASH_MASK    _BV(4)

#define OUT_PORT     PORTB
#define OUT_DDR      DDRB
#define OUT_MASK     _BV(0)
/* End of port predefs */

#define OUT_ON()     OUT_PORT |= OUT_MASK
#define OUT_OFF()    OUT_PORT &= ((uint8_t)~OUT_MASK)
#define OUT_TOGGLE() OUT_PORT ^= ((uint8_t)OUT_MASK)

enum MKEY_TYPE { 
    NONE,
    DIT,
    DAH,
    DIT_ALTER, 
    DAH_ALTER 
};

//static const volatile WPM = 30;

void setup_pins(void)
{
    /* The output is the only output :) */
    OUT_DDR = OUT_MASK;
}

void setup_timers(void)
{
    TCCR0A = _BV(WGM01);
    TCCR0B = _BV(CS02)|_BV(CS00); // ClkIO/1024, CTC
    TIMSK0 = _BV(OCIE0A); // Enable timer match interrupt
    OCR0A  = 100; // ~0.004 seconds (30 WPM)
    sei();
}

static volatile uint8_t key_type = NONE;
static volatile uint8_t dah_counter = 0;
static volatile uint8_t prev_key_type = NONE;

ISR(TIM0_COMPA_vect)
{
    switch (key_type) {
        case DAH:
        if (dah_counter < 4) {
            dah_counter++;
            OUT_ON();
        } else {
            dah_counter = 0;
            OUT_OFF();
        }
        break;

        case DIT:
        OUT_TOGGLE(); 
        break;

        case NONE:
        default:
        OUT_OFF();
        break;
    }
    prev_key_type = key_type;
}

int main() /* __attribute__((noreturn)) */
{
    setup_pins();
    setup_timers();
    while (1) {
        if (DOT_PIN & DOT_MASK) { // DIT
            key_type = DIT; //(key_type == DAH) ? DAH_ALTER : DIT;
        } else if (DASH_PIN & DASH_MASK) { // DAH
            key_type = DAH; //(key_type == DAH) ? DIT_ALTER : DAH;
        } else {
            dah_counter = 0;
            key_type = NONE;
        }
    }
}
