/*
 * UserCommandHandler.h
 * Created: 13.05.2020
 * Author: SL
 */ 

#ifndef USERCOMMANDHANDLER_H_
#define USERCOMMANDHANDLER_H_

#include "UserCommandParser.h"

class UserCommandHandlerSingleton
{
  public:
    bool handleUserCommand(UserCommand *pc);
    bool handleUserCommandSuccess(char *message);
    bool handleUserCommandSyntaxError(char *text);
    bool handleUserCommandError(char *message, char *param=NULL);
};

extern UserCommandHandlerSingleton UserCommandHandler;

#endif /* USERCOMMANDHANDLER_H_ */