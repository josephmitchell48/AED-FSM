#include <avr/io.h>
#include "helpers.h"

// declaring global timer controlled by timer0 interrupt, increments at 1kHz
volatile unsigned long GLOBAL_TIME, ready_to_classify_counter;

// declaring the volatile vals for low battery interrupts
volatile long LOW_BATTERY_FLAG  = 120; //measured in seconds
volatile int LOW_BATTERY_FLAG_COUNTER  = 0;

// declaring all the pin numbers
const int HR_pin2 = 13, HR_pin1 = 12, HR_pin0 = 11, CPR_pin = 5, charging_pin = 6, shocking_pin = 7, battery_pin = 2, ready_to_classify_pin = 4;
const int ledPins[] = {8, 9, 10};

// declaring values for classifying heart rates 
int currentHR2, currentHR1, currentHR0, lastHR2, lastHR1, lastHR0, HR2_switch, HR1_switch, HR0_switch;

// declaring state encoding variables
bool s0,s1,s2,s3,cprPrint;

// assorted boolean values used to determine when to stay in loops and when to give outputs
volatile bool charging, charged, shock, CPR, readyToClassify, compressions, breathes, firstCPRThrough, readyToClassifyPrint;
bool classify, first_pass;

void setup() {










  
  cli();//stop interrupts while bit bashing
  //Bit Bashing timer0 to interrupt at 1kHz for CPR timing & global counter
  
  TCCR0A = 0, TCCR0B = 0;// set TCCR2A & TCCR2B register to 0 -- Clearing the register to ensure all values input are controlled
  TCNT0  = 0;//initialize counter value to 0
  // Set 256 prescaler - previously was 64, now 4 times slower (used to be 1khz, now 4)
  TCCR0B |= (_BV(CS02));
  // set compare match register for 4khz increments
  OCR0A = 255;
  // turn on CTC mode - clear timer on compare match with OCR0A above
  TCCR0A |= _BV(WGM01);
  // enable timer compare interrupt -- allows the interrupts found at very bottom to work
  TIMSK0 |= _BV(OCIE0A);

  //Bit Bashing timer1 to interrupt at 1Hz for low battery
  TCCR1A = 0, TCCR1B = 0;// set TCCR1A & TCCR1B register to 0 -- same as above, clear slate
  TCNT1  = 0;//initialize counter value to 0
  // Set 1024 prescaler
  TCCR1B |= (_BV(CS12) | _BV(CS10));
  // set compare match register for 1hz increments 
  OCR1A = 15624;
  // turn on CTC mode - clear timer on compare with 0CR1A above
  TCCR1B |= _BV(WGM12);
  // enable timer compare interrupt
  TIMSK1 |= _BV(OCIE1A);
  sei();//allow interrupts now that bit bashing is complete

  // begin serial
  Serial.begin(9600);

  // set all input and output pins
  pinMode(charging_pin, INPUT);
  pinMode(shocking_pin, INPUT);
  pinMode(CPR_pin, INPUT);
  pinMode(HR_pin2, INPUT);
  pinMode(HR_pin1, INPUT);
  pinMode(HR_pin0, INPUT);
  pinMode(ledPins[0], OUTPUT);
  pinMode(ledPins[1], OUTPUT);
  pinMode(ledPins[2], OUTPUT);
  pinMode(battery_pin, OUTPUT);
  pinMode(ready_to_classify_pin, OUTPUT);

  // declaring button interrupt
  attachInterrupt(digitalPinToInterrupt(3), shock_interrupt, RISING);

  // initializing most bools and ints to start at 0, includes seting state to 0,0,0,0
  currentHR2 = 0, currentHR1 = 0, currentHR0 = 0;
  HR2_switch = 0, HR1_switch = 0, HR0_switch = 0;
  Serial.println("System Power On");
  updateState(0, 0, 0, 0);
}

