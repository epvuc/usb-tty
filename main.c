#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <avr/eeprom.h>
#include "lufa_serial.h"
#include "baudot.h"
#include "softuart.h"
#include "usb_serial_getstr.h"
#include "conf.h"
#define EEWRITE
#define CMDBUFLEN 64
// These are just tested values that will override specific entered values. You can
// set any value at all, and if it's not in this list, it will just use F_CPU/64/3/X. 
#define NSPEEDS 4
const uint16_t speeds[4][2] = { {45, 1833}, {50, 1667}, {56, 1464}, {75, 1123} };

// function protos
void help(void);
void commandline(void);
void ee_dump(void);
void ee_wipe(void);
void usbserial_tasks(void);
int tty_putchar(char c, FILE *stream);
void softuart_status(void);
uint16_t divisor_to_baud(uint16_t);
uint16_t baud_to_divisor(uint16_t);
void set_softuart_divisor(uint16_t);
void ee_write(char *);

// globals, clean this up. 
extern volatile unsigned char  flag_tx_ready;
extern volatile uint8_t framing_error;
volatile uint8_t host_break = 0;
uint8_t tableselector = 0; // which ascii/baudot translation table we're using
static char buf[CMDBUFLEN]; // command line input buf
uint16_t baudtmp;
uint8_t confflags = 0;
uint8_t saved;
static FILE USBSerialStream;
volatile uint8_t txbits=8, rxbits=5;

// LUFA CDC Class driver interface configuration and state information. stolen from droky@radikalbytes.com.com
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface = {
.Config = {
    .ControlInterfaceNumber         = 0,
    .DataINEndpoint = {
      .Address                = CDC_TX_EPADDR,
      .Size                   = CDC_TXRX_EPSIZE,
      .Banks                  = 1,
    },
    .DataOUTEndpoint = {
      .Address                = CDC_RX_EPADDR,
      .Size                   = CDC_TXRX_EPSIZE,
      .Banks                  = 1,
    },
    .NotificationEndpoint = {
      .Address                = CDC_NOTIFICATION_EPADDR,
      .Size                   = CDC_NOTIFICATION_EPSIZE,
      .Banks                  = 1,
    },
  },
};

