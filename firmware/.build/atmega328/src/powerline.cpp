#include <Arduino.h>
void flashLed(int pin, int times, int wait);
void toggleLed();
void setup();
void loop();
#line 1 "src/powerline.ino"

#include <Stream.h>
#include <stdio.h>
#include "PowerlineCmdProcessor.h"
#include "Modem.h"
//#include "plmcfg.h"
//#include "plm1.h"

Modem  theModem = Modem();

PowerlineCmdProcessor cmdProc = PowerlineCmdProcessor(theModem);

int statusLed = 7;
int errorLed = 7;
int loopCtr = 0;

// Timeout handling
long oneSecondInterval = 1000;
long oneSecondCounter = 0;
int ledCounter = 0;

int counter = 0;

void flashLed(int pin, int times, int wait) {

  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(wait);
    digitalWrite(pin, LOW);

    if (i + 1 < times) {
      delay(wait);
    }
  }
}

void toggleLed()
{
  // blink
  if (ledCounter % 2) {
    digitalWrite(statusLed, HIGH);
  } else {
    digitalWrite(statusLed, LOW);
  }
  ledCounter++;
}  


// ------------------ S E T U P ----------------------------------------------

void setup() {
    Serial.begin(9600);

    //Serial.setTimeout(1000);
    cmdProc.setSerial(Serial);
    theModem.setSerial(Serial);
    
	pinMode(statusLed,OUTPUT);
	
	Serial.print("Welcome to the Powerline Controller\n");
}

// ------------------ M A I N ( ) --------------------------------------------

void loop()
{
    char buffer[128];

    cmdProc.Loop();

	if ( millis() - oneSecondCounter > oneSecondInterval) {
		oneSecondCounter = millis();
		// Things to do at a one-second interval. 
		//toggleLed();
        //pctrl.Loop();
	}

    delay(1);

    loopCtr++;

}



