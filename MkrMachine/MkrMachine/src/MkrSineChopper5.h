/*
 * MkrSineChopper5.h
 * Created: 01.11.2020 22:34:19
 * Author: SL
 */ 
 
#ifndef MKRSINECHOPPER5_H_
#define MKRSINECHOPPER5_H_

// Arduino MKR Zero Timer5 outputs PWM to D5~ pin.
class MkrSineChopper5Singleton {
  public:
  int convertHertzToMicroseconds(int hertz);
  void start(int periodMicros, int duty1024 = 0, void (*zeroCrossCallback)(bool) = 0);
  void stop();
};

extern MkrSineChopper5Singleton MkrSineChopper5;

#endif /* MKRSINECHOPPER5_H_ */