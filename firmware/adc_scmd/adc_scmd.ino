//teensyduino standard
#include <stdint.h>

//teensyduino community
#include <ADC.h>
#include <ADC_util.h>


//third party
#include <SerialCommand.h>  /* https://github.com/p-v-o-s/Arduino-SerialCommand */

//local
//#include "RingBufferDynamic.h"
////////////////////////////////////////////////////////////////////////////////
#define IDN_STRING "EdgeCollective ADC Device"
#define DEBUG_PORT Serial

// SerialCommand parser
const unsigned int SERIAL_BAUDRATE = 115200;
SerialCommand sCmd_USB(Serial, 20); // (Stream, int maxCommands)

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
    sCmd_USB.addCommand("ADC.CONFIG",         ADC_CONFIG_sCmd_action_handler);
    sCmd_USB.addCommand("ADC.SET_READ_PIN",   ADC_SET_READ_PIN_sCmd_action_handler);
    sCmd_USB.addCommand("ADC.SET_SAMP_SPEED", ADC_SET_SAMP_SPEED_sCmd_action_handler);
    sCmd_USB.addCommand("ADC.SET_CONV_SPEED", ADC_SET_CONV_SPEED_sCmd_action_handler);
    sCmd_USB.addCommand("ADC.SET_RES",        ADC_SET_RES_sCmd_action_handler);
    sCmd_USB.addCommand("ADC.SET_BUFFER",     ADC_SET_BUFFER_sCmd_action_handler);
    sCmd_USB.addCommand("ADC.SET_NUM",        ADC_SET_NUM_sCmd_action_handler);
    sCmd_USB.addCommand("ADC.SET_RATE",       ADC_SET_RATE_sCmd_action_handler);
    sCmd_USB.addCommand("ADC.START",          ADC_START_sCmd_action_handler);
    sCmd_USB.addCommand("ADC.STOP",           ADC_STOP_sCmd_action_handler);
    sCmd_USB.addCommand("T",  TEMPMON_GET_TEMP_sCmd_query_handler);
    sCmd_USB.addCommand("TEMPMON.GET_TEMP?",  TEMPMON_GET_TEMP_sCmd_query_handler);
    //--------------------------------------------------------------------------
    pinMode(LED_BUILTIN, OUTPUT);
}

/******************************************************************************/
// ADC subsystem
ADC *adc = new ADC(); // adc object

enum adc_config_mode_t {ADC_CONFIG_CHAN0,ADC_CONFIG_CHAN1,ADC_CONFIG_SYNC};
adc_config_mode_t adc_config_mode = ADC_CONFIG_SYNC;
bool adc_chan0_is_configured = false;
bool adc_chan1_is_configured = false;
int  adc_num_timers_active = 0;
int  adc0_readPin = A0;
int  adc1_readPin = A2;

unsigned long   adc0_buffer_size = 10;
unsigned long   adc1_buffer_size = 10;
unsigned long   adc0_num_sample_groups = 1;
unsigned long   adc1_num_sample_groups = 1;
float  adc0_group_rate = 1.0;
float  adc1_group_rate = 1.0;

uint16_t *adc0_buffer = nullptr;
uint16_t *adc1_buffer = nullptr;

// Create an IntervalTimer object, ref: https://www.pjrc.com/teensy/td_timing_IntervalTimer.html
IntervalTimer adc0_timer;
IntervalTimer adc1_timer;

volatile int  adc0_group_iter   = 0;
volatile int  adc1_group_iter   = 0;
volatile int  adc0_samp_iter    = 0;
volatile int  adc1_samp_iter    = 0;
volatile bool adc0_is_busy = false;
volatile bool adc1_is_busy = false;
volatile bool adc0_group_is_ready = false;
volatile bool adc1_group_is_ready = false;
volatile unsigned long  adc0_group_start_timestamp_micros;
volatile unsigned long  adc1_group_start_timestamp_micros;
volatile unsigned long  adc0_group_end_timestamp_micros;
volatile unsigned long  adc1_group_end_timestamp_micros;

