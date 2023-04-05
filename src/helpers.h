//#include "Arduino.h"
//
//boolean debounce(int pin){
//  boolean state;
//  boolean previousState;
//  previousState = digitalRead(pin); // store switch state
//  for (int counter = 0; counter < debounceDelay; counter++)
//  {
//    delay(1); // wait for 1 millisecond
//    state = digitalRead(pin); // read the pin
//    if ( state != previousState)
//    {
//      counter = 0; // reset the counter if the state changes
//      previousState = state; // and save the current state
//    }
//  }
//  // here when the switch state has been stable longer than the debounce period
//  return state;
//}
//  
