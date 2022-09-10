#pragma once
#ifndef LEDHANDLER
#define LEDHANDLER

#include <Arduino.h>

#define nLeds 2 // number of LEDS
#define nPins 3 // number of pins per led, RGB
#define nColors 5 // number of colors

class LedHandler
{
public:
  void setup(byte _pins[nLeds][nPins], byte _colors[nColors][nPins]);
  void setLed(byte _led, byte _color, byte _prio = 0, u_int16_t _timer = 0);
  void draw();

private:
  byte pins[nLeds][nPins];
  byte colors[nColors][nPins];
  byte currentColor[nLeds];
  byte prios[nLeds];
  u_int16_t timers[nLeds];
  bool defaultState;
};

#endif // LEDHANDLER