void adc0_sampleGroupTimerCallback(void){
    //DEBUG_PORT.print(since_setup_started_micros);
    //DEBUG_PORT.println(" adc0_sampleGroupTimerCallback");
    if (adc0_group_iter < adc0_num_sample_groups || adc0_num_sample_groups == -1){
        if (!adc0_is_busy){
            adc0_samp_iter = 0;
            adc0_is_busy = true;
            adc0_group_is_ready = false;
            digitalWriteFast(LED_BUILTIN,HIGH);
            //record micros timestamp
            adc0_group_start_timestamp_micros = since_setup_started_micros;
            if (adc_config_mode == ADC_CONFIG_SYNC){
                //handle special case of synchronized sampling
                adc1_samp_iter = 0;
                adc1_is_busy = true;
                adc->startSynchronizedContinuous(adc0_readPin, adc1_readPin);
            } else{
                adc->startContinuous(adc0_readPin, ADC_0);
            }
        }else{
            DEBUG_PORT.print(F("# WARNING: ADC_CHAN0 skipping scheduled sample group!\n"));
        }
    }else{
        //shut down group iteration
        adc0_timer.end();
        adc_num_timers_active--; //decrement
    }
}

void adc1_sampleGroupTimerCallback(void){
    //DEBUG_PORT.print(since_setup_started_micros);
    //DEBUG_PORT.println(" adc1_sampleGroupTimerCallback");
    if (adc1_group_iter < adc1_num_sample_groups || adc1_num_sample_groups == -1){
        if (!adc1_is_busy){
            adc1_samp_iter = 0;
            adc1_is_busy = true;
            adc1_group_is_ready = false;
            digitalWriteFast(LED_BUILTIN,HIGH);
            //record micros timestamp
            adc1_group_start_timestamp_micros = since_setup_started_micros;
            adc->startContinuous(adc1_readPin, ADC_1);
        }else{
            DEBUG_PORT.print(F("# WARNING: ADC_CHAN1 skipping scheduled sample group!\n"));
        }
    }else{
        //shut down group iteration
        adc1_timer.end();
        adc_num_timers_active--; //decrement
    }
}

void adc0_sampleISR(void) {
    //DEBUG_PORT.print(since_setup_started_micros);
    //DEBUG_PORT.println(" adc0_sampleISR");
    //even indices if 2 channels, otherwise all indices
    adc0_buffer[adc0_samp_iter] = adc->adc0->analogReadContinuous();
    adc0_samp_iter++;
    if  (adc0_samp_iter >= adc0_buffer_size){
        //last sample, shut it down
        adc0_group_end_timestamp_micros = since_setup_started_micros;
        adc->stopContinuous(ADC_0);
        digitalWriteFast(LED_BUILTIN,LOW);
        adc0_group_is_ready = true; //signal to main loop for readout
        adc0_group_iter++;
    }
}

void adc1_sampleISR(void) {
    //DEBUG_PORT.print(since_setup_started_micros);
    //DEBUG_PORT.println(" adc1_sampleISR");
    //odd indices if 2 channels, otherwise all indices
    adc1_buffer[adc1_samp_iter] = adc->adc1->analogReadContinuous();
    adc1_samp_iter++;
    if  (adc1_samp_iter >= adc1_buffer_size){
        //last sample, shut it down
        adc1_group_end_timestamp_micros = since_setup_started_micros;
        adc->stopContinuous(ADC_1);
        digitalWriteFast(LED_BUILTIN,LOW);
        adc1_group_is_ready = true; //signal to main loop for readout
        adc1_group_iter++;
    }
}

void adc_synchronizedSampleISR(void) {
    //DEBUG_PORT.print(since_setup_started_micros);
    //DEBUG_PORT.println(" adc0_sampleISR");
    ADC::Sync_result result = adc->readSynchronizedContinuous();
    // interleave samples in the buffer
    int index = 2*adc0_samp_iter;
    // if using 16 bits and single-ended is necessary to typecast to unsigned,
    // otherwise values larger than 3.3/2 will be interpreted as negative
    adc0_buffer[index]   = (uint16_t)result.result_adc0;
    adc0_buffer[index+1] = (uint16_t)result.result_adc1;
    adc0_samp_iter++;
    adc1_samp_iter++;
    if  (adc0_samp_iter >= adc0_buffer_size/2){
         //last sample, shut it down
        adc0_group_end_timestamp_micros = since_setup_started_micros;
        adc->stopSynchronizedContinuous();
        digitalWriteFast(LED_BUILTIN,LOW);
        adc0_group_is_ready = true; //signal to main loop for readout
        adc0_group_iter++;
    }
}

