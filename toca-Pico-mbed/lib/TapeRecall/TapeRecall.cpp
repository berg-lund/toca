#include "TapeRecall.h"

void TapeRecall::setup()
{
  tapeIndex = 0; // current number of events in "tape"
  playing = false;
}

void TapeRecall::setHandleEvents(GeneralCallback _noteOn, GeneralCallback _noteOff, GeneralCallback _pressure)
{
  noteOn = _noteOn;
  noteOff = _noteOff;
  pressure = _pressure;
}

void TapeRecall::addEvent(byte _midiEvent, u_int16_t _padNum, u_int16_t _timeStamp)
{
  masterMidiEvent[tapeIndex] = _midiEvent;
  masterPadNum[tapeIndex] = _padNum;
  masterTimeStamp[tapeIndex] = _timeStamp;

  tapeIndex = (tapeIndex + 1) % tapeLenght; // is this right?
}

void TapeRecall::recall(unsigned long _recallLenght)
{
  startOfRecal = millis();
  playing = true;
  playbackIndex = 0;
  // lenght in millis of recal
  recallLenght = _recallLenght;

  // clear channel arrays
  memset(channelMidiEvent, 0, sizeof(channelMidiEvent));
  memset(channelTimeStamp, 0, sizeof(channelTimeStamp));
  size_t channelIndex = 0;

  for (int i = 0; i < (int)tapeLenght; i++)
  {
    // start at tapeIndex, move backwards throught array and wrap around zero
    size_t currentIndex = 0 < (int)tapeIndex - i ? tapeIndex - i : tapeLenght + tapeIndex - i;

    // check if timeStamp at currentIndex is within recal
    if (masterTimeStamp[currentIndex] > (startOfRecal - recallLenght))
    {
      // add timestamp to array with zero being the start of the recall loop
      channelTimeStamp[channelIndex] = masterTimeStamp[currentIndex] - startOfRecal - recallLenght;
      channelPadNum[channelIndex] = masterPadNum[currentIndex];
      channelMidiEvent[channelIndex] = masterMidiEvent[currentIndex];

      // save how many entries are added to channelArrays;
      recallIndexLength = channelIndex;

      channelIndex++;
    }
    else
    {
      break;
    }
  }
}

void TapeRecall::playback(u_int16_t _clockValue, unsigned long _lastClock)
{
  // calculate where in loop the playback is.
  unsigned long playheadPosition = (millis() - startOfRecal) % recallLenght;

  // if time for next event in line has passed
  while (channelTimeStamp[playbackIndex] < playheadPosition)
  {
    // callback to note/presseue relevant for the event
    byte eventValue = channelMidiEvent[playbackIndex];
    if (eventValue == 0)
    {
      noteOff(channelPadNum[playbackIndex], 0, true);
    }
    else if (eventValue < 128)
    {
      noteOn(channelPadNum[playbackIndex], eventValue, true);
    }
    else
    {
      pressure(channelPadNum[playbackIndex], eventValue - 127, true);
    }
    // increment index and check next. Break at overflow to avoid locked in loop
    // send note off on all active notes
    playbackIndex++;
    if (playbackIndex > recallIndexLength)
    {
      playbackIndex = 0;
      break;
    }
  }
}