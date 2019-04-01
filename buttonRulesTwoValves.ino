const int ledPin = 13;
const int buttonPin = 2;
const int openPinA =  8;
const int clsePinA =  9;
const int openPinB =  10;
const int clsePinB =  11;

// переменные
int buttonState = 0;
int buttonTemp = 0;
int ledVar = 0;

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(openPinA, OUTPUT);
  pinMode(clsePinA, OUTPUT);
  pinMode(openPinB, OUTPUT);
  pinMode(clsePinB, OUTPUT);
  pinMode(buttonPin, INPUT);
  digitalWrite(ledPin, LOW);
  digitalWrite(openPinA, HIGH);
  digitalWrite(clsePinA, HIGH);
  digitalWrite(openPinB, HIGH);
  digitalWrite(clsePinB, HIGH);
}
void loop(){
  // считываем значения с входа кнопки
  buttonState = digitalRead(buttonPin);

  if (buttonState == LOW) {    
    if (buttonTemp == 0) {
      // Закрываем кран A
      digitalWrite(ledPin, HIGH);
      digitalWrite(clsePinA, LOW);  
      ledBlink(250);
      digitalWrite(clsePinA, HIGH);
      // Ждем 15 сек
      ledBlink(1000);
      // Открываем кран B
      digitalWrite(openPinB, LOW);
      ledBlink(250);
      digitalWrite(openPinB, HIGH);
      buttonTemp = 1;
    }
    else {
      // Закрываем кран B
      digitalWrite(ledPin, LOW);
      digitalWrite(clsePinB, LOW);  
      ledBlink(250);
      digitalWrite(clsePinB, HIGH);
      // Ждем 15 сек
      ledBlink(1000);
      // Открываем кран A
      digitalWrite(openPinA, LOW);
      ledBlink(250);
      digitalWrite(openPinA, HIGH);
      digitalWrite(ledPin, LOW);
      buttonTemp = 0;
    }
  }
}

void ledBlink(int blinkTime) {
  while(ledVar < 17) {
    digitalWrite(ledPin, LOW);
    delay(blinkTime);
    digitalWrite(ledPin, HIGH);
    delay(blinkTime);
    ledVar++;
  }
  ledVar = 0;
}
