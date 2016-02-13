
#define SOFTUART_BAUD_RATE      45

// This is where we choose which IO pins to send and receive on
#define SOFTUART_RXPIN   PIND
#define SOFTUART_RXDDR   DDRD
#define SOFTUART_RXBIT   PD2

#define SOFTUART_TXPORT  PORTD
#define SOFTUART_TXDDR   DDRD
#define SOFTUART_TXBIT   PD6

// which AVR timer to use? 
#define SOFTUART_T_COMP_LABEL      TIMER1_COMPA_vect
#define SOFTUART_T_COMP_REG        OCR1A
#define SOFTUART_T_CONTR_REGA      TCCR1A
#define SOFTUART_T_CONTR_REGB      TCCR1B
#define SOFTUART_T_CNT_REG         TCNT1
#define SOFTUART_T_INTCTL_REG      TIMSK1

#define SOFTUART_CMPINT_EN_MASK    (1<<OCIE1A)

#define SOFTUART_CTC_MASKA         (1<<WGM11)
#define SOFTUART_CTC_MASKB         (0)

/* "A timer interrupt must be set to interrupt at three times 
   the required baud rate." */
#define SOFTUART_PRESCALE (8)
// #define SOFTUART_PRESCALE (1)

#if (SOFTUART_PRESCALE==8)
#define SOFTUART_PRESC_MASKA         (0)
#define SOFTUART_PRESC_MASKB         (1<<CS11)
#elif (SOFTUART_PRESCALE==1)
#define SOFTUART_PRESC_MASKA         (0)
#define SOFTUART_PRESC_MASKB         (1<<CS10)
#else 
#error "prescale unsupported"
#endif


#define SOFTUART_TIMERTOP ( F_CPU/SOFTUART_PRESCALE/SOFTUART_BAUD_RATE/3 -1)

#if (SOFTUART_TIMERTOP > 0xff)
#warning "Check SOFTUART_TIMERTOP"
#endif

#define SOFTUART_IN_BUF_SIZE     32

// Init the Software Uart
void softuart_init(void);

// Clears the contents of the input buffer.
void softuart_flush_input_buffer( void );

// Tests whether an input character has been received.
unsigned char softuart_kbhit( void );

// Reads a character from the input buffer, waiting if necessary.
char softuart_getchar( void );

// To check if transmitter is busy
unsigned char softuart_can_transmit( void );

// Writes a character to the serial port.
void softuart_putchar( const char );

// Turns on the receive function.
void softuart_turn_rx_on( void );

// Turns off the receive function.
void softuart_turn_rx_off( void );

// Write a NULL-terminated string from RAM to the serial port
void softuart_puts( const char *s );

// Write a NULL-terminated string from program-space (flash) 
// to the serial port. i.e. softuart_puts_p(PSTR("test"))
void softuart_puts_p( const char *prg_s );

// Helper-Macro - "automaticly" inserts PSTR
// when used: include avr/pgmspace.h before this include-file
#define softuart_puts_P(s___) softuart_puts_p(PSTR(s___))
