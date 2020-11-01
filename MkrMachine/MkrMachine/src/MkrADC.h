/*
 * UnoADC.h
 * Created: 10.05.2020
 * Author: SL
 */ 

#ifndef MKRADC_H_
#define MKRADC_H_

// Maximum number of samples ADC buffer can record in one batch.
#define ADC_MAX_SAMPLES 256

class MkrADCSingleton {
   
  public: 
    void enableConvertorInFreeRunningMode(uint8_t arduinoAnalogPin, uint32_t usecFrameLatency);
    
    void startCollectingSamples();
    void stopCollectingSamples();
    
    int GetNumberOfSamples();
    uint8_t GetSampleValue(int index);
    
    void disableConvertor();
};

extern MkrADCSingleton MkrADC;

#endif /* UNOADC_H_ */
