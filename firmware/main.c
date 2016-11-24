/*
 * Project: Micronucleus -	v2.32
 *
 * Micronucleus V2.33						(c) 2016 Zhenyu Wu - Adam_5Wu@hotmail.com
 *															(c) 2016 Tim Bo"scke - cpldcpu@gmail.com
 *															(c) 2014 Shay Green
 * Original Micronucleus				(c) 2012 Jenna Fox
 *
 * Based on USBaspLoader-tiny85	(c) 2012 Louis Beaudoin
 * Based on USBaspLoader				(c) 2007 by OBJECTIVE DEVELOPMENT Software GmbH
 *
 * License: GNU GPL v2 (see License.txt)
 */

#define MICRONUCLEUS_VERSION_MAJOR 2
#define MICRONUCLEUS_VERSION_MINOR 32

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <avr/boot.h>
#include <util/delay.h>

#define bool uint8_t
#define true 1
#define false 0

#include "bootloaderconfig.h"
#include "usbdrv/usbdrv.c"

// verify the bootloader address aligns with page size
#if (defined __AVR_ATtiny841__)||(defined __AVR_ATtiny441__)
	#if BOOTLOADER_ADDRESS % (SPM_PAGESIZE * 4) != 0
		#error "BOOTLOADER_ADDRESS in makefile must be a 4x multiple of chip's pagesize"
	#endif
#else
	#if BOOTLOADER_ADDRESS % SPM_PAGESIZE != 0
		#error "BOOTLOADER_ADDRESS in makefile must be a multiple of chip's pagesize"
	#endif
#endif

#if SPM_PAGESIZE>256
	#error "Micronucleus only supports pagesizes up to 256 bytes"
#endif

#if ((AUTO_EXIT_MS>0) && (AUTO_EXIT_MS<1000))
	#error "Do not set AUTO_EXIT_MS to below 1s to allow Micronucleus to function properly"
#endif

// Device configuration reply
// Length: 6 bytes
//	 Byte 0:	User program memory size, high byte
//	 Byte 1:	User program memory size, low byte
//	 Byte 2:	Flash Pagesize in bytes
//	 Byte 3:	Page write timing in ms.
//		Bit 7 '0': Page erase time equals page write time
//		Bit 7 '1': Page erase time equals page write time divided by 4
//	 Byte 4:	SIGNATURE_1
//	 Byte 5:	SIGNATURE_2

PROGMEM const uint8_t configurationReply[6] = {
	(((uint16_t)PROGMEM_SIZE) >> 8) & 0xff,
	((uint16_t)PROGMEM_SIZE) & 0xff,
	SPM_PAGESIZE,
	MICRONUCLEUS_WRITE_SLEEP,
	SIGNATURE_1,
	SIGNATURE_2
};

#if OSCCAL_RESTORE_DEFAULT
	register uint8_t osccal_default	asm("r5");				// r7: default osc
#endif

typedef union {
	uint16_t w;
	uint8_t b[2];
} uint16_union_t;

register uint16_union_t currentAddress	asm("r6");	// r6/r7: current progmem address, used for erasing and writing
#if USB_CFG_IMPLEMENT_FN_WRITE && USB_CFG_IMPLEMENT_FN_READ && USB_CFG_IMPLEMENT_FN_WRITEOUT
register uint16_union_t idlePolls				asm("r8");	// r8/r9: idle counter
#else
register uint16_union_t idlePolls				asm("r16");	// r16/r17: idle counter
#endif

// command system schedules functions to run in the main loop
enum {
	cmd_local_nop = 0,
	cmd_device_info = 0,
	cmd_transfer_page = 1,
	cmd_erase_application = 2,
	cmd_write_data = 3,
	cmd_exit = 4,
	cmd_write_page = 64	// internal commands start at 64
};
register uint8_t command asm("r10");								// r10: command

#define nop() asm volatile("nop")
#define wdr() asm volatile("wdr")
#define spm() asm volatile("spm")

// Use the old delay routines without NOP padding. This saves memory.
#define __DELAY_BACKWARD_COMPATIBLE__

/* ------------------------------------------------------------------------ */
// erase all pages until bootloader, in reverse order (so our vectors stay in place for as long as possible)
// to minimise the chance of leaving the device in a state where the bootloader wont run, if there's power failure
// during upload
static inline void eraseApplication(void) {
	uint16_t ptr = BOOTLOADER_ADDRESS;

	while (ptr) {
#if (defined __AVR_ATtiny841__)||(defined __AVR_ATtiny441__)
		ptr -= SPM_PAGESIZE * 4;
#else
		ptr -= SPM_PAGESIZE;
#endif
		boot_page_erase(ptr);
	}

	// Reset address to ensure the reset vector is written first.
	currentAddress.w = 0;
}

