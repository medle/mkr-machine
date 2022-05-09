/*
 * CommandHandler.cpp
 * Created: 13.05.2020
 * Author: SL
 */ 
#include <Arduino.h>
#include "UserCommandHandler.h"
#include "Machine.h"

#define PROGRAM_NAME "InductorMachine"

UserCommandHandlerSingleton UserCommandHandler;

bool UserCommandHandlerSingleton::handleUserCommand(UserCommand *pCommand)
{
  if(pCommand->commandId == STOP_COMMAND_ID) {
    Machine.disablePWM();
    return handleUserCommandSuccess("PWM/ADC is disabled.");
  }
  
  if(pCommand->commandId == ADC_COMMAND_ID) {
    int pin = pCommand->parameter1;
    if(pin < 0 || pin > 5) 
      return handleUserCommandError("ADC pin number out of range [0,5]");
    
    return Machine.recordNewBatchAndSendToUser(pin);
  }
  
  if(pCommand->commandId == PWM_COMMAND_ID) {
    int cycleMicroseconds = pCommand->parameter1;
    if(cycleMicroseconds < 10)
      return handleUserCommandError("cycle microseconds < 10");
    int duty1024 = pCommand->parameter2;
    if(duty1024 >= 1024)
      return handleUserCommandError("duty cycle out of range [0,1023]");
    int numChops = pCommand->parameter3;
    if(numChops < 0 || numChops > 20)
      return handleUserCommandError("numChops value out of range [0, 20]");

    Machine.enablePWM(cycleMicroseconds, duty1024, numChops);
    return handleUserCommandSuccess("PWM (ADC running) is enabled.");
  }

  if(pCommand->commandId == VERSION_COMMAND_ID) {
    return handleUserCommandSuccess(PROGRAM_NAME " (" __DATE__ ").");
  }

  if(pCommand->commandId == HELLO_COMMAND_ID) {
    return handleUserCommandSuccess(PROGRAM_NAME " ready.");
  }
  
  return handleUserCommandError("unknown command id");
}

bool UserCommandHandlerSingleton::handleUserCommandSuccess(char *message)
{
  Serial.print("OK: ");
  if(message != NULL) Serial.print(message);
  Serial.println();
  return true;
}

bool UserCommandHandlerSingleton::handleUserCommandSyntaxError(char *text)
{
  return handleUserCommandError("Syntax error", text);
}

bool UserCommandHandlerSingleton::handleUserCommandError(char *message, char *param)
{
  Serial.print("ERR: ");
  if(message != NULL) Serial.print(message);
  if(param != NULL) {
    Serial.print(" [");
    Serial.print(param);
    Serial.print("]");
  }
  Serial.println("");
  return false;
}
