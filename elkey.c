//#define F_CPU   1E6
#include <stdbool.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <avr/interrupt.h>

// For ATtiny13

/* Port definitions, free to customize */
#define DOT_PORT     PORTB
#define DOT_PIN      PINB
#define DOT_DDR      DDRB
#define DOT_MASK     _BV(1)
#define DOT_INT      INT0

#define DASH_PORT    PORTB
#define DASH_PIN     PINB
#define DASH_DDR     DDRB
#define DASH_MASK    _BV(2)
#define DASH_INT     PCINT2 /* Must match with the mask */

#define OUT_PORT     PORTB
#define OUT_DDR      DDRB
#define OUT_MASK     _BV(0)
/* End of port predefs */

#define OUT_ON()     OUT_PORT |= OUT_MASK
#define OUT_OFF()    OUT_PORT &= ((uint8_t)~OUT_MASK)
#define OUT_TOGGLE() OUT_PORT ^= ((uint8_t)OUT_MASK)

#define IS_DIT()     (DOT_PIN & DOT_MASK)
#define IS_DAH()     (DASH_PIN & DASH_MASK)

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
    OUT_DDR    = OUT_MASK;
    /* Pull up for dih and dah */
    DOT_PORT  |= DOT_MASK;
    DASH_PORT |= DASH_MASK;

    MCUCR     |= _BV(ISC00); /* Any logical change triggers interrupts */

    /* Enable interrupts for INTn and PCINTn pins */
    GIMSK     |= _BV(DOT_INT)|_BV(PCIE);
    PCMSK     |= _BV(DASH_INT);
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

/* DOT interrupt */
ISR(INT0_vect)
{
   _delay_ms(25); 
   /* If dit is unset (pressed, pulled down) */
   if ((DOT_PIN & DOT_MASK)) {
       key_type = (key_type == DAH) ? DAH_ALTER : DIT;
   } else {
       key_type = (key_type == DIT) ? NONE      : DAH;
   }
}

/*
 * Dash interrupt
 * Only one used -> no need for check the pin
 */
ISR(PCINT0_vect)
{
   _delay_ms(25); 
   /* If dah is unset (pressed, pulled down) */
   if ((DASH_PIN & DASH_MASK)) {
       key_type = (key_type == DAH) ? DAH_ALTER : DIT;
   } else {
       key_type = (key_type == DIT) ? NONE      : DAH;
   }
}

static volatile uint8_t dah_counter   = 0;
static volatile uint8_t prev_key_type = NONE;
static volatile bool    is_ready      = false;

void key_handler(uint8_t key)
{
    switch (key) {
        case DAH:
        if (dah_counter < 3) { // Standard morse timing
            is_ready = false;
            dah_counter++;
            OUT_ON();
        } else {
            dah_counter = 0;
            OUT_OFF();
            is_ready = true;
        }
        break;

        case DIT:
        is_ready = (OUT_PORT & OUT_MASK);
        OUT_TOGGLE(); 
        break;

        default:
        OUT_OFF();
        break;
    }
    prev_key_type = key;
}

ISR(TIM0_COMPA_vect)
{
    if (is_ready) {
        switch (key_type) {
            case DIT_ALTER:
            key_handler((prev_key_type == DIT) ? DAH : DIT);
            break;

            case DAH_ALTER:
            key_handler((prev_key_type == DAH) ? DIT : DAH);
            break;
        }
    } else {
        key_handler(key_type);
    }
}

void goto_bed(void)
{
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_cpu();	
}

int main() /* __attribute__((noreturn)) */
{
    setup_pins();
    setup_timers();
    //sei();
    while (1) {
        if (key_type == NONE) {
            OUT_OFF();
#if 1
            goto_bed();
#endif
        }
    }
}
