#include <string.h>
#include <stdio.h>
#include "lufa_serial.h"
#include "baudot.h"
#include "softuart.h"

#define SU_FALSE 0

// config flags
#define CONF_CRLF 0x01
#define CONF_AUTOCR 0x02
#define CONF_UNSHIFT_ON_SPACE 0x04
#define CONF_TRANSLATE 0x08

#define EEPROM_SECTION  __attribute__ ((section (".eeprom")))
uint16_t dummy EEPROM_SECTION = 0; // voodoo, i dunno
uint8_t eep_confflags EEPROM_SECTION = 0;
uint16_t eep_baudtimer EEPROM_SECTION = 1833;

uint8_t confflags; 

void ee_dump(void);
void usbserial_tasks(void);
int tty_putchar(char c, FILE *stream);
void softuart_status(void);

int16_t received;
uint8_t command, channel, data2;
int status;

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

static char buf[64]; // command line input buf
static FILE USBSerialStream;
extern volatile unsigned char  flag_tx_ready;

uint8_t confflags = 0;
uint8_t saved;
uint16_t saved_baudtimer;

int main(void)
{
  uint8_t column = 0;
  int16_t c;
  char in_char;

  // Read saved config settings from eeprom. 
//  eeprom_read_block(&confflags, eep_confflags, sizeof(eep_confflags));
 // eeprom_read_block(&OCR1A, eep_baudtimer, sizeof(eep_baudtimer)); 

  SetupHardware(); // USB interface setup
  wdt_reset();
  softuart_init();
  // setup pins for softuart, led, etc. 
  DDRD |= _BV(6) | _BV(3);

  GlobalInterruptEnable();
  CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);
  stdin = stdout = &USBSerialStream; // so printf, etc go to usb serial.
  sei();
  while(1) { 
    // Have we been told to go into config mode?
    //    if (PORTB & (1<<1)) 
    //      commandline();

    // Do we have a character received from USB, to send to the TTY loop?
    if (flag_tx_ready == SU_FALSE) { // Only pick a char from USB host if we're ready to process it.
      c = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
      if (c > 0) { 
	if (confflags & CONF_TRANSLATE) { 
	  // ASCII CR or LF ---> tty CR _and_ LF
	  if ((confflags & CONF_CRLF) && ((c==0x0d) || (c==0x0a))) {
	    tty_putchar('\r',0);
	    tty_putchar('\n',0);
	  } else
	    tty_putchar(c,0);
	  
	  // half-assed auto-CRLF. only works once we've seen the first newline
	  if((confflags & CONF_AUTOCR)) {
	    if(isprint(c))
	      column++;
	    if((c==0x0d) || (c==0x0a))
	      column=0;
	    if(column >= 68) {
	      tty_putchar('\r',0);
	      tty_putchar('\n',0);
	      column = 0;
	    }
	  }
	} else { 
	  // we are in transparent mode, just pass the character through unchanged.
	  tty_putchar_raw(c & 0x1F);
	}
      }
      if (c == '%') { 
        commandline(); // just for testing.
        column = 0;
      }
    }
    // Do we have a character from the TTY loop ready to send to USB?
    if (softuart_kbhit()) {
      if (confflags & CONF_TRANSLATE) 
	in_char = baudot_to_ascii(softuart_getchar());
      else
	in_char = softuart_getchar() & 0x1F;
      if(in_char != 0)
        usb_serial_putchar(in_char);
    }

    // Process USB events. 
    CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
    USB_USBTask();
  }
}

