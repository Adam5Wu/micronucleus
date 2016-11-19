This is a modified version of the Digispark Mouse project.

Major changes are:

1. The original V-USB code has been replaced by the clone of the bootloader's V-USB.

  Actually, none of the functions are evetually linked to the final produced program,
  because we will use the bootloader exported USB functions.
  
  The cloned code are mainly here to keep data types and structures synchronized with
  the bootloaders's export.
  
  Consequently, all USB related assemblers have been removed, since they are not needed
  anyway.

2. A new assembler file crt1.S is included.

  The purpose of this assembler is to bypass the standard stack initialization.
  
  This allows the user program to "inherit" the stack from the bootloader, so all exported
  USB functions can continue to work.

3. The original DigiMouse implementation has been replaced by DigiMouseLite.

  Instead of referring to the included V-USB functions, the "lite" implementation
  imports all USB functions from the bootloader's export table, located at the end
  of the flash storage.

  1. The use of timer has been removed.
  
    Instead of using timers, the "lite" implementation uses the timing information passed
    in the callback from the bootloader's USB poll loop to track time.

    The poll loop should ensure at least one callback per 5ms. When there are USB activities
    the callbacks come more often. On each callback, the first parameter "rem_us" tells how
    much time **left** before the 5ms deadline, in unit of micro-seconds. So:
    - When there was no USB activity, callback happens at the deadline, rem_us=0;
    - When USB pipe is active, callback happens before the deadline, 0 <= rem_us <= 5000.
    
    User programs usually should not worry about the above details, because the "lite"
    implementation already tracks the time, and keep the milli-seconds since boot in the
    "clock_ms" gloal variable. So simply replace all millis() with a load of "clock_ms",
    and you are done.

  2. The DigiMouse_main() does not return (for as long as the USB device is not shutdown).
  
    To get control back periodically, the user program should implement checkpoint() function.
    - This function should return true if you wish to continue the function of USB device;
      Otherwise, the USB device will shutdown, and then DigiMouse_main() returns.
    - This function should perform fairly light workloads.
      Do NOT block the execution (such as using wait functions)
    - NEVER attempt to enable interrupt! (Unless you wanted a chip reset)
