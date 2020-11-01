/*
 * Machine.cpp
 * Created: 13.05.2020
 * Author: SL
 */ 
#include <Arduino.h>
#include "MkrTimer5.h"
#include "MkrADC.h"
#include "Machine.h"
#include "UserCommandHandler.h"

#define PIN_MACHINE_ENABLED 6

MachineSingleton Machine;

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
static int _periodUsec;

static void _isrPeriodEndCallback();

void MachineSingleton::enablePWM(int usec, int duty1024)
{
  _machineMode = MachineModeEnabled;
  _periodUsec = usec;
  MkrTimer5.start(usec, duty1024, _isrPeriodEndCallback);
  
  // enable builtin led and "enabled" pin
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(PIN_MACHINE_ENABLED, OUTPUT);
  digitalWrite(PIN_MACHINE_ENABLED, HIGH);
}

void MachineSingleton::disablePWM()
{
  // disable builtin led, and "enabled" pin 
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(PIN_MACHINE_ENABLED, LOW);
  
  MkrTimer5.stop();
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
    
  MkrADC.enableConvertorInFreeRunningMode(arduinoPin, _periodUsec);
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

static void _isrPeriodEndCallback()
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
