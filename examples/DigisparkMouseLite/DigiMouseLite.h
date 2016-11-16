/*
 * Based on Obdev's AVRUSB code and under the same license.
 *
 * TODO: Make a proper file header. :-)
 * Modified for Digispark by Digistump
 * And now modified by Sean Murphy (duckythescientist) from a keyboard device to a mouse device
 * Most of the credit for the joystick code should go to Raphaël Assénat
 * And now mouse credit is due to Yiyin Ma and Abby Lin of Cornell
 * Modified by Zhenyu Wu for use with MicroNucleus exported USB handler
 */
#ifndef __DigiMouse_h__
#define __DigiMouse_h__

//#include <Arduino.h>
//#include <avr/pgmspace.h>
//#include <avr/io.h>
#include "usbdrv.inc"


// ----- BEGIN -----
// Prototype USB routines from MicroNucleus bootloader

extern "C" void _initUSB(
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
);

extern "C" void _shutdownUSB(void);

// Compensate some process delays
#define USB_POLLTIME_US (5000+1080)

extern "C" void _loopUSB(
	bool (*f_beforePoll)(uint16_t rem_us, uchar dev_addr),
	bool (*f_afterPoll)(uint16_t rem_us, uchar dev_addr)
);

extern "C" void _usbMsgData(void const *data, uchar flags);

extern "C" void _usbStall(uchar endpoint);
	
#if USB_CFG_HAVE_INTRIN_ENDPOINT && !USB_CFG_SUPPRESS_INTR_CODE
	extern "C" void _usbSetInterrupt(uchar endpoint, void const *data, uchar len);
	extern "C" bool _usbInterruptIsReady(uchar endpoint);
#endif

// ----- END -----


#define MOUSEBTN_LEFT_MASK		0x01
#define MOUSEBTN_RIGHT_MASK		0x02
#define MOUSEBTN_MIDDLE_MASK	0x04

struct mouseReport {
		uchar buttons;
		char  deltaX;
		char  deltaY;
		char  deltaS;
};
#define REPORT_SIZE sizeof(mouseReport)

static mouseReport report_buffer, report_buffer_last;
	
// report frequency set to default of 50hz
#define DIGIMOUSE_DEFAULT_REPORT_INTERVAL 20
static uchar idle_rate; // in units of 4ms

static bool must_report;
static uint16_t us_buffer;
static uint16_t idle_ms;
static uint16_t clock_ms;

#undef usbHidReportDescriptor
const PROGMEM uchar usbHidReportDescriptor[] = { /* USB report descriptor */
		0x05, 0x01,										 // USAGE_PAGE (Generic Desktop)
		0x09, 0x02,										 // USAGE (Mouse)
		0xa1, 0x01,										 // COLLECTION (Application)
		0x09, 0x01,										 //		USAGE_PAGE (Pointer)
		0xa1, 0x00,										 //		COLLECTION (Physical)
		0x05, 0x09,										 //		USAGE_PAGE (Button)
		0x19, 0x01,										 //		USAGE_MINIMUM (Button 1)
		0x29, 0x03,										 //		USAGE_MAXIMUM (Button 3)
		0x15, 0x00,										 //		LOGICAL_MINIMUM (0)
		0x25, 0x01,										 //		LOGICAL_MAXIMUM (1)
		0x95, 0x03,										 //		REPORT_COUNT (3)
		0x75, 0x01,										 //		REPORT_SIZE (1)
		0x81, 0x02,										 //		INPUT (Data,Var,Abs)
		0x95, 0x01,										 //		REPORT_COUNT (1)
		0x75, 0x05,										 //		REPORT_SIZE (5)
		0x81, 0x01,										 //		Input(Cnst)
		0x05, 0x01,										 //		USAGE_PAGE(Generic Desktop)
		0x09, 0x30,										 //		USAGE(X)
		0x09, 0x31,										 //		USAGE(Y)
		0x15, 0x81,										 //		LOGICAL_MINIMUM (-127)
		0x25, 0x7f,										 //		LOGICAL_MAXIMUM (127)
		0x75, 0x08,										 //		REPORT_SIZE (8)
		0x95, 0x02,										 //		REPORT_COUNT (2)
		0x81, 0x06,										 //		INPUT (Data,Var,Rel)
		0x09, 0x38,											//	 Usage (Wheel)
		0x95, 0x01,											//	 Report Count (1),
		0x81, 0x06,											//	 Input (Data, Variable, Relative)
		0xc0,														// END_COLLECTION
		0xc0													 // END_COLLECTION
};

