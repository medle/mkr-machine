/*
 * UserCommandParser.h
 * Created: 11.05.2020
 * Author: SL
 */ 

#ifndef USERCOMMANDPARSER_H_
#define USERCOMMANDPARSER_H_

enum {
  HELLO_COMMAND_ID,
  VERSION_COMMAND_ID,
  PWM_COMMAND_ID,
  ADC_COMMAND_ID,
  STOP_COMMAND_ID
};

struct UserCommand 
{
  byte commandId;
  int parameter1;
  int parameter2;
  int parameter3;
};

class UserCommandParserSingleton 
{
  public:
    void processNextChar(char ch);
};

extern UserCommandParserSingleton UserCommandParser;

#endif /* USERCOMMANDPARSER_H_ */