/******************************************************************************/
// Mainloop
volatile bool mainloop_is_idle = true;

void loop() {
    mainloop_is_idle = true; //assume we will be idle until we do something
    //process Serial commands over USB
    size_t num_bytes = sCmd_USB.readSerial();
    if (num_bytes > 0){
        mainloop_is_idle = false;
        // CRITICAL SECTION --------------------------------------------------------
        //unsigned char sreg_backup = SREG;   /* save interrupt enable/disable state */
        cli();
        sCmd_USB.processCommand();
        sei();
        //SREG = sreg_backup;                 /* restore interrupt state */
        // END CRITICAL SECTION ----------------------------------------------------
    }
    // if we are currently streaming ADC data
    if (adc_num_timers_active > 0){
        mainloop_is_idle = false; //make sure our response is quick!
        if (adc0_group_is_ready){
            //print out the header
            if (adc_config_mode == ADC_CONFIG_SYNC){
                //handle special case of synchronized sampling
                Serial.print("C01:");
            } else{
                Serial.print("C0:");
            }
            Serial.print(adc0_group_iter-1);
            Serial.print(":");
            Serial.print(adc0_group_start_timestamp_micros);
            Serial.print(":");
            //now readout the samples in the buffer
            if (adc_config_mode == ADC_CONFIG_SYNC){
                 // if there are two channels in sync mode the  will 
                //  be interleaved
                // ch0 at even indices
                int i;
                for(i=0; i < adc0_buffer_size/2 - 1; i++){
                    Serial.print(adc0_buffer[2*i]);
                    Serial.print(",");
                }
                Serial.print(adc0_buffer[i]);
                Serial.print(":");
                // ch1 at odd indices
                for(i=0; i < adc0_buffer_size/2 - 1; i++){
                    Serial.print(adc0_buffer[2*i + 1]);
                    Serial.print(",");
                }
                Serial.print(adc0_buffer[i+1]);
                adc1_is_busy = false;
            }else{
                int i;
                for(i=0; i < adc0_buffer_size - 1; i++){
                    Serial.print(adc0_buffer[i]);
                    Serial.print(",");
                }
                Serial.print(adc0_buffer[i]);
            }
            //finish up packet
            Serial.print(":");
            Serial.print(adc0_group_end_timestamp_micros-adc0_group_start_timestamp_micros);
            Serial.print("\n");
            adc0_is_busy = false;        //we can start a new group now
            adc0_group_is_ready = false; //make sure we don't print out same group
        }
        if (adc1_group_is_ready){
            //print out the header
            Serial.print("C1:");
            Serial.print(adc1_group_iter-1);
            Serial.print(":");
            Serial.print(adc1_group_start_timestamp_micros);
            Serial.print(":");
            //now readout the samples in the buffer
            int i=0;
            for(; i < adc1_buffer_size - 1; i++){
                Serial.print(adc1_buffer[i]);
                Serial.print(",");
            }
            Serial.print(adc1_buffer[i]);
            //finish up packet
            Serial.print(":");
            Serial.print(adc1_group_end_timestamp_micros-adc1_group_start_timestamp_micros);
            Serial.print("\n");
            adc1_is_busy = false;        //we can start a new group now
            adc1_group_is_ready = false; //make sure we don't print out same group
        }
    }
    //rest a bit if doing nothing
    if (mainloop_is_idle){delay(10);}
}


/******************************************************************************/
// SerialCommand Handlers

void IDN_sCmd_query_handler(SerialCommand this_sCmd){
    this_sCmd.println(F(IDN_STRING));
}

