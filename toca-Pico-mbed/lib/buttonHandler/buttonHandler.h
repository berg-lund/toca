#pragma once
#ifndef BUTTONHANDLER
#define BUTTONHANDLER

#include <Arduino.h>

class buttonHandler
{
public:
  void setup(byte _pinNum, u_int16_t _debounceDelay, bool _modePulldown = false);
  bool getState();
  bool getToggle();
  bool getRelease();

private:
  void update();

  bool modePulldown, state, toggleState, releaseState, lastState, lastReading;
  byte pinNum;
  u_int16_t debounceDelay;
  unsigned long timer;
};

#endif // BUTTONHANDLER