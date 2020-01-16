
#include <SerialCommand.h>
SerialCommand sCmd(Serial);         // The demo SerialCommand object, initialize with any Stream object


#define max_samples 100000
unsigned long values[max_samples];

void setup() {

//sCmd.addCommand("SAMPLE",    Sample);        
sCmd.addCommand("SAMPLE",     processCommand);  // Converts two arguments to integers and echos them back
  sCmd.setDefaultHandler(unrecognized);      // Handler for command that isn't matched  (says "What?")

  pinMode(A0, INPUT);
  Serial.begin(9600);

  analogReadResolution(10);
}

int value;

void loop() {

int num_bytes = sCmd.readSerial();      // fill the buffer
  if (num_bytes > 0){
    sCmd.processCommand();  // process the command
  }
  delay(10);
}

void processCommand(SerialCommand this_sCmd) {
  int aNumber;
  char *arg;

  //this_sCmd.println("We're in processCommand");
  arg = this_sCmd.next();
  if (arg != NULL) {
    aNumber = atoi(arg);    // Converts a char string to an integer
   // this_sCmd.print("First argument was: ");
   // this_sCmd.println(aNumber);

   // Serial.print("Sampling for ");
    //Serial.print(aNumber);
    //Serial.println(" values");

    for (uint16_t i = 0; i < aNumber; i++) {
    value = analogRead(A0);
    values[i]=value;
  }


  for (uint16_t i = 0; i < aNumber; i++) {
    //value = analogRead(A0);
    //values[i]=value;
    Serial.println(values[i]);
  }

  
 
  }
  else {
    this_sCmd.println("No arguments");
  }
}

// This gets set as the default handler, and gets called when no other command matches.
void unrecognized(SerialCommand this_sCmd) {
  SerialCommand::CommandInfo command = this_sCmd.getCurrentCommand();
  this_sCmd.print("Did not recognize \"");
  this_sCmd.print(command.name);
  this_sCmd.println("\" as a command.");
}
