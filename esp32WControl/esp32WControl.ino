// Sketch for control fan and 2 valves, with tranning valves.
// Allows control fan and valves using GET requests like http://x.x.x.x/?30on to start fan for 30 min
// and http://x.x.x.x/?valve2close to close valve 2.
// Development by vansatchen.

#include <WiFi.h>
#include <Update.h>
#include "time.h"
#include <Wire.h>
#include <RtcDS3231.h>
#include <PCF8591.h>
#include "paj7620.h"
#include "auth.h"

#define FW_VERSION 1033

unsigned long previousWFMillis = 0;
const long checkWFInterval = 60000;

// For OTA update
long contentLength = 0;
bool isValidContentType = false;
String host = "192.168.1.159";
#define port 80
#define phpfile "/UpgradeFW/update.php"
#define bin "esp32WControl.bin"

// Utility to extract header value from headers
String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}

WiFiServer server(port);
WiFiClient client;

String header;  // Variable to store the HTTP request

// NTP
//const char* ntpServer = "pool.ntp.org";
const char* ntpServer = "192.168.1.159";
const long  gmtOffset_sec = 14400;
const int   daylightOffset_sec = 3600;
#define hour4ntp 3
#define min4ntp  0
bool isHour4ntp = false;
int currenthour, currentmin;
#define hour4valves 4
#define min4valves  0
bool isHour4valves = false;

// RTC
RtcDS3231<TwoWire> rtcObject(Wire);

// MQ-2
unsigned long previousMillis = 0;
const long interval = 10000;
bool isHour4mq = false;
int patternGaz;
#define ledPin 2
#define gazDiff 10
bool isFanOn = false;

// Memory
unsigned long previousMemMillis = 0;

// PCF8591
#define PCF8591_I2C_ADDRESS 0x48
PCF8591 pcf8591(PCF8591_I2C_ADDRESS);

// HTTP
bool forceRun = false;
int timerInt = 0;
bool timerVal = false;
unsigned long currentForceRunMillis = 0;

// Valves
#define kran1close 26
#define kran1open  14
#define kran2close 27
#define kran2open  13

// Domoticz
String domoserver = "192.168.1.44";
#define domoport 8080

// Gesture sensor
#define GES_REACTION_TIME    300
#define GES_ENTRY_TIME      500
#define GES_QUIT_TIME      800

void setup() {
  Serial.begin(115200);
  
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  for (int i = 0; i < 60; i++){
    if ( WiFi.status() != WL_CONNECTED ){
      delay(500);
      Serial.print(".");
    } else {
      Serial.println("");
      Serial.println("WiFi connected.");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      break;
    }
  }
  // Print local IP address and start web server
  server.begin();

  execOTA();  // Execute OTA Update

  // RTC
  rtcObject.Begin();
  RtcDateTime currentTime = rtcObject.GetDateTime();
  char rtcstr[40];
  sprintf(rtcstr, "%d.%d.%d %d:%d:%d", currentTime.Year(), currentTime.Month(), currentTime.Day(), currentTime.Hour(), currentTime.Minute(), currentTime.Second());
  Serial.print("Localtime: ");
  Serial.println(rtcstr);

  // NTP
  execNtpUpdate();

  // Fan
  pinMode(ledPin, OUTPUT);
  domoSwitchUpdate(0, 24);

  // Valves
  pinMode(kran1close, OUTPUT);
  pinMode(kran1open, OUTPUT);
  pinMode(kran2close, OUTPUT);
  pinMode(kran2open, OUTPUT);
  digitalWrite(kran1close, HIGH);
  digitalWrite(kran1open, HIGH);
  digitalWrite(kran2close, HIGH);
  digitalWrite(kran2open, HIGH);

  // PCF8591
  pcf8591.begin();
  delay(1000);
  patternGaz = pcf8591.analogRead(AIN2);

  // Gesture sensor
  paj7620Init();
}

