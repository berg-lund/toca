#include "TapeRecall.h"
#include <Adafruit_TinyUSB.h>

void TapeRecall::setup(byte _nPads)
{
  nPads = _nPads;
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
  // lenght in millis of recal
  recallLenght = _recallLenght;

  // clear channel arrays
  memset(channelMidiEvent, 0, sizeof(channelMidiEvent));
  memset(channelTimeStamp, 0, sizeof(channelTimeStamp));

  // add total note off event at start of array. (array is in backwards order)
  channelTimeStamp[0] = recallLenght - 1;
  channelPadNum[0] = 255;
  channelMidiEvent[0] = 0;
  size_t channelIndex = 0;

  for (int i = 0; i < (int)tapeLenght; i++)
  {
    // start at last used tapeIndex, move backwards throught array and wrap around zero
    int lastUsedTapeIndex = (int)tapeIndex - 1;
    size_t currentIndex = 0 < lastUsedTapeIndex - i ? lastUsedTapeIndex - i : tapeLenght - 1 + lastUsedTapeIndex - i;

    // check if timeStamp at currentIndex is within recal
    if (masterTimeStamp[currentIndex] > (startOfRecal - recallLenght))
    {
      channelIndex++;
      // add timestamp to array with zero being the start of the recall loop
      channelTimeStamp[channelIndex] = masterTimeStamp[currentIndex] - startOfRecal + recallLenght;
      channelPadNum[channelIndex] = masterPadNum[currentIndex];
      channelMidiEvent[channelIndex] = masterMidiEvent[currentIndex];
    }
    else
    {
      break;
    }
  }
  // save how many entries that are added to channelArrays;
  recallIndexLength = channelIndex;
  playbackIndex = recallIndexLength;
  SerialTinyUSB.print("recallIndexLength: ");
  SerialTinyUSB.println(recallIndexLength);
}

void TapeRecall::playback()
{
  if (playing)
  {
    // calculate where in loop the playback is.
    u_int32_t playheadPosition = (millis() - startOfRecal) % recallLenght;

    // keep track of when looping is about to occure. Works together with special event at start of array with timestamp == recallLenght - 1
    if (playheadPosition < (recallLenght - 1))
      looped = false;

    // max amount of events to read per call to playback
    int buffer = 3;
    for (int i = 0; i < buffer; i++)
    {

      // if time for next event in line has passed
      if ((channelTimeStamp[playbackIndex] <= playheadPosition) && !looped)
      {
        SerialTinyUSB.print("playbackIndex: ");
        SerialTinyUSB.print(playbackIndex);
        SerialTinyUSB.print(" i: ");
        SerialTinyUSB.println(i);

        // SerialTinyUSB.print("channelTimeStamp: ");
        // SerialTinyUSB.print(channelTimeStamp[playbackIndex]);
        // SerialTinyUSB.print(" <= playheadPosition: ");
        // SerialTinyUSB.println(playheadPosition);
        // callback to note/presseue relevant for the event
        byte eventValue = channelMidiEvent[playbackIndex];

        //----if loop noteOff trigger----
        if (channelPadNum[playbackIndex] == 255)
        {
          looped = true;
          // send note off to all currently playing notes
          stopNotes(padsPlaying);
          // clear for next loop
          padsPlaying = 0;
        }
        //----if noteOff----
        else if (eventValue == 0)
        {
          noteOff(channelPadNum[playbackIndex], 0, true);

          // Keep track of playing notes
          bitClear(padsPlaying, channelPadNum[playbackIndex]);
        }
        //----if noteOn----
        else if (eventValue < 128)
        {
          noteOn(channelPadNum[playbackIndex], eventValue, true);

          // Keep track of playing notes
          bitSet(padsPlaying, channelPadNum[playbackIndex]);
        }
        //----if pressure----
        else
        {
          pressure(channelPadNum[playbackIndex], eventValue - 127, true);
        }

        // itterate backwards trough arrays
        playbackIndex = playbackIndex > 0 ? (playbackIndex - 1) : recallIndexLength;
      }
      else
      {
        break;
      }
    }
  }
}

void TapeRecall::stopPlayback()
{
  SerialTinyUSB.println("Playback stopped");
  playing = false;
  // send note off to all currently playing notes
  stopNotes(padsPlaying);
  // clear for next loop
  padsPlaying = 0;
}

void TapeRecall::stopNotes(byte _padsPlaying)
{
  for (int i = 0; i < nPads; i++)
  {
    if (bitRead(_padsPlaying, i))
    {
      SerialTinyUSB.print("loop noteOff: ");
      noteOff(i, 0, true);
    }
  }
}