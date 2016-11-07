// DigiMouseLite Mouse Wiggler
// Jeff White (jwhite@white.nu)
// Zhenyu Wu (Adam_5Wu@hotmail.com)
// MIT License

#define MODEL_A
//#define MODEL_B

//#define USE_PRNG

#include "DigiMouseLite.h"

#define MOUSE_LEFT 1
#define MOUSE_DOWN 2
#define MOUSE_RIGHT 3
#define MOUSE_UP 4

// Milliseconds range to move the mouse
#define DELAY_MIN 10
#define DELAY_MAX 500

// Pixel range to move the mouse
#define MOUSE_MIN 1
#define MOUSE_MAX 9

unsigned short moveamount;
unsigned short mousemove;

uint16_t endtime;

void setup() {
#ifdef MODEL_B
  pinMode(0, OUTPUT); //LED on Model B
#endif
#ifdef MODEL_A
  pinMode(1, OUTPUT); //LED on Model A
#endif

#ifdef USE_PRNG
  randomSeed(analogRead(0));
#endif
  moveamount = MOUSE_MAX;
  mousemove = MOUSE_LEFT;
  endtime = DELAY_MAX;
}

inline void LEDon() {
#ifdef MODEL_B
  digitalWrite(0, HIGH);  // turn the LED on (HIGH is the voltage level)
#endif
#ifdef MODEL_A
  digitalWrite(1, HIGH);
#endif
}

inline void LEDoff() {
#ifdef MODEL_B
  digitalWrite(0, LOW);   // turn the LED off (HIGH is the voltage level)
#endif
#ifdef MODEL_A
  digitalWrite(1, LOW);
#endif
}

bool checkpoint(uchar dev_addr) {
  if (dev_addr) {
    if (clock_ms >= endtime) {
      switch (mousemove) {
        case MOUSE_LEFT :
          DigiMouse_moveX(-moveamount);
          LEDon();
          break;
        case MOUSE_DOWN :
          DigiMouse_moveY(moveamount);
          LEDoff();
          break;
        case MOUSE_RIGHT :
          DigiMouse_moveX(moveamount);
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
#ifdef USE_PRNG
      moveamount = random(MOUSE_MIN, MOUSE_MAX);
      endtime += random(DELAY_MIN, DELAY_MAX);
#else
      endtime += DELAY_MAX;
#endif

      // Wrap-around hack (dirty, but works)
      if (endtime & clock_ms & 0x8000) {
        endtime &= 0x7FFF;
        clock_ms &= 0x7FFF;
      }
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
