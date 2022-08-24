/*
 * MkrSineChopperTcc.cpp
 *
 * Created: 04.05.2021 19:06:43
 * Author: SL
 */ 

#include <Arduino.h>
#include <math.h>

#include "tcc\tcc.h"
#include "tcc\tcc_callback.h"
#include "events\events.h"

#include "MkrSineChopperTcc.h"
#include "MkrUtil.h"

// global single instance
__MkrSineChopperTcc MkrSineChopperTcc;

// local TCC modules used 
static struct tcc_module _tcc0;
static struct tcc_module _tcc1;
static bool _isEnabled = false;

// TOP value used for double slope counting
uint32_t _numClocksPerHalfCycle;
uint32_t _chopTopValue;

#define MAX_DEAD_CLOCKS 200

// table of precomputed match values used as a sequence of varying duty-cycle values
#define MAX_CHOPS_PER_HALF_CYCLE 20
static uint32_t _chopMatchValues[MAX_CHOPS_PER_HALF_CYCLE];
static volatile int _currentChopIndex;
static int _numChopsPerHalfCycle;

// TCCx timer callback functions
static void endOfHalfCycleCallback(struct tcc_module *const tcc);
static void endOfChopCallback(struct tcc_module *const tcc);
static int _callbackCounter = 0;
static volatile bool _currentlyAtFirstHalfCycle = false;

// user callback function to be fired at the end of each cycle
static void (*_userSpecifiedCycleEndCallback)();
#define DEBUG_CALLBACKS 0

// local functions
static void precomputeChopMatchValues(int cyclesPerSecond, int chopsPerCycle, int percentage);
static void configureTCC1_FullHigh(int duty1024);
static void configureTCC1_FullHigh_DS(int duty1024, int deadClocks);
static void configureTCC0forChopping();
static void configureTCC0forPulsing(int duty1024);
static void configureTCC0forPulsing_DS(int duty1024);
static void startTimersSimultaneously();