int main(void)
{
  uint8_t column = 0, framing_error_last;
  char char_from_usb;
  char char_from_tty;
  uint16_t configured;

  eeprom_read_block(&configured, (const void *)EEP_CONFIGURED_LOCATION, (size_t)EEP_CONFIGURED_SIZE);
  
  SetupHardware(); // USB interface setup
  wdt_reset();
  softuart_init();
  // setup pins for softuart, led, etc. 
  DDRD |= _BV(0) | _BV(1) | _BV(6); // two leds and output to loop
  DDRD &= ~_BV(4); // input from loop
  DDRB &= ~_BV(7); // input from loop
  GlobalInterruptEnable();
  
  // Check for magic number in eeprom to see if unit has valid configuration. 
  if (configured != EEP_CONFIGURED_MAGIC) 
    ee_wipe();  // Make sure there are valid config settings, baud, and tables

  // Read saved config settings from eeprom. 
  eeprom_read_block(&confflags, (const void *)EEP_CONFFLAGS_LOCATION, (size_t)EEP_CONFFLAGS_SIZE);
  eeprom_read_block(&baudtmp, (const void *)EEP_BAUDDIV_LOCATION, (size_t)EEP_BAUDDIV_SIZE);
  set_softuart_divisor(baudtmp);
  tableselector = eeprom_read_byte(EEP_TABLE_SELECT_LOCATION);

  CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);
  stdin = stdout = &USBSerialStream; // so printf, etc go to usb serial.
  sei();
  // Here is a polling loop where we look for characters or events from either USB or TTY
  // and relay to the other side. Nothing in this loop should block. well, send_break() does
  // very briefly but it's ok. 

  while(1) { 
    // Have we been told to go into config mode?
    if (!(PINB & (1<<7)) ) { 
      softuart_turn_rx_off();
      commandline(); 
      softuart_turn_rx_on();
      column = 0;
    }

    // update rxbits/txbits for softuart between chars
    if (confflags & CONF_8BIT) { 
      rxbits = 8; txbits = 10; // ?? i dunno
    } else {
      rxbits = 5; txbits = 8;
    }
    // check for break condition
    if ((framing_error == 0) && (framing_error_last == 1)) 
      printf("[BREAK]\r\n"); // What should I actually do here? 
    framing_error_last = framing_error;

    // check if USB host is trying to send a break. 
    if (host_break == 1) { 
      send_break(); // actually break the loop for 500ms
      host_break = 0;
    }
    
    // Do we have a character received from USB, to send to the TTY loop?
    // Only pick a char from USB host if we're ready to process it. if not, it's the host's job to queue or block or whatever
    if (flag_tx_ready == 0) { 
      char_from_usb = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
      if (char_from_usb != 0xFF) { // CDC_Device_ReceiveByte() returns 0xFF when there's no char available.
	if (confflags & CONF_TRANSLATE) { 
	  // ASCII CR or LF ---> tty CR _and_ LF
	  if ((confflags & CONF_CRLF) && ((char_from_usb==0x0d) || (char_from_usb==0x0a))) {
	    tty_putchar('\r',0);
	    tty_putchar('\n',0);
	  } else
	    tty_putchar(char_from_usb,0);  
	  
	  // half-assed auto-CRLF. only works once we've seen the first newline
	  if((confflags & CONF_AUTOCR)) {
	    if(isprint(char_from_usb))
	      column++;
	    if((char_from_usb==0x0d) || (char_from_usb==0x0a))
	      column=0;
	    if(column >= 68) { // prob should be a config option
	      tty_putchar('\r',0);
	      tty_putchar('\n',0);
	      column = 0;
	    }
	  }
	} else { 
	  // we are in transparent mode, just pass the character through unchanged.
	  if (confflags & CONF_8BIT) 
	    tty_putchar_raw(char_from_usb);
	  else // not sure if i need to actually mask here, but let's be safe
	    tty_putchar_raw(char_from_usb & 0x1F);
	}
      }
      if (char_from_usb == '%') { // just for testing.
	softuart_turn_rx_off();
        commandline(); 
	softuart_turn_rx_on();
        column = 0;
      }
    }

    // Do we have a character from the TTY loop ready to send to USB?
    if (softuart_kbhit()) {
      if (confflags & CONF_TRANSLATE) 
	char_from_tty = baudot_to_ascii(softuart_getchar());
      else
	if (confflags & CONF_8BIT) 
	  char_from_tty = softuart_getchar();
	else
	  char_from_tty = softuart_getchar() & 0x1F; // masking may not be necessary
      if(char_from_tty != 0)
        usb_serial_putchar(char_from_tty);
    }

    // Process USB events. 
    CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
    USB_USBTask();
  }
}

