#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_MCP3008.h>
#include <MIDI.h>
#include "ResSensor.h"
#include "buttonHandler.h"
#include "LedHandler.h"
#include "TapeRecall.h"

/*
------TO DO------
* Why does it just hang some times??? Probably some array that tries to get read or written out of bounds
* Sometimes the loop gets stuck. I don't know why.

* Make functions for buttons and joystick
  * Can updateJoy be done inside buttons instead of its own function?
  * can getJoy be just joyValue * getClock?

* LEDHandler
  * Prio doesn't seem to work.
  * Can nLeds, nPins and nColors be defined in main.h?
  * Add LED events on pad

* First event after startup is missed in playback in TapeRecall
* Clock
  * Averageing isn't doing what I think. Bigger array doesn't help. even a 1 millisecond error per quarternote gets in to 16th note territory prety quickly.
  * Might be because of all the serial data slowing the pico down
  * can clockArray be byte? 255 * 24 is 6 second per quarternote? But wouldn't work well with first new value. Needs to be capped instead of overflowed
  *

* If recal is made witout any data in the recal lenght. recallIndex will stay the same as from the last successfull recal. Might not be a bug but a feature? Or might not do anything
* Is setting up the pins with input pulldown (in ResSensor.setup()) affecting the values read by them?
* Look into defining callsbacks in main.h file
* Clean upp commented serial prints in TapeRecall

* Exclude libraries that aren't needed
* Add CV functionality
* Add tap tempo functionality??

# Serial communication is buffered and not realtime. For example, note off might not be showned until more data is sent. Debugg through MIDIOx
# Serial communication doesn't initiate until a while. between 100 and 4000 loops. Doesn't need a fix just remember.
# &
# static variables are always global to it's class. Not specific to any specific instance of it.
# do intilialize variables to a value before use
*/

// ----- USB MIDI object ----
Adafruit_USBD_MIDI usb_midi;

// Create a new instance of the Arduino MIDI Library,
// and attach usb_midi as the transport.
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

// ----- SPI -----
Adafruit_MCP3008 adc;

// spi pins
const u_int16_t PIN_CS = 17;  // chip select   (pico pin 22)
const u_int16_t PIN_TX = 19;  // mosi/MCP Din  (pico pin 25)ä
const u_int16_t PIN_RX = 16;  // miso/MCP DOut (pico pin 21)
const u_int16_t PIN_CLK = 18; // clock/SCK     (pico pin 24)

// ----- Buttons -----
void buttons();
buttonHandler buttonTop, buttonBottom, joyButton, joyUp, joyRight, joyDown, joyLeft;
const byte buttonDebounceDelay = 20;

const u_int16_t buttonTopPin = 1;
const u_int16_t buttonBottomPin = 0;

void updateJoy();
u_int32_t getJoy();
byte joyValue;

const u_int16_t joyButtonPin = 3;
const u_int16_t joyRightPin = 6;
const u_int16_t joyUpPin = 5;
const u_int16_t joyDownPin = 2;
const u_int16_t joyLeftPin = 4;

// ---- LED ----
// Number of leds, pins and colors hardcoded in ledHandler.h

LedHandler ledHandler;
byte ledPins[2][3]{
    {11, 12, 13},
    {8, 9, 10}};

byte colors[5][3]{
    {85, 15, 40},  // default
    {85, 5, 14},   // button press
    {100, 60, 80}, // sensor press
    {85, 15, 80},  // playback on
    {85, 35, 14}   // clock
};

// ----- Tape -----
TapeRecall tape;

// ---- Global Const ----
const u_int16_t ccMaxspeed = 75;    // Frequency to send MIDICC messages on. Value in millis.
const u_int16_t sensorLag = 25;     // From first sensorvalue in millis to get abetter velocity value. Max value set by array size in ResSensor
const u_int16_t midiCooldown = 150; // Minimum time between last note ended and new one

const byte nSensors = 8;
const u_int16_t threshold = 120;
const u_int16_t averaging = 12;
const u_int16_t sensorPins[nSensors] = {7, 6, 5, 4, 3, 2, 1, 0};
const u_int16_t noteNum[nSensors] = {50, 52, 53, 55, 57, 58, 60, 62};
const u_int16_t channelNum[nSensors] = {1, 2, 3, 4, 5, 6, 7, 8};
const u_int16_t ccChannels[nSensors] = {1, 1, 1, 1, 1, 1, 1, 1};
const float rangeTune = 1.0;

