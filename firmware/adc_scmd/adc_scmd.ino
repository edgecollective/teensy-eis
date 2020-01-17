//teensyduino standard
#include <stdint.h>

//teensyduino community
#include <ADC.h>
#include <ADC_util.h>


//third party
#include <SerialCommand.h>  /* https://github.com/p-v-o-s/Arduino-SerialCommand */

//local
#include "RingBufferDynamic.h"
////////////////////////////////////////////////////////////////////////////////
#define IDN_STRING "EdgeCollective ADC Device"
#define DEBUG_PORT Serial

// SerialCommand parser
const unsigned int SERIAL_BAUDRATE = 115200;
SerialCommand sCmd_USB(Serial, 10); // (Stream, int maxCommands)

elapsedMicros since_setup_started_micros;
void setup() {
    since_setup_started_micros = 0;
    //==========================================================================
    // Setup SerialCommand for USB interface
    //--------------------------------------------------------------------------
    // intialize the serial device
    Serial.begin(SERIAL_BAUDRATE);  //USB serial on the Teensy 3/4
    //while (!Serial){};
    sCmd_USB.setDefaultHandler(UNRECOGNIZED_sCmd_default_handler);
    sCmd_USB.addCommand("IDN?",          IDN_sCmd_query_handler);
    sCmd_USB.addCommand("D",             DEBUG_sCmd_action_handler);// dumps data to debugging port
    sCmd_USB.addCommand("ADC.SET_SAMP_SPEED", ADC_SET_SAMP_SPEED_sCmd_action_handler);
    sCmd_USB.addCommand("ADC.SET_CONV_SPEED", ADC_SET_CONV_SPEED_sCmd_action_handler);
    sCmd_USB.addCommand("ADC.SET_RES",        ADC_SET_RES_sCmd_action_handler);
    sCmd_USB.addCommand("ADC.SET_SIZE",       ADC_SET_SIZE_sCmd_action_handler);
    sCmd_USB.addCommand("ADC.SET_NUM",        ADC_SET_NUM_sCmd_action_handler);
    sCmd_USB.addCommand("ADC.SET_RATE",       ADC_SET_RATE_sCmd_action_handler);
    sCmd_USB.addCommand("ADC.START",          ADC_START_sCmd_action_handler);
    sCmd_USB.addCommand("ADC.STOP",           ADC_STOP_sCmd_action_handler);
    //--------------------------------------------------------------------------
    pinMode(LED_BUILTIN, OUTPUT);
}

/******************************************************************************/
// ADC subsystem
ADC *adc = new ADC(); // adc object

// Create an IntervalTimer object, ref: https://www.pjrc.com/teensy/td_timing_IntervalTimer.html
unsigned long   adc0_sample_group_size = 10;
unsigned long   adc0_num_sample_groups = 1;
float  adc0_group_rate = 1.0;
float  adc0_readPin = A2;

RingBufferDynamicUINT16 *adc0_buffer = nullptr;

IntervalTimer adc0_timer;

volatile int  adc0_group_iter     = 0;
volatile int  adc0_samp_iter      = 0;
volatile int  adc0_samp_iter_last = 0;
volatile bool adc0_group_is_ready = false;
volatile bool adc0_is_busy = false;
volatile unsigned long adc0_group_start_timestamp_micros = 0;
volatile unsigned long adc0_group_end_timestamp_micros   = 0;

void adc0_sampleGroupTimerCallback(void){
    //DEBUG_PORT.print(since_setup_started_micros);
    //DEBUG_PORT.println(" adc0_sampleGroupTimerCallback");
    digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN));
    if (adc0_group_iter < adc0_num_sample_groups){
        if (!adc0_is_busy){
            adc0_samp_iter      = 0;
            adc0_samp_iter_last = 0;
            adc0_is_busy = true;
            //record micros timestamp
            adc0_group_start_timestamp_micros = since_setup_started_micros;
            adc->startContinuous(adc0_readPin, ADC_0);
        }
    }else{
        //shut down group iteration
        adc0_timer.end();
    }
}

void adc0_sampleISR(void) {
    //DEBUG_PORT.print(since_setup_started_micros);
    //DEBUG_PORT.println(" adc0_sampleISR");
    adc0_buffer->write((uint16_t) adc->adc0->analogReadContinuous());
    adc0_samp_iter++;
    if  (adc0_samp_iter == adc0_sample_group_size){
        //record micros timestamp
        adc0_group_end_timestamp_micros = since_setup_started_micros;
        //last sample, shut it down
        adc->stopContinuous(ADC_0);
        adc0_group_iter++;
        adc0_is_busy = false;
    }
}