void loop(){
  // HTTP remote
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // turns the GPIOs pwm. Use variables like off, fade, rise and pwm=N, where N: 1-255
            if (header.indexOf("GET /?15on") >= 0) {
              Serial.println("ForceRun for 15min ON");
              forceRun = true;
              timerInt = 900000;
              timerVal = true;
              currentForceRunMillis = millis();
            }
            if (header.indexOf("GET /?15off") >= 0) {
              Serial.println("ForceRun for 15min OFF");
              forceRun = true;
              timerInt = 900000;
              timerVal = false;
              currentForceRunMillis = millis();
            }
            if (header.indexOf("GET /?30on") >= 0) {
              Serial.println("ForceRun for 30min ON");
              forceRun = true;
              timerInt = 1800000;
              timerVal = true;
              currentForceRunMillis = millis();
            }
            if (header.indexOf("GET /?30off") >= 0) {
              Serial.println("ForceRun for 30min OFF");
              forceRun = true;
              timerInt = 1800000;
              timerVal = false;
              currentForceRunMillis = millis();
            }
            if (header.indexOf("GET /?60on") >= 0) {
              Serial.println("ForceRun for 60min ON");
              forceRun = true;
              timerInt = 3600000;
              timerVal = true;
              currentForceRunMillis = millis();
            }
            if (header.indexOf("GET /?60off") >= 0) {
              Serial.println("ForceRun for 60min OFF");
              forceRun = true;
              timerInt = 3600000;
              timerVal = false;
              currentForceRunMillis = millis();
            }
            if (header.indexOf("GET /?valve1open") >= 0) {
              Serial.println("Start to open valve1");
              digitalWrite(kran1open, LOW);
              delay(8000);
              digitalWrite(kran1open, HIGH);
            }
            if (header.indexOf("GET /?valve1close") >= 0) {
              Serial.println("Start to close valve1");
              digitalWrite(kran1close, LOW);
              delay(8000);
              digitalWrite(kran1close, HIGH);
            }
            if (header.indexOf("GET /?valve1half") >= 0) {
              Serial.println("Start to halfopen valve1");
              digitalWrite(kran1open, LOW);
              delay(4000);
              digitalWrite(kran1open, HIGH);
            }
            if (header.indexOf("GET /?valve2open") >= 0) {
              Serial.println("Start to open valve2");
              digitalWrite(kran2open, LOW);
              delay(8000);
              digitalWrite(kran2open, HIGH);
            }
            if (header.indexOf("GET /?valve2close") >= 0) {
              Serial.println("Start to close valve2");
              digitalWrite(kran2close, LOW);
              delay(8000);
              digitalWrite(kran2close, HIGH);
            }
            if (header.indexOf("GET /?valve2half") >= 0) {
              Serial.println("Start to halfopen valve2");
              digitalWrite(kran2open, LOW);
              delay(4000);
              digitalWrite(kran2open, HIGH);
            }
            if (header.indexOf("GET /?reboot") >= 0) {
              Serial.println("Rebooting");
              ESP.restart();
            }
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
    header = "";
    client.stop();
  }
  
  // NTP update at 3:00 and Valve training at 4:00
  RtcDateTime currentTime = rtcObject.GetDateTime();
  int currenthour = currentTime.Hour();
  int currentmin = currentTime.Minute();
  if(currenthour == hour4ntp & currentmin == min4ntp){
    if(!isHour4ntp){
      execNtpUpdate();
      isHour4ntp = true;
    }
  } else {
    isHour4ntp = false;
  }
  if(currenthour == hour4valves & currentmin == min4valves){
    if(!isHour4valves){
      execValvesTraining();
      isHour4valves = true;
    }
  } else {
    isHour4valves = false;
  }

  // Set exemplary value from mq-2 per every hour
  if(currentmin == 0){
    if(!isHour4mq){
      int newPatternGaz = pcf8591.analogRead(AIN2);
      if(newPatternGaz - patternGaz >= 15){  // Pseudo smart detect new pattern for gaz sensor
        Serial.println("New pattern gaz is too big, ignoring");
      } else {
        patternGaz = newPatternGaz;
        isHour4mq = true;
        Serial.print("Gaz pattern: ");
        Serial.println(patternGaz);
      }
    }
  } else {
    isHour4mq = false;
  }

  execMQsens(patternGaz);

  if(forceRun){
    unsigned long currentRunMillis = millis();
    if(timerVal){
      digitalWrite(ledPin, HIGH);
    } else {
      digitalWrite(ledPin, LOW);
    }
    if (currentRunMillis - currentForceRunMillis >= timerInt){
      Serial.println("Timer is over");
      if(timerVal){
        digitalWrite(ledPin, LOW);
        Serial.println("Led OFF");
        domoSwitchUpdate(0, 40);
      }
      forceRun = false;
    }
  }

  // Memory Usage
