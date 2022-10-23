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

void TapeRecall::addEvent(u_int16_t _midiEvent, byte _padNum, u_int32_t _timeStamp)
{
  masterMidiEvent[tapeIndex] = _midiEvent;
  masterPadNum[tapeIndex] = _padNum;
  masterTimeStamp[tapeIndex] = _timeStamp;

  tapeIndex = (tapeIndex + 1) % tapeLenght;
}

void TapeRecall::startOfRecall()
{
  SerialTinyUSB.println("Recall start time");
  startOfRecal = millis();
}

void TapeRecall::recall(u_int32_t _recallLenght)
{
  SerialTinyUSB.print("Recall start. Lenght: ");
  SerialTinyUSB.println(_recallLenght);

  if (_recallLenght <= 0)
  {
    return;
  }

  playing = true;
  // lenght in millis of recal
  recallLength = _recallLenght;

  // clear channel arrays
  memset(channelMidiEvent, 0, sizeof(channelMidiEvent));
  memset(channelTimeStamp, 0, sizeof(channelTimeStamp));

  // add total note off event at start of array. (array is in backwards order)
  channelTimeStamp[0] = recallLength - 1;
  channelPadNum[0] = 255;
  channelMidiEvent[0] = 0;

  size_t channelIndex = 0;

  for (int i = 0; i < (int)tapeLenght; i++)
  {
    // start at last used tapeIndex, move backwards throught array and wrap around zero
    int lastUsedTapeIndex = (int)tapeIndex - 1;
    size_t currentIndex = 0 < lastUsedTapeIndex - i ? lastUsedTapeIndex - i : tapeLenght - 1 + lastUsedTapeIndex - i;

    // check if timeStamp at currentIndex is within recal
    if (masterTimeStamp[currentIndex] > (startOfRecal - recallLength))
    {
      channelIndex++;
      // add timestamp to array with zero being the start of the recall loop
      channelTimeStamp[channelIndex] = masterTimeStamp[currentIndex] - startOfRecal + recallLength;
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
  playbackIndex = channelIndex;
  SerialTinyUSB.print("recallIndexLength: ");
  SerialTinyUSB.println(recallIndexLength);
}

void TapeRecall::playback()
{
  if (!playing)
  {
    return;
  }

  // calculate where in loop the playback is.
  u_int32_t playheadPosition = (millis() - startOfRecal) % recallLength;

  // keep track of when looping is about to occure. Works together with special event at start of array with timestamp == recallLenght - 1
  if (playheadPosition < (recallLength - 1))
  {
    looped = false;
  }

  // max amount of events to read per call to playback
  int buffer = 3;
  for (int i = 0; i < buffer; i++)
  {
    // if time for next event in line has passed
    if ((channelTimeStamp[playbackIndex] > playheadPosition) || looped)
    {
      break;
    }
    // SerialTinyUSB.print("playbackIndex: ");
    // SerialTinyUSB.print(playbackIndex);
    // SerialTinyUSB.print(" i: ");
    // SerialTinyUSB.println(i);

    // SerialTinyUSB.print("channelTimeStamp: ");
    // SerialTinyUSB.print(channelTimeStamp[playbackIndex]);
    // SerialTinyUSB.print(" <= playheadPosition: ");
    // SerialTinyUSB.println(playheadPosition);

    // callback to note/presseue relevant for the event
    u_int16_t eventValue = channelMidiEvent[playbackIndex];

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
    else if (eventValue < 1024)
    {
      noteOn(channelPadNum[playbackIndex], eventValue, true);

      // Keep track of playing notes
      bitSet(padsPlaying, channelPadNum[playbackIndex]);
    }

    //----if pressure----
    else
    {
      pressure(channelPadNum[playbackIndex], eventValue - 1023, true);
    }

    // itterate backwards trough arrays
    playbackIndex = playbackIndex > 0 ? (playbackIndex - 1) : recallIndexLength;
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