/******************************************************************************/
// Mainloop
void loop() {
    bool is_idle = true;
    //process Serial commands over USB
    size_t num_bytes = sCmd_USB.readSerial();
    if (num_bytes > 0){
        is_idle = false;
        // CRITICAL SECTION --------------------------------------------------------
        //unsigned char sreg_backup = SREG;   /* save interrupt enable/disable state */
        cli();
        sCmd_USB.processCommand();
        sei();
        //SREG = sreg_backup;                 /* restore interrupt state */
        // END CRITICAL SECTION ----------------------------------------------------
    }
    //check to see if there is new ADC data in the buffer
    if (!adc0_is_busy){
        // CRITICAL SECTION --------------------------------------------------------
        //unsigned char sreg_backup = SREG;   /* save interrupt enable/disable state */
        cli();
        // a lot of these variables are volatile, so don't use them outside critical section!
        int adc0_new_samples = adc0_samp_iter - adc0_samp_iter_last;
        int group_index = adc0_group_iter - 1; //we are considering result of last iteration
        unsigned long group_start_timestamp_micros = adc0_group_start_timestamp_micros;
        unsigned long group_duration_micros = adc0_group_end_timestamp_micros - group_start_timestamp_micros;
        sei();
        //SREG = sreg_backup;                 /* restore interrupt state */
        // END CRITICAL SECTION ----------------------------------------------------
        if (adc0_new_samples > 0){
            //DEBUG_PORT.print(F("# \tadc0_new_samples = "));
            //DEBUG_PORT.println(adc0_new_samples);
            is_idle = false;
            Serial.print(group_index);
            Serial.print(":");
            Serial.print(group_start_timestamp_micros);
            Serial.print(":");
            for(int i=0; i < adc0_new_samples; i++){
                int buff_index = adc0_samp_iter_last + i;
                Serial.print(adc0_buffer->read());
                if (buff_index < adc0_sample_group_size - 1){
                    Serial.print(",");
                } else{
                    //finish up packet
                    Serial.print(":");
                    Serial.print(group_duration_micros);
                    Serial.print("\n");
                }
                
            }
             // save state of the past loop
            adc0_samp_iter_last += adc0_new_samples;
        }
    }
    //rest a bit if doing nothing
    if (is_idle){
        delay(100);
    }
    
}


/******************************************************************************/
// SerialCommand Handlers

void IDN_sCmd_query_handler(SerialCommand this_sCmd){
    this_sCmd.println(F(IDN_STRING));
}

void DEBUG_sCmd_action_handler(SerialCommand this_sCmd){
    DEBUG_PORT.print(F("# MainLoop params:\n"));
    DEBUG_PORT.print(F("# ADC params:\n"));
    DEBUG_PORT.print(F("# \tadc0_samp_iter = "));
    DEBUG_PORT.println(adc0_samp_iter);
    DEBUG_PORT.print(F("# \tadc0_samp_iter_last = "));
    DEBUG_PORT.println(adc0_samp_iter_last);
}

void ADC_SET_SAMP_SPEED_sCmd_action_handler(SerialCommand this_sCmd){
    //ADC_SAMPLING_SPEED enum: VERY_LOW_SPEED, LOW_SPEED, MED_SPEED, HIGH_SPEED or VERY_HIGH_SPEED
    int level;
    char *arg = this_sCmd.next();
    if (arg == NULL){
        this_sCmd.print(F("#ERROR: ADC.SET_SAMP_SPEED requires 1 argument 'level'= {0,1,2,3,4}\n"));
    }
    else{
        level = atoi(arg);
        switch(level){
            case 0: adc->setSamplingSpeed(ADC_SAMPLING_SPEED::VERY_LOW_SPEED,  ADC_0); break;
            case 1: adc->setSamplingSpeed(ADC_SAMPLING_SPEED::LOW_SPEED,       ADC_0); break;
            case 2: adc->setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED,       ADC_0); break;
            case 3: adc->setSamplingSpeed(ADC_SAMPLING_SPEED::HIGH_SPEED,      ADC_0); break;
            case 4: adc->setSamplingSpeed(ADC_SAMPLING_SPEED::VERY_HIGH_SPEED, ADC_0); break;
            default:{
                this_sCmd.print(F("#ERROR: ADC.SET_SAMP_SPEED requires 1 argument 'level'= {0,1,2,3,4}\n"));
                break;
            }
        }
    }
}