void __MkrSineChopperTcc::initPins()
{
  pinMode(0, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
}

int __MkrSineChopperTcc::start(int cycleMicroseconds, 
  int dutyCycle1024, int deadClocks, void (*cycleEndCallback)())
{
  if(cycleMicroseconds < 1 || cycleMicroseconds > 0x00ffffff) return 1;
  if(deadClocks < 0 || deadClocks > MAX_DEAD_CLOCKS) return 1;
  if(dutyCycle1024 < 0 || dutyCycle1024 > 1023) return 1;
  
  if(_isEnabled) stop();

  _userSpecifiedCycleEndCallback = cycleEndCallback;

  precomputeChopMatchValues(cycleMicroseconds, 0, dutyCycle1024);

  configureTCC0forPulsing_DS(dutyCycle1024);
  configureTCC1_FullHigh_DS(dutyCycle1024, deadClocks);
  
  startTimersSimultaneously();

  // using this simple way timers will start not at the same time
  //tcc_enable(&_tcc0);
  //tcc_enable(&_tcc1);
  
  _isEnabled = true;
  return 0;
}

// Configure 24-bit TCC1 as "high-side" LEFT and RIGHT signals.
// Version 3: high side is open all the time except when the 
// low side is open (dual-slope version with dead-time insertion).
static void configureTCC1_FullHigh_DS(int dutyCycle1024, int deadClocks)
{
  // there are two periods in each cycle for dual-slope operation
  uint32_t periodValue = _numClocksPerHalfCycle / 2;

  // output is HIGH when COUNTER is above the MATCH value in dual-slope operation
  uint32_t halfPulseWidth = periodValue * dutyCycle1024 / 1023;
  uint32_t matchValue = periodValue - halfPulseWidth;
  
  // dead-time insertion makes pulse begin earlier and end later
  if(matchValue > deadClocks) matchValue -= deadClocks; 
  
  struct tcc_config config_tcc;
  tcc_get_config_defaults(&config_tcc, TCC1);
  config_tcc.counter.period = periodValue;
  config_tcc.compare.wave_generation = TCC_WAVE_GENERATION_DOUBLE_SLOPE_BOTTOM;

  config_tcc.compare.match[0] = matchValue;
  config_tcc.pins.enable_wave_out_pin[0] = true;
  config_tcc.pins.wave_out_pin[0]        = PIN_PA10E_TCC1_WO0; // D2 on MKR ZERO
  config_tcc.pins.wave_out_pin_mux[0]    = MUX_PA10E_TCC1_WO0;

  config_tcc.compare.match[1] = matchValue;
  config_tcc.pins.enable_wave_out_pin[1] = true;
  config_tcc.pins.wave_out_pin[1]        = PIN_PA11E_TCC1_WO1; // D3 on MKR ZERO
  config_tcc.pins.wave_out_pin_mux[1]    = MUX_PA11E_TCC1_WO1;

  // invert signal so when low switch is open high switch is always off
  config_tcc.wave_ext.invert[0] = true;
  config_tcc.wave_ext.invert[1] = true;

  // RAMP2 operation: in cycle A, odd channel output (_WO1) is disabled, and in cycle B,
  // even channel output (_WO0) is disabled. The ramp cycle changes after each update.
  config_tcc.compare.wave_ramp = TCC_RAMP_RAMP2;
  
  expect0(tcc_init(&_tcc1, TCC1, &config_tcc));
}

// Configure 24-bit TCC1 as "high-side" LEFT and RIGHT signals.
// Version 2: high side is open all the time except when the low side is open.
static void configureTCC1_FullHigh(int dutyCycle1024)
{
  // single-slope frequency = F_CPU / (TOP + 1) so we need to subtract
  // one cycle from TOP to get exact match of frequency with double-slope
  // operation of the second timer
  uint32_t periodValue = (_numClocksPerHalfCycle - 1);
  uint32_t matchValue = _numClocksPerHalfCycle * dutyCycle1024 / 1023;
  if(matchValue > periodValue) matchValue = periodValue;
  
  struct tcc_config config_tcc;
  tcc_get_config_defaults(&config_tcc, TCC1);
  config_tcc.counter.period = periodValue;
  config_tcc.compare.wave_generation = TCC_WAVE_GENERATION_SINGLE_SLOPE_PWM;
  
  config_tcc.compare.match[0] = matchValue;
  config_tcc.pins.enable_wave_out_pin[0] = true;
  config_tcc.pins.wave_out_pin[0]        = PIN_PA10E_TCC1_WO0; // D2 on MKR ZERO
  config_tcc.pins.wave_out_pin_mux[0]    = MUX_PA10E_TCC1_WO0;

  config_tcc.compare.match[1] = matchValue;
  config_tcc.pins.enable_wave_out_pin[1] = true;
  config_tcc.pins.wave_out_pin[1]        = PIN_PA11E_TCC1_WO1; // D3 on MKR ZERO
  config_tcc.pins.wave_out_pin_mux[1]    = MUX_PA11E_TCC1_WO1;

  // invert signal so when low switch is open high switch is always off
  config_tcc.wave_ext.invert[0] = true;
  config_tcc.wave_ext.invert[1] = true;

  // RAMP2 operation: in cycle A, odd channel output (_WO1) is disabled, and in cycle B, 
  // even channel output (_WO0) is disabled. The ramp cycle changes after each update.
  config_tcc.compare.wave_ramp = TCC_RAMP_RAMP2;
  
  expect0(tcc_init(&_tcc1, TCC1, &config_tcc));
}

static void configureTCC0outputPin(struct tcc_config *config, uint32_t match)
{
  // Of all the options possible for the TCC0 output pin:
  // PA04 - A3 - floats when not on timer
  // PA08 - D11 - pulled up, has high value when not on timer
  // PA05 - A4 - floats when not on timer
  // PA09 - D12 - pulled up, has high value when not on timer
  // PA10 - D2 - used in TCC1
  // PA18 - not available
  // PA11 - D3 - used in TCC1
  // PA19 - MISO - floats when not on timer
  // PA14 - not available
  // PA22 - D0 - digital output gives safe ground when unused

  config->compare.match[0] = match;
  config->pins.enable_wave_out_pin[0] = true;
  config->pins.wave_out_pin[0]        = PIN_PA22F_TCC0_WO4; // D0 on MKR-ZERO
  config->pins.wave_out_pin_mux[0]    = MUX_PA22F_TCC0_WO4;
}

// Configure 24-bit TCC0 as "low-side" signal for single pulse 
// with given duty-cycle (Dual-slope version version).
static void configureTCC0forPulsing_DS(int dutyCycle1024)
{
  // there are two periods in each cycle for dual-slope operation
  uint32_t periodValue = _numClocksPerHalfCycle / 2;

  // output is HIGH when COUNTER is above the MATCH value in dual-slope operation
  uint32_t halfPulseWidth = periodValue * dutyCycle1024 / 1023;
  uint32_t matchValue = periodValue - halfPulseWidth;

  _currentlyAtFirstHalfCycle = true;
  
  struct tcc_config config_tcc;
  tcc_get_config_defaults(&config_tcc, TCC1);
  config_tcc.counter.period = periodValue;
  config_tcc.compare.wave_generation = TCC_WAVE_GENERATION_DOUBLE_SLOPE_BOTTOM;
  
  configureTCC0outputPin(&config_tcc, matchValue);
  expect0(tcc_init(&_tcc0, TCC0, &config_tcc));
  
  expect0(tcc_register_callback(&_tcc0, endOfHalfCycleCallback, TCC_CALLBACK_OVERFLOW));
  tcc_enable_callback(&_tcc0, TCC_CALLBACK_OVERFLOW);
}

// Configure 24-bit TCC0 as "low-side" signal for single pulse with given duty-cycle.
static void configureTCC0forPulsing(int dutyCycle1024)
{
  // single-slope frequency = F_CPU / (TOP + 1) so we need to subtract 
  // one cycle from TOP to get exact match of frequency with double-slope
  // operation of the second timer
  uint32_t period = (_numClocksPerHalfCycle - 1);
  uint32_t match = _numClocksPerHalfCycle * dutyCycle1024 / 1023;
  if(match > period) match = period;

  _currentlyAtFirstHalfCycle = true;

  // single-slope PWM mode with up counting: output active at ZERO and cleared 
  // on compare MATCH, so the pulse appears at the start of each cycle,
  // fires overflow callback at TOP=period
  struct tcc_config config_tcc;
  tcc_get_config_defaults(&config_tcc, TCC0);
  config_tcc.counter.period = period;
  config_tcc.compare.wave_generation = TCC_WAVE_GENERATION_SINGLE_SLOPE_PWM;

  configureTCC0outputPin(&config_tcc, match);  
  expect0(tcc_init(&_tcc0, TCC0, &config_tcc));
  
  expect0(tcc_register_callback(&_tcc0, endOfHalfCycleCallback, TCC_CALLBACK_OVERFLOW));
  tcc_enable_callback(&_tcc0, TCC_CALLBACK_OVERFLOW);
}

// Configure 24-bit TCC0 as "low-side" signal for chopping with variable duty-cycle.
static void configureTCC0forChopping()
{
  expect0(_numChopsPerHalfCycle == 0);
  
  _currentlyAtFirstHalfCycle = true;
  _currentChopIndex = 0;
  uint32_t firstMatchValue = _chopMatchValues[_currentChopIndex];

  // dual-slope operation to make pulse at the center of the chop:
  // count up from zero to top, then down to bottom zero,
  // output active when counter is above "match value",
  // fires overflow callback at *bottom* (end of second period)
  struct tcc_config config_tcc;
  tcc_get_config_defaults(&config_tcc, TCC0);
  config_tcc.counter.period = _chopTopValue;
  config_tcc.compare.wave_generation = TCC_WAVE_GENERATION_DOUBLE_SLOPE_BOTTOM;
  
  configureTCC0outputPin(&config_tcc, firstMatchValue);
  expect0(tcc_init(&_tcc0, TCC0, &config_tcc));

  expect0(tcc_register_callback(&_tcc0, endOfChopCallback, TCC_CALLBACK_OVERFLOW));
  tcc_enable_callback(&_tcc0, TCC_CALLBACK_OVERFLOW);
}

// Start the two timers from the same clock using MCU event system.
static void startTimersSimultaneously()
{
  struct events_resource eventResource;
  struct events_config eventResourceConfig;
  events_get_config_defaults(&eventResourceConfig);
  expect0(events_allocate(&eventResource, &eventResourceConfig));
  expect0(events_attach_user(&eventResource, EVSYS_ID_USER_TCC0_EV_0));
  expect0(events_attach_user(&eventResource, EVSYS_ID_USER_TCC1_EV_0));
  
  struct tcc_events eventActionConfig;
  memset(&eventActionConfig, 0, sizeof(eventActionConfig));
  eventActionConfig.on_input_event_perform_action[0] = true;
  eventActionConfig.input_config[0].modify_action = true;
  eventActionConfig.input_config[0].action = (tcc_event_action)TCC_EVENT0_ACTION_START;
  
  expect0(tcc_enable_events(&_tcc0, &eventActionConfig));
  expect0(tcc_enable_events(&_tcc1, &eventActionConfig));

  tcc_enable(&_tcc0);
  tcc_stop_counter(&_tcc0);
  tcc_set_count_value(&_tcc0, 0);

  tcc_enable(&_tcc1);
  tcc_stop_counter(&_tcc1);
  tcc_set_count_value(&_tcc1, 0);
  
  _currentChopIndex = 0;

  // trigger the ACTION_START event by software  
  while(events_is_busy(&eventResource)); 
  expect0(events_trigger(&eventResource)); 

  // cleanup   
  while(events_is_busy(&eventResource)); 
  //tcc_disable_events(&_tcc0, &events); // tcc_reset() at stop() will do it anyway
  //tcc_disable_events(&_tcc1, &events); // may be done only when timer is not enabled,
  expect0(events_detach_user(&eventResource, EVSYS_ID_USER_TCC0_EV_0));
  expect0(events_detach_user(&eventResource, EVSYS_ID_USER_TCC1_EV_0));
  expect0(events_release(&eventResource));
}

void __MkrSineChopperTcc::stop()
{
  if(_isEnabled) {
    _isEnabled = false;
    
    tcc_reset(&_tcc0);
    tcc_reset(&_tcc1);
    
    tcc_disable_callback(&_tcc0, TCC_CALLBACK_OVERFLOW);
    tcc_unregister_callback(&_tcc0, TCC_CALLBACK_OVERFLOW);
    
    pinMode(PIN_SPI1_MISO, OUTPUT);
    pinMode(A3, OUTPUT);
    
  }
}

// at the end of each second half-cycle run a user's cycle callback
static void handleEndOfHalfCycle()
{
  bool atFirst = _currentlyAtFirstHalfCycle;
  _currentlyAtFirstHalfCycle = !atFirst;
  if(!atFirst) {
    if(_userSpecifiedCycleEndCallback != NULL)
      _userSpecifiedCycleEndCallback();
  }
}

static void endOfHalfCycleCallback(struct tcc_module *const tcc)
{
  #if DEBUG_CALLBACKS
  _callbackCounter += 1;
  #endif
  
  handleEndOfHalfCycle();
}  

// This callback is called by TCC0 module at the end of each chop period, after
// counter went up from zero to "top" and returned back down to "bottom" zero.
// NOTE: this handler is very time-sensitive so at the start of the MCU when USB
// setup interrupts are active this callback may miss a call or two. Thus it is 
// found useful to perform a blank "delay(1000)" at the start of the MCU to
// let USB stabilize and not compete for cycles with this handler.
static void endOfChopCallback(struct tcc_module *const tcc)
{
  #if DEBUG_CALLBACKS
  _callbackCounter += 1;
  #endif
  
  // the current chop index advances
  if(++_currentChopIndex == _numChopsPerHalfCycle) {
    _currentChopIndex = 0;
  }

  // Because writing of "compare value" is done in a double-buffered mode
  // the value we will write now will get into compare register not 
  // immediately but only at the end of the current chop. So we choose
  // the match value not for the "current" chop but for the "next" chop. 
  // This double-buffering allows to have enough time for setting new compare 
  // value in "relatively slow" callback routine avoiding wave-distortion 
  // effects when writing occurs in "race condition" with the TCC counter.
  int nextIndex = _currentChopIndex + 1;
  if(nextIndex == _numChopsPerHalfCycle) nextIndex = 0;
  uint32_t nextMatchValue = _chopMatchValues[nextIndex];  
  tcc_set_compare_value(&_tcc0, (tcc_match_capture_channel)0, nextMatchValue);
  
  if(_currentChopIndex == 0) handleEndOfHalfCycle();
}

// Writes an array of the "match" values for individual chops in a sequence of sine wave generation.
// The idea is as follows: for each chop we want the time when current is on be just such as to
// pass power equal in amount as a true sine wave generator.
static void precomputeChopMatchValues(int cycleMicroseconds, int chopsPerHalfCycle, int dutyCycle1024)
{
  // compute the period for TCC as clocks per chop / 2 for double slope counting
  uint32_t clocksPerCycle = convertCycleMicrosecondsToClocksPerCycle(cycleMicroseconds);
  _numClocksPerHalfCycle = clocksPerCycle / 2;
  _numChopsPerHalfCycle = chopsPerHalfCycle;
  
  // special case when chopping is disabled
  if(chopsPerHalfCycle == 0) {
    _chopTopValue = 0;
    return;
  }
  
  // there are two [bottom-top][top-bottom] periods in each chop for double-slope operation
  _chopTopValue = _numClocksPerHalfCycle / (_numChopsPerHalfCycle * 2);
  
  // update the cycle length in clocks after cycle is divided on (half)chops
  _numClocksPerHalfCycle = (_chopTopValue * 2 * _numChopsPerHalfCycle); 
  
  float chopCx = PI / _numChopsPerHalfCycle; // angle of one chop in radians

  // As both half-cycles of wave are the same we recalculate only the first half-cycle,
  // the sign of the wave is handled by TCC2 "direction" signal.
  for(int i = 0; i < _numChopsPerHalfCycle; i++) {

    float x1 = i * chopCx; // angle where chop begins
    float x2 = x1 + chopCx; // angle where chop ends
    float chopS = cos(x1) - cos(x2); // square of chop sine graph between x1 and x2
    float fillFactor = chopS / chopCx; // divide sine wave chop square by 100% chop square
    if(dutyCycle1024 != 1023) fillFactor = fillFactor * dutyCycle1024 / 1023;

    // Output will be active when counter is above match, so fill factor should be inverted,
    // for example when fill factor is 60% match value should be 40% thus there will be 60% 
    // time counter will be above match value.
    int matchValue = (int)(_chopTopValue * (1 - fillFactor));
    _chopMatchValues[i] = matchValue;
  }
}

// Debug print method.
void __MkrSineChopperTcc::printValues()
{
  #if DEBUG_CALLBACKS
  Serial.print("callbacks=");
  Serial.print(_callbackCounter);
  #endif
  
  for(int i=0; i<_numChopsPerHalfCycle; i++) {
    Serial.print(" ");
    Serial.print(i);
    Serial.print("=");
    Serial.print((float)(100 - (_chopMatchValues[i] * 100 / _chopTopValue)));
    Serial.print("%");
  }

  if(_chopTopValue > 0) {
    Serial.print(" chopTOP=");
    Serial.print(_chopTopValue);
  }  
  
  Serial.print(" clocksPerHalfCycle=");
  Serial.print(_numClocksPerHalfCycle);
}
