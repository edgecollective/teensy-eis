//teensyduino standard
#include <stdint.h>

//third party
#include <SerialCommand.h>  /* https://github.com/p-v-o-s/Arduino-SerialCommand */

////////////////////////////////////////////////////////////////////////////////
#define IDN_STRING "EdgeCollective DDS Device"
#define DEBUG_PORT Serial

// SerialCommand parser
const unsigned int SERIAL_BAUDRATE = 115200;
SerialCommand sCmd_USB(Serial, 10); // (Stream, int maxCommands)

void setup() {
    //==========================================================================
    // Setup SerialCommand for USB interface
    //--------------------------------------------------------------------------
    // intialize the serial device
    Serial.begin(SERIAL_BAUDRATE);  //USB serial on the Teensy 3/4
    //while (!Serial){};
    sCmd_USB.setDefaultHandler(UNRECOGNIZED_sCmd_default_handler);
    sCmd_USB.addCommand("IDN?",         IDN_sCmd_query_handler);
    sCmd_USB.addCommand("D",            DEBUG_sCmd_action_handler);// dumps data to debugging port
    sCmd_USB.addCommand("DDS.SET_FREQ", DDS_SET_FREQ_sCmd_action_handler);
    sCmd_USB.addCommand("DDS.START",    DDS_START_sCmd_action_handler);
    sCmd_USB.addCommand("DDS.STOP",     DDS_STOP_sCmd_action_handler);
    //--------------------------------------------------------------------------
   
}

/******************************************************************************/
// DDS subsystem
const int pwmPin = 13;


//generated in python:
//    from pylab import *
//    X = linspace(0,1,100)
//    Y = 128*sin(2*pi*X)+127
//    print(",".join(map(str,Y.astype('uint8'))))
const uint8_t sineTable[] = {127,135,143,151,159,166,174,182,189,196,202,209,215,221,226,231,235,239,243,246,249,251,253,254,254,254,254,253,252,250,247,245,241,237,233,228,223,218,212,206,199,192,185,178,170,163,155,147,139,131,122,114,106,98,90,83,75,68,61,54,47,41,35,30,25,20,16,12,8,6,3,1,0,0,0,0,0,0,2,4,7,10,14,18,22,27,32,38,44,51,57,64,71,79,87,94,102,110,118,126};
const int sineTableLength = 100;

// Create an IntervalTimer objec, ref: https://www.pjrc.com/teensy/td_timing_IntervalTimer.html
IntervalTimer ddsTimer;
float ddsLookupFreq  = 100.0;
int ddsTableLength = sineTableLength;
volatile uint8_t ddsTableIndex=0;

void ddsISR(){
    //cycle through the lookup table, and change the anolog (PWM) value 
    ddsTableIndex = (ddsTableIndex + 1) % ddsTableLength;
    analogWrite(pwmPin, sineTable[ddsTableIndex]);
}

/******************************************************************************/
// Mainloop

void loop() {
    //process Serial commands over USB
    size_t num_bytes = sCmd_USB.readSerial();
    if (num_bytes > 0){
        // CRITICAL SECTION --------------------------------------------------------
        //unsigned char sreg_backup = SREG;   /* save interrupt enable/disable state */
        cli();
        sCmd_USB.processCommand();
        sei();
        //SREG = sreg_backup;                 /* restore interrupt state */
        // END CRITICAL SECTION ----------------------------------------------------
    }else{
        delay(10);
    }
}


/******************************************************************************/
// SerialCommand Handlers

void IDN_sCmd_query_handler(SerialCommand this_sCmd){
  this_sCmd.println(F(IDN_STRING));
}

void DEBUG_sCmd_action_handler(SerialCommand this_sCmd){
  DEBUG_PORT.print(F("# MainLoop params:\n"));
  //DEBUG_PORT.print(F("# \tcycle_counter = "));
  //DEBUG_PORT.println(cycle_counter);
}

void DDS_SET_FREQ_sCmd_action_handler(SerialCommand this_sCmd){
  float freq;
  char *arg = this_sCmd.next();
  if (arg == NULL){
    this_sCmd.print(F("#ERROR: DDS.SET_FREQ requires 1 argument (float freq)\n"));
  }
  else{
    freq = atof(arg);
    ddsLookupFreq = ddsTableLength*freq;
    ddsTimer.update(1000000/ddsLookupFreq);   // function called by interrupt at micros interval 
  }
}

void DDS_START_sCmd_action_handler(SerialCommand this_sCmd){
     // PWM setup, ref: https://www.pjrc.com/teensy/td_pulse.html
    analogWriteFrequency(pwmPin, 10000);            // this is the PWM frequency should be >> DDS frequency
    analogWriteResolution(8);                       // analogWrite value 0 to 255
    ddsTimer.priority(0); //highest priority
    ddsTimer.begin(ddsISR, 1000000/ddsLookupFreq);   // function called by interrupt at micros interval 
}

void DDS_STOP_sCmd_action_handler(SerialCommand this_sCmd){
     // PWM setup, ref: https://www.pjrc.com/teensy/td_pulse.html
    ddsTimer.end();
}

//------------------------------------------------------------------------------
// Unrecognized command
void UNRECOGNIZED_sCmd_default_handler(SerialCommand this_sCmd)
{
  SerialCommand::CommandInfo command = this_sCmd.getCurrentCommand();
  this_sCmd.print(F("#ERROR: command '"));
  this_sCmd.print(command.name);
  this_sCmd.print(F("' not recognized ###\n"));
}

/******************************************************************************/