void loop() {
  //Change the values of HR variables based on inputs from buttons and update state
  if (s0 == 0 && s1 == 0 && s2 == 0 && s3 == 0 && !CPR) waitForInput();

  //Only runs once, right after waitForInput has executed
  if (classify) {
    readyToClassify = false;
    digitalWrite(ledPins[0], HIGH);
    digitalWrite(ledPins[1], HIGH);
    digitalWrite(ledPins[2], HIGH);
    classifyHeartRates(HR2_switch, HR1_switch, HR0_switch);
    classify = false;
  }

  // wait for start charging intterupt
  unsigned long charging_entry_time = GLOBAL_TIME;
  first_pass = true;
  while(charging) {
    //logic to turn on charging light for 8 seconds
    if(first_pass){
      updateState(0, 0, 1, 1); //update state to S3
      Serial.println(" ");
      Serial.print("Global Time is ");
      Serial.print(GLOBAL_TIME/256);
      Serial.println(" seconds");
      Serial.println("Wait here for 8 Seconds while the AED charges");
      first_pass = false;
    }
    digitalWrite(ledPins[0], HIGH);
    digitalWrite(ledPins[1], LOW);
    digitalWrite(ledPins[2], LOW);
    if(GLOBAL_TIME>=charging_entry_time+(8000/4)){
      Serial.println(" ");
      Serial.print("Global Time is ");
      Serial.print(GLOBAL_TIME/256);
      Serial.println(" seconds");
      Serial.println("The AED has charged");
      digitalWrite(ledPins[0], LOW);
      digitalWrite(ledPins[1], HIGH);
      digitalWrite(ledPins[2], LOW);
      charged = true;
      charging = false;
    }
  }
  
  first_pass = true;
  while(charged) {
    if(first_pass){
          digitalWrite(ledPins[0], LOW);
          digitalWrite(ledPins[1], HIGH);
          digitalWrite(ledPins[2], LOW);
          Serial.println(" ");
          Serial.print("Global Time is ");
          Serial.print(GLOBAL_TIME/256);
          Serial.println(" seconds");
          Serial.println("The Shock is ready, press the blue button to administer");
          updateState(0, 1, 0, 0); //update state to S4
          first_pass = false;
    }
  }

  unsigned long shock_entry_time = GLOBAL_TIME;
  first_pass = true;
  while(shock){
    //logic here to turn on light that indicates shock is being administered
    if(first_pass){
        Serial.println(" ");
        Serial.print("Global Time is ");
        Serial.print(GLOBAL_TIME/256);
        Serial.println(" seconds");
        Serial.println("The Shock is being administered");
        digitalWrite(ledPins[0], HIGH);
        digitalWrite(ledPins[1], HIGH);
        digitalWrite(ledPins[2], LOW);
        first_pass = false;
      }
    digitalWrite(ledPins[0], HIGH);
    digitalWrite(ledPins[1], HIGH);
    digitalWrite(ledPins[2], LOW);
    //2 second delay before shock is over
    if(GLOBAL_TIME >= shock_entry_time+(2000/4)){
        digitalWrite(ledPins[0], HIGH);
        digitalWrite(ledPins[1], HIGH);
        digitalWrite(ledPins[2], HIGH);
        Serial.println(" ");
        Serial.print("Global Time is ");
        Serial.print(GLOBAL_TIME/256);
        Serial.println(" seconds");
        Serial.println("Shock complete, press RED button to begin CPR");
        shock = false;
        updateState(0,1,0,1);//update state to S4
      }
  }
//  Serial.println

  //Start CPR here using interrupt to wait for a button to be clicked while in either s5 or s1
  unsigned long cpr_entry_time = GLOBAL_TIME;
  if (CPR) {
    if(firstCPRThrough){
      Serial.println(" ");
      Serial.print("Global Time is ");
      Serial.print(GLOBAL_TIME/256);
      Serial.println(" seconds");
      Serial.println("Entering CPR Loop, reclassification of heart rate will be available in ~20 seconds");
      firstCPRThrough = false;
    }
    unsigned long compressions_entry_time, breathes_entry_time;
    compressions = true, breathes = false, first_pass = true;
    //Compressions loop
    compressions_entry_time = GLOBAL_TIME;
    while (compressions) {
      if (first_pass) {
        Serial.println(" ");
        Serial.print("Global Time is ");
        Serial.print(GLOBAL_TIME/256);
        Serial.println(" seconds");
        Serial.println("Begin Compressions");
        first_pass = false;
        cprPrint = true;
      }
      if (GLOBAL_TIME % (500/4) == 0) { // light flashing at 0.5Hz
        if(cprPrint){
          Serial.println("PRESS");
          cprPrint = false;
        }
        digitalWrite(ledPins[0], LOW);
        digitalWrite(ledPins[1], LOW);
        digitalWrite(ledPins[2], HIGH);
        //turn on compression LED
      }
      else {
        cprPrint = true;
        digitalWrite(ledPins[0], HIGH);
        digitalWrite(ledPins[1], HIGH);
        digitalWrite(ledPins[2], HIGH);
      }
      if (GLOBAL_TIME >= compressions_entry_time + (15000/4)) { // this means you will do 30 compressions - needs to be adjusted to be 30*modulus val
        compressions = false;
        breathes = true;
      }
      if(readyToClassifyPrint){
        Serial.println(" ");
        Serial.print("Global Time is ");
        Serial.print(GLOBAL_TIME/256);
        Serial.println(" seconds");
        Serial.println("Ready to classify");
        readyToClassifyPrint = false;
      }
    }
    first_pass = true;
    breathes_entry_time = GLOBAL_TIME;
    while (breathes) {
      if (first_pass) {
        Serial.println(" ");
        Serial.print("Global Time is ");
        Serial.print(GLOBAL_TIME/256);
        Serial.println(" seconds");
        cprPrint = true;
        Serial.println("Begin Breaths");
        first_pass = false;
      }
      if (GLOBAL_TIME >= breathes_entry_time + (2000/4) & GLOBAL_TIME <= breathes_entry_time + (4000/4)) {
        digitalWrite(ledPins[0], HIGH);
        digitalWrite(ledPins[1], HIGH);
        digitalWrite(ledPins[2], HIGH);
        cprPrint = true;
      }
      else {
        if(cprPrint){
          Serial.println("ADMINISTER BREATH");
          cprPrint = false;
        }
        digitalWrite(ledPins[0], HIGH);
        digitalWrite(ledPins[1], LOW);
        digitalWrite(ledPins[2], HIGH);
      }
      if (GLOBAL_TIME >= breathes_entry_time + (6000/4)) {
        compressions = true;
        breathes = false;
      }
        if(readyToClassifyPrint){
        Serial.println(" ");
        Serial.print("Global Time is ");
        Serial.print(GLOBAL_TIME/256);
        Serial.println(" seconds");
        Serial.println("Ready to classify");
        readyToClassifyPrint = false;
      }
    }
  }
}

