#pragma once
#ifndef TAPERECALL
#define TAPERECALL

#include <Arduino.h>

class TapeRecall
{
public:
  void setup(byte _nPads);
  void addEvent(u_int16_t _midiEvent, byte _padNum, u_int32_t _timeStamp);
  void recall(u_int32_t _recallLenght);
  void playback();
  void stopPlayback();

private:
  void stopNotes(byte _padsPlaying);

  bool playing, looped;
  u_int32_t recallLenght, startOfRecal;
  u_int16_t recallIndexLength;
  byte nPads, padsPlaying;

  // the master arrays saves all midi event info
  const size_t tapeLenght = 256;
  size_t tapeIndex;
  u_int16_t masterMidiEvent[256]; // same value as tapeLenght
  byte masterPadNum[256];         // same value as tapeLenght
  u_int32_t masterTimeStamp[256]; // same value as tapeLenght

  // the channel arrays stores a copy from the master array for playback as defined on Recall start
  size_t playbackIndex;
  u_int16_t channelMidiEvent[256];
  byte channelPadNum[256];
  u_int16_t channelTimeStamp[256];

  // callbacks
  typedef void (*GeneralCallback)(byte padNum, u_int16_t value, bool playback);
  GeneralCallback noteOn, noteOff, pressure;

public:
  void setHandleEvents(GeneralCallback _noteOn, GeneralCallback _noteOff, GeneralCallback _pressure);
};

#endif // TAPERECALL