// ---- Global Variables ----
ResSensor sensors[nSensors];
u_int16_t pressureArray[nSensors];

//---- Clock ----
u_int32_t lastClock = 0;
const byte clockAveraging = 8;
const u_int16_t clockArrayLength = 24 * clockAveraging; // 24 ticks per quartenote in MIDI standard
u_int16_t clockArray[clockArrayLength];
u_int16_t clockValue = 500; // startup value of clock before midiClock is recived
byte clockIndex = 0;
bool midiStart = false;

//---- Functions ----
void sendCC();

//---- Define Callbacks ----
void handleClock();
void handleStart();
void handleNoteOn(byte padNum, u_int16_t velocity, bool playback = false);
void handleNoteOff(byte padNum, u_int16_t velocity, bool playback = false);
void handlePressure(byte padNum, u_int16_t pressure, bool playback = false);
void startRecall(byte tapeChannel, u_int32_t length);
void endRecall(byte tapeChannel);
void LEDHandler(byte state);
u_int16_t handleADC(byte padNum);

void setup()
{
  // use built in led to show when setup is complete
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // ---- USB, Serial and MIDI ----
#if defined(ARDUINO_ARCH_MBED) && defined(ARDUINO_ARCH_RP2040)
  // Manual begin() is required on core without built-in support for TinyUSB such as mbed rp2040
  TinyUSB_Device_Init(0);
#endif

  usb_midi.setStringDescriptor("Rescontroll USB MIDI");

  // Initialize MIDI, and listen to all MIDI channels
  // This will also call usb_midi's begin()
  MIDI.begin(MIDI_CHANNEL_OMNI);

  // ---- MIDI Callbacks ----
  MIDI.setHandleClock(handleClock);
  MIDI.setHandleStart(handleStart);

  // ---- Setup sensors ----
  for (int i = 0; i < nSensors; i++)
  {
    sensors[i].setup(i, threshold, averaging, sensorLag, midiCooldown);
    sensors[i].setHandleEvents(handleNoteOn, handleNoteOff, handlePressure);
    sensors[i].setADCCallback(handleADC);

    pinMode(sensorPins[i], INPUT_PULLDOWN);
  }

  // ----- Buttons -----
  buttonTop.setup(buttonTopPin, buttonDebounceDelay);
  buttonBottom.setup(buttonBottomPin, buttonDebounceDelay);
  joyButton.setup(joyButtonPin, buttonDebounceDelay);
  joyDown.setup(joyDownPin, buttonDebounceDelay);
  joyLeft.setup(joyLeftPin, buttonDebounceDelay);
  joyUp.setup(joyUpPin, buttonDebounceDelay);
  joyRight.setup(joyRightPin, buttonDebounceDelay);

  // ----- Tape -----
  tape.setup(nSensors);
  tape.setHandleEvents(handleNoteOn, handleNoteOff, handlePressure);

  // wait until device mounted
  while (!TinyUSBDevice.mounted())
    delay(1);

  digitalWrite(LED_BUILTIN, HIGH); // setup is complete
  SerialTinyUSB.begin(115200);

  //----- SPI -----
  // Software SPI (specify all, use any available digital)
  // (sck, mosi, miso, cs);
  adc.begin(PIN_CLK, PIN_TX, PIN_RX, PIN_CS);

  // LED setup
  ledHandler.setup(ledPins, colors);
}

void loop()
{
  buttons();

  for (auto &s : sensors)
  {
    s.update();
  }
  sendCC();

  tape.playback();

  // Recive new midi
  MIDI.read();
  ledHandler.draw();
}

// gets called on recived midi Clock
void handleClock()
{
  u_int32_t time = millis();
  clockArray[clockIndex] = millis() - lastClock;

  // after first two new clocks after midi start message clear array and set value from these two
  if (midiStart && clockIndex == 1)
  {
    for (auto &c : clockArray)
    {
      c = clockArray[1];
    }
    midiStart = false;
  }

  // calculate averaged quarternote value
  clockValue = 0;
  for (auto &c : clockArray)
  {
    clockValue += c;
  }
  clockValue /= clockAveraging;

  // update led once per quarternote
  if ((clockIndex % 24) == 0)
  {
    ledHandler.setLed(1, 4, 1, clockValue / 2);
    SerialTinyUSB.print("Clock recived: ");
    SerialTinyUSB.print(clockArray[clockIndex]);
    SerialTinyUSB.print(" Averaged: ");
    SerialTinyUSB.print(clockValue);
    SerialTinyUSB.print(" index: ");
    SerialTinyUSB.println(clockIndex);
  }

  clockIndex = (clockIndex + 1) % clockArrayLength;
  lastClock = time;
}

