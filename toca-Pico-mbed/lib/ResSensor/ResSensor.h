#pragma once
#ifndef RESSENSOR
#define RESSENSOR

#include <Arduino.h>

class ResSensor
{
public:
  void setup(u_int16_t _padNum, u_int16_t _threshold, u_int16_t _averaging, u_int16_t _sensorLag, u_int16_t _midiCooldown);
  void update();
  void sendPressure();
  bool isPressed();
  bool isNewPress();
  bool hasEnded();
  int getValue();
  u_int16_t getVelocity();

private:
  u_int16_t padNum, threshold; // byte?
  int averaging;

  const u_int16_t averageArraySize = 48;
  u_int16_t averageArray[48]; // length of array is max value of averageing
  const u_int16_t velocityArraySize = 48;
  u_int16_t velocityArray[48]; // length of array is max value of sensorLag

  u_int16_t velocityCounter;
  u_int32_t noteOnTimer;
  bool state;
  bool timerOn;
  u_int16_t averagedValue;
  unsigned long sensorLag, midiCooldown;
  unsigned long averageCounter;
  unsigned long startTime;
  unsigned long midiCooldownTimer;

  // callbacks
  typedef void (*GeneralCallback)(byte padNum, u_int16_t value, bool playback);
  GeneralCallback noteOn, noteOff, pressure;
  typedef u_int16_t (*GeneralADCCallback)(byte padNum);
  GeneralADCCallback ADCCallback;

public:
  void setHandleEvents(GeneralCallback _noteOn, GeneralCallback _noteOff, GeneralCallback _pressure);
  void setADCCallback(GeneralADCCallback _ADCCallback);
};
#endif // RESSENSOR
