// DigiMouseLite Mouse Wiggler
// Jeff White (jwhite@white.nu)
// Zhenyu Wu (Adam_5Wu@hotmail.com)
// MIT License

#include "DigiMouseLite.h"

#include <util/delay.h>

// Use the old delay routines without NOP padding. This saves memory.
#define __DELAY_BACKWARD_COMPATIBLE__

#define MOUSE_LEFT 1
#define MOUSE_DOWN 2
#define MOUSE_RIGHT 3
#define MOUSE_UP 4

// Milliseconds range to move the mouse
#define DELAY_MIN 50
#define DELAY_MAX 200

// Pixel range to move the mouse
#define MOUSE_MIN 1
#define MOUSE_MAX 9

unsigned short moveamount;
unsigned short mousemove;

unsigned short us_buffer;
unsigned short curtime;
unsigned short endtime;

void setup() {
  pinMode(0, OUTPUT); //LED on Model B
  pinMode(1, OUTPUT); //LED on Model A

  randomSeed(analogRead(0));
  moveamount = MOUSE_MAX;
  mousemove = MOUSE_LEFT;
  endtime = DELAY_MIN;
  us_buffer = curtime = 0;
}

inline void LEDon() {
  digitalWrite(0, HIGH);  // turn the LED on (HIGH is the voltage level)
  digitalWrite(1, HIGH);
}

inline void LEDoff() {
  digitalWrite(0, LOW);   // turn the LED off (HIGH is the voltage level)
  digitalWrite(1, LOW);
}

bool checkpoint(uint16_t rem_us) {
  us_buffer += USB_POLLTIME_US - rem_us;
  uint8_t whole_ms = us_buffer / 1000;
  if (whole_ms) {
    curtime += whole_ms;
    us_buffer %= 1000;
    if (curtime >= endtime) {
      curtime -= endtime;
      switch (mousemove) {
        case MOUSE_LEFT :
          DigiMouse_moveX(-moveamount);
          LEDon();
          break;
        case MOUSE_RIGHT :
          DigiMouse_moveX(moveamount);
          LEDoff();
          break;
        case MOUSE_DOWN :
          DigiMouse_moveY(moveamount);
          LEDon();
          break;
        case MOUSE_UP :
          DigiMouse_moveY(-moveamount);
          LEDoff();
        //break;
        default :
          mousemove = 0;
      }
      mousemove++;
      moveamount = random(MOUSE_MIN, MOUSE_MAX);
      endtime = random(DELAY_MIN, DELAY_MAX);
    }
  }
  return true;
}

__attribute__((naked, section(".init9")))
__attribute__((__noreturn__))
int main() {
  setup();
  DigiMouse_main(checkpoint);
}