// simply write currently stored page in to already erased flash memory
static inline void writeFlashPage(void) {
#if OSCCAL_SLOW_PROGRAMMING
	uint8_t osccal_tmp;
	osccal_tmp	 = OSCCAL;
	OSCCAL			 = osccal_default;
	nop();
#endif
	if (currentAddress.w - 2 <BOOTLOADER_ADDRESS) {
			boot_page_write(currentAddress.w - 2);	 // will halt CPU, no waiting required
	}
#if OSCCAL_SLOW_PROGRAMMING
	OSCCAL			 = osccal_tmp;
	nop();
#endif
}

// Write a word into the page buffer.
// Will patch the bootloader reset vector into the main vectortable to ensure
// the device can not be bricked. Saving user-reset-vector is done in the host
// tool, starting with firmware V2
static inline void writeWordToPageBuffer(uint16_t data) {

#ifndef ENABLE_UNSAFE_OPTIMIZATIONS
	#if BOOTLOADER_ADDRESS < 8192
	// rjmp
	if (currentAddress.w == RESET_VECTOR_OFFSET * 2) {
		data = 0xC000 + (BOOTLOADER_ADDRESS/2) - 1;
	}
	#else
	// far jmp
	if (currentAddress.w == RESET_VECTOR_OFFSET * 2) {
		data = 0x940c;
	} else if (currentAddress.w == (RESET_VECTOR_OFFSET+1) * 2) {
		data = (BOOTLOADER_ADDRESS/2);
	}
	#endif
#endif

#if OSCCAL_SAVE_CALIB
	if (currentAddress.w == BOOTLOADER_ADDRESS - TINYVECTOR_OSCCAL_OFFSET) {
		data = OSCCAL;
	}
#endif

	boot_page_fill(currentAddress.w, data);
	currentAddress.w += 2;
}

#define usbMsgData(data, flags) \
usbMsgPtr = (usbMsgPtr_t)data;	\
usbMsgFlags = flags;						\

/* ------------------------------------------------------------------------ */
static usbMsgLen_t usbFunctionSetup(uint8_t data[8]) {
	usbRequest_t *rq = (void *)data;

	switch (rq->bRequest) {
		case cmd_device_info: // get device info
			usbMsgData(configurationReply, USB_FLG_MSGPTR_IS_ROM);
			return sizeof(configurationReply);

		case cmd_transfer_page:
			// Set page address. Address zero always has to be written first to ensure reset vector patching.
			// Mask to page boundary to prevent vulnerability to partial page write "attacks"
			if (currentAddress.w != 0) {
				currentAddress.b[0] = rq->wIndex.bytes[0] & (~(SPM_PAGESIZE-1));
				currentAddress.b[1] = rq->wIndex.bytes[1];

				// clear page buffer as a precaution before filling the buffer in case
				// a previous write operation failed and there is still something in the buffer.
				__SPM_REG = (_BV(CTPB)|_BV(__SPM_ENABLE));
				spm();
			}
			break;

		case cmd_write_data: // Write data
			writeWordToPageBuffer(rq->wValue.word);
			writeWordToPageBuffer(rq->wIndex.word);
			if ((currentAddress.b[0] % SPM_PAGESIZE) == 0)
				command = cmd_write_page; // ask runloop to write our page
			break;

		default:
			// Handle cmd_erase_application and cmd_exit
			command = rq->bRequest&0x3f;
	}
	return 0;
}

#if EXPORT_USB

#if USB_CFG_IMPLEMENT_FN_WRITE
// Dummy implementation, not used in bootloader
static uchar usbFunctionWrite(uchar *data, uchar len) {
	return 0;
}
#endif /* USB_CFG_IMPLEMENT_FN_WRITE */

#if USB_CFG_IMPLEMENT_FN_READ
// Dummy implementation, not used in bootloader
static uchar usbFunctionRead(uchar *data, uchar len) {
	return 0;
}
#endif /* USB_CFG_IMPLEMENT_FN_READ */

#if USB_CFG_IMPLEMENT_FN_WRITEOUT
// Dummy implementation, not used in bootloader
static void usbFunctionWriteOut(uchar *data, uchar len) {
	// Do Nothing
}
#endif /* USB_CFG_IMPLEMENT_FN_WRITEOUT */

