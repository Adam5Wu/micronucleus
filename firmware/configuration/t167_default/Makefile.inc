# Name: Makefile
# Project: Micronucleus
# License: GNU GPL v2 (see License.txt)
#
# Controller type: ATtiny 167
# Configuration:   Default configuration. 
# Last Change:     Oct 20,2016
 

F_CPU = 16000000
DEVICE = attiny167
SPM_PAGESIZE = 128
FLASHEND = 16383
RAMEND = 767

# makefile will calculate these
BOOTLOADER_ADDRESS = 0
BOOTLOADER_DATA = 0

FUSEOPT = # TODO
FUSEOPT_DISABLERESET = # TODO
