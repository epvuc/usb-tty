This is an attempt to get a useful USB CDC ACM endpoint on AVR using the 
latest LUFA USB stack. Eventual target is atmega32u2 so I can use it on the
usb-to-tty loop adapter board, but i only have a mega32u4 right now. It
works on that and compiles cleanly for 32u2, so maybe it'll be ok. 

It needs the current LUFA distro from 
http://www.github.com/abcminiuser/lufa/archive/LUFA-151115.zip
unzipped into ../lufa

epv 2/11/2016

2/13/16
added softuart from usbtty2 project. 
Sending works fine. 
Receives the first char from the tty correctly, then freezes. 
The ISR softuart receive works perfectly - if you don't call softuart_getchar()
the buffer fills with correctly received characters and doesn't ever freeze. 

This was because i was a fool and forgot to #include "softuart.h" so the function
prototypes were wrong. 

