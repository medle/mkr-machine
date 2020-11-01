/*
 * MkrADC.cpp
 * Created: 22.07.2020
 * Author: SL
 */ 
#include <Arduino.h>
#include <adc/adc.h> // ASF ADC driver
#include <adc/adc_callback.h>
#include "MkrADC.h"

#define MICROS_PER_SECOND 1000000

// global single instance
MkrADCSingleton MkrADC;

static volatile bool _isCollecting = false;
static bool _isEnabledFreeRunning = false;

static uint8_t _sampleValues[ADC_MAX_SAMPLES];
static int _numSamples = 0;

//=================================================================================
// ASF ADC Documentation
// https://asf.microchip.com/docs/latest/sam0.applications.asf_programmers_manual.atsaml21/html/group__asfdoc__sam0__adc__group.html
static adc_module adc_instance;
//=================================================================================

// ASF ADC driver calls this callback routine at each ADC conversion completed
static void userCallbackADCResultReady(struct adc_module *const module)
{
  if(_isCollecting) {
    if(_numSamples < (ADC_MAX_SAMPLES - 1)) {
      uint8_t value = module->hw->RESULT.reg;
      _sampleValues[_numSamples] = value;
      _numSamples += 1;
    }
  }
}

// Choose ADC prescaler based on PWM frequency, experimental manual values used here.
static adc_clock_prescaler choosePrescaler(uint32_t usecFrameLatency)
{
  // prescaler<32 gives zero reading holes in sequence.
  // Values were choosen to produce around 256 samples on frame of usecFrameLatency.
  if(usecFrameLatency < (MICROS_PER_SECOND / 1200)) return ADC_CLOCK_PRESCALER_DIV32;
  if(usecFrameLatency < (MICROS_PER_SECOND / 600)) return ADC_CLOCK_PRESCALER_DIV64;
  if(usecFrameLatency < (MICROS_PER_SECOND / 300)) return ADC_CLOCK_PRESCALER_DIV128;
  if(usecFrameLatency < (MICROS_PER_SECOND / 150)) return ADC_CLOCK_PRESCALER_DIV256;
  return ADC_CLOCK_PRESCALER_DIV512;
}

// Convert Arduino MKR Zero input pin to ASF ADC driver pin number.
static adc_positive_input convertMkrZeroPinToSamdAin(uint8_t arduinoAnalogPin)
{
  switch(arduinoAnalogPin) {
    // A0 Arduino pin => SAMD21G18 pin AIN0, taken from MKRZero pinout diagram
    case A0: return ADC_POSITIVE_INPUT_PIN0;
    case A1: return ADC_POSITIVE_INPUT_PIN10;
    case A2: return ADC_POSITIVE_INPUT_PIN11;
    case A3: return ADC_POSITIVE_INPUT_PIN4;
    case A4: return ADC_POSITIVE_INPUT_PIN5;
    case A5: return ADC_POSITIVE_INPUT_PIN6;
    case A6: return ADC_POSITIVE_INPUT_PIN7;
    default: return ADC_POSITIVE_INPUT_PIN0;  
  }
}

// Configure and start freerunning conversion mode.
static void configureAndStartFreeRunningADC(uint8_t arduinoAnalogPin, uint32_t usecFrameLatency)
{
  struct adc_config config_adc;
  adc_get_config_defaults(&config_adc);
  
  config_adc.clock_source = GCLK_GENERATOR_0; // frequency 48M
  config_adc.resolution = ADC_RESOLUTION_8BIT; // resolution 8bit gives 300ksps, 10bit gives 250ksps
  config_adc.clock_prescaler = choosePrescaler(usecFrameLatency);
  config_adc.positive_input = convertMkrZeroPinToSamdAin(arduinoAnalogPin); 
  config_adc.negative_input = ADC_NEGATIVE_INPUT_GND; // ground is negative input
  config_adc.reference = ADC_REFERENCE_INTVCC1; // reference is half the 3V3
  config_adc.gain_factor = ADC_GAIN_FACTOR_DIV2; // multiply measure by two
  config_adc.sample_length = 0; // default Arduino value=63, switch to 0 gives speed boost from 40ksps to 300ksps
  config_adc.freerunning = true; // continuous operation
  adc_init(&adc_instance, ADC, &config_adc);
  
  adc_register_callback(&adc_instance, userCallbackADCResultReady, ADC_CALLBACK_RESULT_READY);
  adc_enable_callback(&adc_instance, ADC_CALLBACK_RESULT_READY);

  adc_enable(&adc_instance);
  
  NVIC_EnableIRQ(ADC_IRQn);
  NVIC_SetPriority(ADC_IRQn, 0); // zero is maximum priority
  
  adc_enable_interrupt(&adc_instance, ADC_INTERRUPT_RESULT_READY);
  adc_start_conversion(&adc_instance); // start first conversion
  
  _isEnabledFreeRunning = true;
}

void MkrADCSingleton::enableConvertorInFreeRunningMode(uint8_t arduinoAnalogPin, uint32_t usecFrameLatency)
{
  disableConvertor();
  configureAndStartFreeRunningADC(arduinoAnalogPin, usecFrameLatency);  
}

void MkrADCSingleton::disableConvertor()
{
  if(_isEnabledFreeRunning) {
    _isCollecting = false;
    adc_disable_interrupt(&adc_instance, ADC_INTERRUPT_RESULT_READY);
    adc_disable(&adc_instance);
    _isEnabledFreeRunning = false;
  }    
}  

void MkrADCSingleton::startCollectingSamples() 
{ 
  _numSamples = 0;
  _isCollecting = true;
}

void MkrADCSingleton::stopCollectingSamples() 
{ 
  _isCollecting = false; 
}

int MkrADCSingleton::GetNumberOfSamples()
{
  return _numSamples;
}

uint8_t MkrADCSingleton::GetSampleValue(int index)
{
  if(index >= 0 && index < _numSamples) return _sampleValues[index];
  return 0;
}

/*
 * End of MkrADC.cpp
 */