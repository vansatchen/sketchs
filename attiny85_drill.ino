#include <ButtonEvents.h>

int buttonPin = 0;
int PWM_pin = 1;
const int L0=0, L1=15, L2=60, L3=160, L4=240;
int drillState = L0, newDrillState;

ButtonEvents myButton;

void setup() {
  pinMode(buttonPin, INPUT);
  myButton.attach(buttonPin);
  myButton.activeHigh();
  myButton.debounceTime(25);
  
  pinMode(PWM_pin, OUTPUT);
  analogWrite(PWM_pin, L1);
  delay(80);
  digitalWrite(PWM_pin, LOW);
}

void loop() {
  myButton.update();
  if(myButton.tapped()){ // One click
    pressButton(drillState);
    if(!newDrillState == L1){
      for(int i=drillState; i <= newDrillState; i++){
        analogWrite(PWM_pin, i);
        delay(10);
      }
    } else {
      analogWrite(PWM_pin, newDrillState);
    }
    drillState = newDrillState;
  }

  if(myButton.doubleTapped()){ // Double click
    pressButton(1000);
    for(int i=drillState; i >= newDrillState; i--){
      analogWrite(PWM_pin, i);
      delay(10);
    }
    digitalWrite(PWM_pin, LOW);
    drillState = newDrillState;
  }
  delay(10);
}

int pressButton(int currentState){
  switch (currentState){
    case L0:
      newDrillState = L1;
      break;
    case L1:
      newDrillState = L2;
      break;
    case L2:
      newDrillState = L3;
      break;
    case L3:
      newDrillState = L4;
      break;
    case L4:
      newDrillState = L1;
      break;
    case 1000:
      newDrillState = L0;
  }
  return newDrillState;
}
