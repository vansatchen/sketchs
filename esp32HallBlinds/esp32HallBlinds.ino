// Sketch for control blinds with encoder or http requests.
// Allows control blinds using GET requests like http://x.x.x.x/?blinds1=34 to move blinds 1 to position 34
// Development by vansatchen.

#include <WiFi.h>
#include <Update.h>
#include <EEPROM.h>
#include "GyverEncoder.h"
#include "auth.h"

#define blindsNum  3
#define FW_VERSION 1000

// For OTA update
long contentLength = 0;
bool isValidContentType = false;
String host = "192.168.1.159";
#define port 80
#define phpfile "/UpgradeFW/update.php"
#define bin "esp32HallBlinds.bin"

// Utility to extract header value from headers
String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}

WiFiServer server(port);
WiFiClient client;

String header;  // Variable to store the HTTP request

// Encoder
#define CLK 26
#define DT 14
#define SW 27

Encoder enc1(CLK, DT, SW);

// Motor
int pins[][4] = {{15, 2, 0, 4},{16, 17, 5, 18},{19, 21, 22, 23}};
bool motorPhases[8][4] = {
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1},
  {1, 0, 0, 0}
};
int motoSteps = 512; // 512 - full circle
int motor = 0;
bool clickFlag = false;
unsigned long previousMillis = 0;
const long interval = 4000;
int counter[] = {0, 1, 2};

#define blindsNum 3

// Leds
int led[] = {32, 33, 25};

// Domoticz
String domoserver = "192.168.1.44";
#define domoport 8080

void setup() {
  EEPROM.begin(blindsNum);
  for (int i = 0; i < blindsNum; i++) {
    counter[blindsNum] = EEPROM.read(blindsNum);
    for (int m = 0; m < 4; m++) pinMode(pins[i][m], OUTPUT); // Motors
  }

  // Encoder
  enc1.setType(TYPE2);
  attachInterrupt(0, isr, CHANGE);

  for (int i = 0; i < blindsNum; i++) pinMode(led[i], OUTPUT); // Leds

  // Connect to Wi-Fi network with SSID and password
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  server.begin();

  execOTA();  // Execute OTA Update

  blinking(2); // Blinking leds at startup
}

void loop() {
  enc1.tick();
  if (clickFlag == false){
    while(enc1.isPress()){ // Watch switch
      digitalWrite(led[motor], HIGH);
      clickFlag = true;
      previousMillis = millis(); // Set timer
    }
  } else {
    if (enc1.isTurn()) previousMillis = millis(); // Set timer
    if (enc1.isRight()){
      stepForward(motor, 8);
      counter[motor]++;
    }
    if (enc1.isLeft()){
      stepReverse(motor, 8);
      counter[motor]--;
    }
    if (enc1.isPress()){
      if (motor == 0) {
        motor = 1;
        digitalWrite(led[0], LOW);
        digitalWrite(led[1], HIGH);
        digitalWrite(led[2], LOW);
        previousMillis = millis();
      } else if (motor == 1) {
        motor = 2;
        digitalWrite(led[0], LOW);
        digitalWrite(led[1], LOW);
        digitalWrite(led[2], HIGH);
        previousMillis = millis();
      } else if (motor == 2) {
        motor = 0;
        digitalWrite(led[0], HIGH);
        digitalWrite(led[1], LOW);
        digitalWrite(led[2], LOW);
        previousMillis = millis();
      }
    }
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis; // Disable moving
      clickFlag = false;
      for (int i = 0; i < blindsNum; i++) digitalWrite(led[i], LOW);
      counter[0] = constrain(counter[0], 0, 50); // Limit from 0 to 255
      counter[1] = constrain(counter[1], 0, 50);
      counter[2] = constrain(counter[2], 0, 50);
      EEPROM.write(0, counter[0]);
      EEPROM.write(1, counter[1]);
      EEPROM.write(2, counter[2]);
      EEPROM.commit();
    }
  }

  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // Use variables like blindsN=x, where x: 1-50
            if (header.indexOf("?blinds1=") >= 0) {
              header.replace("GET /?blinds1=", "");
              header.replace(" HTTP/1.1", "");
              int blinds1Val = atoi(header.c_str());
              blinds1Val = constrain(blinds1Val, 0, 50);
              int checkBlinds1Val = blinds1Val - counter[0];
              if (checkBlinds1Val > 0) {
                stepForward(0, checkBlinds1Val * 8);
                EEPROM.write(0, blinds1Val);
                EEPROM.commit();
                counter[0] = blinds1Val;
              }
              if (checkBlinds1Val < 0) {
                checkBlinds1Val = checkBlinds1Val * -1;
                stepReverse(0, checkBlinds1Val * 8);
                EEPROM.write(0, blinds1Val);
                EEPROM.commit();
                counter[0] = blinds1Val;
              }
            }
            if (header.indexOf("?blinds2=") >= 0) {
              header.replace("GET /?blinds2=", "");
              header.replace(" HTTP/1.1", "");
              int blinds2Val = atoi(header.c_str());
              blinds2Val = constrain(blinds2Val, 0, 50);
              int checkBlinds2Val = blinds2Val - counter[1];
              if (checkBlinds2Val > 0) {
                stepForward(1, checkBlinds2Val * 8);
                EEPROM.write(1, blinds2Val);
                EEPROM.commit();
                counter[1] = blinds2Val;
              }
              if (checkBlinds2Val < 0) {
                checkBlinds2Val = checkBlinds2Val * -1;
                stepReverse(1, checkBlinds2Val * 8);
                EEPROM.write(1, blinds2Val);
                EEPROM.commit();
                counter[1] = blinds2Val;
              }
            }
            if (header.indexOf("?blinds3=") >= 0) {
              header.replace("GET /?blinds3=", "");
              header.replace(" HTTP/1.1", "");
              int blinds3Val = atoi(header.c_str());
              blinds3Val = constrain(blinds3Val, 0, 50);
              int checkBlinds3Val = blinds3Val - counter[2];
              if (checkBlinds3Val > 0) {
                stepForward(2, checkBlinds3Val * 8);
                EEPROM.write(2, blinds3Val);
                EEPROM.commit();
                counter[2] = blinds3Val;
              }
              if (checkBlinds3Val < 0) {
                checkBlinds3Val = checkBlinds3Val * -1;
                stepReverse(2, checkBlinds3Val * 8);
                EEPROM.write(2, blinds3Val);
                EEPROM.commit();
                counter[2] = blinds3Val;
              }
            }
            
            // The HTTP response ends with another blank line
            client.println();
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    header = "";  // Clear the header variable
    client.stop();  // Close the connection
  }
}

