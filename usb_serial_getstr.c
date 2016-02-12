#include <stdint.h>
#include "CDC_LUFA.h"

char usb_serial_getchar(void);
void usb_serial_putchar(char);

extern USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface;

void usb_serial_putchar(char c)
{
  CDC_Device_SendByte(&VirtualSerial_CDC_Interface, (uint8_t)c);
  CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
  USB_USBTask();       
  // _delay_us(50);
}

char usb_serial_getchar(void)
{
  while(1) {
    int16_t data1 = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
    if(!(data1<0))
      return(data1);

    CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
    USB_USBTask();
  }
}

int usb_serial_getstr(char *str, int count)
{ 
  int i=0, j;
  uint8_t c;
  
  while (i<count) {
    c = usb_serial_getchar();
    if ((c==10) || (c==13)) break; // cr/lf
    if (c == 0x15) {    // ^U
      if (i>0) { 
	for(j=0; j<i; ++j) { 
	  usb_serial_putchar(8);
	  usb_serial_putchar(32);
	  usb_serial_putchar(8);
	}
	i = 0;
      }
    } else if (c==127) { // backspace
      if (i > 0) { 
	i--;
	usb_serial_putchar(8);
	usb_serial_putchar(32);
	usb_serial_putchar(8);
      }
    } else { 
      usb_serial_putchar(c);
      str[i] = c;
      i++;
    }
  }
  str[i]=0;
  return(i);
}