extern "C" {

void clearMove(mouseReport &report) {
	report.deltaX = 0;
	report.deltaY = 0;
	report.deltaS = 0;
}
	
usbMsgLen_t usbFunctionSetup(uint8_t data[8]) {
	usbRequest_t *rq = (usbRequest_t *)data;
	if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {		/* class request type */
		switch (rq->bRequest) {
			case USBRQ_HID_GET_REPORT:
				/* wValue: ReportType (highbyte), ReportID (lowbyte) */
				/* we only have one report type, so don't look at wValue */
				report_buffer_last = report_buffer;
				_usbMsgData(&report_buffer_last, 0);
				clearMove(report_buffer);
				return REPORT_SIZE;

			case USBRQ_HID_GET_IDLE:
				_usbMsgData(&idle_rate, 0);
				return sizeof(idle_rate);

			case USBRQ_HID_SET_IDLE:
				idle_rate = rq->wValue.bytes[1];
				break;
		}
	} else {
		/* no vendor specific requests implemented */
	}
	return 0;
}

#if USB_CFG_IMPLEMENT_FN_WRITE
	// Dummy implementation, not used in bootloader
	uchar usbFunctionWrite(uchar *data, uchar len) {
		return 0;
	}
#endif /* USB_CFG_IMPLEMENT_FN_WRITE */

#if USB_CFG_IMPLEMENT_FN_READ
	// Dummy implementation, not used in bootloader
	uchar usbFunctionRead(uchar *data, uchar len) {
		return 0;
	}
#endif /* USB_CFG_IMPLEMENT_FN_READ */

#if USB_CFG_IMPLEMENT_FN_WRITEOUT
	// Dummy implementation, not used in bootloader
	void usbFunctionWriteOut(uchar *data, uchar len) {
		// Do Nothing
	}
#endif /* USB_CFG_IMPLEMENT_FN_WRITEOUT */

#define USB_FLG_MSGPTR_IS_ROM	(1<<6)

// Handle descriptor requests for bootloader
usbMsgLen_t usbFunctionDescriptor(struct usbRequest *rq) {
	switch (rq->wValue.bytes[1]) {
		case USBDESCR_DEVICE:		/* 1 */
			_usbMsgData(usbDescriptorDevice, USB_FLG_MSGPTR_IS_ROM);
			return USB_PROP_LENGTH(USB_CFG_DESCR_PROPS_DEVICE);

		case USBDESCR_CONFIG:		/* 2 */
			_usbMsgData(usbDescriptorConfiguration, USB_FLG_MSGPTR_IS_ROM);
			return USB_PROP_LENGTH(USB_CFG_DESCR_PROPS_CONFIGURATION);

		case USBDESCR_STRING:		/* 3 */
		switch (rq->wValue.bytes[0]) {
			case 0:
				_usbMsgData(usbDescriptorString0, USB_FLG_MSGPTR_IS_ROM);
				return USB_PROP_LENGTH(USB_CFG_DESCR_PROPS_STRING_0);

			case 1:
				if (USB_CFG_DESCR_PROPS_STRING_VENDOR) {
					_usbMsgData(usbDescriptorStringVendor, USB_FLG_MSGPTR_IS_ROM);
					return USB_PROP_LENGTH(USB_CFG_DESCR_PROPS_STRING_VENDOR);
				}

			case 2:
				if (USB_CFG_DESCR_PROPS_STRING_PRODUCT) {
					_usbMsgData(usbDescriptorStringDevice, USB_FLG_MSGPTR_IS_ROM);
					return USB_PROP_LENGTH(USB_CFG_DESCR_PROPS_STRING_PRODUCT);
				}

			case 3:
				if (USB_CFG_DESCR_PROPS_STRING_SERIAL_NUMBER) {
					_usbMsgData(usbDescriptorStringSerialNumber, USB_FLG_MSGPTR_IS_ROM);
					return USB_PROP_LENGTH(USB_CFG_DESCR_PROPS_STRING_SERIAL_NUMBER);
				}
		}
		case USBDESCR_HID:
			_usbMsgData(usbDescriptorConfiguration + 18, USB_FLG_MSGPTR_IS_ROM);
			return USB_PROP_LENGTH(USB_CFG_DESCR_PROPS_HID);

		case USBDESCR_HID_REPORT:
			_usbMsgData(usbHidReportDescriptor, USB_FLG_MSGPTR_IS_ROM);
			return sizeof(usbHidReportDescriptor);
	}
	return 0;
}

bool (*_checkpoint)(uchar dev_addr);

bool communicate(uint16_t rem_us, uchar dev_addr) {
	us_buffer += USB_POLLTIME_US - rem_us;
	uint8_t whole_ms = us_buffer / 1000;

	if (whole_ms) {
		idle_ms+= whole_ms;
		clock_ms+= whole_ms;
		us_buffer %= 1000;

		// if idle report interval is up (in unit of 4ms)
		must_report = idle_rate && (idle_ms >= idle_rate*4);
	}
	if (!_checkpoint(dev_addr)) return false;

	// if the report has changed, try force an update
	if (!must_report)
		must_report = ((report_buffer_last.buttons != report_buffer.buttons) ||
									(report_buffer.deltaX | report_buffer.deltaY | report_buffer.deltaS));

	// if we want to send a report, signal the host computer to ask us for it with a usb 'interrupt'
	if (must_report && dev_addr && _usbInterruptIsReady(1)) {
		idle_ms = 0;
		must_report = false;
		_usbSetInterrupt(1, &report_buffer, REPORT_SIZE);
		
		// because we send deltas in movement, so when we send them, we clear them
		clearMove(report_buffer);
		report_buffer_last.buttons = report_buffer.buttons;
	}
	return true;
}

bool nop(uint16_t rem_us, uchar dev_addr) {
	return true;
}

} // extern "C"