void ADC_SET_CONV_SPEED_sCmd_action_handler(SerialCommand this_sCmd){
    // ADC_CONVERSION_SPEED enum: VERY_LOW_SPEED, LOW_SPEED, MED_SPEED,
    // HIGH_SPEED_16BITS, HIGH_SPEED, VERY_HIGH_SPEED
    int level;
    char *arg = this_sCmd.next();
    if (arg == NULL){
        this_sCmd.print(F("#ERROR: ADC.SET_CONV_SPEED requires 1 argument 'level'= {0,1,2,3,4}\n"));
    }
    else{
        level = atoi(arg);
        switch(level){
            case 0: adc->setConversionSpeed(ADC_CONVERSION_SPEED::VERY_LOW_SPEED,    ADC_0); break;
            case 1: adc->setConversionSpeed(ADC_CONVERSION_SPEED::LOW_SPEED,         ADC_0); break;
            case 2: adc->setConversionSpeed(ADC_CONVERSION_SPEED::MED_SPEED,         ADC_0); break;
            case 3: adc->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED,        ADC_0); break;
            case 4: adc->setConversionSpeed(ADC_CONVERSION_SPEED::VERY_HIGH_SPEED,   ADC_0); break;
            default:{
                this_sCmd.print(F("#ERROR: ADC.SET_CONV_SPEED requires 1 argument 'level'= {0,1,2,3,4}\n"));
                break;
            }
        }
    }
}

void ADC_SET_RES_sCmd_action_handler(SerialCommand this_sCmd){
    int bits;
    char *arg = this_sCmd.next();
    if (arg == NULL){
        this_sCmd.print(F("#ERROR: ADC.SET_RES requires 1 argument 'bits'= {8,10,12,16}\n"));
    }
    else{
        bits = atoi(arg);
        switch(bits){
            case 8:
            case 10:
            case 12:
            case 16: adc->setResolution(bits, ADC_0);break;
            default:{
                this_sCmd.print(F("#ERROR: ADC.SET_RES requires 1 argument 'bits'= {8,10,12,16}\n"));
                break;
            }
        }
    }
}

void ADC_SET_SIZE_sCmd_action_handler(SerialCommand this_sCmd){
  long size;
  char *arg = this_sCmd.next();
  if (arg == NULL){
    this_sCmd.print(F("#ERROR: ADC.SET_SIZE requires 1 argument (long size)\n"));
  }
  else{
    size = atol(arg);
    if (size > 0){
        adc0_sample_group_size = size;
    } else{
        this_sCmd.print(F("#ERROR: ADC.SET_SIZE argument 'size' must be > 0!\n"));
    }
  }
}

void ADC_SET_NUM_sCmd_action_handler(SerialCommand this_sCmd){
  long num;
  char *arg = this_sCmd.next();
  if (arg == NULL){
    this_sCmd.print(F("#ERROR: ADC.SET_NUM requires 1 argument (long num)\n"));
  }
  else{
    num = atol(arg);
    if (num > 0){
        adc0_num_sample_groups = num;
    } else{
        this_sCmd.print(F("#ERROR: ADC.SET_NUM argument 'num' must be > 0!\n"));
    }
  }
}

void ADC_SET_RATE_sCmd_action_handler(SerialCommand this_sCmd){
  float rate;
  char *arg = this_sCmd.next();
  if (arg == NULL){
    this_sCmd.print(F("#ERROR: ADC.SET_RATE requires 1 argument (float rate)\n"));
  }
  else{
    rate = atof(arg);
    if (rate > 0.0){
        adc0_group_rate = rate;
        adc0_timer.update(1000000/adc0_group_rate);//update the rate if already running
    } else{
        this_sCmd.print(F("#ERROR: ADC.SET_RATE argument 'rate' was 0.0 or undefined!\n"));
    }
  }
}


void ADC_START_sCmd_action_handler(SerialCommand this_sCmd){
    adc0_group_iter = 0;
    adc0_samp_iter = 0;
    //allocate memory for new buffer
    if (adc0_buffer != nullptr){
        delete adc0_buffer;
    }
    //compute next power of 2 bounding n, ref: https://www.geeksforgeeks.org/smallest-power-of-2-greater-than-or-equal-to-n/
    unsigned long n = adc0_sample_group_size;
    unsigned int p = 1;
    while (p < n)
        p <<= 1;
      
    adc0_buffer = new RingBufferDynamicUINT16(p);
    if (!adc0_buffer->isAlloc()){
        this_sCmd.print(F("#ERROR: ADC.START cannot allocate memory for size p=2**"));
        this_sCmd.println(p);
        return;
    }
    adc->setAveraging(1, ADC_0);
    adc->enableInterrupts(adc0_sampleISR, ADC_0);
    // IntervalTimer object, ref: https://www.pjrc.com/teensy/td_timing_IntervalTimer.html
    adc0_timer.priority(1); //second highest priority
    adc0_timer.begin(adc0_sampleGroupTimerCallback, 1000000/adc0_group_rate);   // function called by interrupt at micros interval 
    
    //while (adc0_samp_iter < adc0_sample_group_size){};//FIXME test blocking
}

void ADC_STOP_sCmd_action_handler(SerialCommand this_sCmd){
    // IntervalTimer object, ref: https://www.pjrc.com/teensy/td_timing_IntervalTimer.html
    adc0_timer.end();
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

