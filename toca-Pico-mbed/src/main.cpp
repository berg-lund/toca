#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_MCP3008.h>
#include <MIDI.h>
#include "ResSensor.h"
#include "buttonHandler.h"
//#include "TapeRecall.h"

/*

------TO DO------
* Whats the difference between midiCooldown and noteMaxspeed?
* Incorporate buttons with temp change LED functionality
* fix last two channels of ADC. Does this not work?
* Reinclude TapeTecall and fix RTOS crashes when flashed
* add start recal of set value on buttonpress

* Is setting up the pins with input pulldown (in ResSensor.setup()) affecting the values read by them?
* Create tape function
* Look into defining callsbacks in main.h file
* How should LEDHandler behave? send events with timers or with lifespann?
* Stabilize clock
* Check where to use byte and where int
* Exclude libraries that aren't needed

* Add CV functionality

# Serial communication is buffered and not realtime. Debugg through MIDIOx
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
buttonHandler button1;
const byte buttonPin = 0;
const byte buttonDebounceDelay = 20;

const u_int16_t buttonTopPin = 1;
const u_int16_t buttonBottomPin = 0;

const u_int16_t joyButtonPin = 3;
const u_int16_t joyUpPin = 4;
const u_int16_t joyRightPin = 5;
const u_int16_t joyDownPin = 6;
const u_int16_t joyLeftPin = 7;

// ---- LED ----
void LED();
const u_int16_t LED1_RPin = 8;
const u_int16_t LED1_GPin = 9;
const u_int16_t LED1_BPin = 10;

const u_int16_t LED2_RPin = 11;
const u_int16_t LED2_GPin = 12;
const u_int16_t LED2_BPin = 13;

// ----- Tape -----
// TapeRecall tape;

// ---- Global Const ----
const u_int16_t ccMaxspeed = 150;   // Frequency to send MIDICC messages on. Value in millis.
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
unsigned long lastClock = 0;
const byte clockArrayLength = 24; // 24 ticks per quartenote in MIDI standard
u_int16_t clockArray[clockArrayLength];
u_int16_t clockValue = 0;
byte clockIndex = 0;

//---- Functions ----
void sendCC();

//---- Define Callbacks ----
void handleClock();
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

  // ---- Callbacks ----
  MIDI.setHandleClock(handleClock);

  // ---- Setup sensors ----
  for (int i = 0; i < nSensors; i++)
  {
    sensors[i].setup(i, threshold, averaging, sensorLag, midiCooldown);
    sensors[i].setHandleEvents(handleNoteOn, handleNoteOff, handlePressure);
    sensors[i].setADCCallback(handleADC);

    pinMode(sensorPins[i], INPUT_PULLDOWN);
  }

  // ----- Buttons -----
  button1.setup(buttonPin, buttonDebounceDelay, false);

  // ----- Tape -----
  // tape.setup();

  // wait until device mounted
  while (!TinyUSBDevice.mounted())
    delay(1);

  digitalWrite(LED_BUILTIN, HIGH); // setup is complete
  SerialTinyUSB.begin(115200);
  SerialTinyUSB.println("Setup complete");

  //----- SPI -----
  // Software SPI (specify all, use any available digital)
  // (sck, mosi, miso, cs);
  adc.begin(PIN_CLK, PIN_TX, PIN_RX, PIN_CS);

  // temp LED setup
  pinMode(LED1_RPin, OUTPUT);
  pinMode(LED1_GPin, OUTPUT);
  pinMode(LED1_BPin, OUTPUT);

  pinMode(LED2_RPin, OUTPUT);
  pinMode(LED2_GPin, OUTPUT);
  pinMode(LED2_BPin, OUTPUT);
}

void loop()
{
  // buttonHandler.update();
  for (auto &s : sensors)
  {
    s.update();
  }
  sendCC();
  LED(); // LEDHandler.update();
  // tape.playback(clockValue, lastClock);

  // Recive new midi
  MIDI.read();
}

void handleClock()
{
  unsigned long time = millis();
  clockArray[clockIndex] = millis() - lastClock;

  clockValue = 0;
  for (auto &c : clockArray)
  {
    clockValue += c;
  }

  if (clockIndex == 0)
  {
    SerialTinyUSB.print("Recived Clock: ");
    SerialTinyUSB.print(clockValue);
    SerialTinyUSB.print("\t index: ");
    SerialTinyUSB.println(clockIndex);
  }

  clockIndex = (clockIndex + 1) % clockArrayLength;
  lastClock = time;
}

void handleNoteOn(byte padNum, u_int16_t velocity, bool playback)
{
  u_int16_t v = (velocity / 8) * rangeTune;
  v = constrain(v, 0, 127);
  MIDI.sendNoteOn(noteNum[padNum], v, channelNum[padNum]);
  //## CV gate on
  //## Indicate with led?
  // if not playing back add to tape
  if (!playback)
  {
    // tape.addEvent(v, padNum, millis());
  }
  SerialTinyUSB.print(" Note on. Pad = ");
  SerialTinyUSB.print(padNum);
  SerialTinyUSB.print("\t Sensor value = ");
  SerialTinyUSB.print(velocity);
  SerialTinyUSB.print("\t v = ");
  SerialTinyUSB.println(v);
}

void handleNoteOff(byte padNum, u_int16_t velocity, bool playback)
{
  MIDI.sendNoteOn(noteNum[padNum], 0, channelNum[padNum]);
  //## CV gate of
  // if not playing back add to tape
  if (!playback)
  {
    // tape.addEvent(0, padNum, millis());
  }
  SerialTinyUSB.print(" Note off. Pad = ");
  SerialTinyUSB.println(padNum);
}

void handlePressure(byte padNum, u_int16_t pressure, bool playback)
{
  u_int16_t p = (pressure / 8) * rangeTune;
  p = constrain(p, 0, 127);
  MIDI.sendControlChange(ccChannels[padNum], p, channelNum[padNum]);

  // if not playing back add to tape
  if (!playback)
  {
    // tape.addEvent(127 + p, padNum, millis());
  }

  SerialTinyUSB.print("Pressure p: ");
  SerialTinyUSB.print(p);
  SerialTinyUSB.print(" \t Pad: ");
  SerialTinyUSB.println(padNum);
  //## CV pressure
}

void sendCC()
{
  static unsigned long ccTimer = millis();

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
  // convert pad number to pin number on ADC
  u_int16_t readPin = sensorPins[padNum];
  return (u_int16_t)adc.readADC(readPin);
}

void LED()
{
  analogWrite(LED1_RPin, 85);
  analogWrite(LED1_GPin, 25);
  analogWrite(LED1_BPin, 40);

  analogWrite(LED2_RPin, 40);
  analogWrite(LED2_GPin, 25);
  analogWrite(LED2_BPin, 40);
}