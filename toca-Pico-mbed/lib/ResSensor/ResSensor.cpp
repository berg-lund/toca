#include "ResSensor.h"
#include <Adafruit_TinyUSB.h>

void ResSensor::setup(u_int16_t _padNum, u_int16_t _threshold, u_int16_t _averaging, u_int16_t _sensorLag, u_int16_t _midiCooldown)
{
  // set up all necessary values
  padNum = _padNum;
  threshold = _threshold;
  averaging = constrain(_averaging, 0, averageArraySize);                 // to not overflow array. The statically defined sice of the array
  sensorLag = (unsigned long)constrain(_sensorLag, 0, velocityArraySize); // to not overflow array. The statically defined sice of the array
  midiCooldown = _midiCooldown;
  midiCooldownTimer = 0;

  timerOn = false;
  noteOnTimer = 0;
  averageCounter = 0;
  startTime = 0;
}
void ResSensor::setHandleEvents(GeneralCallback _noteOn, GeneralCallback _noteOff, GeneralCallback _pressure)
{
  noteOn = _noteOn;
  noteOff = _noteOff;
  pressure = _pressure;
}

void ResSensor::setADCCallback(GeneralADCCallback _ADCCallback)
{
  ADCCallback = _ADCCallback;
}

void ResSensor::update()
{
  // get value and calculate average
  averageArray[averageCounter % averaging] = ADCCallback(padNum);
  averageCounter++;

  int value = 0;
  for (int i = 0; i < averaging; i++)
  {
    value += averageArray[i];
  }
  averagedValue = value / averaging;

  // if average is over threshold and !state then start timer
  if ((averagedValue > threshold) && !state)
  {
    // startTime = 0;
    //  timer - add to noteOnTimer until state change
    if (!timerOn)
    {
      timerOn = true;
      startTime = millis();
    }
    noteOnTimer = millis() - startTime;

    // record velocity
    velocityArray[velocityCounter % velocityArraySize] = averagedValue;
    velocityCounter++;
  }
  else if (timerOn)
  {
    // timer needs to reset if averagedValue goes under threshold before sensorLag is reached
    noteOnTimer = 0;
    timerOn = false;
    velocityCounter = 0;
  }

  // if timer is over sensorLag and !state and cooldown time since last note is reaced then noteOn
  if ((noteOnTimer > sensorLag) && !state && (millis() > (midiCooldownTimer + midiCooldown)))
  {
    noteOn(padNum, getVelocity(), false);
    state = true;
  }

  // if average under threshold and state then noteOff
  if ((averagedValue < threshold) && state)
  {
    noteOff(padNum, 0, false);

    midiCooldownTimer = millis();
    state = false;
  }
}

void ResSensor::sendPressure()
{
  if (state)
  {
    pressure(padNum, averagedValue, false);
  }
}

bool ResSensor::isPressed()
{
  // returns the averaged state of the sensor from the last call to update
  return state;
}

int ResSensor::getValue()
{
  // returns the averaged value of the sensor from the last call to update
  return averagedValue;
}

u_int16_t ResSensor::getVelocity()
{
  // Velocity is the highest value recorded and stored in the velocity array

  u_int16_t velocityMax = 0;
  // only read the values added since start of timerOn
  u_int16_t nValues = constrain(velocityCounter, 0, velocityArraySize);

  // get the highest value
  for (int i = 0; i < nValues; i++)
  {
    velocityMax = (velocityArray[i] > velocityMax) ? velocityArray[i] : velocityMax;
  }
  // Scale to 7bit value
  return (velocityMax);
}