// gets called on recived MIDI start message
void handleStart()
{
  SerialTinyUSB.println("Start recived");
  midiStart = true;
  // set downbeat of clock
  clockIndex = 0;
}

void handleNoteOn(byte padNum, u_int16_t velocity, bool playback)
{
  // if not playing back add to tape
  if (!playback)
  {
    tape.addEvent(velocity, padNum, millis());
  }

  // tune and scale to 7bit value for MIDI
  u_int16_t v = (velocity / 8) * rangeTune;
  v = constrain(v, 0, 127);
  MIDI.sendNoteOn(noteNum[padNum], v, channelNum[padNum]);

  //## CV gate on
  //## Indicate with led?

  // SerialTinyUSB.print(" Note on. Pad = ");
  // SerialTinyUSB.print(padNum);
  // SerialTinyUSB.print("\t Sensor value = ");
  // SerialTinyUSB.print(velocity);
  // SerialTinyUSB.print("\t v = ");
  // SerialTinyUSB.println(v);
}

void handleNoteOff(byte padNum, u_int16_t velocity, bool playback)
{
  // if not playing back add to tape
  if (!playback)
  {
    tape.addEvent(0, padNum, millis());
  }
  MIDI.sendNoteOn(noteNum[padNum], 0, channelNum[padNum]);
  //## CV gate of

  // SerialTinyUSB.print(" Note off. Pad = ");
  // SerialTinyUSB.println(padNum);
}

void handlePressure(byte padNum, u_int16_t pressure, bool playback)
{
  // if not playing back add to tape
  if (!playback)
  {
    tape.addEvent(1023 + pressure, padNum, millis());
  }
  // tune and scale to 7bit value for MIDI
  u_int16_t p = (pressure / 8) * rangeTune;
  p = constrain(p, 0, 127);
  MIDI.sendControlChange(ccChannels[padNum], p, channelNum[padNum]);

  // SerialTinyUSB.print(" Pressure p: ");
  // SerialTinyUSB.print(p);
  // SerialTinyUSB.print(" \t Pad: ");
  // SerialTinyUSB.println(padNum);
  //## CV pressure
}

void sendCC()
{
  static u_int32_t ccTimer = millis();

  if (millis() > (ccTimer + ccMaxspeed))
  {
    ccTimer += ccMaxspeed;

    for (auto &s : sensors)
    {
      s.sendPressure();
    }
  }
}

u_int16_t handleADC(byte padNum)
{
  // this function returns the read value from the ADC. Used by callback functions.
  //  convert pad number to pin number on ADC
  u_int16_t readPin = sensorPins[padNum];
  return (u_int16_t)adc.readADC(readPin);
}

void buttons()
{
  // when button is pressed is where the loop ends.
  if (buttonTop.getToggle())
  {
    tape.startOfRecall();

    // clear joystick value before read
    joyValue = 0;
  }
  // playback of the loop won't begin until button is released.
  if (buttonTop.getRelease())
  {
    tape.recall(getJoy()); // get joysitck value
  }

  if (buttonTop.getState())
    ledHandler.setLed(0, 1);

  if (buttonBottom.getToggle())
    tape.stopPlayback();

  if (buttonBottom.getState())
    ledHandler.setLed(1, 1);

  updateJoy();
}

void updateJoy()
{
  // read all buttons and set bits
  if (joyButton.getToggle())
  {
    bitSet(joyValue, 0);
  }

  if (joyRight.getToggle())
  {
    bitSet(joyValue, 1);
  }

  if (joyDown.getToggle())
  {
    bitSet(joyValue, 2);
  }

  if (joyLeft.getToggle())
  {
    bitSet(joyValue, 3);
  }

  if (joyUp.getToggle())
  {
    bitSet(joyValue, 4);
  }
}

u_int32_t getJoy()
{
  // value of joy times clock
  return joyValue * clockValue;
}