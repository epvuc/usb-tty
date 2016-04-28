#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "baudot.h"
#include "conf.h"
#include "main.h"
extern char *buf;

void do_autoprint(void)
{
  char c;
  uint16_t i;
  for(i=0; i<512; i++) {
    c = eeprom_read_byte(i);
    if (c == 0xff) break;
    tty_putchar(c,0);
  }
  tty_putchar('\r', 0);
  tty_putchar('\n', 0);
}

void create_automsg(void)
{
  uint8_t n;
  uint16_t addr = 512;
  uint16_t i;
  
  printf_P(PSTR("enter up to 512 bytes. EOF at beginning of line to finish.\r\n"));
  while(1) {
    printf("> ");
    n = usb_serial_getstr(buf, CMDBUFLEN-1);
    printf("\r\n");
    if (strncmp(buf, "EOF", 3) == 0)
      break;
    for(i=0; i<strnlen(buf, CMDBUFLEN); i++) {
      eeprom_write_byte(addr, buf[i]);
      addr++;
      if (addr >=510) break;
    }
  }    
  printf_P(PSTR("end of message.\r\n"));
}
