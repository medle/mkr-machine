/*
 * MkrTimerUtil.h
 *
 * Created: 01.11.2020 22:43:06
 *  Author: SL
 */ 

#ifndef MKRTIMERUTIL_H_
#define MKRTIMERUTIL_H_

int convertHertzToMicroseconds(int hertz);
int chooseTcClockPrescalerAndPeriodFor16BitTimer(int periodMicros, uint32_t *outPeriodValue);

#endif /* MKRTIMERUTIL_H_ */