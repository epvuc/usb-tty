To work with my board, the default avr dfu bootloader has to be replaced
with a different one that doesn't need HWB brought out. Fine on teensy.

# erase chip and change lockbyte from 0x2c to 0x2f
avrdude -p atmega32u2 -P /dev/ttyACM0 -c stk500v2 -e -U lock:w:0x2f:m
# write LUFA's CDC bootloader to boot area
avrdude -p atmega32u2 -P /dev/ttyACM0 -c stk500v2 -U flash:w:BootloaderCDC.hex
# set BOOTRST bit by changing hfuse from 0xD9 to 0xD8, so bootloader always
# starts on powerup and decides for itself whether to jump to user app or not.
avrdude -p atmega32u2 -P /dev/ttyACM0 -c stk500v2 -e -U hfuse:w:0xd8:m
# Program the actual application to the chip using the bootloader (avr109 type)
# -- don't do it with the hardware programmer because it'll wipe the bootloader.
make install

-------------------------


This is an attempt to get a useful USB CDC ACM endpoint on AVR using the 
latest LUFA USB stack. Eventual target is atmega32u2 so I can use it on the
usb-to-tty loop adapter board. Compiles and works correctly on both 32u2 and 32u4.

It needs the current LUFA distro from 
http://www.github.com/abcminiuser/lufa/archive/LUFA-151115.zip
unzipped into ../lufa

epv 2/11/2016

2/13/16
added softuart from usbtty2 project. 
Sending and receiving work fine. 

added test stuff to switch to and from commandline mode using "%" 

still need to do:

1. actual config mode:  (done)

translating vs transparent mode, so you can use it with HeavyMetal in place of cp2102
enable/disable auto-cr, cr->crlf
set auto-cr column? 
select character set maps?
speed? Might be able to set baud rate just by setting OCR1A = 83333 / baudrate

16000000 / 64 / (3 * 45.45) = 1833 // 60 WPM
16000000 / 64 / (3 * 50.00) = 1667 // 66 WPM
16000000 / 64 / (3 * 56.90) = 1464 // 75 WPM
16000000 / 64 / (3 * 74.20) = 1123 // 100 WPM

Maybe a setting for inverting or not inverting the RX and/or TX sense? 

2. implement unshift on space, i think

3. switch to/from config mode by hardware button vs "%" key. 
