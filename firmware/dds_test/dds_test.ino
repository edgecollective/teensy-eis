//teensyduino standard
#include <stdint.h>

//third party
#include <SerialCommand.h>  /* https://github.com/p-v-o-s/Arduino-SerialCommand */


const int pwmPin = 13;
const int ddsLookupFeq = 100;

//generated in python:
//    from pylab import *
//    X = linspace(0,1,100)
//    Y = 128*sin(2*pi*X)+127
//    print(",".join(map(str,Y.astype('uint8'))))
const uint8_t sineTable[] = {127,135,143,151,159,166,174,182,189,196,202,209,215,221,226,231,235,239,243,246,249,251,253,254,254,254,254,253,252,250,247,245,241,237,233,228,223,218,212,206,199,192,185,178,170,163,155,147,139,131,122,114,106,98,90,83,75,68,61,54,47,41,35,30,25,20,16,12,8,6,3,1,0,0,0,0,0,0,2,4,7,10,14,18,22,27,32,38,44,51,57,64,71,79,87,94,102,110,118,126};
const int sineTableLength = 100;

// Create an IntervalTimer object 
IntervalTimer ddsTimer;

void setup() {
    analogWriteFrequency(pwmPin, 10000); // Teensy 3.0 pin 3 also changes to 375 kHz
    analogWriteResolution(8);        // analogWrite value 0 to 255
    ddsTimer.begin(ddsISR, 1000000/ddsLookupFeq);
}

//uint8_t ddsTable[] = sineTable;
int ddsTableLength = sineTableLength;
volatile uint8_t ddsTableIndex=0;

void ddsISR(){
    ddsTableIndex = (ddsTableIndex + 1) % ddsTableLength;
    analogWrite(pwmPin, sineTable[ddsTableIndex]);
}

void loop() {
  delay(250);
}
