#avrdude -p atmega32u2 -P /dev/ttyACM0 -c stk500v2 -U hfuse:w:0xd0:m
#avrdude -p atmega32u2 -P /dev/ttyACM0 -c stk500v2 -U lfuse:w:0xde:m

avrdude -p atmega32u2 -P /dev/ttyACM0 -c stk500v2 -e -U hfuse:w:0xd0:m	
avrdude -p atmega32u2 -P /dev/ttyACM0 -c stk500v2 -e -U lfuse:w:0xde:m	
avrdude -p atmega32u2 -P /dev/ttyACM0 -c stk500v2 -e -U flash:w:BootloaderDFU-32u2.hex
