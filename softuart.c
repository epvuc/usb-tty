/* This has been modified to work with 5-bit teletype code, and will
likely no longer work for 7 or 8 bit ASCII -- EPV */

// softuart.c
// AVR-port of the generic software uart written in C
//
// Generic code from
// Colin Gittins, Software Engineer, Halliburton Energy Services
// (available from the iar.com web-site -> application notes)
//
// Adapted to AVR using avr-gcc and avr-libc
// by Martin Thomas, Kaiserslautern, Germany
// <eversmith@heizung-thomas.de> 
// http://www.siwawi.arubi.uni-kl.de/avr_projects
//
// AVR-port Version 0.3  4/2007
//
// ---------------------------------------------------------------------
//
// Remarks from Colin Gittins:
//
// Generic software uart written in C, requiring a timer set to 3 times
// the baud rate, and two software read/write pins for the receive and
// transmit functions.
//
// * Received characters are buffered
// * putchar(), getchar(), kbhit() and flush_input_buffer() are available
// * There is a facility for background processing while waiting for input
// The baud rate can be configured by changing the BAUD_RATE macro as
// follows:
//
// #define BAUD_RATE  19200.0
//
// The function init_uart() must be called before any comms can take place
//
// Interface routines required:
// 1. get_rx_pin_status()
//    Returns 0 or 1 dependent on whether the receive pin is high or low.
// 2. set_tx_pin_high()
//    Sets the transmit pin to the high state.
// 3. set_tx_pin_low()
//    Sets the transmit pin to the low state.
// 4. idle()
//    Background functions to execute while waiting for input.
// 5. timer_set( BAUD_RATE )
//    Sets the timer to 3 times the baud rate.
// 6. set_timer_interrupt( timer_isr )
//    Enables the timer interrupt.
//
// Functions provided:
// 1. void flush_input_buffer( void )
//    Clears the contents of the input buffer.
// 2. char kbhit( void )
//    Tests whether an input character has been received.
// 3. char getchar( void )
//    Reads a character from the input buffer, waiting if necessary.
// 4. void turn_rx_on( void )
//    Turns on the receive function.
// 5. void turn_rx_off( void )
//    Turns off the receive function.
// 6. void putchar( char )
//    Writes a character to the serial port.
//
// ---------------------------------------------------------------------

/* 
Remarks by Martin Thomas (avr-gcc):
V0.1:
- stdio.h not used
- AVR-Timer in CTC-Mode ("manual" reload may not be accurate enough)
  Timer1 used here (Timer0 CTC not available i.e. on ATmega8)
- Global Interrupt Flag has to be enabled (see Demo-Application)
- Interface timer_set and set_timer_interrupt not used here
- internal_tx_buffer was defined as unsigned char - thas could not
  work since more than 8 bits needed, changed to unsigned short
- some variables moved from "global scope" into ISR function-scope
- GPIO initialisation included
- Added functions for string-output inspired by P. Fleury's AVR UART-lib.
V0.2:
- adjust num of RX-bits
- adapted to avr-libc ISR-macro (replaces SIGNAL)
- disable interrupts during timer-init
- used unsigned char (uint8_t) where apropriate
- removed "magic" char checking (0xc2)
- added softuart_can_transmit()
- Makefile based on template from WinAVR 1/2007
- reformated
- extended demo-application to show various methods to 
  send a string from flash and RAM
- demonstrate usage of avr-libc's stdio in demo-applcation
- tested with ATmega644 @ 3,6864MHz system-clock using
  avr-gcc 4.1.1/avr-libc 1.4.5 (WinAVR 1/2007)
V0.3
- better configuration options in softuart.h.
  ->should be easier to adapt to different AVRs
- tested with ATtiny85 @ 1MHz (int R/C) with 2400 bps
- AVR-Studio Project-File
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "softuart.h"
#include "conf.h"

#define SU_TRUE 1
#define SU_FALSE 0

// startbit and stopbit parsed internaly (see ISR)
volatile static char              inbuf[SOFTUART_IN_BUF_SIZE];
volatile static unsigned char    qin  = 0;
volatile static unsigned char qout = 0;
volatile static unsigned char    flag_rx_off;
volatile static unsigned char    flag_rx_ready;

// 1 Startbit, 8 Databits, 1 Stopbit = 10 Bits/Frame
// or for teletype, 1 start, 5 data, 2 stop = 8 bits/frame
extern uint8_t rxbits, txbits; // epv
volatile uint8_t framing_error = 0; // epv

#define TX_NUM_OF_BITS (txbits)
#define RX_NUM_OF_BITS (rxbits)
volatile unsigned char  flag_tx_ready;
extern uint8_t confflags; // epv

// volatile static unsigned char  flag_tx_ready;
volatile static unsigned char  timer_tx_ctr;
volatile static unsigned char  bits_left_in_tx;
volatile static unsigned short internal_tx_buffer; /* ! mt: was type uchar - this was wrong */

