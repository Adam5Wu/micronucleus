# Name: Makefile
# Project: Micronucleus
# Author: Jenna Fox; portions by Christian Starkjohann, Louis Beaudoin
# Creation Date: 2007-12-10
# Tabsize: 4
# Copyright: (c) 2007 by OBJECTIVE DEVELOPMENT Software GmbH
# License: GNU GPL v2 (see License.txt)

CONFIG ?= t85_export
#CONFIG ?= t85_default
#CONFIG ?= t85_aggressive
#CONFIG ?= t841_default
#CONFIG ?= t167_default

###############################################################################
# Configure the following variables according to your AVR.
# Program the device with
#     make fuse    			# to set the clock generator, boot section size etc.
#     make flash   			# to load the boot loader into flash
#     make disablereset		# use external reset line for IO (CAUTION: this is not easy to enable again, see README) 

CFLAGS =
CONFIGPATH	  = configuration/$(CONFIG)
include $(CONFIGPATH)/Makefile.inc

PROGRAMMER ?= -c usbtiny
# PROGRAMMER contains AVRDUDE options to address your programmer

# Tools:
AVRDUDE = avrdude $(PROGRAMMER) -p $(DEVICE) 
CC = avr-gcc

ifeq ($(BOOTLOADER_ADDRESS),0)
	BOOTLOADER_CALC=1
	BOOTLOADER_ADDRESS=1000
	BOOTLOADER_DATA=200
	BOOTLOADER_EXPORTS=1F00
endif

# Options:
CFLAGS += -I. -g2 -Os # -Wall
CFLAGS += -I$(CONFIGPATH) -mmcu=$(DEVICE) -DF_CPU=$(F_CPU)
CFLAGS += -DBOOTLOADER_ADDRESS=0x$(BOOTLOADER_ADDRESS)
CFLAGS += -nostartfiles -ffunction-sections -fdata-sections -fpack-struct -fno-inline-small-functions -fno-move-loop-invariants -fno-tree-scev-cprop

LDFLAGS = -Wl,--relax,--section-start=.text=$(BOOTLOADER_ADDRESS),--gc-sections,-Map=main.map

# prepare global data to enable exports
CFLAGS += -DBOOTLOADER_DATA=0x$(BOOTLOADER_DATA)
LDFLAGS += -Wl,--undefined=__exports
# place the data above the stack
LDFLAGS += -Wl,--section-start=.data=0x$(shell echo "obase=16; ibase=16; scale=0; $(BOOTLOADER_DATA)+800000" | bc)

# place the exports at end of flash
LDFLAGS += -Wl,--section-start=.exports=0x$(BOOTLOADER_EXPORTS)

OBJECTS = crt1.o usbdrv/usbdrvasm.o usbdrv/oddebug.o main.o 
OBJECTS += osccalASM.o

# symbolic targets:
all: main.hex

.c.o:
	@$(CC) $(CFLAGS) -c $< -o $@ -Wa,-ahls=$<.lst

.S.o:
	@$(CC) $(CFLAGS) -x assembler-with-cpp -c $< -o $@
# "-x assembler-with-cpp" should not be necessary since this is the default
# file type for the .S (with capital S) extension. However, upper case
# characters are not always preserved on Windows. To ensure WinAVR
# compatibility define the file type manually.

.c.s:
	@$(CC) $(CFLAGS) -S $< -o $@

flash:	all $(if $(SERIAL),serial.hex)
	$(AVRDUDE) -U flash:w:main.hex:i $(if $(SERIAL),-U flash:w:serial.hex:i) -B 20

.PHONY: serial.hex
.INTERMEDIATE: serial.hex
serial.hex:
	@(echo "\":02\" ; obase=16; scale=0; a=$(FLASHEND)-1; \
	a / 4096 % 16; a / 256 % 16; a / 16 % 16; a % 16; \"00\" ;\
	d=$(SERIAL); (d2 = d / 16 % 16); (d3 = d % 16); \
	(d0 = d / 4096 % 16); (d1 = d / 256 % 16); \
	(c = (8*256 - 2 - a / 256 - a % 256 - d / 256 - d % 256) % 256) / 16; \
	c % 16" | bc | tr -d '\n' ; \
	echo ; echo :00000001FF) > $@

readflash:
	$(AVRDUDE) -U flash:r:read.hex:i -B 20

fuse:
	$(AVRDUDE) $(FUSEOPT) -B 20
	
disablereset:
	$(AVRDUDE) $(FUSEOPT_DISABLERESET) -B 20

read_fuses:
	$(UISP) --rd_fuses -B 20

clean:
	@rm -f main.hex main.bin main.c.lst main.map main.raw *.o usbdrv/*.o main.s usbdrv/oddebug.s usbdrv/usbdrv.s main.lss

# file targets:
main.bin:	$(OBJECTS)
	@echo "Building Micronucleus configuration: $(CONFIG)"
ifndef BOOTLOADER_CALC
	@echo "BOOTLOADER_ADDRESS = 0x$(BOOTLOADER_ADDRESS)"
	@echo "BOOTLOADER_DATA = 0x$(BOOTLOADER_DATA)"
	@echo "BOOTLOADER_EXPORTS = 0x$(BOOTLOADER_EXPORTS)"
endif
	@$(CC) $(CFLAGS) -o main.bin $(OBJECTS) $(LDFLAGS)
ifdef BOOTLOADER_CALC
	@echo "Calculating addresses..." ; avr-size $@ ;\
	text=`avr-size $@ | tail -n 1 | while read size dummy; do echo $$size; done` ;\
	tfit=$$(( ($$text+$(SPM_PAGESIZE)-1)/$(SPM_PAGESIZE)*$(SPM_PAGESIZE) )) ;\
	echo -n "Bootloader text $$tfit ($$(( $$tfit-$$text )) bytes padding), " ;\
	addr=`echo "obase=16; scale=0; $(FLASHEND)+1-$$tfit" | bc` ;\
	echo "address 0x$$addr" ;\
	data=`avr-size $@ | tail -n 1 | while read dummy0 size0 size1 dummy1; do echo $$(( $$size0+$$size1 )); done` ;\
	daddr=`echo "obase=16; scale=0; $(RAMEND)+1-$$data" | bc` ;\
	echo "Bootloader data $$data bytes, address 0x$$daddr" ;\
	exp=`avr-size -A $@ | grep ^.exports | while read section size dummy; do echo $$size; done` ;\
	expaddr=`echo "obase=16; scale=0; $(FLASHEND)+1-$$exp" | bc` ;\
	echo "Bootloader exports $$exp bytes, address 0x$$expaddr" ;\
	"$(MAKE)" --no-print-directory clean $@ BOOTLOADER_ADDRESS=$$addr BOOTLOADER_DATA=$$daddr BOOTLOADER_EXPORTS=$$expaddr
endif

main.hex:	main.bin
	@rm -f main.hex main.eep.hex
	@avr-objcopy -j .text -j .exports -O ihex main.bin main.hex
	@avr-size $@

upgrade: main.bin
	avr-objcopy -O binary main.bin main.raw
	avr-objcopy -I binary -O elf32-avr \
        --rename-section .data=.text \
        --redefine-sym _binary_main_raw_start=loader \
        --redefine-sym _binary_main_raw_end=loader_end \
		main.raw bootloader_linkable.o	
	
disasm:	main.bin
	@avr-objdump -d -S main.bin >main.lss