// Handle descriptor requests for bootloader
static usbMsgLen_t usbFunctionDescriptor(struct usbRequest *rq) {
	switch (rq->wValue.bytes[1]) {
		case USBDESCR_DEVICE:		/* 1 */
			usbMsgData(usbDescriptorDevice, USB_FLG_MSGPTR_IS_ROM);
			return USB_PROP_LENGTH(USB_CFG_DESCR_PROPS_DEVICE);

		case USBDESCR_CONFIG:		/* 2 */
			usbMsgData(usbDescriptorConfiguration, USB_FLG_MSGPTR_IS_ROM);
			return USB_PROP_LENGTH(USB_CFG_DESCR_PROPS_CONFIGURATION);

		case USBDESCR_STRING:		/* 3 */
			switch (rq->wValue.bytes[0]) {
				case 0:
					usbMsgData(usbDescriptorString0, USB_FLG_MSGPTR_IS_ROM);
					return USB_PROP_LENGTH(USB_CFG_DESCR_PROPS_STRING_0);

				case 1:
					if (USB_CFG_DESCR_PROPS_STRING_VENDOR) {
						usbMsgData(usbDescriptorStringVendor, USB_FLG_MSGPTR_IS_ROM);
						return USB_PROP_LENGTH(USB_CFG_DESCR_PROPS_STRING_VENDOR);
					}

				case 2:
					if (USB_CFG_DESCR_PROPS_STRING_PRODUCT) {
						usbMsgData(usbDescriptorStringDevice, USB_FLG_MSGPTR_IS_ROM);
						return USB_PROP_LENGTH(USB_CFG_DESCR_PROPS_STRING_PRODUCT);
					}

				case 3:
					if (USB_CFG_DESCR_PROPS_STRING_SERIAL_NUMBER) {
						usbMsgData(usbDescriptorStringSerialNumber, USB_FLG_MSGPTR_IS_ROM);
						return USB_PROP_LENGTH(USB_CFG_DESCR_PROPS_STRING_SERIAL_NUMBER);
					}
			}
	}
	return 0;
}

#else

static usbMsgLen_t usbFunctionDescriptor(struct usbRequest *rq) {
	return 0;
}

#endif

void initUSB (
#if EXPORT_USB
	usbMsgLen_t (*f_usbFunctionSetup)(uchar data[8]),
	#if USB_CFG_IMPLEMENT_FN_WRITE
		uchar (*f_usbFunctionWrite)(uchar *data, uchar len),
	#endif /* USB_CFG_IMPLEMENT_FN_WRITE */
	#if USB_CFG_IMPLEMENT_FN_READ
		uchar (*f_usbFunctionRead)(uchar *data, uchar len),
	#endif /* USB_CFG_IMPLEMENT_FN_READ */
	#if USB_CFG_IMPLEMENT_FN_WRITEOUT
		void (*f_usbFunctionWriteOut)(uchar *data, uchar len),
	#endif /* USB_CFG_IMPLEMENT_FN_WRITEOUT */
	usbMsgLen_t (*f_usbFunctionDescriptor)(struct usbRequest *rq)
#else
	void
#endif
) {
	usbInit(
	#if EXPORT_USB
		f_usbFunctionSetup,
		#if USB_CFG_IMPLEMENT_FN_WRITE
			f_usbFunctionWrite,
		#endif /* USB_CFG_IMPLEMENT_FN_WRITE */
		#if USB_CFG_IMPLEMENT_FN_READ
			f_usbFunctionRead,
		#endif /* USB_CFG_IMPLEMENT_FN_READ */
		#if USB_CFG_IMPLEMENT_FN_WRITEOUT
			f_usbFunctionWriteOut,
		#endif /* USB_CFG_IMPLEMENT_FN_WRITEOUT */
		f_usbFunctionDescriptor
	#endif
	);		// Initialize INT settings after reconnect
	usbDeviceConnect();
}

void shutdownUSB (void) {
	usbDeviceDisconnect();

	USB_INTR_ENABLE = 0;
	USB_INTR_CFG = 0;			 /* also reset config bits */

	_delay_ms(300);
}

static inline void initHardware (void)
{
	// Disable watchdog and set timeout to maximum in case the WDT is fused on
#ifdef CCP
	// New ATtinies841/441 use a different unlock sequence and renamed registers
	MCUSR = 0;
	CCP = 0xD8;
	WDTCSR = 1<<WDP2 | 1<<WDP1 | 1<<WDP0;
#else
	MCUSR = 0;
	WDTCR = 1<<WDCE | 1<<WDE;
	WDTCR = 1<<WDP2 | 1<<WDP1 | 1<<WDP0;
#endif

	LED_MACRO(0x00);
	shutdownUSB();
	LED_MACRO(0xFF);
}