// data is D6, led1 is D0, led2 is D1
void set_tx_pin_high(void)
{ 
  // data on, led0 on
  PORTD |= _BV(6); // set data line on

  PORTD |= _BV(0);  
  //  PORTD &= ~_BV(1); 
}
void set_tx_pin_low(void)
{
  // data off, led0 off
  PORTD &= ~_BV(6); // set data line off

  PORTD &= ~_BV(0); 
  //  PORTD |= _BV(1);
}

int8_t get_rx_pin_status(void)
{
  uint8_t val;
  val = PIND & _BV(4);
  // val = SOFTUART_RXPIN  & ( 1<<SOFTUART_RXBIT );
  if (val)
    PORTD |= _BV(1);
  else
    PORTD &= ~_BV(1);
  return (!val);
}

// needs to be inverted if being fed through inverting optoisolator (6N139)
// or normal if being fed directly
// #define get_rx_pin_status()    ( SOFTUART_RXPIN  & ( 1<<SOFTUART_RXBIT ) )
//#define get_rx_pin_status()    (!( SOFTUART_RXPIN  & ( 1<<SOFTUART_RXBIT ) )) // opto



ISR(SOFTUART_T_COMP_LABEL)
{
	static unsigned char flag_rx_waiting_for_stop_bit = SU_FALSE;
	static unsigned char rx_mask;
	
	static char timer_rx_ctr;
	static char bits_left_in_rx;
	static unsigned char internal_rx_buffer;
	
	char start_bit, flag_in;
	char tmp;
	
	// Transmitter Section
	if ( flag_tx_ready ) {
	  if (!(confflags & CONF_8BIT))
	    if ((bits_left_in_tx == 1) && (timer_tx_ctr == 2)) // short circuit state machine for tty use
	      timer_tx_ctr=1;
	  
		tmp = timer_tx_ctr;
		if ( --tmp <= 0 ) { // if ( --timer_tx_ctr <= 0 )
			if ( internal_tx_buffer & 0x01 ) {
				set_tx_pin_high();
			}
			else {
				set_tx_pin_low();
			}
			internal_tx_buffer >>= 1;
			tmp = 3; // timer_tx_ctr = 3;
			if ( --bits_left_in_tx <= 0 ) {
				flag_tx_ready = SU_FALSE;
			}
		}
		timer_tx_ctr = tmp;
	}

	// Receiver Section
	if ( flag_rx_off == SU_FALSE ) {
		if ( flag_rx_waiting_for_stop_bit ) {
			if ( --timer_rx_ctr <= 0 ) {
				flag_rx_waiting_for_stop_bit = SU_FALSE;
				flag_rx_ready = SU_FALSE;
				inbuf[qin] = internal_rx_buffer;
				if ( ++qin >= SOFTUART_IN_BUF_SIZE ) {
					// overflow - rst inbuf-index
					qin = 0;
				}
			} else {if ((internal_rx_buffer == 0) && (get_rx_pin_status() == 0)) framing_error = 1; }// EPV BREAK
		}
		else {  // rx_test_busy
			if ( flag_rx_ready == SU_FALSE ) {
				start_bit = get_rx_pin_status();
				// test for start bit
				if ( start_bit == 0 ) {
					flag_rx_ready      = SU_TRUE;
					internal_rx_buffer = 0;
					timer_rx_ctr       = 4;
					bits_left_in_rx    = RX_NUM_OF_BITS;
					rx_mask            = 1;
				}
			}
			else {  // rx_busy
				if ( --timer_rx_ctr <= 0 ) {
					// rcv
					timer_rx_ctr = 3;
					flag_in = get_rx_pin_status();
					if ( flag_in ) {
						internal_rx_buffer |= rx_mask;
					}
					rx_mask <<= 1;
					if ( --bits_left_in_rx <= 0 ) {
						flag_rx_waiting_for_stop_bit = SU_TRUE;
					}
				}
			}
		}
	}
}

