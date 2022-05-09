/*
 * Machine.cpp
 * Created: 13.05.2020
 * Author: SL
 */ 
#include <Arduino.h>
#include "MkrSineChopperTcc.h"
#include "MkrADC.h"
#include "Machine.h"
#include "UserCommandHandler.h"

MachineSingleton Machine;

// PWM indicator LED pin
#define PWM_LED_PIN LED_BUILTIN

enum MachineMode 
{ 
  MachineModeDisabled,
  MachineModeEnabled,
  MachineModeStarting, 
  MachineModeRunning, 
  MachineModeFinished 
};
                   
static MachineMode _machineMode = MachineModeDisabled;
static char _replyBuffer[ADC_MAX_SAMPLES * 4]; // each sample has 3 digits and space
static int _cycleMicroseconds = 0;

static void isrCycleEndCallback();

void MachineSingleton::enablePWM(int cycleMicroseconds, int duty1024, int numChops)
{
  _machineMode = MachineModeEnabled;
  _cycleMicroseconds = cycleMicroseconds;
  MkrSineChopperTcc.start(cycleMicroseconds, duty1024, numChops, isrCycleEndCallback);
  
  // turn led on
  pinMode(PWM_LED_PIN, OUTPUT);
  digitalWrite(PWM_LED_PIN, HIGH);
}

void MachineSingleton::disablePWM()
{
  // turn led off
  digitalWrite(PWM_LED_PIN, LOW);
  
  MkrSineChopperTcc.stop();
  _machineMode = MachineModeDisabled;
}

bool MachineSingleton::isPWMEnabled()
{
  return (_machineMode != MachineModeDisabled);
}

void MachineSingleton::startNewRecording()
{
  _machineMode = MachineModeStarting;
}

bool MachineSingleton::hasFinishedRecording()
{
  return (_machineMode == MachineModeFinished);
}

bool MachineSingleton::recordNewBatchAndSendToUser(uint8_t analogPin)
{
  if(_machineMode == MachineModeDisabled) 
    return UserCommandHandler.handleUserCommandError("Can't record ADC, PWM is not enabled");
    
  static int arduinoPins[] = { A0, A1, A2, A3, A4, A5, A6 };  
  if(analogPin < 0 || analogPin > 6)  
    return UserCommandHandler.handleUserCommandError("PIN number of of range [0, 6]");
  int arduinoPin = arduinoPins[analogPin];
    
  MkrADC.enableConvertorInFreeRunningMode(arduinoPin, _cycleMicroseconds);
  startNewRecording();

  // wait while batch is recording     
  while(_machineMode != MachineModeFinished) delay(0);

  MkrADC.disableConvertor();
   
  // produce a string of space separated sample values
  _replyBuffer[0] = '\0';
  int n = MkrADC.GetNumberOfSamples();
  for(int i=0; i<n; i++) {
    char part[10];
    itoa(MkrADC.GetSampleValue(i), part, 10);  
    if(i < (n - 1)) strcat(part, " ");
    strcat(_replyBuffer, part);
  }     
  
  return UserCommandHandler.handleUserCommandSuccess(_replyBuffer);
}

static void isrCycleEndCallback()
{
  switch(_machineMode) {
    
    case MachineModeStarting:
      _machineMode = MachineModeRunning;
      MkrADC.startCollectingSamples();
      break;
      
    case MachineModeRunning:
      MkrADC.stopCollectingSamples();
      _machineMode = MachineModeFinished;
      break;
  }
}
