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

void ee_dump(void);
void usbserial_tasks(void);
int tty_putchar(char c, FILE *stream);
void softuart_status(void);

int16_t received;
uint8_t command, channel, data2;
int status;

/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another. stolen from droky@radikalbytes.com.com
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
	{
		.Config =
			{
				.ControlInterfaceNumber         = 0,
				.DataINEndpoint                 =
					{
						.Address                = CDC_TX_EPADDR,
						.Size                   = CDC_TXRX_EPSIZE,
						.Banks                  = 1,
					},
				.DataOUTEndpoint                =
					{
						.Address                = CDC_RX_EPADDR,
						.Size                   = CDC_TXRX_EPSIZE,
						.Banks                  = 1,
					},
				.NotificationEndpoint           =
					{
						.Address                = CDC_NOTIFICATION_EPADDR,
						.Size                   = CDC_NOTIFICATION_EPSIZE,
						.Banks                  = 1,
					},
			},
	};


static char buf[64];
static FILE USBSerialStream;

extern volatile unsigned char  flag_tx_ready;
uint8_t confflags=CONF_CRLF;


int main(void)
{
  uint8_t column = 0;
  int16_t c;
  char in_char;

  SetupHardware();
  wdt_reset();

  softuart_init();
  // setup pins for softuart, led, etc. 
  DDRD |= _BV(6) | _BV(3) | _BV(2);

  GlobalInterruptEnable();
  CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);
  stdin = stdout = &USBSerialStream;
  sei();
  while(1) { 
    // Have we been told to go into config mode?
    //    if (PORTB & (1<<1)) 
    //      commandline();

    // Do we have a character received from USB, to send to the TTY loop?
    if (flag_tx_ready == SU_FALSE) { 
      c = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
      if (c > 0) { 
	// ASCII CR or LF ---> tty CR _and_ LF
	// comment this out if you don't want that.
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
      }
      if (c == '%') { 
        commandline(); // just for testing.
        column = 0;
      }
    }
    // Do we have a character from the TTY loop ready to send to USB?
    if (softuart_kbhit()) {
      in_char=baudot_to_ascii(softuart_getchar());
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
      printf_P(PSTR("\r\nCommands available:\r\nhelp, wallaby, eedump, 60wpm, 100wpm, exit\r\n"));
    }

    if(strncmp(res, "exit", 5) == 0) { 
      valid = 1;
      printf_P(PSTR("Returning to adapter mode.\r\n"));
      return;
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
    
    if(strncmp(res, "wallaby", 8) == 0) { 
      valid = 1;
      for (n=1; n<6; n++) { 
	printf("%u. This is a kind of dry baby melon powder.\r\n", n);
	usbserial_tasks(); // make sure the whole thing makes it out via USB before the delay. 
	_delay_ms(750);
      }
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