// OTA Logic 
void execOTA() {
  if (client.connect(host.c_str(), port)) {
    // Connection Succeed. Fecthing the bin

    // Get the contents of the bin file
    client.print(String("GET ") + phpfile + "?file=" + bin + "&" + "FW_VERSION=" + FW_VERSION + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Cache-Control: no-cache\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        client.stop();
        return;
      }
    }

    // Once the response is available, check stuff
    while (client.available()) {
      // read line till /n
      String line = client.readStringUntil('\n');
      // remove space, to check if the line is end of headers
      line.trim();

      // if the the line is empty, this is end of headers
      // break the while and feed the remaining `client` to the Update.writeStream();
      if (!line.length()) {
        //headers ended
        break; // and get the OTA started
      }

      // Check if the HTTP Response is 200 else break and Exit Update
      if (line.startsWith("HTTP/1.1")) {
        if (line.indexOf("200") < 0) {
          break;
        }
      }

      // extract headers here. Start with content length
      if (line.startsWith("Content-Length: ")) {
        contentLength = atol((getHeaderValue(line, "Content-Length: ")).c_str());
      }

      // Next, the content type
      if (line.startsWith("Content-Type: ")) {
        String contentType = getHeaderValue(line, "Content-Type: ");
        if (contentType == "application/octet-stream") {
          isValidContentType = true;
        }
      }
    }
  }

  // check contentLength and content type
  if (contentLength && isValidContentType) {
    // Check if there is enough to OTA Update
    bool canBegin = Update.begin(contentLength);

    // If yes, begin
    if (canBegin) {
      // No activity would appear on the Serial monitor
      size_t written = Update.writeStream(client);

      if (Update.end()) {
        if (Update.isFinished()) {
          ESP.restart();
        }
      }
    } else {
      // not enough space to begin OTA. Understand the partitions and space availability
      client.flush();
    }
  } else {
    client.flush();
  }
}

void isr() { // Encoder lookup
  enc1.tick();
}

void stepForward(int motorVal, int stepsData){
  for (int motoStep = 0; motoStep < stepsData; motoStep++) {
    for (int phase = 0; phase < 8; phase++) {
      for (int i = 0; i < 4; i++) {
        digitalWrite(pins[motorVal][i], ((motorPhases[phase][i] == 1) ? HIGH : LOW));
      }
      delay(2);
    }
  }
  for (int i = 0; i < 4; i++) digitalWrite(pins[motorVal][i], LOW); // Clear motor gpios
}

void stepReverse(int motorVal, int stepsData){
  for (int motoStep = 0; motoStep < stepsData; motoStep++) {
    for (int phase = 8; phase > 0; phase--) {
      for (int i = 0; i < 4; i++) {
        digitalWrite(pins[motorVal][i], ((motorPhases[phase][i] == 1) ? HIGH : LOW));
      }
      delay(2);
    }
  }
  for (int i = 0; i < 4; i++) digitalWrite(pins[motorVal][i], LOW); // Clear motor gpios
}

void domoUpdate(int dataVar, int idxVar){
  if (client.connect(domoserver.c_str(), domoport)) {
    client.print("GET /json.htm?type=command&param=udevice&idx=");
    client.print(idxVar);
    client.print("&svalue=");
    client.print(dataVar);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.print(domoserver);
    client.print(":");
    client.println(domoport);
    client.println("Cache-Control: no-cache");
    client.println("Connection: close");
    client.println();
  }
}

void blinking(int blinkNum){
  for (int blinkData = 0; blinkData < blinkNum; blinkData++){
    for (int i = 0; i < blindsNum; i++) digitalWrite(led[i], LOW);
    delay(100);
    for (int i = 0; i < blindsNum; i++) digitalWrite(led[i], HIGH);
    delay(100);
    for (int i = 0; i < blindsNum; i++) digitalWrite(led[i], LOW);
    delay(100);
  }
}