//  memoryUsage();
  
  // Check wifi
  checkWIFI();

  // Read gestures
  gestures();

  delay(10);
}

// OTA Logic 
void execOTA() {
  Serial.println("Connecting to: " + String(host));
  if (client.connect(host.c_str(), port)) {
    // Connection Succeed. Fecthing the bin
    Serial.println("Fetching Bin: " + String(bin));

    // Get the contents of the bin file
    client.print(String("GET ") + phpfile + "?file=" + bin + "&" + "FW_VERSION=" + FW_VERSION + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Cache-Control: no-cache\r\n" +
                 "Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println("Client Timeout !");
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
          Serial.println("Got a non 200 status code from server. Exiting OTA Update.");
          break;
        }
      }

      // extract headers here. Start with content length
      if (line.startsWith("Content-Length: ")) {
        contentLength = atol((getHeaderValue(line, "Content-Length: ")).c_str());
        Serial.println("Got " + String(contentLength) + " bytes from server");
      }

      // Next, the content type
      if (line.startsWith("Content-Type: ")) {
        String contentType = getHeaderValue(line, "Content-Type: ");
        Serial.println("Got " + contentType + " payload.");
        if (contentType == "application/octet-stream") {
          isValidContentType = true;
        }
      }
    }
  } else {
    // Connect to server failed
    Serial.println("Connection to " + String(host) + " failed. Please check your setup");
  }

  // Check what is the contentLength and if content type is `application/octet-stream`
  Serial.println("contentLength : " + String(contentLength) + ", isValidContentType : " + String(isValidContentType));

  // check contentLength and content type
  if (contentLength && isValidContentType) {
    // Check if there is enough to OTA Update
    bool canBegin = Update.begin(contentLength);

    // If yes, begin
    if (canBegin) {
      Serial.println("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
      // No activity would appear on the Serial monitor
      size_t written = Update.writeStream(client);

      if (written == contentLength) {
        Serial.println("Written : " + String(written) + " successfully");
      } else {
        Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?" );
      }

      if (Update.end()) {
        Serial.println("OTA done!");
        if (Update.isFinished()) {
          Serial.println("Update successfully completed. Rebooting.");
          ESP.restart();
        } else {
          Serial.println("Update not finished? Something went wrong!");
        }
      } else {
        Serial.println("Error Occurred. Error #: " + String(Update.getError()));
      }
    } else {
      // not enough space to begin OTA. Understand the partitions and space availability
      Serial.println("Not enough space to begin OTA");
      client.flush();
    }
  } else {
    Serial.println("There was no content in the response");
    client.flush();
  }
}

// NTP
void execNtpUpdate(){
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  for(int i=1; i <= 5; i++){
    if(getLocalTime(&timeinfo)){
      char timeStringBuff[40];
      strftime(timeStringBuff, sizeof(timeStringBuff), "%Y.%m.%d %H:%M:%S", &timeinfo);
      Serial.print("NTPdate: ");
      Serial.println(timeStringBuff);
      // Set NTP time to RTC module
      int ntpyear = timeinfo.tm_year;
      int ntpmon  = timeinfo.tm_mon;
      int ntpday  = timeinfo.tm_mday;
      int ntphour = timeinfo.tm_hour;
      int ntpmin  = timeinfo.tm_min;
      int ntpsec  = timeinfo.tm_sec;
      RtcDateTime currentTime = RtcDateTime(ntpyear,ntpmon,ntpday,ntphour,ntpmin,ntpsec);
      rtcObject.SetDateTime(currentTime);
      Serial.println("Localtime updated");
      break;
    } else {
      Serial.println("Failed to obtain time");
      delay(1000);
    }
  }
}