void commandline(void)
{ 
  uint8_t n, valid;
  char *res=NULL;

  help();
  while(1) { 
    valid = 0;
    printf("cmd> ");
    n = usb_serial_getstr(buf, CMDBUFLEN-1);
    printf("\r\n");
    if (n==0) continue;
    res=strtok(buf, " ");
    
    if(strncmp(res, "help", 5) == 0) { 
      valid = 1;
      help();
    }

    if(strncmp(res, "exit", 5) == 0) { 
      valid = 1;
      printf_P(PSTR("Returning to adapter mode.\r\n"));
      return;
    }
    // save/load/show settings
    if(strncmp(res, "save", 5) == 0) { 
      valid = 1;
      eeprom_write_block(&confflags, (void *)EEP_CONFFLAGS_LOCATION, (size_t)EEP_CONFFLAGS_SIZE);
      baudtmp = OCR1A; 
      eeprom_write_block(&baudtmp, (void *)EEP_BAUDDIV_LOCATION, (size_t)EEP_BAUDDIV_SIZE);
      eeprom_write_byte(EEP_TABLE_SELECT_LOCATION, tableselector);
      printf_P(PSTR("Settings saved.\r\n"));
    }

    if(strncmp(res, "load", 5) == 0) { 
      valid = 1;
      eeprom_read_block(&confflags, (const void *)EEP_CONFFLAGS_LOCATION, (size_t)EEP_CONFFLAGS_SIZE);
      eeprom_read_block(&baudtmp, (const void *)EEP_BAUDDIV_LOCATION, (size_t)EEP_BAUDDIV_SIZE);
      set_softuart_divisor(baudtmp);
      tableselector = eeprom_read_byte(EEP_TABLE_SELECT_LOCATION);
      printf_P(PSTR("Settings loaded.\r\n"));
    }

    if(strncmp(res, "show", 5) == 0) { 
      valid = 1;
      eeprom_read_block(&saved, (const void *)EEP_CONFFLAGS_LOCATION, (size_t)EEP_CONFFLAGS_SIZE);
      eeprom_read_block(&baudtmp, (const void *)EEP_BAUDDIV_LOCATION, (size_t)EEP_BAUDDIV_SIZE);

      printf_P(PSTR("Settings:                                  Cur     Saved\r\n"));

      printf_P(PSTR("[no]translate   Translate ASCII/Baudot:    %c      %c\r\n"), 
	       (confflags & CONF_TRANSLATE)?'Y':'N', (saved & CONF_TRANSLATE)?'Y':'N');

      printf_P(PSTR("[no]crlf        CR or LF --> CR+LF:        %c      %c\r\n"), 
	       (confflags & CONF_CRLF)?'Y':'N', (saved & CONF_CRLF)?'Y':'N');

      printf_P(PSTR("[no]autocr      Auto CR at end of line:    %c      %c\r\n"), 
	       (confflags & CONF_AUTOCR)?'Y':'N', (saved & CONF_AUTOCR)?'Y':'N');

      printf_P(PSTR("[no]usos        Unshift on space:          %c      %c\r\n"), 
	       (confflags & CONF_UNSHIFT_ON_SPACE)?'Y':'N', (saved & CONF_UNSHIFT_ON_SPACE)?'Y':'N');

      printf_P(PSTR("[no]8bit        8bit mode:                 %c      %c\r\n"), 
	       (confflags & CONF_8BIT)?'Y':'N', (saved & CONF_8BIT)?'Y':'N');

      printf_P(PSTR("table N         Translation table number:  %u      %u\r\n"), 
               tableselector, eeprom_read_byte(EEP_TABLE_SELECT_LOCATION));

      printf_P(PSTR("baud N          Baud rate:                 %u     %u\r\n"), 
               divisor_to_baud(OCR1A), divisor_to_baud(baudtmp));
      
    }
	
    // confflags settings
    if(strncmp(res, "translate", 10) == 0) { 
      valid = 1;
      confflags |= CONF_TRANSLATE;
      printf_P(PSTR("Set to ASCII/Baudot translate mode.\r\n"));
    }
      
    if ((strncmp(res, "notranslate", 12) == 0) || (strncmp(res, "passthru", 9) == 0)) { 
      valid = 1;
      confflags &= ~CONF_TRANSLATE;
      printf_P(PSTR("Set to passthru mode.\r\n"));
    }

    if(strncmp(res, "crlf", 5) == 0) { 
      valid = 1;
      confflags |= CONF_CRLF;
      printf_P(PSTR("CR or LF --> CRLF.\r\n"));
    }
    if(strncmp(res, "nocrlf", 7) == 0) { 
      valid = 1;
      confflags &= ~CONF_CRLF;
      printf_P(PSTR("CR & LF independent.\r\n"));
    }

    if(strncmp(res, "8bit", 5) == 0) { 
      valid = 1;
      confflags |= CONF_8BIT;
      confflags &= ~CONF_TRANSLATE;
      printf_P(PSTR("8 bit mode for ascii machines.\r\n"));
    }
    if(strncmp(res, "no8bit", 7) == 0) { 
      valid = 1;
      confflags &= ~CONF_8BIT;
      // turning on 8bit mode forces translate mode off.
      // But on turning off 8bit mode, do we force translate mode on? I think it's better
      // to revert to whatever setting the user has previously saved. 
      eeprom_read_block(&saved, (const void *)EEP_CONFFLAGS_LOCATION, (size_t)EEP_CONFFLAGS_SIZE);
      if (saved & CONF_TRANSLATE) confflags |= CONF_TRANSLATE;
      printf_P(PSTR("normal mode for 5-level machines.\r\n"));
    }

    if(strncmp(res, "autocr", 7) == 0) { 
      valid = 1;
      confflags |= CONF_AUTOCR;
      printf_P(PSTR("Auto-CRLF at end of line.\r\n"));
    }
    if(strncmp(res, "noautocr", 6) == 0) { 
      valid = 1;
      confflags &= ~CONF_AUTOCR;
      printf_P(PSTR("No Auto-CRLF at end of line.\r\n"));
    }
    
    if(strncmp(res, "usos", 5) == 0) { 
      valid = 1;
      confflags |= CONF_UNSHIFT_ON_SPACE;
      printf_P(PSTR("Unshift-on-space enabled.\r\n"));
    }
    if(strncmp(res, "nousos", 7) == 0) { 
      valid = 1;
      confflags &= ~CONF_UNSHIFT_ON_SPACE;
      printf_P(PSTR("Unshift-on-space disabled.\r\n"));
    }
    
    if(strncmp(res, "baud", 5) == 0) { 
      valid = 1;
      res = strtok(NULL, " ");
      if (res != NULL) {
	baudtmp = baud_to_divisor(atoi(res));
	// if user entered a nonstandard baud rate, wing it.
	if (baudtmp == 0) {
	  printf_P(PSTR("Nonstandard baud rate selected, winging it.\r\n"));
	  baudtmp = F_CPU / 64 / 3 / (unsigned long)atoi(res);
	}
	printf_P(PSTR("Baud rate set to %s (divisor %u)\r\n"), res, baudtmp);
	set_softuart_divisor(baudtmp);
      } else  { 
	printf_P(PSTR("baud <45|50|56|75>\r\n"));
      }
    }
    
    if(strncmp(res, "table", 6) == 0) { 
      valid = 1;
      res = strtok(NULL, " ");
      if(res != NULL) { 
	tableselector = atoi(res);
	if ((tableselector < 0) || (tableselector > 6))  { 
	  printf_P(PSTR("Table numbers are 0 - 6; selecting 0.\r\n"));
	  tableselector = 0;
	}  else
	  printf_P(PSTR("Selected translation table #%u\r\n"), tableselector);
      } else
	  printf_P(PSTR("table <0-6>\r\n"));
    }

    if(strncmp(res, "eedump", 7) == 0) { 
      valid = 1;
      ee_dump();
    }

    if(strncmp(res, "eewipe", 7) == 0) { 
      valid = 1;
      ee_wipe();
    }
    
#ifdef EEWRITE
    if(strncmp(res, "eewrite", 8) == 0) { 
      valid = 1;
      res = strtok(NULL, " ");
      if(res != NULL) { 
	ee_write(res);
      } 
    }
#endif
    if(valid == 0)
      printf("No such command.\r\n");
  }
}

