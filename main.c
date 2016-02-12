/**
 * Copyright (c) 2012 Alfredo Prado <droky@radikalbytes.com.com>. All rights reserved.
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdio.h>
#include "CDC_LUFA.h"



void ee_dump(void);
void usbserial_tasks(void);
int16_t received;
uint8_t command, channel, data2;
int status;

/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
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


/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */

static char buf[128];
static FILE USBSerialStream;

int main(void)
{
  uint8_t n, valid;
  char *res=NULL;
  SetupHardware();
  wdt_reset();
  GlobalInterruptEnable();
  CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);
  stdin = stdout = &USBSerialStream;

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
      printf_P(PSTR("Commands available:\r\nhelp, wallaby, eedump\r\n"));
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

	/* LEDs */
	DDRC |= (1<<6); //PC6 MIDI
	DDRC |= (1<<7); //PC7 USB

	/* LEDs OFF */
	MIDI_LED_OFF();
	USB_LED_OFF();

	/* Hardware Initialization */
	USB_Init();
	MIDI_LED_ON();
	_delay_ms(150);
	MIDI_LED_OFF();

}



/*********************************************************
 **                                                     **
 **   USB Functions used from LUFA CDC Virtual Serial   **
 **                                                     **
 ********************************************************/

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
	USB_LED_ON();
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
	USB_LED_OFF();
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;
	uint8_t ff;
	ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
	for (ff=0;ff<6;ff++)
	{
			MIDI_LED_ON();
			_delay_ms(25);
			MIDI_LED_OFF();
			_delay_ms(25);
	}

}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

void EVENT_CDC_Device_BreakSent(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo, uint8_t duration)
{
  printf("\r\n[%ums BREAK]\r\n");
}

/** Event handler for the CDC Class driver Line Encoding Changed event.
 *
 *  \param[in] CDCInterfaceInfo  Pointer to the CDC class interface configuration structure being referenced
 */
void EVENT_CDC_Device_LineEncodingChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
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

