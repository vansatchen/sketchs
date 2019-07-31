#include <Wire.h>
#include <VL53L0X.h>
#include <Servo.h>

VL53L0X sensor;
Servo servo;

int presencePlace = 400;
int maxCheckCount = 25, checkCount = 0, workCount = 0;
int timeForWC = 10000;
unsigned long previousCheckMillis = 0, previousWorkMillis = 0;
const long intervalCheck = 200, intervalWork = 200;
bool ledState = false, checkState = false, checkCat = false, timeToWork = false;;
int blueLed = 2, greenLed = 3, redLed = 4;

#define servoPin 9
#define servoMinImp 640
#define servoMaxImp 2400

void setup(){
  Serial.begin(9600);
  Wire.begin();

  sensor.init();
  sensor.setTimeout(500);
  sensor.setMeasurementTimingBudget(200000);

  pinMode(blueLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(redLed, OUTPUT);

  servo.attach(servoPin, servoMinImp, servoMaxImp);
  servo.write(0);
  
  Serial.println("Going to loop");
}

void loop(){
  checkSensor();
  if((!checkCat) and (timeToWork)){
    Serial.println("Time to work!!!");
    letsWork();
  }
}

void checkSensor(){
  int distanceLaser = sensor.readRangeSingleMillimeters();
  if(distanceLaser <= presencePlace){
    Serial.println("Somebody present");
    checkState = true;
    objectDetected();
  } else {
    if(checkState){ // If count is interrupted
      checkCount = 0;
      checkCat = false;
    }
    gl();
  }
}
void objectDetected(){
  unsigned long currentCheckMillis = millis();
  if (currentCheckMillis - previousCheckMillis >= intervalCheck) {
    previousCheckMillis = currentCheckMillis;
    if(!ledState){ // Blink led - showing that cat is detected
      gl();
      ledState = true;
    } else {
      el();
      ledState = false;
    }
    checkCount++;
    Serial.println(checkCount);
    if(checkCount >= 20){
      bl();
      Serial.println("Ok, waiting");
      delay(timeForWC);
      checkCount = 0;
      checkCat = true;
      timeToWork = true;
    }
  }
}

void bl(){
  digitalWrite(blueLed, HIGH);
  digitalWrite(greenLed, LOW);
  digitalWrite(redLed, LOW);
}

void gl(){
  digitalWrite(blueLed, LOW);
  digitalWrite(greenLed, HIGH);
  digitalWrite(redLed, LOW);
}

void rl(){
  digitalWrite(blueLed, LOW);
  digitalWrite(greenLed, LOW);
  digitalWrite(redLed, HIGH);
}

void el(){ // Empty leds
  digitalWrite(blueLed, LOW);
  digitalWrite(greenLed, LOW);
  digitalWrite(redLed, LOW);
}

void letsWork(){
/*  unsigned long currentWorkMillis = millis();
  if (currentWorkMillis - previousWorkMillis >= intervalWork) {
    previousWorkMillis = currentWorkMillis;
    if(!ledState){ // Blink led - working
      rl();
      ledState = true;
    } else {
      el();
      ledState = false;
    }
    workCount++;
    Serial.println(workCount);
    if(workCount >= 10){
      el();
      timeToWork = false;
      workCount = 0;
    }
  }*/
  rl();
  for(int i=0 ; i<90 ; i++){
    servo.write(i);
    delay(20);
  }
  delay(2000);
  for(int i=90 ; i>=0 ; i--){
    servo.write(i);
    delay(20);
  }
  timeToWork = false;
  el();
}