void DigiMouse_main(bool (*f_checkpoint)(uchar dev_addr)) {
	_initUSB(
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
	);

	idle_rate = DIGIMOUSE_DEFAULT_REPORT_INTERVAL / 4;
	clock_ms = us_buffer = idle_ms = 0;
	clearMove(report_buffer);
	report_buffer.buttons = 0;
	clearMove(report_buffer_last);
	report_buffer_last.buttons = 0;
	must_report = false;

	_checkpoint = f_checkpoint;
	_loopUSB(communicate, nop);

	// Should not reach
	//_shutdownUSB();
}

void DigiMouse_moveX(char deltaX)	{
	report_buffer.deltaX = (deltaX == -128)?-127:deltaX;
}

void DigiMouse_moveY(char deltaY) {
	report_buffer.deltaY = (deltaY == -128)?-127:deltaY;
}

void DigiMouse_scroll(char deltaS)	{
	report_buffer.deltaS = (deltaS == -128)?-127:deltaS;
}

void DigiMouse_move(char deltaX, char deltaY, char deltaS) {
	DigiMouse_moveX(deltaX);
	DigiMouse_moveY(deltaY);
	DigiMouse_scroll(deltaS);
}

void DigiMouse_moveClick(char deltaX, char deltaY, char deltaS, char buttons) {
	DigiMouse_move(deltaX,deltaY,deltaS);
	report_buffer.buttons = buttons;
}

void DigiMouse_rightClick(bool down){
	if (down) report_buffer.buttons|= MOUSEBTN_RIGHT_MASK;
	else report_buffer.buttons&= ~MOUSEBTN_RIGHT_MASK;
}

void DigiMouse_leftClick(bool down){
	if (down) report_buffer.buttons|= MOUSEBTN_LEFT_MASK;
	else report_buffer.buttons&= ~MOUSEBTN_LEFT_MASK;
}

void DigiMouse_middleClick(bool down){
	if (down) report_buffer.buttons|= MOUSEBTN_MIDDLE_MASK;
	else report_buffer.buttons&= ~MOUSEBTN_MIDDLE_MASK;
}

void DigiMouse_setButtons(uchar buttons) {
	report_buffer.buttons = buttons;
}

void DigiMouse_setValues(uchar values[]) {
	memcpy(&report_buffer, values, REPORT_SIZE);
}

// ----- BEGIN -----
// Import USB routines from MicroNucleus bootloader
// Location is important, otherwise seems to trigger compiler bug

#define STRINGIZE(s) #s
#define FEXPAND(f,s) asm volatile(				\
"\n .global " STRINGIZE(f)								\
"\n .set " STRINGIZE(f) "," STRINGIZE(s)	\
);

void __imports(void) {
	FEXPAND(_initUSB,FLASHEND-1);
	FEXPAND(_shutdownUSB,FLASHEND-3);
	FEXPAND(_loopUSB,FLASHEND-5);
	FEXPAND(_usbMsgData,FLASHEND-7);
	FEXPAND(_usbStall,FLASHEND-9);
#if USB_CFG_HAVE_INTRIN_ENDPOINT && !USB_CFG_SUPPRESS_INTR_CODE
	FEXPAND(_usbSetInterrupt,FLASHEND-11);
	FEXPAND(_usbInterruptIsReady,FLASHEND-13);
#endif
}

#undef FEXPAND
#undef STRINGIZE

// ----- END -----

#endif // __DigiMouse_h__
