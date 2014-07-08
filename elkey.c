//#define F_CPU   6e5
#include <stdbool.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/delay.h>

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

#define IS_DIT()       (DOT_PIN & DOT_MASK)
#define IS_DAH()       (DASH_PIN & DASH_MASK)
#define IS_ALTERING()  (IS_DIT() && IS_DAH())

//static const volatile WPM = 30;

void setup_pins(void)
{
    /* The output is the only output :) */
    OUT_DDR    = OUT_MASK;
    GIMSK     |= _BV(PCIE);
    PCMSK     |= _BV(DOT_INT)|_BV(DASH_INT);
}

void enable_adc()
{
    /* Left adjust (use 8bit) & select channel 2 (ADC2) */
    ADMUX |= _BV(ADLAR)|_BV(MUX1);
    /* Enable the A/D converter with div128 */
    ADCSRA |= _BV(ADEN)|_BV(ADPS2)|_BV(ADPS1)|_BV(ADPS0);
}


uint8_t read_adc(void)
{
    ADMUX = 2; // Channel 2 (ADC2)
    ADCSRA |= _BV(ADSC); // Start a single conversion
    loop_until_bit_is_clear(ADCSRA, ADSC); // Wait til' done
    return ADCH;
}

void setup_timer(void)
{
    TCCR0A = _BV(WGM01);
    TCCR0B = _BV(CS02)|_BV(CS00); // ClkIO/256, CTC @ 600Khz
    TIMSK0 = _BV(OCIE0A); // Enable timer match interrupt
    OCR0A  = 40;
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
   enable_adc();
}

static volatile enum MKEY_TYPE { NONE = 0, DIT, DAH } curr_key = NONE;
static volatile uint8_t dah_counter   = 0;
static volatile bool    is_ready      = false;

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
    /* Update the code rate */
    OCR0A = (uint8_t)read_adc()*40;
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
            ADCSRA &= (uint8_t)~_BV(ADEN); // disable adc
            sleep_cpu();
#endif
        }
    }
}
