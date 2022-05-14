#include "buttonHandler.h"

void buttonHandler::setup(byte _pinNum, u_int16_t _debounceDelay, bool _modePulldown)
{
  pinNum = _pinNum;
  debounceDelay = _debounceDelay;
  modePulldown = _modePulldown;

  if (modePulldown)
  {
    pinMode(pinNum, INPUT_PULLDOWN);
  }
  else
  {
    pinMode(pinNum, INPUT_PULLUP);
  }
}

bool buttonHandler::getState()
{
  update();
  return state;
}

bool buttonHandler::getToggle()
{
  // return true once when state turned true
  update();
  bool temp = toggleState;
  toggleState = false;

  return temp;
}

void buttonHandler::update()
{
  bool reading = digitalRead(pinNum);
  // invert if configured in pullup mode
  reading = modePulldown ? reading : !reading;

  // if state changed, start timer
  if (reading != lastState)
  {
    timer = millis();
  }

  if ((millis() - timer) > debounceDelay)
  {
    // only change toggleState on first cycle of state == true
    if (!state)
      toggleState = reading;
    state = reading;
  }

  lastState = reading;
}