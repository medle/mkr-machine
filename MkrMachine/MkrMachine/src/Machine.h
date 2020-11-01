/*
 * Machine.h
 * Created: 13.05.2020
 * Author: SL
 */ 

#ifndef MACHINE_H_
#define MACHINE_H_

class MachineSingleton
{
  public:
    void enablePWM(int usec, int duty1024); 
    void disablePWM();
    bool isPWMEnabled();
    bool hasFinishedRecording();
    void sendRecordingToUser();
    void startNewRecording(); 
    bool recordNewBatchAndSendToUser(uint8_t analogPin);
};

extern MachineSingleton Machine;

#endif /* MACHINE_H_ */