// function to obtain heart rate inputs
// if it gains an input it will set classify to true so that the heart rate can be classified
// if no input then runs again to try to find one
void waitForInput() {
  Serial.println(" ");
  Serial.print("Global Time is ");
  Serial.print(GLOBAL_TIME/256);
  Serial.println(" seconds");
  Serial.println("Classifying Heart Rate, This Will Take ~10 seconds");
  updateState(0, 0, 0, 0); //update state to S0
  first_pass = true;
  unsigned long entry_time = GLOBAL_TIME;
  //loop for 10 seconds
  while (GLOBAL_TIME <= entry_time + (10000/4)) {
    if(first_pass)currentHR2 = 0, currentHR1 = 0, currentHR0 = 0;

    digitalWrite(ledPins[0], LOW);
    digitalWrite(ledPins[1], LOW);
    digitalWrite(ledPins[2], LOW);

    // sets 'last' vals to 'current' vals then reads new values
    lastHR2 = currentHR2, lastHR1 = currentHR1, lastHR0 = currentHR0;
    currentHR2 = debounce(HR_pin2), currentHR1 = debounce(HR_pin1),currentHR0 = debounce(HR_pin0);

    // every button press changes the value of the HR_switch
    if (lastHR2 == HIGH && currentHR2 == LOW) {
      HR2_switch = !HR2_switch;
      Serial.println(" ");
      Serial.print("Global Time is ");
      Serial.print(GLOBAL_TIME/256);
      Serial.println(" seconds");
      Serial.println((String)"HR2 value is:" + HR2_switch);
    }
    if (lastHR1 == HIGH && currentHR1 == LOW) {
      HR1_switch = !HR1_switch;
      Serial.println(" ");
      Serial.print("Global Time is ");
      Serial.print(GLOBAL_TIME/256);
      Serial.println(" seconds");
      Serial.println((String)"HR1 value is:" + HR1_switch);
    }
    if (lastHR0 == HIGH && currentHR0 == LOW) {
      HR0_switch = !HR0_switch;
      Serial.println(" ");
      Serial.print("Global Time is ");
      Serial.print(GLOBAL_TIME/256);
      Serial.println(" seconds");
      Serial.println((String)"HR0 value is:" + HR0_switch);
    }
    first_pass = false;
  }
  // checks if the input is invalid, if so sets all back to 0
  if( HR2_switch && HR1_switch ){
    Serial.println(" ");
    Serial.print("Global Time is ");
    Serial.print(GLOBAL_TIME/256);
    Serial.println(" seconds");
    Serial.println("Invalid Heart Rate Detected");
    HR0_switch = 0;
    HR1_switch = 0;
    HR2_switch = 0;
  }
  // checks if any input happened, as if so we now know it is not invalid
  else if (HR2_switch | HR1_switch | HR0_switch) {
    classify = true;
    return;
  }
  // if nothing detected, notifies user and remains in the waitForInput() loop
  else{
    Serial.println(" ");
    Serial.print("Global Time is ");
    Serial.print(GLOBAL_TIME/256);
    Serial.println(" seconds");
    Serial.println("No HR Detected");
  }
}