/** Configures the board hardware and chip peripherals */
void SetupHardware(void)
{
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);

	/* Hardware Initialization */
	USB_Init();
}

/*********************************************************
 **                                                     **
 **   USB Functions used from LUFA CDC Virtual Serial   **
 **                                                     **
 ********************************************************/

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;
	ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

void EVENT_CDC_Device_BreakSent(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo, uint8_t duration)
{
  if (duration > 0) 
    host_break = 1;
}

/** Event handler for the CDC Class driver Line Encoding Changed event.
 *
 *  \param[in] CDCInterfaceInfo  Pointer to the CDC class interface configuration structure being referenced
 */
void EVENT_CDC_Device_LineEncodingChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
#if 0
	uint8_t ConfigMask = 0;

	switch (CDCInterfaceInfo->State.LineEncoding.ParityType)
	{
		case CDC_PARITY_Odd:
			ConfigMask = ((1 << UPM11) | (1 << UPM10));
			break;
		case CDC_PARITY_Even:
			ConfigMask = (1 << UPM11);
			break;
	}

	if (CDCInterfaceInfo->State.LineEncoding.CharFormat == CDC_LINEENCODING_TwoStopBits)
	  ConfigMask |= (1 << USBS1);

	switch (CDCInterfaceInfo->State.LineEncoding.DataBits)
	{
		case 6:
			ConfigMask |= (1 << UCSZ10);
			break;
		case 7:
			ConfigMask |= (1 << UCSZ11);
			break;
		case 8:
			ConfigMask |= ((1 << UCSZ11) | (1 << UCSZ10));
			break;
	}

        // printf("\r\n[serial change to %lu bps]\r\n", CDCInterfaceInfo->State.LineEncoding.BaudRateBPS);
	/* Must turn off USART before reconfiguring it, otherwise incorrect operation may occur */
	UCSR1B = 0;
	UCSR1A = 0;
	UCSR1C = 0;

	/* Set the new baud rate before configuring the USART */
	UBRR1  = SERIAL_2X_UBBRVAL(CDCInterfaceInfo->State.LineEncoding.BaudRateBPS);

	/* Reconfigure the USART in double speed mode for a wider baud rate range at the expense of accuracy */
	UCSR1C = ConfigMask;
	UCSR1A = (1 << U2X1);
	UCSR1B = ((1 << RXCIE1) | (1 << TXEN1) | (1 << RXEN1));
#endif
}

void ee_dump(void)
{
  uint16_t i;
  for (i=0; i<1024; i++) {
    if (!(i % 16))
      printf("\r\n%04x ", i);
    printf("%02x ", eeprom_read_byte((const uint8_t *)i));
  }
  printf("\r\n");
}

