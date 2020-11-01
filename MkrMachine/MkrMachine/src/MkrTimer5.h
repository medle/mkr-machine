/*
 * UnoTimer1.h
 * Arduino MKR Zero 16-bit Timer/Counter5 methods.
 * Created: 11.07.2020
 * Author: SL
 */ 
#ifndef MKRTIMER5_H_
#define MKRTIMER5_H_

#include <Arduino.h>

// Arduino MKR Zero Timer5 outputs PWM to D5~ pin.
#define MKRTIMER5_PIN D5

class MkrTimer5Singleton {
    public:
      int convertHertzToMicroseconds(int hertz);
      void start(int periodMicros, int duty1024 = 0, void (*isrCallback)() = 0);
      void stop();
      
      int getPrescaler();
      int getPeriod();
      int getMatch();
};

extern MkrTimer5Singleton MkrTimer5;

#endif /* MKRTIMER5_H_ */