static inline void usbPollLite(void) {
	// This is usbpoll() minus reset logic and double buffering
	int8_t	len;
	len = usbRxLen - 3;

	if(len >= 0){
		usbProcessRx(usbRxBuf + 1, len); // only single buffer due to in-order processing
#if USB_CFG_HAVE_FLOWCONTROL
		if(usbRxLen > 0)		/* only mark as available if not inactivated */
#endif
			usbRxLen = 0;			 /* mark rx buffer as available */
	}
	// transmit system is always idle in sequential mode
	//if(usbTxLen & 0x10) {		/* transmit system idle */
		if(usbMsgLen != USB_NO_MSG){		/* transmit data pending? */
			usbBuildTxBlock();
		}
	//}
}

#define POOL_CYCLE						24

#define USB_RESET_MS					2.5
#define USB_RESET_TIMEOUT			(uint16_t)(F_CPU/(1.0e3*POOL_CYCLE/USB_RESET_MS))

#define USB_POLL_MS						5.0
#define USB_POLL_TIMEOUT			(uint16_t)(F_CPU/(1.0e3*POOL_CYCLE/USB_POLL_MS))

#define USB_POLL_REMUS(CTR)		(CTR*(uint16_t)(1.6e7*POOL_CYCLE/F_CPU)/16)

// 5 clockcycles per loop, 8.8µs timeout
#define USB_RESYNC_CYCLE			5
#define USB_RESYNC_TIMEOUT		8.8f
#define USB_RESYNC_IDLE				(uint8_t)(USB_RESYNC_TIMEOUT*(F_CPU/1.0e6/USB_RESYNC_CYCLE)+0.5)

//void USB_INTR_VECTOR(void);
void loopUSB(
	bool (*f_beforePoll)(uint16_t rem_us, uchar dev_addr),
	bool (*f_afterPoll)(uint16_t rem_us, uchar dev_addr)
) {
	//uint16_t resetctr = 0;
	register uint16_t resetctr asm("r24") = 0;
	do {
		//uint16_t fastctr = USB_POLL_TIMEOUT;
		register uint16_t fastctr asm("r26") = USB_POLL_TIMEOUT;

		do {
			if ((USBIN & USBMASK) != 0) resetctr = USB_RESET_TIMEOUT;

			if (usbDeviceAddr && !--resetctr) { // reset encountered

				// bits from the reset handling of usbpoll()
				usbNewDeviceAddr = 0;
				usbDeviceAddr = 0;
				usbResetStall();

#if AUTO_EXIT_NO_USB_MS
				idlePolls.w = max(idlePolls.w, (AUTO_EXIT_MS-AUTO_EXIT_NO_USB_MS)/5);
#endif
#if !OSCCAL_HAVE_XTAL
				calibrateOscillatorASM();
#endif
			}

			if (USB_INTR_PENDING & (1<<USB_INTR_PENDING_BIT)) {
				USB_INTR_VECTOR();
				USB_INTR_PENDING = 1<<USB_INTR_PENDING_BIT;	// Clear int pending, in case timeout occurred during SYNC
				break;
			}
		} while(--fastctr);

		wdr();

		uint16_t rem_us = USB_POLL_REMUS(fastctr);
		if (!f_beforePoll(rem_us, usbDeviceAddr)) break;

		usbPollLite();

		if (!f_afterPoll(rem_us, usbDeviceAddr)) break;

		// Test whether another interrupt occurred during the processing of USBpoll and commands.
		// If yes, we missed a data packet on the bus. Wait until the bus was idle for 8.8µs to
		// allow synchronising to the next incoming packet.
		if (USB_INTR_PENDING & (1<<USB_INTR_PENDING_BIT)) {
			uint8_t ctr;

			// loop takes 5 cycles
			asm volatile(
				"				 ldi	%0, %1	\n\t"
				"loop%=: sbis %2, %3	\n\t"
				"				 ldi	%0, %1	\n\t"
				"				 subi %0, 1		\n\t"
				"				 brne loop%=	\n\t"
				: "=&d" (ctr)
				:	"M" (USB_RESYNC_IDLE), "I" (_SFR_IO_ADDR(USBIN)), "M" (USB_CFG_DMINUS_BIT)
			);
			USB_INTR_PENDING = 1<<USB_INTR_PENDING_BIT;
		}
	} while(1);

	shutdownUSB();
}