void ee_wipe(void)
{
  uint16_t i;

  // this is big waste of space. 
  static char ltrs[32] = { 0, 'E', 0x0A, 'A', ' ', 'S', 'I', 'U',
		    0x0D, 'D', 'R', 'J', 'N', 'F', 'C', 'K',
		    'T', 'Z', 'L', 'W', 'H', 'Y', 'P', 'Q',
		    'O', 'B', 'G', 0, 'M', 'X', 'V', 0 };

  static char figs[32] = { 0, '3', 0x0A, '-', ' ', 7, '8', '7',
		    0x0D, '$', '4', 0x27, ',', '!', ':', '(',
		    '5', '"', ')', '2', '#', '6', '0', '1',
		    '9', '?', '&', 0, '.', '/', ';', 0 };
  
  memset(buf, 0xFF, 64);
  for (i=0; i<128; i=i+64) { // only wipe first 128 bytes for now
    usb_serial_putchar('.');
    eeprom_write_block(buf, (void *)i, (size_t)64);
  }
  // put in some sane defaults or it will hang on next boot.
  i = 1833; // 45.45 baud
  eeprom_write_block(&i, (void *)EEP_BAUDDIV_LOCATION, (size_t)EEP_BAUDDIV_SIZE);
  i = CONF_TRANSLATE | CONF_CRLF;
  eeprom_write_block(&i, (void *)EEP_CONFFLAGS_LOCATION, (size_t)EEP_CONFFLAGS_SIZE);
  // install the default ascii/baudot translation table in eeprom, for convenience
  eeprom_write_block(&ltrs, (void *)EEP_TABLES_START, FIGS_OFFSET);
  eeprom_write_block(&figs, (void *)(EEP_TABLES_START+FIGS_OFFSET), FIGS_OFFSET);
  eeprom_write_byte(EEP_TABLE_SELECT_LOCATION, 0);
  i = 0x4545;  // magic number to indicate device has a configuration
  eeprom_write_block(&i, (void *)EEP_CONFIGURED_LOCATION, (size_t)EEP_CONFIGURED_SIZE);

  
printf("\r\n");
}


void usbserial_tasks(void)
{ 
  CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
  USB_USBTask();
}

uint16_t divisor_to_baud(uint16_t divisor)
{
  uint8_t i;
  uint16_t baud = 0;
  for(i=0; i<NSPEEDS; i++) { 
    if (speeds[i][1] == divisor)
      baud = speeds[i][0];
  }
  if (baud == 0)
    baud = F_CPU / 64 / 3 / divisor;
  return baud;
}

uint16_t baud_to_divisor(uint16_t baud)
{
  uint8_t i;
  uint16_t divisor = 0;
  for(i=0; i<NSPEEDS; i++) { 
    if (speeds[i][0] == baud)
      divisor = speeds[i][1];
  }
  return divisor;
}

void set_softuart_divisor(uint16_t divisor)
{ 
  TCNT1 = 0;
  OCR1A = divisor;
} 

void help(void)
{ 
      printf_P(PSTR("\r\nCommands available:\r\nhelp, baud, table, [no]translate, [no]usos, [no]autocr, [no]8bit, save, load, show, exit\r\n"));
}

#ifdef EEWRITE
uint8_t unhex(char h, char l)
{
  if(h > 70) h -= 32;
  if(h > 57) h -= 7;
  if(l > 70) l -= 32;
  if(l > 57) l -= 7;
  return ((h-48)<<4)+(l-48);
}

bool ishexchar(char c)
{ 
  if (c>70) c -= 32;
  if (c<48) return (FALSE);
  if (c>70) return (FALSE);
  return (TRUE);
}

// leave this undocumented, it's not very safe, but might be useful someday
// eewrite XXXX YY YY YY YY YY YY YY YY -- write bytes sequentially starting at XXXX
void ee_write(char *buf)
{ 
  uint8_t i, j;
  uint16_t eeaddr;
  eeaddr = (unhex(buf[0], buf[1])<<8) + unhex(buf[2], buf[3]);
  buf += 5; // leave a space after addr.
  j=0;
  for(i=0; 
      ishexchar(buf[i]) && ishexchar(buf[i+1]) && i < strnlen(buf, CMDBUFLEN); 
      i=i+3) {  // skip a space after each byte
    printf_P(PSTR("%u (%04x): %u (%02X)\r\n"), eeaddr + j, eeaddr + j,
	   unhex(buf[i], buf[i+1]), unhex(buf[i], buf[i+1]));
    eeprom_write_byte(eeaddr + j, unhex(buf[i], buf[i+1]));
    j++;
  }
}
#endif
