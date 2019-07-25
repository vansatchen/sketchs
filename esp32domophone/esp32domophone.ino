// Sketch for control domophone like Cyfral.
// Allows accept calls and reset or open door.

#include <WiFi.h>
#include "time.h"
#include <RtcDS3231.h>
#include <Wire.h>
#include <Update.h>

#include "AudioFileSourcePROGMEM.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"
#include "sound.h"

#define FW_VERSION 1000

// Replace with your network credentials
#define ssid      "ugu"
#define password  "van_SATCHen"

// For OTA update
long contentLength = 0;
bool isValidContentType = false;
String host = "192.168.1.159";
#define port 80
#define phpfile "/UpgradeFW/update.php"
#define bin "esp32domophone.bin"

// Utility to extract header value from headers
String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}

WiFiServer server(port);
WiFiClient client;

String header;  // Variable to store the HTTP request

// NTP
//const char* ntpServer = "192.168.1.159";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 14400;
const int   daylightOffset_sec = 3600;
#define hour4ntp 3
#define min4ntp  0
bool isHour4ntp = false;
int currenthour, currentmin;

RtcDS3231<TwoWire> rtcObject(Wire); // RTC

#define dfRelay 33 // Pin for relay to take control of calls
#define callDetect 17 // Pin for detect calling
#define answerPin 16 // Pin for relay to switch to answer
#define openPin 15 // Pin for open door
bool callState = false;

#define nightModeOn 20
#define nightModeOff 23
bool nightMode = false;

// HTTP
bool forceRunNMOn = false;
bool forceRunNMOff = false;

// Domoticz
String domoserver = "192.168.1.159";
#define domoport 8080

// Sound
AudioGeneratorWAV *wav;
AudioFileSourcePROGMEM *file;
AudioOutputI2S *out;
bool soundStop = true;

void setup() {
  pinMode(dfRelay, OUTPUT);
  digitalWrite(dfRelay, LOW); // Line to native domophone as default
  pinMode(callDetect, INPUT);
  pinMode(answerPin, OUTPUT);
  digitalWrite(answerPin, LOW); // Wait for call
  pinMode(openPin, OUTPUT);
  digitalWrite(openPin, LOW); // Door closed

  Serial.begin(115200);

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("");
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

  // Sound
  file = new AudioFileSourcePROGMEM( sound, sizeof(sound) );
  out = new AudioOutputI2S(0, 1);
  wav = new AudioGeneratorWAV();
}

void loop() {
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

            if (header.indexOf("GET /?night") >= 0) {
              Serial.println("Night Mode On");
              nightMode = true;
              forceRunNMOn = true;
            }
            if (header.indexOf("GET /?day") >= 0) {
              Serial.println("Night Mode Off");
              nightMode = false;
              forceRunNMOff = true;
            }
            if (header.indexOf("GET /?open") >= 0) {
              Serial.println("Opening door");
              digitalWrite(dfRelay, HIGH); // Switch line to our gadget
              momentOpen();
              digitalWrite(dfRelay, LOW); // Switch line back to native domophone
              domoSWToOff();
            }
            if (header.indexOf("GET /?reset") >= 0) {
              Serial.println("Reseting the call");
              digitalWrite(dfRelay, HIGH); // Switch line to our gadget
              momentClose();
              digitalWrite(dfRelay, LOW); // Switch line back to native domophone
              domoSWToOff();
            }
            if (header.indexOf("GET /?answer") >= 0) {
              Serial.println("Answering the call");
              digitalWrite(dfRelay, HIGH); // Switch line to our gadget
              soundStop = false;
              soundWithReset();
              digitalWrite(dfRelay, LOW); // Switch line back to native domophone
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

  // Check datetime and switch line to esp32 if sleep time is come
  checkForNM();
  
  // Detect calling
  if(nightMode){
    digitalWrite(dfRelay, HIGH);
    callState = digitalRead(callDetect);
    if(callState){ // If calling
      digitalWrite(answerPin, HIGH); // Answer the call
      delay(2500);
      soundStop = false;
      soundWithReset();
      digitalWrite(answerPin, LOW); // Reset the call
      delay(500);
    }
  } else {
    digitalWrite(dfRelay, LOW);
  }
  
  // NTP update at 3:00
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

  delay(10);
}

// OTA Logic 
void execOTA() {
  Serial.println("Trying to update FW via OTA");
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
        Serial.println("Timeout. OTA not available!");
        Serial.println("");
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
      RtcDateTime currentTime = RtcDateTime(ntpyear - 100,ntpmon + 1,ntpday,ntphour,ntpmin,ntpsec);
      rtcObject.SetDateTime(currentTime);
      Serial.println("Localtime updated");
      break;
    } else {
      Serial.println("Failed to obtain time");
      delay(1000);
    }
  }
}

void checkForNM(){
  RtcDateTime currentTime = rtcObject.GetDateTime();
  int currenthour = currentTime.Hour();
  if(currenthour >= nightModeOn & currenthour <= nightModeOff){
    nightMode = true;
//    forceRunNMOff = false;
  } else {
    nightMode = false;
//    forceRunNMOn = false;
  }
}

void momentClose(){
  digitalWrite(answerPin, HIGH); // Answer the call
  delay(2500);
  digitalWrite(answerPin, LOW); // Reset the call
  delay(500);
}

void momentOpen(){
  digitalWrite(answerPin, HIGH); // Answer the call
  delay(2500);
  digitalWrite(openPin, HIGH); // Open door
  delay(1000);
  digitalWrite(openPin, LOW);
  digitalWrite(answerPin, LOW);
}

void soundWithReset(){
  file = new AudioFileSourcePROGMEM( sound, sizeof(sound) );
  while(!soundStop){
    wav->begin(file, out);
    if (wav->isRunning()) {
      if (!wav->loop()){
        wav->stop();
        soundStop = true;
      }
    }
  }
}

void domoSWToOff(){
  if (client.connect(domoserver.c_str(), domoport)) {
    client.print("GET /json.htm?type=command&param=switchlight&idx=46&switchcmd=Set%20Level&level=0");
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