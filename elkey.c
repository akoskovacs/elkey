//#define F_CPU   1E6
#include <stdbool.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <avr/interrupt.h>

/* Null to disable power-down sleep mode */
#define DO_PWR_DOWN 1
#define DO_DELAY 0
// For ATtiny13

#define DELAY_MS 10

/* Dit types */
#define DOT_PORT     PORTB
#define DOT_PIN      PINB
#define DOT_DDR      DDRB
#define DOT_MASK     _BV(PB1)
#define DOT_INT      PCINT1

/* Dah types */
#define DASH_PORT    PORTB
#define DASH_PIN     PINB
#define DASH_DDR     DDRB
#define DASH_MASK    _BV(PB2)
#define DASH_INT     PCINT2 /* Must match with the mask */

/* Output pin can be any pin (logic high when keyed) */
#define OUT_PORT     PORTB
#define OUT_DDR      DDRB
#define OUT_MASK     _BV(PB0)
/* End of port predefs */

#define OUT_ON()     OUT_PORT |= OUT_MASK
#define OUT_OFF()    OUT_PORT &= ((uint8_t)~OUT_MASK)
#define OUT_TOGGLE() OUT_PORT ^= ((uint8_t)OUT_MASK)

//#define IS_ALTERING(k) (k == DIT_ALTER || k == DAH_ALTER)
#define IS_DIT()       (DOT_PIN & DOT_MASK)
#define IS_DAH()       (DASH_PIN & DASH_MASK)
#define IS_ALTERING() (IS_DIT() && IS_DAH())


//static const volatile WPM = 30;

void setup_pins(void)
{
    /* The output is the only output :) */
    OUT_DDR    = OUT_MASK;
    /* Pull up for dih and dah */
    //DOT_PORT  |= DOT_MASK;
    //DASH_PORT |= DASH_MASK;

    //MCUCR     |= _BV(ISC00); /* Any logical change trigger interrupts */

//    GIMSK     |= _BV(DOT_INT)|_BV(PCIE);
    /* Enable interrupts for PCINTn pins */
    GIMSK     |= _BV(PCIE);
    PCMSK     |= _BV(DOT_INT)|_BV(DASH_INT);
}

void setup_timer(void)
{
    TCCR0A = _BV(WGM01);
    TCCR0B = _BV(CS02)|_BV(CS00); // ClkIO/1024, CTC
    TIMSK0 = _BV(OCIE0A); // Enable timer match interrupt
    OCR0A  = 100; // ~0.004 seconds (30 WPM)
}

/*
 * This interrupt handler mainly used to power-up the chip.
 * It can also do debouncing, when DO_DELAY=1
 */
ISR(PCINT0_vect)
{
#if DO_DELAY
   _delay_ms(DELAY_MS); 
#endif 
}

enum MKEY_TYPE { NONE = 0, DIT, DAH };
static volatile uint8_t     dah_counter = 0;
static volatile enum MKEY_TYPE curr_key = NONE;
static volatile bool           is_ready = false;

void key_handler()
{
    switch (curr_key) {
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
		is_ready = true;
        break;
    }
}

ISR(TIM0_COMPA_vect)
{
    if (is_ready) {
        /* No beeping currently, do the altering (if any) */
        if (IS_ALTERING()) {
            if (curr_key == DIT) {
                curr_key = DAH;
            } else { 
                curr_key = DIT;
            }
        } else {
            if (IS_DIT()) {
                curr_key = DIT;
            } else if (IS_DAH()) {
                curr_key = DAH;
            } else {
                curr_key = NONE;
            }
        }
    }
    /* Do the keying */
    key_handler();
}

int main() /* __attribute__((noreturn)) */
{
    setup_pins();
    setup_timer();
    sei();

#if DO_PWR_DOWN
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
#endif

    while (1) {
        /* No key is pressed, go to bed... */
        if (!IS_DIT() && !IS_DAH()) {
            OUT_OFF();
            dah_counter = 0;
            is_ready = true;
			curr_key = NONE;
#if DO_PWR_DOWN
            sleep_cpu();
#endif
        }
    }
}
