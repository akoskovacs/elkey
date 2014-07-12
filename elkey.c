// For ATtiny13
#ifndef F_CPU
# define F_CPU   6e5 // 600Khz CPU CLOCK
#endif 

#include <stdbool.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/* ----- GENERAL SETTINGS ----- */
/* Null to disable power-down sleep mode */
#define DO_PWR_DOWN 1
/* Null to disable debouncing*/
#define DO_DELAY 0
/* Null to disable speed controll, with the ADC*/
#define WPM_CONTROLL 1 

/* Debounce by N miliseconds, if DO_DELAY=1 */
#define DELAY_MS 10

/* ----- PORT DEFINITIONS ----- */
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
#define DASH_INT     PCINT2

/* Output pin can be any pin (logic high when keyed) */
#define KEY_PORT     PORTB
#define KEY_PIN      PINB
#define KEY_DDR      DDRB
#define KEY_MASK     _BV(PB0)

/* Sound generator output */
#define SND_PORT     PORTB
#define SND_DDR      DDRB
#define SND_MASK     _BV(PB3)

#define WPM_CTRL_MUX MUX1
#define WPM_CTRL_CH  2
/* -----  END OF CONFIG  ----- */

#define KEY_ON()     KEY_PORT |= KEY_MASK
#define KEY_TOGGLE() KEY_PORT ^= ((uint8_t)KEY_MASK)
#define KEY_OFF()    KEY_PORT &= ((uint8_t)~KEY_MASK); \
                      SND_PORT &= ((uint8_t)~SND_MASK)

#define IS_DIT()       (DOT_PIN & DOT_MASK)
#define IS_DAH()       (DASH_PIN & DASH_MASK)
#define IS_ALTERING()  (IS_DIT() && IS_DAH())

//static const volatile WPM = 30;

void setup_pins(void)
{
    KEY_DDR   |= KEY_MASK;
    SND_DDR   |= SND_MASK;
    /* Setup interrupt pins */
    GIMSK     |= _BV(PCIE);
    PCMSK     |= _BV(DOT_INT)|_BV(DASH_INT);
}

void enable_adc()
{
#if WPM_CONTROLL
    /* Left adjust (use 8bit) & select channel 2 (ADC2) */
    ADMUX |= _BV(ADLAR)|_BV(WPM_CTRL_MUX);
    /* Enable the A/D converter with div128 */
    ADCSRA |= _BV(ADEN)|_BV(ADPS2)|_BV(ADPS1)|_BV(ADPS0);
#endif
}


uint8_t read_adc(void)
{
    ADMUX = WPM_CTRL_CH; // Channel 2 (ADC2)
    ADCSRA |= _BV(ADSC); // Start a single conversion
    loop_until_bit_is_clear(ADCSRA, ADSC); // Wait til' done
    return ADCH;
}

void setup_timer(void)
{
    TCCR0A = _BV(WGM01);
    TCCR0B = _BV(CS01)|_BV(CS00); // ClkIO/64, CTC @ 600Khz
    TIMSK0 = _BV(OCIE0A); // Enable timer match interrupt
    OCR0A  = 0; // ~ 600 Hz
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

static volatile uint16_t dit_ticks     = 0; /* Basic DIT timer counter */
static volatile uint8_t  can_do_morse  = 0; // software "interrupt" flag

ISR(TIM0_COMPA_vect)
{
    /* ATtiny13 has only one timer, do it in software */
    /* Toggle if keyed */
    if (KEY_PIN & KEY_MASK) {
        SND_PORT ^= SND_MASK;
    }

    if (dit_ticks >= (read_adc()*20)) {
        /* Time to do morse stuff, but not here */
        can_do_morse = 1;
        dit_ticks    = 0;
    } else {
        dit_ticks++;
    }
}

static enum MKEY_TYPE { NONE = 0, DIT, DAH } curr_key = NONE;
static uint8_t dah_counter   = 0;
static bool    is_ready      = false;

void key_handler()
{
    switch (curr_key) {
        case DAH:
        if (dah_counter < 3) { // Standard morse timing
            is_ready = false;
            dah_counter++;
            KEY_ON();
        } else {
            dah_counter = 0;
            KEY_OFF();
            is_ready = true;
        }
        break;

        case DIT:
        is_ready = (KEY_PORT & KEY_MASK);
        KEY_TOGGLE(); 
        break;
		
		default:
        KEY_OFF();
		is_ready = true;
        break;
    }
}

void do_morse(void)
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
        if (can_do_morse) {
            do_morse();
            can_do_morse = 0;
        }
        /* No key is pressed, go to bed... */
        if (!IS_DIT() && !IS_DAH()) {
            KEY_OFF();
            dah_counter = 0;
            is_ready = true;
            curr_key = NONE;
#if DO_PWR_DOWN
# if WPM_CONTROLL
            ADCSRA &= (uint8_t)~_BV(ADEN); // disable adc
# endif // WPM_CONTROLL
            sleep_cpu();
#endif
        }
    }
}