void DEBUG_sCmd_action_handler(SerialCommand this_sCmd){
    DEBUG_PORT.print(F("# MainLoop params:\n"));
    DEBUG_PORT.print(F("# \tmainloop_is_idle = "));
    DEBUG_PORT.println(mainloop_is_idle);
    DEBUG_PORT.print(F("# ADC params:\n"));
    DEBUG_PORT.print(F("# \tadc_config_mode = "));
    DEBUG_PORT.println(adc_config_mode);
    DEBUG_PORT.print(F("# \tadc_chan0_is_configured = "));
    DEBUG_PORT.println(adc_chan0_is_configured);
    DEBUG_PORT.print(F("# \tadc_num_timers_active = "));
    DEBUG_PORT.println(adc_num_timers_active);
    DEBUG_PORT.print(F("# \tadc0_buffer_size = "));
    DEBUG_PORT.println(adc0_buffer_size);
    DEBUG_PORT.print(F("# \tadc1_buffer_size = "));
    DEBUG_PORT.println(adc1_buffer_size);
    DEBUG_PORT.print(F("# \tadc0_num_sample_groups = "));
    DEBUG_PORT.println(adc0_num_sample_groups);
    DEBUG_PORT.print(F("# \tadc1_num_sample_groups = "));
    DEBUG_PORT.println(adc1_num_sample_groups);
    DEBUG_PORT.print(F("# \tadc0_group_rate  = "));
    DEBUG_PORT.println(adc0_group_rate );
    DEBUG_PORT.print(F("# \tadc1_group_rate  = "));
    DEBUG_PORT.println(adc1_group_rate );
    DEBUG_PORT.print(F("# \tadc0_readPin = "));
    DEBUG_PORT.println(adc0_readPin);
    DEBUG_PORT.print(F("# \tadc1_readPin = "));
    DEBUG_PORT.println(adc1_readPin);
    DEBUG_PORT.print(F("# \tadc0_group_iter = "));
    DEBUG_PORT.println(adc0_group_iter);
    DEBUG_PORT.print(F("# \tadc1_group_iter = "));
    DEBUG_PORT.println(adc1_group_iter);
    DEBUG_PORT.print(F("# \tadc0_samp_iter = "));
    DEBUG_PORT.println(adc0_samp_iter);
    DEBUG_PORT.print(F("# \tadc1_samp_iter = "));
    DEBUG_PORT.println(adc1_samp_iter);
    DEBUG_PORT.print(F("# \tadc0_is_busy = "));
    DEBUG_PORT.println(adc0_is_busy);
    DEBUG_PORT.print(F("# \tadc1_is_busy = "));
    DEBUG_PORT.println(adc1_is_busy);
    DEBUG_PORT.print(F("# \tadc0_group_is_ready = "));
    DEBUG_PORT.println(adc0_group_is_ready);
    DEBUG_PORT.print(F("# \tadc1_group_is_ready = "));
    DEBUG_PORT.println(adc1_group_is_ready);
}

void ADC_CONFIG_sCmd_action_handler(SerialCommand this_sCmd){
    // 0: channel 0, 1: channel 1, 2: synchronized
    int level;
    char *arg = this_sCmd.next();
    if (arg == NULL){
        this_sCmd.print(F("#ERROR: ADC.CONFIG requires 1 argument 'mode' = {0:CHAN0,1:CHAN1,2:SYNC}\n"));
    }
    else{
        level = atoi(arg);
        switch(level){
            case 0: adc_config_mode = ADC_CONFIG_CHAN0;
                    adc_chan0_is_configured = true;
                    break;
            case 1: adc_config_mode = ADC_CONFIG_CHAN1;
                    adc_chan1_is_configured = true;
                    break;
            case 2: adc_config_mode = ADC_CONFIG_SYNC;
                    break;
            default:{
                this_sCmd.print(F("#ERROR: ADC.CONFIG requires 1 argument 'mode' = {0:CHAN0,1:CHAN1,2:SYNC}\n"));
                break;
            }
        }
    }
}

