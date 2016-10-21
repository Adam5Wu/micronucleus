# Name: Makefile
# Project: Micronucleus
# License: GNU GPL v2 (see License.txt)
#
# Controller type: ATtiny 841 
# Configuration:   Default configuration - 12 Mhz RC oscillator
# Last Change:     Oct 20,2016
 

F_CPU = 12000000
DEVICE = attiny841
SPM_PAGESIZE = 64
FLASHEND = 8191
RAMEND = 767

# makefile will calculate these
BOOTLOADER_ADDRESS = 0
BOOTLOADER_DATA = 0

FUSEOPT = -U lfuse:w:0xe2:m -U hfuse:w:0xdd:m -U efuse:w:0xf4:m
FUSEOPT_DISABLERESET =  # Not implemented



