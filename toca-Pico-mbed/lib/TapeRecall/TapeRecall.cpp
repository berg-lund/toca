#include "TapeRecall.h"
#include <Adafruit_TinyUSB.h>

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

void TapeRecall::addEvent(byte _midiEvent, u_int16_t _padNum, u_int32_t _timeStamp)
{
  masterMidiEvent[tapeIndex] = _midiEvent;
  masterPadNum[tapeIndex] = _padNum;
  masterTimeStamp[tapeIndex] = _timeStamp;

  SerialTinyUSB.print("Event added: ");
  SerialTinyUSB.print(tapeIndex);

  tapeIndex = (tapeIndex + 1) % tapeLenght;
}

void TapeRecall::recall(unsigned long _recallLenght)
{
  SerialTinyUSB.println("Recall start");
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
    // start at last used tapeIndex, move backwards throught array and wrap around zero
    int lastUsedTapeIndex = (int)tapeIndex - 1;
    size_t currentIndex = 0 < lastUsedTapeIndex - i ? lastUsedTapeIndex - i : tapeLenght - 1 + lastUsedTapeIndex - i;

    SerialTinyUSB.print("currentIndex: ");
    SerialTinyUSB.println(currentIndex);

    SerialTinyUSB.print("Time evaluation: ");
    SerialTinyUSB.print(masterTimeStamp[currentIndex]);
    SerialTinyUSB.print(" > ");
    SerialTinyUSB.println(startOfRecal - recallLenght);

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
  SerialTinyUSB.print("recallIndexLength: ");
  SerialTinyUSB.println(recallIndexLength);
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