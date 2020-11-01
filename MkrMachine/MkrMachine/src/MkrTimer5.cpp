/*
 * MkrTimer5.cpp
 * Arduino MKR Zero 16-bit TimerCounter5 methods.
 * Created: 11.07.2020 
 * Author: SL
 */ 
#include <Arduino.h>
#include "MkrTimer5.h"

#include "tc\tc.h" // ASF Timer driver
#include "tc\tc_interrupt.h"

// global single instance
MkrTimer5Singleton MkrTimer5;

// TimerCounter5 16-bit resolution.
#define MAX_PERIOD_VALUE 0xFFFF

#define MICROS_PER_SECOND 1000000

// attached user callback routine
static void (*_isrUserCallback)();
static void _tc5_isr(tc_module *const module)
{
  _isrUserCallback();
}

// ASF/TC driver assets
static bool _is_enabled = false;
static struct tc_config _config_tc;
static struct tc_module _tc_instance;

void MkrTimer5Singleton::start(int periodMicros, int duty1024, void (*isrCallback)())
{
  if(periodMicros <= 0) return;
  
  stop();

  // choose the prescaler to slow down the timer when period value is greater than max period value
  tc_clock_prescaler prescaler;
  uint32_t periodValue = (F_CPU / MICROS_PER_SECOND) * periodMicros;
  if(periodValue <= MAX_PERIOD_VALUE) prescaler = TC_CLOCK_PRESCALER_DIV1;
  else if((periodValue >>= 1) <= MAX_PERIOD_VALUE) prescaler = TC_CLOCK_PRESCALER_DIV2; 
  else if((periodValue >>= 1) <= MAX_PERIOD_VALUE) prescaler = TC_CLOCK_PRESCALER_DIV4;
  else if((periodValue >>= 1) <= MAX_PERIOD_VALUE) prescaler = TC_CLOCK_PRESCALER_DIV8;
  else if((periodValue >>= 1) <= MAX_PERIOD_VALUE) prescaler = TC_CLOCK_PRESCALER_DIV16;
  else if((periodValue >>= 2) <= MAX_PERIOD_VALUE) prescaler = TC_CLOCK_PRESCALER_DIV64;
  else if((periodValue >>= 2) <= MAX_PERIOD_VALUE) prescaler = TC_CLOCK_PRESCALER_DIV256;
  else if((periodValue >>= 2) <= MAX_PERIOD_VALUE) prescaler = TC_CLOCK_PRESCALER_DIV1024;

  // load default configuration values
  tc_get_config_defaults(&_config_tc);
  
  _config_tc.clock_prescaler = prescaler;
  _config_tc.counter_size    = TC_COUNTER_SIZE_16BIT;
  _config_tc.wave_generation = TC_WAVE_GENERATION_MATCH_PWM;
  _config_tc.count_direction = TC_COUNT_DIRECTION_UP;
  
  // enable inversion on output channel 1 to force pulse appear at the start of the cycle,
  // default behavior without inversion produces pulse at the end of the cycle
  bool invertWave = true;
  if(invertWave) _config_tc.waveform_invert_output = TC_WAVEFORM_INVERT_CC1_MODE; 

  // configure PWM pin, use channel 1 hard-wired with PB11 pin 
  if(duty1024 > 0 && duty1024 < 1024) {
    int effectiveDuty = (invertWave) ? (1023 - duty1024) : duty1024;
    uint32_t matchValue = (periodValue * effectiveDuty / 1023);
    
    int pwm_channum = TC_COMPARE_CAPTURE_CHANNEL_1;
    _config_tc.pwm_channel[pwm_channum].enabled = true;
    _config_tc.pwm_channel[pwm_channum].pin_out = PIN_PB11E_TC5_WO1; // PB11 => D5~ (on MKR Zero)
    _config_tc.pwm_channel[pwm_channum].pin_mux = MUX_PB11E_TC5_WO1; 

    // configure PWM period value and duty cycle value
    _config_tc.counter_16_bit.compare_capture_channel[TC_COMPARE_CAPTURE_CHANNEL_0] = periodValue;
    _config_tc.counter_16_bit.compare_capture_channel[TC_COMPARE_CAPTURE_CHANNEL_1] = matchValue;
  }    

  // initialize TimerCounter5
  tc_init(&_tc_instance, TC5, &_config_tc);

  // maybe attach a user callback routine
  if(isrCallback != NULL) {
    _isrUserCallback = isrCallback;
    tc_callback cb_type = TC_CALLBACK_CC_CHANNEL1;
    tc_register_callback(&_tc_instance, _tc5_isr, cb_type);
    tc_enable_callback(&_tc_instance, cb_type);
  }
  
  // start timer 
  _is_enabled = true;
  tc_enable(&_tc_instance);
}

void MkrTimer5Singleton::stop()
{
  if(_is_enabled) {
    _is_enabled = false;
    tc_disable(&_tc_instance);  

    if(_isrUserCallback != NULL) {     
      _isrUserCallback = NULL;
      
      tc_callback cb_type = TC_CALLBACK_CC_CHANNEL1;
      tc_disable_callback(&_tc_instance, cb_type);
      tc_unregister_callback(&_tc_instance, cb_type);
    }    
  }    
}

int MkrTimer5Singleton::convertHertzToMicroseconds(int hertz)
{
  if(hertz <= 0) return 0;
  return (MICROS_PER_SECOND / hertz);
}

int MkrTimer5Singleton::getPrescaler() 
{ 
  return _config_tc.clock_prescaler; 
}

int MkrTimer5Singleton::getPeriod() 
{ 
  return _config_tc.counter_16_bit.compare_capture_channel[TC_COMPARE_CAPTURE_CHANNEL_0]; 
}

int MkrTimer5Singleton::getMatch() 
{ 
  return _config_tc.counter_16_bit.compare_capture_channel[TC_COMPARE_CAPTURE_CHANNEL_1]; 
}

/*
 * End of MkrTimer5.cpp
 */
