//#define F_CPU   1E6
#include <stdbool.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <avr/interrupt.h>

/* Null to disable power-down sleep mode */
#define DO_PWR_DOWN 0
#define DO_DELAY 1
// For ATtiny13

#define DELAY_MS 10

/* Dit type, must be INT0  */
#define DOT_PORT     PORTB
#define DOT_PIN      PINB
#define DOT_DDR      DDRB
#define DOT_MASK     _BV(PB1)
#define DOT_INT      INT0

/* Dah predefs, must be a PCINTn pin */
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

#define IS_ALTERING(k) (k == DIT_ALTER || k == DAH_ALTER)

enum MKEY_TYPE {
    NONE = 0,
    DIT,
    DIT_ALTER, 
    DAH,
    DAH_ALTER 
};
#define ALTERING_OF(key) (key+1)

//static const volatile WPM = 30;

void setup_pins(void)
{
    /* The output is the only output :) */
    OUT_DDR    = OUT_MASK;
    /* Pull up for dih and dah */
    //DOT_PORT  |= DOT_MASK;
    //DASH_PORT |= DASH_MASK;

    MCUCR     |= _BV(ISC00); /* Any logical change trigger interrupts */

    /* Enable interrupts for INTn and PCINTn pins */
    GIMSK     |= _BV(DOT_INT)|_BV(PCIE);
    PCMSK     |= _BV(DASH_INT);
}

void setup_timer(void)
{
    TCCR0A = _BV(WGM01);
    TCCR0B = _BV(CS02)|_BV(CS00); // ClkIO/1024, CTC
    TIMSK0 = _BV(OCIE0A); // Enable timer match interrupt
    OCR0A  = 100; // ~0.004 seconds (30 WPM)
}

static volatile enum MKEY_TYPE key_type = NONE;

static void 
set_key_type(const volatile uint8_t *pin, uint8_t pmask
            , enum MKEY_TYPE this_key, enum MKEY_TYPE opp_key)
{
   if (((*pin) & pmask)) {
       key_type = (key_type == opp_key) ? ALTERING_OF(opp_key) : this_key;
   } else {
       if (key_type == ALTERING_OF(this_key)) {
           key_type = opp_key;
       } else if (key_type == ALTERING_OF(opp_key)) {
           key_type = opp_key;
       } else {
           key_type = NONE;
       }
   }
}

/* Dih interrupt */
ISR(INT0_vect)
{
#if DO_DELAY
   _delay_ms(DELAY_MS); 
#endif 
   set_key_type(&DOT_PIN, DOT_MASK, DIT, DAH);
}

static volatile uint8_t dah_counter   = 0;
/*
 * Dah interrupt
 * Only one used -> no need for check the pin
 */
ISR(PCINT0_vect)
{
#if DO_DELAY
   _delay_ms(DELAY_MS); 
#endif 
   /* If dah is unset (pressed, pulled down) */
   set_key_type(&DASH_PIN, DASH_MASK, DAH, DIT);
   dah_counter = 0;
}

static volatile enum MKEY_TYPE curr_key = NONE;
static volatile bool           is_ready = false;

void key_handler(enum MKEY_TYPE key)
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
		is_ready = true;
        break;
    }
}

ISR(TIM0_COMPA_vect)
{
    enum MKEY_TYPE key = key_type;
	if (key == NONE) {
		return;
	}
	
    /* No beeping currently, do the altering (if any) */
    if (IS_ALTERING(key)) {
		if (curr_key == NONE) {
			if (key == DIT_ALTER) {
				curr_key = DIT;
			} else if (key == DAH_ALTER) {
				curr_key = DAH;
			}
		}
		if (is_ready) {
			if (curr_key == DIT) {
				curr_key = DAH;
			} else if (curr_key == DAH) {
				curr_key = DIT;
			} 
		}
		key = curr_key;
    }
    /* Do the keying */
    key_handler(key);
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
        if (key_type == NONE) {
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
