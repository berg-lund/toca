#pragma once
#ifndef TAPERECALL
#define TAPERECALL

#include <Arduino.h>

class TapeRecall
{
public:
  void setup(byte _nPads);
  void addEvent(byte _midiEvent, u_int16_t _padNum, u_int32_t _timeStamp);
  void recall(unsigned long _recallLenght);
  void playback();
  void stopPlayback();

private:
  bool playing, looped;
  unsigned long recallLenght, startOfRecal;
  u_int16_t recallIndexLength;
  byte nPads, padsPlaying;

  // the master arrays saves all midi event info
  const size_t tapeLenght = 256;
  size_t tapeIndex;
  byte masterMidiEvent[256];      // same value as tapeLenght
  byte masterPadNum[256];         // same value as tapeLenght
  u_int32_t masterTimeStamp[256]; // same value as tapeLenght

  // the channel arrays stores a copy from the master array for playback as defined on Recall start
  size_t playbackIndex;
  byte channelMidiEvent[256];
  byte channelPadNum[256];
  u_int16_t channelTimeStamp[256];

  // callbacks
  typedef void (*GeneralCallback)(byte padNum, u_int16_t value, bool playback);
  GeneralCallback noteOn, noteOff, pressure;

public:
  void setHandleEvents(GeneralCallback _noteOn, GeneralCallback _noteOff, GeneralCallback _pressure);
};

#endif // TAPERECALL