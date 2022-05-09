/*
 * MkrTimerUtil.cpp
 *
 * Created: 01.11.2020 22:51:23
 *  Author: SL
 */ 
#include <Arduino.h>
#include "tc\tc.h" // ASF Timer driver
#include "tc\tc_interrupt.h"

#include "MkrUtil.h"
#include "MkrTimerUtil.h"

#define MAX_PERIOD_VALUE_16BIT 0xFFFF // 16-bit resolution

int chooseTcClockPrescalerAndPeriodFor16BitTimer(int periodMicros, uint32_t *outPeriodValue)
{
  tc_clock_prescaler prescaler;
  uint32_t periodValue = convertCycleMicrosecondsToClocksPerCycle(periodMicros);
  
  if(periodValue <= MAX_PERIOD_VALUE_16BIT) prescaler = TC_CLOCK_PRESCALER_DIV1;
  else if((periodValue >>= 1) <= MAX_PERIOD_VALUE_16BIT) prescaler = TC_CLOCK_PRESCALER_DIV2;
  else if((periodValue >>= 1) <= MAX_PERIOD_VALUE_16BIT) prescaler = TC_CLOCK_PRESCALER_DIV4;
  else if((periodValue >>= 1) <= MAX_PERIOD_VALUE_16BIT) prescaler = TC_CLOCK_PRESCALER_DIV8;
  else if((periodValue >>= 1) <= MAX_PERIOD_VALUE_16BIT) prescaler = TC_CLOCK_PRESCALER_DIV16;
  else if((periodValue >>= 2) <= MAX_PERIOD_VALUE_16BIT) prescaler = TC_CLOCK_PRESCALER_DIV64;
  else if((periodValue >>= 2) <= MAX_PERIOD_VALUE_16BIT) prescaler = TC_CLOCK_PRESCALER_DIV256;
  else if((periodValue >>= 2) <= MAX_PERIOD_VALUE_16BIT) prescaler = TC_CLOCK_PRESCALER_DIV1024;
  else prescaler = TC_CLOCK_PRESCALER_DIV1024; 

  *outPeriodValue = periodValue;  
  return prescaler;
}
