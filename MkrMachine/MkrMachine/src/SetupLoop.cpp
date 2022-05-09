
#include <Arduino.h>
#include <system.h> // ASF core

#include "MkrUtil.h"
#include "UserCommandParser.h"
#include "Machine.h"

// the setup function runs once when you press reset or power the board
void setup() 
{
  //Serial.begin(9600); // opens serial port, sets data rate to 9600 bps
  Serial.begin(115200); // opens serial port, sets data rate to 115200 bps

  // initialize ASF core
  system_init();
	
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  // perform 3 blinks to signal proper starting  
  blink(3, 300);
}

// the loop function runs over and over again forever
void loop() 
{
  while(Serial.available()) {
    int ch = Serial.read();
    UserCommandParser.processNextChar(ch);
  }
}

