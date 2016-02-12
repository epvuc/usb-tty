#include <stdint.h>

char usb_serial_getchar(void);
void usb_serial_putchar(char);


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

