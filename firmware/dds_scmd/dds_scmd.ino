//teensyduino standard
#include <stdint.h>

//third party
#include <SerialCommand.h>  /* https://github.com/p-v-o-s/Arduino-SerialCommand */

////////////////////////////////////////////////////////////////////////////////
#define IDN_STRING "EdgeCollective DDS Device"
#define DEBUG_PORT Serial

/******************************************************************************/
// DDS subsystem
int ddsPwmPin = 13;

uint16_t *ddsLookupTable = nullptr;
size_t    ddsLookupTableLength = 0;
float     ddsLookupFreq  = 100.0;

// Create an IntervalTimer object, ref: https://www.pjrc.com/teensy/td_timing_IntervalTimer.html
IntervalTimer ddsTimer;

bool _allocate_ddsLookupTable(size_t size){
    //allocate memory for new table, if it hasn't been already
    if (ddsLookupTable == nullptr){
        //DEBUG_PORT.println("malloc");
        ddsLookupTable = (uint16_t*) malloc(sizeof(uint16_t)*size);
    } else {
        //DEBUG_PORT.println("realloc");
        ddsLookupTable = (uint16_t *) realloc(ddsLookupTable,sizeof(uint16_t)*size);
    }
    if (ddsLookupTable == nullptr){
        DEBUG_PORT.print(F("#ERROR: cannot allocate memory for ddsLookupTable size = "));
        DEBUG_PORT.print(size);
        return false;
    }
    ddsLookupTableLength = size;
    return true;
}

void dds_load_sineTable8bit(){
    //generated in python:
    //    from pylab import *
    //    X = linspace(0,1,100)
    //    Y = 128*sin(2*pi*X)+127
    //    print(",".join(map(str,Y.astype('uint8'))))
    const uint8_t sineTable[] = {127,135,143,151,159,166,174,182,189,196,202,209,215,221,226,231,235,239,243,246,249,251,253,254,254,254,254,253,252,250,247,245,241,237,233,228,223,218,212,206,199,192,185,178,170,163,155,147,139,131,122,114,106,98,90,83,75,68,61,54,47,41,35,30,25,20,16,12,8,6,3,1,0,0,0,0,0,0,2,4,7,10,14,18,22,27,32,38,44,51,57,64,71,79,87,94,102,110,118,126};
    const int sineTableLength = 100;
    if (_allocate_ddsLookupTable(sineTableLength)){
        for(int i=0; i < sineTableLength; i++){
            ddsLookupTable[i] = (uint16_t) sineTable[i];
        }
    }
    
}

volatile uint8_t ddsLookupTableIndex=0;

void ddsISR(){
    //cycle through the lookup table, and change the anolog (PWM) value 
    ddsLookupTableIndex = (ddsLookupTableIndex + 1) % ddsLookupTableLength;
    analogWrite(ddsPwmPin, ddsLookupTable[ddsLookupTableIndex]);
}
/******************************************************************************/
// SerialCommand parser
const unsigned int SERIAL_BAUDRATE = 115200;
SerialCommand sCmd_USB(Serial, 20); // (Stream, int maxCommands)

/******************************************************************************/

