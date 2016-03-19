quickstart: 

signal from loop is received on PD4, signal toward loop output on PD6
LED indicators are on D0 and D1, currently D0 shows outbound data and
D1 shows inbound. 

To work with custom board, the default avr dfu bootloader has to be replaced
with a different one that doesn't need HWB brought out. 

Set the fuses: 

avrdude -p atmega16u2 -P /dev/ttyACM0 -c stk500v2 -e -U hfuse:w:0xd0:m	
avrdude -p atmega16u2 -P /dev/ttyACM0 -c stk500v2 -e -U lfuse:w:0xde:m	

Install the bootloader:

cd ~/git/avr-lufa/lufa/Bootloaders/CDC/
make clean; make
Connect ISP programmer and run: 
avrdude -p atmega16u2 -P /dev/ttyACM0 -c stk500v2 -e -U flash:w:BootloaderCDC.hex

Install this software:
Remove ISP programmer, connect only USB cable.
cd ~/git/avr-lufa/lufa_serial/
make clean; make
make program

If you program it using the ISP programmer afterward, you'll need to reinstall the
bootloader too. 

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

2. implement unshift on space, i think (done)

3. switch to/from config mode by hardware button vs "%" key. 
