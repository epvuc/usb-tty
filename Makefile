MCU          = atmega16u2
ARCH         = AVR8
BOARD        = USER
F_CPU        = 16000000
F_USB        = $(F_CPU)
OPTIMIZATION = s
TARGET       = main
SRC          = $(TARGET).c baudot.c softuart.c usb_serial_getstr.c Descriptors.c $(LUFA_SRC_USB) $(LUFA_SRC_USBCLASS)
LUFA_PATH    = ../lufa/LUFA
CC_FLAGS     = -DUSE_LUFA_CONFIG_HEADER -IConfig/
# LD_FLAGS     = -Wl,-u,vfprintf -lprintf_min  # use minimal printf library which is limited but way smaller
CC	     = avr-gcc

CPP	     = avr-g++

# Default target
all:

program: $(TARGET).hex
	avrdude -p $(MCU) -P /dev/ttyACM0  -c avr109    -U flash:w:$(TARGET).hex

pteensy: $(TARGET).hex
	teensy_loader_cli -mmcu=$(MCU) -w -v $(TARGET).hex

dfu: $(TARGET).hex
	dfu-programmer $(MCU) erase
	dfu-programmer $(MCU) flash main.hex
	dfu-programmer $(MCU) reset

# Include LUFA build script makefiles
include $(LUFA_PATH)/Build/lufa_core.mk
include $(LUFA_PATH)/Build/lufa_sources.mk
include $(LUFA_PATH)/Build/lufa_build.mk
include $(LUFA_PATH)/Build/lufa_cppcheck.mk
include $(LUFA_PATH)/Build/lufa_doxygen.mk
include $(LUFA_PATH)/Build/lufa_dfu.mk
include $(LUFA_PATH)/Build/lufa_hid.mk
include $(LUFA_PATH)/Build/lufa_avrdude.mk
include $(LUFA_PATH)/Build/lufa_atprogram.mk
