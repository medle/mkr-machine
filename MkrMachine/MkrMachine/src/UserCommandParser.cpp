/*
 * UserCommandParser.cpp
 * Created: 11.05.2020
 * Author: SL
 */ 

#include <Arduino.h>
#include "UserCommandParser.h"
#include "UserCommandHandler.h"

UserCommandParserSingleton UserCommandParser;
static UserCommand _userCommand;

#define HELLO_COMMAND_NAME "HELLO"
#define VERSION_COMMAND_NAME "VERSION"
#define PWM_COMMAND_NAME "PWM"
#define ADC_COMMAND_NAME "ADC"
#define STOP_COMMAND_NAME "STOP"

#define MAX_BUFFER 32
static char _buffer[MAX_BUFFER];
static uint8_t _bufferSize = 0;

static bool CompareChars(char *p1, char *p2, int n)
{
  for(int i=0; i<n; i++) {
    if(p1[i] != p2[i]) return false;
  }
  return true;
}

static int CountSpaces(char *p)
{
  int n = 0;
  while(p[n] == ' ') n++;
  return n;
}

static int ParseNumberAfterSpaces(char *p, int *pNumber)
{
  int numSpaces = CountSpaces(p);
  if(numSpaces < 1) return 0;
  
  int numDigits = 0;
  long n = 0;
  
  char *digits = p + numSpaces;
  for(;;) {
    char ch = digits[numDigits];
    if(ch < '0' || ch > '9') break;
    n = (n * 10) + (ch - '0');
    numDigits += 1;
  }

  if(numDigits == 0) return 0;  
  
  *pNumber = n;
  return (numSpaces + numDigits);
}

static bool ParseCommand(char *commandName, int commandId, int numParams)
{
  int commandLength = strlen(commandName);
  if(CompareChars(_buffer, commandName, commandLength)) {
    
    _userCommand.commandId = commandId;
    if(numParams == 0) return true;
    
    char *inside = _buffer + commandLength;
    int length = ParseNumberAfterSpaces(inside, &_userCommand.parameter1);
    if(length < 1) return false;
    if(numParams == 1) return true;
    
    inside += length;
    length = ParseNumberAfterSpaces(inside, &_userCommand.parameter2);
    if(length < 1) return false;
    if(numParams == 2) return true;
  }

  return false;  
}

static bool ParseCommandInBuffer()
{
  // parse ADC command with one parameter
  if(ParseCommand(ADC_COMMAND_NAME, ADC_COMMAND_ID, 1)) return true;

  // parse PWM command with two parameters 
  if(ParseCommand(PWM_COMMAND_NAME, PWM_COMMAND_ID, 2)) return true;

  // commands without parameters
  if(ParseCommand(STOP_COMMAND_NAME, STOP_COMMAND_ID, 0)) return true;
  if(ParseCommand(VERSION_COMMAND_NAME, VERSION_COMMAND_ID, 0)) return true;
  if(ParseCommand(HELLO_COMMAND_NAME, HELLO_COMMAND_ID, 0)) return true;
  
  return false;
}

void UserCommandParserSingleton::processNextChar(char ch)
{
  // ignore caret return
  if(ch == '\r' || ch == 0) {
    return;
  }
  
  // at the end of line
  if(ch == '\n') {
    
    // ugly hack, ignore empty lines, when new client connects we sometimes
    // get empty line which client didn't send and probing fails.
    if(_bufferSize == 0) return; 
    
    _buffer[_bufferSize] = 0;
    
    // analyze buffer
    if(ParseCommandInBuffer()) {
      UserCommandHandler.handleUserCommand(&_userCommand);
    } else {
      UserCommandHandler.handleUserCommandSyntaxError(_buffer);
    }
    
    _bufferSize = 0;
    return;
  } 
  
  if(_bufferSize == (MAX_BUFFER - 1)) {
    // ignore data from too long an input
    return;
  } 

  // ignore all 8-bit characters, strangely when new client connects 
  // we sometimes get four 0xF0 chars and device probing fails.
  if(ch & (1 << 7)) {
    return;
  }
  
  // store next char for later processing
  _buffer[_bufferSize] = ch;
  _bufferSize += 1;
}

/*
 * End of UserCommandParser.cpp
 */