void commandline(void)
{ 
  uint8_t n, valid;
  uint16_t baudtmp;
  char *res=NULL;

  while(1) { 
    valid = 0;
    printf("heepy> ");
    n = usb_serial_getstr(buf, 127);
    printf("\r\n");
    if (n==0) continue;
    res=strtok(buf, " ");
    
    if(strncmp(res, "help", 5) == 0) { 
      valid = 1;
      printf_P(PSTR("This is ssh://eric@limpoc.com:/home/eric/git/lufa_serial.git\r\n"));
      printf_P(PSTR("It is an experiment in using LUFA's CDC ACM class on atmega32u2\r\nand atmega32u4 in preparation for trying to port the USB-to-teletype\r\nadapter code from pjrc's cdc acm code to LUFA, so that I can run it\r\non the 32u2 chip I designed the adapter board for.\r\n")); 
      printf_P(PSTR("\r\nCommands available:\r\nhelp, 60wpm, 100wpm, [no]translate, [no]usos, [no]autocrlf, save, load, show, exit\r\n"));
    }

    if(strncmp(res, "exit", 5) == 0) { 
      valid = 1;
      printf_P(PSTR("Returning to adapter mode.\r\n"));
      return;
    }
    // save/load/show settings
    if(strncmp(res, "save", 5) == 0) { 
      valid = 1;
      eeprom_write_block(&confflags, eep_confflags, sizeof(eep_confflags));
      baudtmp = OCR1A; 
      eeprom_write_block(&baudtmp, eep_baudtimer, sizeof(eep_baudtimer));
      printf_P(PSTR("Conf saved to eeprom.\r\n"));
    }

    if(strncmp(res, "load", 5) == 0) { 
      valid = 1;
      eeprom_read_block(&confflags, eep_confflags, sizeof(confflags));
      eeprom_read_block(&baudtmp, eep_baudtimer, sizeof(eep_baudtimer));
      OCR1A = baudtmp;
      printf_P(PSTR("Conf read from eeprom.\r\n"));
    }

    if(strncmp(res, "show", 5) == 0) { 
      valid = 1;
      eeprom_read_block(&saved, eep_confflags, sizeof(saved));
      eeprom_read_block(&baudtmp, eep_baudtimer, sizeof(eep_baudtimer));
      printf("confflags=%02x, saved=%02x, baudtimer=%04x, saved_baudtimer=%04x\r\n", 
        confflags, saved, OCR1A, baudtmp);

      printf_P(PSTR("Settings:               Cur     Saved\r\n"));

      printf_P(PSTR("Translate ASCII/Baudot: %c      %c\r\n"), 
	       (confflags & CONF_TRANSLATE)?'Y':'N', (saved & CONF_TRANSLATE)?'Y':'N');

      printf_P(PSTR("CR or LF --> CR+LF:     %c      %c\r\n"), 
	       (confflags & CONF_CRLF)?'Y':'N', (saved & CONF_CRLF)?'Y':'N');

      printf_P(PSTR("Auto CR at end of line: %c      %c\r\n"), 
	       (confflags & CONF_AUTOCR)?'Y':'N', (saved & CONF_AUTOCR)?'Y':'N');

      printf_P(PSTR("Unshift on space:       %c      %c\r\n"), 
	       (confflags & CONF_UNSHIFT_ON_SPACE)?'Y':'N', (saved & CONF_UNSHIFT_ON_SPACE)?'Y':'N');

      printf_P(PSTR("Baud rate:              %lu   %lu\r\n"), 83333L/(unsigned long)OCR1A, 83333L/(unsigned long)baudtmp);
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
    
    
    if(strncmp(res, "60wpm", 6) == 0) { 
      valid = 1;
      OCR1A = 1833;
      printf_P(PSTR("Speed set to 60wpm / 45 baud.\r\n"));
    }
    
    if(strncmp(res, "100wpm", 7) == 0) { 
      valid = 1;
      OCR1A = 1123;
      printf_P(PSTR("Speed set to 100wpm / 75 baud.\r\n"));
    }
    
    if(strncmp(res, "eedump", 7) == 0) { 
      valid = 1;
      ee_dump();
    }
    
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
    printf("\r\n[%ums BREAK]\r\n", duration);
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
    printf("%02x ", eeprom_read_byte(i));
  }
  printf("\r\n");
}


void usbserial_tasks(void)
{ 
  CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
  USB_USBTask();
}