// Valves training
void execValvesTraining(){
/*  int valve[4] = {kran1open,kran1close,kran2open,kran2close};
  for(int i=0; i<3; i++){
    digitalWrite(valve[i], LOW);
    delay(8000);
    digitalWrite(valve[i], HIGH);
    delay(1000);
  }
  digitalWrite(valve[0], LOW);
  delay(4000);
  digitalWrite(valve[0], HIGH);
  delay(1000);
  digitalWrite(valve[2], LOW);
  delay(4000);
  digitalWrite(valve[2], HIGH);*/
  
  digitalWrite(kran1open, LOW);
  delay(8000);
  digitalWrite(kran1open, HIGH);
  delay(1000);
  digitalWrite(kran1close, LOW);
  delay(8000);
  digitalWrite(kran1close, HIGH);
  delay(1000);
  digitalWrite(kran1open, LOW);
  delay(4000);
  digitalWrite(kran1open, HIGH);
  delay(1000);
  digitalWrite(kran2open, LOW);
  delay(8000);
  digitalWrite(kran2open, HIGH);
  delay(1000);
  digitalWrite(kran2close, LOW);
  delay(8000);
  digitalWrite(kran2close, HIGH);
  delay(1000);
  digitalWrite(kran2open, LOW);
  delay(4000);
  digitalWrite(kran2open, HIGH);
}

// Get data from MQ-2
void execMQsens(int patternGazVal){
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval){
    previousMillis = currentMillis;
    int datchikGaz = pcf8591.analogRead(AIN2);
    if (client.connect(domoserver.c_str(), domoport)) {
      client.print("GET /json.htm?type=command&param=udevice&idx=27&svalue=");
      client.print(datchikGaz);
      client.print(".");
      client.print(patternGazVal);
      client.println(" HTTP/1.1");
      client.print("Host: ");
      client.print(domoserver);
      client.print(":");
      client.println(domoport);
      client.println("Cache-Control: no-cache");
      client.println("Connection: close");
      client.println();
    }
    if(datchikGaz - patternGazVal >= gazDiff){ //turn fan on
      if((!forceRun) and (!isFanOn)){
        digitalWrite(ledPin, HIGH);
        isFanOn = true;
        domoSwitchUpdate(70, 24);
      }
    } else if(datchikGaz <= patternGazVal - 1){ //turn fan off
      if((!forceRun) and (isFanOn)){
        digitalWrite(ledPin, LOW);
        isFanOn = false;
        domoSwitchUpdate(0, 24);
      }
    }
  }
}

// Memory usage
void memoryUsage(){
  unsigned long currentMemMillis = millis();
  if (currentMemMillis - previousMemMillis >= interval){
    previousMemMillis = currentMemMillis;
    int freeMemory = ESP.getFreeHeap();
    domoUpdate(freeMemory, 36);
  }
}

// Check wifi is connected
void checkWIFI(){
  unsigned long currentWFMillis = millis();
  if (currentWFMillis - previousWFMillis >= checkWFInterval){
    previousWFMillis = currentWFMillis;
    if (WiFi.status() != WL_CONNECTED){
      WiFi.disconnect();
      WiFi.begin(ssid, password);
      for (int i = 0; i < 60; i++){
        if ( WiFi.status() != WL_CONNECTED ){
          delay(500);
          Serial.print(".");
        } else {
          Serial.println("");
          Serial.println("WiFi connected.");
          Serial.print("IP address: ");
          Serial.println(WiFi.localIP());
          break;
        }
      }
      server.stop();
      delay(500);
      server.begin();
    }
  }
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

void domoSwitchUpdate(int dataVar, int idxVar){
  if (client.connect(domoserver.c_str(), domoport)) {
    client.print("GET /json.htm?type=command&param=switchlight&idx=");
    client.print(idxVar);
    client.print("&switchcmd=Set%20Level&level=");
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

void gestures(){
  uint8_t data = 0, data1 = 0, error;
  error = paj7620ReadReg(0x43, 1, &data);
  if (!error){
    switch (data){
      case GES_UP_FLAG:
        forceRun = true;
        timerInt = 900000;
        timerVal = true;
        currentForceRunMillis = millis();
        break;
      case GES_DOWN_FLAG:
        forceRun = true;
        timerInt = 900000;
        timerVal = false;
        currentForceRunMillis = millis();
        break;
      default:
        break;
    }
  }
}