void updateState(int state3, int state2, int state1, int state0) {
  s3 = state3;
  s2 = state2;
  s1 = state1;
  s0 = state0;
  Serial.println((String)"The State has been updated to: " + s3 + s2 + s1 + s0);
}

bool classifyHeartRates(int HR2, int HR1, int HR0) {
  // checks if HR1 == 1 OR HR0 == 1 AND that HR2 == 0 as this is the classification shared by the non-shockable rythms
  if ((HR1 | HR0) & !HR2) {
    updateState(0, 0, 0, 1);
    Serial.println(" ");
    Serial.print("Global Time is ");
    Serial.print(GLOBAL_TIME/256);
    Serial.println(" seconds");
    Serial.println("No shock needed, press RED button to begin CPR");
    return false;
  }
  // checks if HR2 == 1 as this is the classification shared by all shockable rythms
  if (HR2) {
    // updates state and informs user, then returns true to inform system a shock is required
    updateState(0, 0, 1, 0); //Advise shock
    Serial.println(" ");
    Serial.print("Global Time is ");
    Serial.print(GLOBAL_TIME/256);
    Serial.println(" seconds");
    Serial.println("Shock required, press GREEN button to charge AED");
    digitalWrite(ledPins[0], HIGH);
    digitalWrite(ledPins[1], HIGH);
    digitalWrite(ledPins[2], HIGH);
    return true;
  }
}

void shock_interrupt() {
  // if in state 2 and appropriate button is pressed, start charging and update state to s3
  if (debounce(charging_pin) && s3 == 0 && s2 == 0 && s1 == 1 && s0 == 0) {
    charging = true;
//    Serial.println("State 3 Interrupt");
    return;
  }
  // if in state 4 and button is pressed, administor shock
  if (debounce(shocking_pin) && s3 == 0 && s2 == 1 && s1 == 0 && s0 == 0) {
    charged = false;
    shock = true;
    LOW_BATTERY_FLAG = LOW_BATTERY_FLAG-20;
//    Serial.println("State 4 Interrupt");
    return;
  }
  // if in state 5 or 1 and button is pressed, start CPR loop
  if (debounce(CPR_pin) && s3 == 0 && s1 == 0 && s0 == 1 && !readyToClassify) {
    CPR = true;
    firstCPRThrough = true;
    readyToClassify = false;
    return;
  }
  // if in CPR loop AND appropriate button is pressed AND ready to classify (time has passed) send back to classification loop
  if (debounce(CPR_pin) && readyToClassify ) {
//    Serial.println("Send Back To Classify");
    CPR = false, breathes = false, compressions = false;
    HR2_switch = 0, HR1_switch = 0, HR0_switch = 0;
    digitalWrite(ready_to_classify_pin, LOW);
    updateState(0, 0, 0, 0);
  }
}

ISR(TIMER0_COMPA_vect) {
  GLOBAL_TIME += 1; //incremented by 4 millisecond since that is the interrupt frequency
  if(CPR){
    ready_to_classify_counter+=1;
    if(ready_to_classify_counter % (20000/4) == 0){
        readyToClassify = true;
        readyToClassifyPrint = true;
        digitalWrite(ready_to_classify_pin, HIGH);
      }
    }
  else ready_to_classify_counter = 0;
}
ISR(TIMER1_COMPA_vect) {
  LOW_BATTERY_FLAG_COUNTER += 1;
  if (LOW_BATTERY_FLAG_COUNTER >= LOW_BATTERY_FLAG) {
    digitalWrite(battery_pin, HIGH);
      Serial.println(" ");
      Serial.print("Global Time is ");
      Serial.print(GLOBAL_TIME/256);
      Serial.println(" seconds");
      Serial.println("Battery Low, device must recharge before applying another shock");
  }
}