void setup() {
    //==========================================================================
    // Setup SerialCommand for USB interface
    //--------------------------------------------------------------------------
    // intialize the serial device
    Serial.begin(SERIAL_BAUDRATE);  //USB serial on the Teensy 3/4
    //while (!Serial){};
    sCmd_USB.setDefaultHandler(UNRECOGNIZED_sCmd_default_handler);
    sCmd_USB.addCommand("IDN?",             IDN_sCmd_query_handler);
    sCmd_USB.addCommand("D",                DEBUG_sCmd_action_handler);// dumps data to debugging port
    sCmd_USB.addCommand("DDS.WRITE_TABLE",  DDS_WRITE_TABLE_sCmd_action_handler);
    sCmd_USB.addCommand("DDS.READ_TABLE?",  DDS_READ_TABLE_sCmd_query_handler);
    sCmd_USB.addCommand("DDS.SET_OUT_PIN",  DDS_SET_OUT_PIN_sCmd_action_handler);
    sCmd_USB.addCommand("DDS.SET_PWM_RES",  DDS_SET_PWM_RES_sCmd_action_handler);
    sCmd_USB.addCommand("DDS.SET_PWM_FREQ", DDS_SET_PWM_FREQ_sCmd_action_handler);
    sCmd_USB.addCommand("DDS.START",        DDS_START_sCmd_action_handler);
    sCmd_USB.addCommand("DDS.STOP",         DDS_STOP_sCmd_action_handler);
    //--------------------------------------------------------------------------
    //DDS subsystem intialization
    dds_load_sineTable8bit(); //start out with a sine wave for generation
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

void DDS_SET_OUT_PIN_sCmd_action_handler(SerialCommand this_sCmd){
    char *arg = this_sCmd.next();
    if (arg == NULL){
        this_sCmd.print(F("#ERROR: DDS.SET_OUT_PIN requires 1 argument 'PWMpin'= {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,18,19,22,23}\n"));
    }
    else{
        int PWMpin = atoi(arg);
        switch(PWMpin){
            case 0:case 1:case 2:case 3:case 4:case 5:case 6:case 7:case 8:
            case 9:case 10:case 11:case 12:case 13:case 14:case 15:case 18:
            case 19:case 22:case 23:break;
            default:{
                this_sCmd.print(F("#ERROR: DDS.SET_OUT_PIN requires 1 argument 'PWMpin'= {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,18,19,22,23}\n"));
                return;
            }
        }
        ddsPwmPin = PWMpin;
    }
}

void DDS_WRITE_TABLE_sCmd_action_handler(SerialCommand this_sCmd){
    char *arg = this_sCmd.next();
    int args_to_read = 1;
    int size = 0;
    if (arg == NULL){
        this_sCmd.print(F("#ERROR: DDS_WRITE_TABLE requires 1 argument 'size' and a series of that many integers, separated by spaces."));
        return;
    }
    size = atoi(arg);
    _allocate_ddsLookupTable(size);
    args_to_read = size; //continue to read this many args
    while(args_to_read > 0){
        arg = this_sCmd.next();
        if (arg == NULL){
            this_sCmd.print(F("#ERROR: DDS_WRITE_TABLE requires 1 argument 'size' and a series of that many integers, separated by spaces."));
            return;
        }
        uint16_t value = atoi(arg);
        //FIXME how to check for errors?
        ddsLookupTable[size-args_to_read] = value;
        args_to_read--;
    }
}

void DDS_READ_TABLE_sCmd_query_handler(SerialCommand this_sCmd){
    for(int i=0; i < ddsLookupTableLength-1; i++){
        this_sCmd.print(ddsLookupTable[i]);
        this_sCmd.print(" ");
    }
    this_sCmd.print(ddsLookupTable[ddsLookupTableLength-1]);
    this_sCmd.print("\n");
}

void DDS_SET_FREQ_sCmd_action_handler(SerialCommand this_sCmd){
  float freq;
  char *arg = this_sCmd.next();
  if (arg == NULL){
    this_sCmd.print(F("#ERROR: DDS.SET_FREQ requires 1 argument (float freq)\n"));
  }
  else{
    freq = atof(arg);
    if (freq > 0.0){
        ddsLookupFreq = ddsLookupTableLength*freq;
        ddsTimer.update(1000000/ddsLookupFreq);// function called by interrupt at micros interval
    } else{
        this_sCmd.print(F("#ERROR: DDS.SET_FREQ argument 'freq' was 0.0 or undefined!\n"));
    }
  }
}

void DDS_SET_PWM_RES_sCmd_action_handler(SerialCommand this_sCmd){
    int bits;
    char *arg = this_sCmd.next();
    if (arg == NULL){
        this_sCmd.print(F("#ERROR: DDS.SET_PWM_RES requires 1 argument 'bits'= {2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}\n"));
    }
    else{
        bits = atoi(arg);
        switch(bits){
            case 2:case 3:case 4:case 5:case 6:case 7:case 8:case 9:case 10:
            case 11:case 12:case 13:case 14:case 15:case 16: break;
            default:{
                this_sCmd.print(F("#ERROR: DDS.SET_PWM_RES requires 1 argument 'bits'= {2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}\n"));
                return;
            }
        }
        analogWriteResolution(bits);
    }
}

void DDS_SET_PWM_FREQ_sCmd_action_handler(SerialCommand this_sCmd){
  long freq;
  char *arg = this_sCmd.next();
  if (arg == NULL){
    this_sCmd.print(F("#ERROR: DDS.SET_PWM_FREQ requires 1 argument 'freq' (long)\n"));
  }
  else{
    freq = atol(arg);
    if (freq > 0){
        analogWriteFrequency(ddsPwmPin, freq); // this is the PWM frequency should be >> DDS frequency
    } else{
        this_sCmd.print(F("#ERROR: DDS.SET_PWM_FREQ argument 'freq' was 0 or undefined!\n"));
    }
  }
}

void DDS_START_sCmd_action_handler(SerialCommand this_sCmd){
     // PWM setup, ref: https://www.pjrc.com/teensy/td_pulse.html
    // IntervalTimer object, ref: https://www.pjrc.com/teensy/td_timing_IntervalTimer.html
    ddsTimer.priority(0); //highest priority
    ddsTimer.begin(ddsISR, 1000000/ddsLookupFreq);   // function called by interrupt at micros interval 
}

void DDS_STOP_sCmd_action_handler(SerialCommand this_sCmd){
    // IntervalTimer object, ref: https://www.pjrc.com/teensy/td_timing_IntervalTimer.html
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