void ADC_SET_READ_PIN_sCmd_action_handler(SerialCommand this_sCmd){
    char *arg = this_sCmd.next();
    if (arg == NULL){
        this_sCmd.print(F("#ERROR: ADC.SET_READ_PIN requires 1 argument 'Apin'= {0,1,2,3,4,5,6,7,8,9,10,11,12,13}\n"));
    }
    else{
        int Apin = atoi(arg);
        int pin;
        switch(Apin){
            case 0: pin = A0; break;
            case 1: pin = A1; break;
            case 2: pin = A2; break;
            case 3: pin = A3; break;
            case 4: pin = A4; break;
            case 5: pin = A5; break;
            case 6: pin = A6; break;
            case 7: pin = A7; break;
            case 8: pin = A8; break;
            case 9: pin = A9; break;
            case 10: pin = A10; break;
            case 11: pin = A11; break;
            case 12: pin = A12; break;
            case 13: pin = A13; break;
            default:{
                this_sCmd.print(F("#ERROR: ADC.SET_READ_PIN requires 1 argument 'Apin'= {0,1,2,3,4,5,6,7,8,9,10,11,12,13}\n"));
                return;
            }
        }
        //set the param depending on the config mode
        if (adc_config_mode == ADC_CONFIG_CHAN0){
            adc0_readPin = pin;
            return;
        } else if (adc_config_mode == ADC_CONFIG_CHAN1){
            adc1_readPin = pin;
            return;
        } else if (adc_config_mode == ADC_CONFIG_SYNC){
            //must specify the channel next
            arg = this_sCmd.next();
            if (arg != NULL){
                int chan = atoi(arg);
                if(chan == 0){
                    adc0_readPin = pin;
                    return;
                } else if (chan == 1){
                    adc1_readPin = pin;
                    return;
                }
            }
            //failed
            this_sCmd.print(F("#ERROR: ADC.SET_READ_PIN requires 2nd argument 'chan'= {0,1} when ADC_CONFIG_SYNC mode is set.\n"));
        }
    }
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
        ADC_SAMPLING_SPEED speed;
        switch(level){
            case 0: speed = ADC_SAMPLING_SPEED::VERY_LOW_SPEED;  break;
            case 1: speed = ADC_SAMPLING_SPEED::LOW_SPEED;       break;
            case 2: speed = ADC_SAMPLING_SPEED::MED_SPEED;       break;
            case 3: speed = ADC_SAMPLING_SPEED::HIGH_SPEED;      break;
            case 4: speed = ADC_SAMPLING_SPEED::VERY_HIGH_SPEED; break;
            default:{
                this_sCmd.print(F("#ERROR: ADC.SET_SAMP_SPEED requires 1 argument 'level'= {0,1,2,3,4}\n"));
                return;
            }
            //set the param depending on the config mode
            if (adc_config_mode == ADC_CONFIG_CHAN0 || adc_config_mode == ADC_CONFIG_SYNC){
                adc->setSamplingSpeed(speed,  ADC_0);
            }
            if (adc_config_mode == ADC_CONFIG_CHAN1 || adc_config_mode == ADC_CONFIG_SYNC){
                adc->setSamplingSpeed(speed,  ADC_1);
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
        ADC_CONVERSION_SPEED speed;
        switch(level){
            case 0: speed = ADC_CONVERSION_SPEED::VERY_LOW_SPEED;  break;
            case 1: speed = ADC_CONVERSION_SPEED::LOW_SPEED;       break;
            case 2: speed = ADC_CONVERSION_SPEED::MED_SPEED;       break;
            case 3: speed = ADC_CONVERSION_SPEED::HIGH_SPEED;      break;
            case 4: speed = ADC_CONVERSION_SPEED::VERY_HIGH_SPEED; break;
            default:{
                this_sCmd.print(F("#ERROR: ADC.SET_CONV_SPEED requires 1 argument 'level'= {0,1,2,3,4}\n"));
                return;
            }
            //set the param depending on the config mode
            if (adc_config_mode == ADC_CONFIG_CHAN0 || adc_config_mode == ADC_CONFIG_SYNC){
                adc->setConversionSpeed(speed,  ADC_0);
            }
            if (adc_config_mode == ADC_CONFIG_CHAN1 || adc_config_mode == ADC_CONFIG_SYNC){
                adc->setConversionSpeed(speed,  ADC_1);
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
            case 16: break;
            default:{
                this_sCmd.print(F("#ERROR: ADC.SET_RES requires 1 argument 'bits'= {8,10,12,16}\n"));
                return;
            }
            //set the param depending on the config mode
            if (adc_config_mode == ADC_CONFIG_CHAN0 || adc_config_mode == ADC_CONFIG_SYNC){
                adc->setResolution(bits, ADC_0);
            }
            if (adc_config_mode == ADC_CONFIG_CHAN1 || adc_config_mode == ADC_CONFIG_SYNC){
                adc->setResolution(bits, ADC_1);
            }
        }
    }
}

bool _allocate_adc0_buffer(size_t size, bool force_realloc){
    //allocate memory for new buffer, if it hasn't been already
    if (adc0_buffer == nullptr){
        //DEBUG_PORT.println("malloc");
        adc0_buffer = (uint16_t*) malloc(sizeof(uint16_t)*size);
    } else if (force_realloc){
        //DEBUG_PORT.println("realloc");
        adc0_buffer = (uint16_t *) realloc(adc0_buffer,sizeof(uint16_t)*size);
    }
    if (adc0_buffer == nullptr){
        DEBUG_PORT.print(F("#ERROR: cannot allocate memory for buffer size = "));
        DEBUG_PORT.print(size);
        adc0_buffer_size = 0;
        return false;
    }
    adc0_buffer_size = size;
    return true;
}

bool _allocate_adc1_buffer(size_t size, bool force_realloc){
    //allocate memory for new buffer, if it hasn't been already
    if (adc1_buffer == nullptr){
        //DEBUG_PORT.println("malloc");
        adc1_buffer = (uint16_t*) malloc(sizeof(uint16_t)*size);
    } else if (force_realloc){
        //DEBUG_PORT.println("realloc");
        adc1_buffer = (uint16_t *) realloc(adc1_buffer,sizeof(uint16_t)*size);
    }
    if (adc1_buffer == nullptr){
        DEBUG_PORT.print(F("#ERROR: cannot allocate memory for buffer size = "));
        DEBUG_PORT.print(size);
        adc1_buffer_size = 0;
        return false;
    }
    adc1_buffer_size = size;
    return true;
}


void ADC_SET_BUFFER_sCmd_action_handler(SerialCommand this_sCmd){
  long size;
  char *arg = this_sCmd.next();
  if (arg == NULL){
      this_sCmd.print(F("#ERROR: ADC.SET_BUFFER requires 1 argument (long size)\n"));
  }
  else{
    size = atol(arg);
    bool success = true;
    if (size > 0){
        //set the param depending on the config mode
        if (adc_config_mode == ADC_CONFIG_CHAN0 || adc_config_mode == ADC_CONFIG_SYNC){
            success &= _allocate_adc0_buffer(size,true);
        }
        if (adc_config_mode == ADC_CONFIG_CHAN1){
            success &= _allocate_adc1_buffer(size,true);
        }
        if (!success){
            this_sCmd.print(F("#ERROR: ADC.SET_BUFFER failed to allocate the requested memory!\n"));
        }
    } else{
        this_sCmd.print(F("#ERROR: ADC.SET_BUFFER argument 'size' must be > 0!\n"));
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
    if (num >= -1){
        //set the param depending on the config mode
        if (adc_config_mode == ADC_CONFIG_CHAN0 || adc_config_mode == ADC_CONFIG_SYNC){
            adc0_num_sample_groups = num;
        }
        if (adc_config_mode == ADC_CONFIG_CHAN1 || adc_config_mode == ADC_CONFIG_SYNC){
            adc1_num_sample_groups = num;
        }
    } else{
        this_sCmd.print(F("#ERROR: ADC.SET_NUM argument 'num' must be >= -1 (-1 == infinite)!\n"));
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
        //set the param depending on the config mode
        if (adc_config_mode == ADC_CONFIG_CHAN0 || adc_config_mode == ADC_CONFIG_SYNC){
            adc0_group_rate = rate;
            adc0_timer.update(1000000/adc0_group_rate);//update the rate if already running
        }
        if (adc_config_mode == ADC_CONFIG_CHAN1 || adc_config_mode == ADC_CONFIG_SYNC){
            adc1_group_rate = rate;
            adc1_timer.update(1000000/adc1_group_rate);//update the rate if already running
        }
        
    } else{
        this_sCmd.print(F("#ERROR: ADC.SET_RATE argument 'rate' was 0.0 or undefined!\n"));
    }
  }
}

void ADC_START_sCmd_action_handler(SerialCommand this_sCmd){
    adc_num_timers_active = 0;//this will be determined depending on config below
    adc0_group_iter = 0;
    adc1_group_iter = 0;
    adc0_samp_iter  = 0;
    adc1_samp_iter  = 0;
    //schedule the interrupts depending on config mode and history
    int interrupt_priority = 1; //second highest
    bool success = true;
    if (adc_config_mode == ADC_CONFIG_SYNC){
        success &= _allocate_adc0_buffer(adc0_buffer_size,false); //we interleave both channels into this buffer
        if(success){
            adc_num_timers_active = 2;
            adc->enableInterrupts(adc_synchronizedSampleISR, interrupt_priority, ADC_0); //isr, priority=255, adc_num=-1
            interrupt_priority++; //prefer this interrupt over the group timer
            // IntervalTimer object, ref: https://www.pjrc.com/teensy/td_timing_IntervalTimer.html
            adc0_timer.priority(interrupt_priority);
            adc0_timer.begin(adc0_sampleGroupTimerCallback, 1000000/adc0_group_rate);   // function called by interrupt at micros interval 
        }else{ this_sCmd.print(F("#ERROR: ADC.START failed to configure SYNC mode!\n")); return;}
    }else{
        //configure channels independently
        if (adc_chan0_is_configured){
            success &= _allocate_adc0_buffer(adc0_buffer_size,false); //we interleave both channels in this buffer
            if(success){
                adc_num_timers_active++; //include this channel
                adc->setAveraging(1, ADC_0);
                adc->enableInterrupts(adc0_sampleISR, interrupt_priority, ADC_0);
                interrupt_priority++; //prefer this interrupt if chan 1 is also configured
            }else{ this_sCmd.print(F("#ERROR: ADC.START failed to configure CHAN0 mode!\n")); return;}
        }
        if (adc_chan1_is_configured){
            success &= _allocate_adc1_buffer(adc1_buffer_size,false); //we interleave both channels in this buffer
            if(success){
                adc_num_timers_active++; //include this channel
                adc->setAveraging(1, ADC_1);
                adc->enableInterrupts(adc1_sampleISR, interrupt_priority, ADC_1);
                interrupt_priority++; //prefer this interrupt over the group timer
            }else{ this_sCmd.print(F("#ERROR: ADC.START failed to configure CHAN1 mode!\n")); return;}
        }
        //now configure callbacks independently, IntervalTimer object, ref: https://www.pjrc.com/teensy/td_timing_IntervalTimer.html
        if (adc_chan0_is_configured){
            adc0_timer.priority(interrupt_priority);
            adc0_timer.begin(adc0_sampleGroupTimerCallback, 1000000/adc0_group_rate);   // function called by interrupt at micros interval 
            interrupt_priority++; //prefer this interrupt over channel 1
        }
        if (adc_chan1_is_configured){
            adc1_timer.priority(interrupt_priority);
            adc1_timer.begin(adc1_sampleGroupTimerCallback, 1000000/adc1_group_rate);   // function called by interrupt at micros interval 
            interrupt_priority++; //in case we add more callbacks
        }
    }
    //anything else to do?
}

void ADC_STOP_sCmd_action_handler(SerialCommand this_sCmd){
    int chan;
    char *arg = this_sCmd.next();
    if (arg == NULL){
        // IntervalTimer object, ref: https://www.pjrc.com/teensy/td_timing_IntervalTimer.html
        adc0_timer.end();
        adc1_timer.end();
    }
    else{
        chan = atoi(arg);
        if(chan == 0){
            adc0_timer.end();
        } else if (chan == 1){
            adc1_timer.end();
        } else {
            this_sCmd.print(F("#ERROR: ADC.STOP must have no args (stop all) or specify channel '0' or '1'!\n"));
        }
    }
}

void TEMPMON_GET_TEMP_sCmd_query_handler(SerialCommand this_sCmd){
    this_sCmd.println(tempmonGetTemp());
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