/* ------------------------------------------------------------------------ */

static inline void enterBootloader(void) {
	bootLoaderInit();

	/* save default OSCCAL calibration	*/
#if OSCCAL_RESTORE_DEFAULT
	osccal_default = OSCCAL;
#endif

#if OSCCAL_SAVE_CALIB
	// adjust clock to previous calibration value, so bootloader starts with proper clock calibration
	unsigned char stored_osc_calibration = pgm_read_byte(BOOTLOADER_ADDRESS - TINYVECTOR_OSCCAL_OFFSET);
	if (stored_osc_calibration != 0xFF) {
		OSCCAL = stored_osc_calibration;
		nop();
	} else
#endif
#if !OSCCAL_HAVE_XTAL
		calibrateOscillatorASM();
#endif

	initHardware();
}

// reset system to a normal state and launch user program
static inline void leaveBootloader(void) {
 	bootLoaderExit();

#if OSCCAL_RESTORE_DEFAULT
	OSCCAL = osccal_default;
	nop(); // NOP to avoid CPU hickup during oscillator stabilization
#endif
}

static bool CommandHandling(uint16_t rem_us, uchar dev_addr) {
	// Commands are only evaluated after next USB transmission or after 5 ms passed
	switch (command) {
		case cmd_erase_application:
			eraseApplication();
			break;

		case cmd_write_page:
			writeFlashPage();
			break;

		case cmd_exit:
			// Only exit after timeout
			return rem_us != 0;
	}
	command = cmd_local_nop;
	return true;
}

static bool IdleCheck(uint16_t rem_us, uchar dev_addr) {
		// reset idle polls when we get usb traffic
	if (rem_us)
		idlePolls.w = 0;

	idlePolls.w++;
	// Try to execute program when bootloader times out
	if (AUTO_EXIT_MS && (idlePolls.w == (AUTO_EXIT_MS/5))) {
		return (pgm_read_byte(BOOTLOADER_ADDRESS - TINYVECTOR_RESET_OFFSET + 1) == 0xff);
	}

	if (dev_addr) LED_MACRO(idlePolls.b[0]);

	return true;
}

__attribute__((naked, section(".init9")))
__attribute__((__noreturn__))
void main(void) {
	enterBootloader();

	if (bootLoaderStartCondition() || (pgm_read_byte(BOOTLOADER_ADDRESS - TINYVECTOR_RESET_OFFSET + 1) == 0xff)) {
		LED_INIT();

		shutdownUSB();

		idlePolls.w = AUTO_EXIT_NO_USB_MS? (AUTO_EXIT_MS-AUTO_EXIT_NO_USB_MS)/5 : 0;
		command = cmd_local_nop;
		currentAddress.w = 0;

		initUSB(
			#if EXPORT_USB
				usbFunctionSetup,
				#if USB_CFG_IMPLEMENT_FN_WRITE
					usbFunctionWrite,
				#endif /* USB_CFG_IMPLEMENT_FN_WRITE */
				#if USB_CFG_IMPLEMENT_FN_READ
					usbFunctionRead,
				#endif /* USB_CFG_IMPLEMENT_FN_READ */
				#if USB_CFG_IMPLEMENT_FN_WRITEOUT
					usbFunctionWriteOut,
				#endif /* USB_CFG_IMPLEMENT_FN_WRITEOUT */
				usbFunctionDescriptor
			#endif
		);

		loopUSB(CommandHandling, IdleCheck);

		LED_EXIT();
	}

	leaveBootloader();

#define STRINGIZE(s)	#s
#define REXPAND(s)	asm volatile ("rjmp __vectors - " STRINGIZE(s))
	REXPAND(TINYVECTOR_RESET_OFFSET); // Jump to application reset vector at end of flash
#undef FEXPAND
#undef STRINGIZE
}
/* ------------------------------------------------------------------------ */

__attribute__((naked, section(".exports"))) void __exports(void) {
#if EXPORT_USB
	asm volatile(
#if USB_CFG_HAVE_INTRIN_ENDPOINT && !USB_CFG_SUPPRESS_INTR_CODE
		" rjmp _usbInterruptIsReady		\n"
		" rjmp _usbSetInterrupt				\n"
#endif
		" rjmp _usbStall							\n"
		" rjmp loopUSB								\n"
		" rjmp initUSB								\n"
	);
#endif
}
