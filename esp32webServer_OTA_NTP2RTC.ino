// Development by vansatchen.

#include <WiFi.h>
#include <Update.h>
#include "time.h"
#include <Wire.h>
#include <RtcDS3231.h>

#define FW_VERSION 1000

// Replace with your network credentials
#define ssid      ""
#define password  ""

// For OTA update
long contentLength = 0;
bool isValidContentType = false;
String host = "192.168.1.57";
#define port 80
#define phpfile "/update.php"
#define bin "esp32WControl.bin"

// Utility to extract header value from headers
String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}

WiFiServer server(port);
WiFiClient client;

String header;  // Variable to store the HTTP request

// NTP
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 14400;
const int   daylightOffset_sec = 3600;

// RTC
RtcDS3231<TwoWire> rtcObject(Wire);
int previoushour = 0;

// Millis
/*unsigned long previousMillis = 0;
const long interval = 10000;*/

void setup() {
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
  server.begin();

  //execOTA();  // Execute OTA Update

  // RTC
  rtcObject.Begin();
/*  RtcDateTime currentTime = rtcObject.GetDateTime();
  char rtcstr[40];
  sprintf(rtcstr, "%d.%d.%d %d:%d:%d", currentTime.Year(), currentTime.Month(), currentTime.Day(), currentTime.Hour(), currentTime.Minute(), currentTime.Second());
  Serial.print("Localtime: ");
  Serial.println(rtcstr);*/

  Serial.println("Setup going to loop");
}

void loop(){
  // NTP update every hour
  RtcDateTime currentTime = rtcObject.GetDateTime();
  int currenthour = currentTime.Hour();
  if(currenthour - previoushour >= 1){
    previoushour = currenthour;
    Serial.println();
    Serial.println(currenthour);
    execNtpUpdate();
    delay(100);
  }
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
