//teensyduino standard
#include <stdint.h>

//teensyduino community
#include <ADC.h>

//third party
#include <SerialCommand.h>  /* https://github.com/p-v-o-s/Arduino-SerialCommand */

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
    sCmd_USB.addCommand("IDN?",             IDN_sCmd_query_handler);
    sCmd_USB.addCommand("D",                DEBUG_sCmd_action_handler);// dumps data to debugging port
    sCmd_USB.addCommand("ADC.START",        ADC_START_sCmd_action_handler);
    sCmd_USB.addCommand("ADC.STOP",         ADC_STOP_sCmd_action_handler);
    //--------------------------------------------------------------------------
    pinMode(LED_BUILTIN, OUTPUT);
}

/******************************************************************************/
// ADC subsystem
ADC *adc = new ADC(); // adc object

// Create an IntervalTimer object, ref: https://www.pjrc.com/teensy/td_timing_IntervalTimer.html
float adc0_sample_rate = 1.0;
float adc0_readPin = A2;
const int adc0_num_continuous_samples = 1;

uint16_t  adc0_buffer[adc0_num_continuous_samples];

IntervalTimer adc0_timer;

volatile int adc0_num_iter = 0;
volatile int adc0_num_iter_last = 0;
volatile bool adc0_is_busy = false;

void adc0_StartConversionTimerCallback(void){
    //DEBUG_PORT.print(since_setup_started_micros);
    //DEBUG_PORT.println(" adc0_StartConversionTimerCallback");
    digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN));
    if (!adc0_is_busy){
        adc0_num_iter = 0;
        adc0_num_iter_last = 0;
        adc0_is_busy = true;
        adc->startContinuous(adc0_readPin, ADC_0);
    }
}

void adc0_isr(void) {
    //DEBUG_PORT.print(since_setup_started_micros);
    //DEBUG_PORT.println(" adc0_isr");
    adc0_buffer[adc0_num_iter] = (uint16_t) adc->adc0->analogReadContinuous();
    adc0_num_iter++;
    if  (adc0_num_iter == adc0_num_continuous_samples){
        //last sample, shut it down
        adc->stopContinuous(ADC_0);
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
    int adc0_new_samples = adc0_num_iter - adc0_num_iter_last;
    if (adc0_new_samples > 0){
        is_idle = false;
        for(int i=0; i < adc0_new_samples; i++){
            int buff_index = adc0_num_iter_last + i;
            Serial.print(adc0_buffer[buff_index]);
            if (buff_index < adc0_num_continuous_samples - 1){
                Serial.print(",");
            } else{
                Serial.print("\n");
            }
        }
        // save state of the past loop
        adc0_num_iter_last = adc0_num_iter;
    }
    //rest a bit if doing nothing
    if (is_idle){
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
    DEBUG_PORT.print(F("# ADC params:\n"));
    DEBUG_PORT.print(F("# \tadc0_num_iter = "));
    DEBUG_PORT.println(adc0_num_iter);
    DEBUG_PORT.print(F("# \tadc0_num_iter_last = "));
    DEBUG_PORT.println(adc0_num_iter_last);
}

void ADC_START_sCmd_action_handler(SerialCommand this_sCmd){
    adc0_num_iter = 0;
    adc->enableInterrupts(adc0_isr, ADC_0);
    // IntervalTimer object, ref: https://www.pjrc.com/teensy/td_timing_IntervalTimer.html
    adc0_timer.priority(1); //second highest priority
    adc0_timer.begin(adc0_StartConversionTimerCallback, 1000000/adc0_sample_rate);   // function called by interrupt at micros interval 
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