static void avr_io_init(void)
{
	// TX-Pin as output (and indicator light)
  SOFTUART_TXDDR |= _BV(7)|_BV(3);
//	SOFTUART_TXDDR |=  ( 1 << SOFTUART_TXBIT );
	// RX-Pin as input
        DDRD &= ~_BV(4);
	SOFTUART_RXDDR &= ~( 1 << SOFTUART_RXBIT );
}

static void avr_timer_init(void)
{
	unsigned char sreg_tmp;
	
	sreg_tmp = SREG;
	cli();
	
#ifdef OLD_TIMER_SETUP
	SOFTUART_T_COMP_REG = SOFTUART_TIMERTOP;     /* set top */

	SOFTUART_T_CONTR_REGA = SOFTUART_CTC_MASKA | SOFTUART_PRESC_MASKA;
	SOFTUART_T_CONTR_REGB = SOFTUART_CTC_MASKB | SOFTUART_PRESC_MASKB;

	SOFTUART_T_INTCTL_REG |= SOFTUART_CMPINT_EN_MASK;

	SOFTUART_T_CNT_REG = 0; /* reset counter */
#endif
	
	SREG = sreg_tmp;


	OCR1A = 1833; // 16MHZ/64/(3 * 45.45 baud) = 1833.5166
	TCCR1A = 0;
	TCCR1B = _BV(WGM12)|_BV(CS11)|_BV(CS10); // WGM=CTC mode, clk prescale = /64
	// TIMSK1 |= TIMSK1 |= _BV(OCIE1A)|_BV(TOIE1); // How did this ever even work??? Wtf
	TIMSK1 |= _BV(OCIE1A)|_BV(TOIE1);
	TCNT1 = 0;
}

void softuart_init( void )
{
	flag_tx_ready = SU_FALSE;
	flag_rx_ready = SU_FALSE;
	flag_rx_off   = SU_FALSE;
	
	set_tx_pin_high(); /* mt: set to high to avoid garbage on init */
	avr_io_init();

	// timer_set( BAUD_RATE );
	// set_timer_interrupt( timer_isr );
	avr_timer_init(); // replaces the two calls above
}

static void idle(void)
{
	// timeout handling goes here 
	// - but there is a "softuart_kbhit" in this code...
	// add watchdog-reset here if needed
}

void softuart_turn_rx_on( void )
{
	flag_rx_off = SU_FALSE;
}

void softuart_turn_rx_off( void )
{
	flag_rx_off = SU_TRUE;
}

char softuart_getchar( void )
{
	char ch;

        if (qout == qin)
            return(0);

	ch = inbuf[qout];
 
	if ( ++qout >= SOFTUART_IN_BUF_SIZE ) {
		qout = 0;
	}
	
	return( ch );
}

unsigned char softuart_kbhit( void )
{
	return( qin != qout );
}

void softuart_flush_input_buffer( void )
{
	qin  = 0;
	qout = 0;
}
	
unsigned char softuart_can_transmit( void ) 
{
	return ( flag_tx_ready );
}

void softuart_putchar( const char ch )
{
	while ( flag_tx_ready ) {
		; // wait for transmitter ready
		  // add watchdog-reset here if needed;
	}

	// invoke_UART_transmit
	timer_tx_ctr       = 3;
	// bits_left_in_tx includes 1 start + 2 stop bits, 
	// so should be 8 for teletype. 
	bits_left_in_tx    = TX_NUM_OF_BITS; 
	if (confflags & CONF_8BIT)
	  internal_tx_buffer = ( ch<<1 ) | 0x200;
	else
	// for teletype, word = Start, data 1-5, Stop, Stop
	  internal_tx_buffer = ( ch<<1 ) | 0xC0;
	flag_tx_ready      = SU_TRUE;
}
	
void softuart_puts( const char *s )
{
	while ( *s ) {
		softuart_putchar( *s++ );
	}
}
	
void softuart_puts_p( const char *prg_s )
{
	char c;

	while ( ( c = pgm_read_byte( prg_s++ ) ) ) {
		softuart_putchar(c);
	}
}

char baudot_to_ascii(char);
void softuart_status(void)
{ 
  uint8_t i;
  char ascii_char;
  printf("%u %u ", qin, qout);
  for(i=0; i<SOFTUART_IN_BUF_SIZE; i++) {
        ascii_char = baudot_to_ascii(inbuf[i]);
	printf("%c", isprint(ascii_char)?ascii_char:'.');
  }
  printf("\r\n");
} 
