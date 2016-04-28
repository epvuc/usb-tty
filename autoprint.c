#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <avr/eeprom.h>
#include "baudot.h"
#include "conf.h"

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
