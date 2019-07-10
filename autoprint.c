#ifdef INCLUDE_AUTOPRINT
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "baudot.h"
#include "conf.h"
#include "main.h"
#include "usb_serial_getstr.h"

// these are so do_autoprint() can keep processing usb events while
// it spends potentially a long time printing text to the tty
#include "lufa_serial.h"
extern USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface;

void do_autoprint(void)
{
  char c;
  uint16_t i;
  softuart_turn_rx_off();
  tty_putchar('\r', 0);
  tty_putchar('\n', 0);
  tty_putchar(64,0); // try to force known ltrs state
  tty_putchar_raw(31); // set ltrs
  for(i=0; i<512; i++) {
    c = eeprom_read_byte(i+0x200);
    if (c == 0xff) break;
    tty_putchar(c,0);
    if (c == '\r')
      tty_putchar('\n', 0);
    CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
    USB_USBTask();
  }
  tty_putchar('\r', 0);
  tty_putchar('\n', 0);
  softuart_turn_rx_on();
}

void create_automsg(void)
{
  uint8_t n;
  uint16_t addr = 512;
  static char linebuf[80];
  uint16_t i;

  printf_P(PSTR("enter up to 512 bytes. EOF at beginning of line to finish.\r\n"));
  while(1) {
    printf("> ");
    n = usb_serial_getstr(linebuf, 79);
    printf("\r\n");
    if (strncmp(linebuf, "EOF", 3) == 0)
      break;

    for(i=0; i<n; i++) {
      eeprom_write_byte(addr, linebuf[i]);
      addr++;
      if (addr >=1021) break;
    }
    eeprom_write_byte(addr, '\r');
    addr++;

  }    
  eeprom_write_byte(addr, 0xff);
  printf_P(PSTR("end of message.\r\n"));
}
#endif
