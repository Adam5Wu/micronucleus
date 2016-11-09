# USB Export
In the official version of this bootloader, althought the bootloader itself contains an implementation of V-USB,
user programs that wish to leverage USB functions will have to include their own copy of V-USB, which inflate the
binary code by around 1.5~2KB. On MCU with 8KB or less storage, it is a significant problem.

Inspired by [plasticassius's fork](https://github.com/plasticassius/micronucleus), this fork aims to enable exporting
of full USB functionality to user program, in order to make more efficient use of flash storage.
The new bootloader is slightly larger in size, and the inflation is mostly due to the need export USB features that
are not used in the bootloader, but potentially useful in user programs.

Bootloader size (for attiny85):
- No interrupt endpoint: 1894 bytes
- With interrupt endpoint 1: 2048 bytes
- With interrupt endpoint 1 (+halt support): 2128 bytes (default configuration)
- With interrupt endpoint 1 (+halt support, usbFunctionWrite, usbFunctionRead): 2254 bytes
 
# How to examples
A modified Digistump DigiMouse library and Arduino project is included in the [examples direcotry](examples/DigisparkMouseLite).

The original project generates a 4,332 byte binary;
while the "lite" version which uses bootloader exported USB function generates 2,228 byte binary.
The saving is over 2KB.
- If PRNG functions (random, etc.) is removed, the binary is further reduced to 1,408 bytes
  - This means bootloader + the mouse program together fit even MCU with 4KB of flash

# Caveats
## Bootloader compatibility
- This bootloader (with USB export) behaves exactly like the original, so completely compatible functionality-wise
  - User programs that include their own V-USB will still work as expected.
  
## Non-standard VUSB implementation
The bootloader uses [interrupt-free VUSB](https://cpldcpu.wordpress.com/2014/03/02/interrupt-free-v-usb/),
as a consequence, user programs that wish to leverage the exported USB function also has to do it "the bootloader way".

Notably:
* No interrupt is allowed!
  - Because interrupt bits are still set on the data pins (although interrupt is disabled,
  read [author's blog](https://cpldcpu.wordpress.com/2014/03/02/interrupt-free-v-usb/) for details.
  - Probably the most notable side-effect is, you cannot use millis() to get time (because it leverages a timer
  interrupt to track time). This is solvable because the interrupt-free USB polling mechanism tracks
  (approximated) time on the order of micro-seconds, and I have exposed this information to the user program.
  See [code example](examples/DigisparkMouseLite) for details.
* Callback workflow must be used, instead of active probe
  - Due to the disable of interrupt, USB data lines have to be constantly monitored. So instead of letting user program
  to perform adhoc checking, the main loop must perform the USB polling, and user program can only be executed as
  side-service during the gap of polling
  - The benefit of doing so is supposedly improved throughput, according to the
  [author's blog](https://cpldcpu.wordpress.com/2014/03/02/interrupt-free-v-usb/)
  - Again see [code example](examples/DigisparkMouseLite) for detailed usages.
