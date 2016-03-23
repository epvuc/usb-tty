/* Functions for translating ascii to/from ITA-2 teletype code, with shifts */
/* Eric Volpe 7/2013 epvgk@limpoc.com */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <avr/eeprom.h>
#include "baudot.h"
#include "conf.h"

extern uint8_t confflags;     // from main.c
extern uint8_t tableselector; // from main.c

// global state variables for baudot shift state
// these get used in a bunch of places
uint8_t baudot_shift_rcv = LTRS;
uint8_t baudot_shift_send = LTRS;

// take an ASCII char, send Baudot to teletype
int tty_putchar(char c, FILE *stream)
{
  char b;
  b = ascii_to_baudot(toupper(c));
  if ( b == 0)
    return(0);

  // if ascii_to_baudot tells us we need to shift
  // the teletype's character set, do that first
  if (b & (1<<5)) { 
    softuart_putchar(baudot_shift_send);
    b &= ~(1<<5); // clear the "need shift" bit
  } 
  // now send the actual Baudot character. 
  softuart_putchar(b);
  return 0;
}

int tty_putchar_raw(char c)
{ 
  softuart_putchar(c);
  return 0;
}


/* ASCII / BAUDOT conversions, with shifts */

/*  moving these to eeprom.
char ltrs[32] = { 0, 'E', 0x0A, 'A', ' ', 'S', 'I', 'U', 
		0x0D, 'D', 'R', 'J', 'N', 'F', 'C', 'K', 
		'T', 'Z', 'L', 'W', 'H', 'Y', 'P', 'Q', 
		'O', 'B', 'G', 0, 'M', 'X', 'V', 0 };

char figs[32] = { 0, '3', 0x0A, '-', ' ', 7, '8', '7', 
		0x0D, '$', '4', 0x27, ',', '!', ':', '(', 
		'5', '"', ')', '2', '#', '6', '0', '1', 
		'9', '?', '&', 0, '.', '/', ';', 0 };
*/

// this is easy, because ascii >> baudot
// Just keep track of LTRS/FIGS shift
char baudot_to_ascii(char b)
{ 
    char asc=0;
    if (b == 0x1B) {	// FIGS shift
	baudot_shift_rcv = FIGS;
	return(0);
    }
    if (b == 0x1F) { // LTRS shift
	baudot_shift_rcv = LTRS;
	return(0);
    }

    if ((confflags & CONF_UNSHIFT_ON_SPACE) && (b == 0x04) ) // space
      baudot_shift_rcv = LTRS;
    
    if (baudot_shift_rcv == LTRS) 
      asc = eeprom_read_byte(EEP_TABLES_START + (EEP_TABLE_SIZE * tableselector) + b);
    else if (baudot_shift_rcv == FIGS)
      asc = eeprom_read_byte(EEP_TABLES_START + (EEP_TABLE_SIZE * tableselector) + FIGS_OFFSET + b);

    return(asc);
} 

// harder, because baudot << ASCII. we track the
// LTRS/FIGS shift, and set the 6th bit in the output 
// if the user is going to need to send a shift code
// prior to sending the actual 5 bit baudot character.

char ascii_to_baudot(char c)
{ 
    uint8_t i;
    uint8_t needcase = 0;
    char b = 0;

    // search for the ascii char in both the letters and figs tables
    for(i=0; i<32; i++) { 
      if (eeprom_read_byte(EEP_TABLES_START + (EEP_TABLE_SIZE * tableselector) + i) == c) { 
	needcase = LTRS;  // we found it in the LTRS table
	b = i;
      } else if (eeprom_read_byte(EEP_TABLES_START + (EEP_TABLE_SIZE * tableselector) + FIGS_OFFSET + i) == c) { 
	needcase = FIGS; // we found it in the FIGS table
	b = i;
      }
    }
    // if called with a character that doesn't exist in Baudot,
    // or a null char, return a blank.
    if (b==0) return(0);

    if(needcase == baudot_shift_send) {  // we're already in the correct shift
	return(b);
    } else { 
	// signal the caller that it needs to transmit a shift character
	// before the baudot character
	b |= (1<<5); 
	baudot_shift_send = needcase;
	return(b);
    } 
} 

