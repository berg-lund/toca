#pragma once
#ifndef TAPERECALL
#define TAPERECALL

#include <Arduino.h>

class TapeRecall
{
public:
  void setup();
  void addEvent(byte _midiEvent, u_int16_t _padNum, u_int16_t _timeStamp);
  void recall(unsigned long _recallLenght);
  void playback(u_int16_t _clockValue, unsigned long _lastClock);

private:
  bool playing;
  unsigned long recallLenght, startOfRecal;
  u_int16_t recallIndexLength, clockValue;

  const size_t tapeLenght = 256;
  size_t tapeIndex;
  byte masterMidiEvent[256];      // same value as tapeLenght
  u_int16_t masterPadNum[256];
  u_int16_t masterTimeStamp[256]; // same value as tapeLenght

  size_t playbackIndex;
  byte channelMidiEvent[256];
  u_int16_t channelPadNum[256];
  u_int16_t channelTimeStamp[256];

  // callbacks
  typedef void (*GeneralCallback)(byte padNum, u_int16_t value, bool playback);
  GeneralCallback noteOn, noteOff, pressure;

  public:
  void setHandleEvents(GeneralCallback _noteOn, GeneralCallback _noteOff, GeneralCallback _pressure);
};

#endif // TAPERECALL