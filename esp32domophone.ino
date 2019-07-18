// Sketch for control domophone like Cyfral.
// Allows accept calls and reset or open door.

#include <WiFi.h>
#include "time.h"
#include <RtcDS3231.h>
#include <Wire.h>

// Replace with your network credentials
#define ssid      ""
#define password  ""

// NTP
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
bool doorToClose = true;

void setup() {
  pinMode(dfRelay, OUTPUT);
  digitalWrite(dfRelay, HIGH); // Switch domophone line to optocoupler
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
//  server.begin();

  // RTC
  rtcObject.Begin();
  RtcDateTime currentTime = rtcObject.GetDateTime();
  char rtcstr[40];
  sprintf(rtcstr, "%d.%d.%d %d:%d:%d", currentTime.Year(), currentTime.Month(), currentTime.Day(), currentTime.Hour(), currentTime.Minute(), currentTime.Second());
  Serial.print("Localtime: ");
  Serial.println(rtcstr);

  // NTP
  execNtpUpdate();
}

void loop() {
  // Detect calling
  callState = digitalRead(callDetect);
  if(callState){ // If calling
    if(doorToClose){
      momentClose();
      doorToClose = false;
    } else {
      momentOpen();
      doorToClose = true;
    }
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
  delay(2000);
  digitalWrite(openPin, LOW);
  digitalWrite(answerPin, LOW);
}
