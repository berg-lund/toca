#include "LedHandler.h"
#include <Adafruit_TinyUSB.h>

// Number of leds, pins and colors hardcoded in ledHandler.h
void LedHandler::setup(byte _pins[nLeds][nPins], byte _colors[nColors][nPins])
{
  defaultState = true;

  memcpy_P(pins, _pins, 6);
  memcpy_P(colors, _colors, 15);

  for (auto &led : pins)
  {
    for (auto p : led)
    {
      pinMode(p, OUTPUT);
    }
  }
}

// update led with new color for _timer amount if time of _prio is higher then other call
void LedHandler::setLed(byte _led, byte _color, byte _prio, u_int16_t _timer)
{
  // protection against to high values of _led and _color
  byte led = _led % nLeds;
  byte color = _color % nColors;

  // only update color if prio is same or higher
  if (_prio >= prios[led])
  {
    timers[led] = _timer;
    currentColor[led] = color;
  }

  defaultState = false;
}

// update state of led Pins
void LedHandler::draw()
{
  // if no new color set or no timer active set default color
  if (defaultState)
  {
    for (int i = 0; i < nLeds; i++)
    {
      if (timers[i] == 0)
      {
        currentColor[i] = 0;
      }
    }
  }

  // update LED pins
  for (int led = 0; led < nLeds; led++)
  {
    for (int pin = 0; pin < nPins; pin++)
    {
      analogWrite(pins[led][pin], colors[currentColor[led]][pin]);
    }
    prios[led] = 0;
    timers[led] = (timers[led] - 1) > 0 ? (timers[led] - 1) : 0;
  }

  defaultState = true;
}