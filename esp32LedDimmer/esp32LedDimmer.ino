// WIFI LED-dimmer on ESP32 with smooth starting, fading, rising, stoping, http remote via GET, bluetooth remote(fade, rise), OTA update from remote server.
// Set ssid and password for WIFI connection to AP, host and bin strings for OTA, ESP_BT.begin("ESP32") for visible bluetooth name.
// Development by vansatchen.

#include <WiFi.h>
#include <Update.h>
#include <EEPROM.h>
//#include "BluetoothSerial.h"
#include "auth.h"

#define FW_VERSION 1001

//BluetoothSerial ESP_BT;

// For OTA update
long contentLength = 0;
bool isValidContentType = false;
String host = "192.168.1.159";
#define port 80
#define phpfile "/UpgradeFW/update.php"
#define bin "esp32LedDimmer.bin"

// Utility to extract header value from headers
String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}

WiFiServer server(port);
WiFiClient client;

String header;  // Variable to store the HTTP request

// For ledPWM
#define LEDC_CHANNEL_0     0
#define LEDC_TIMER_13_BIT  13
#define LEDC_BASE_FREQ     5000
#define LED_PIN            2
#define EEPROM_SIZE 1
int ledState = 255;
#define brightness         0
#define fadeState          10

//String incoming;  // BT variable

void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255) {
  uint32_t duty = (8191 / valueMax) * min(value, valueMax);  // calculate duty, 8191 from 2 ^ 13 - 1
  ledcWrite(channel, duty);  // write duty to LEDC
}

// Domoticz
String domoserver = "192.168.1.44";
#define domoport 8080

void setup() {
  EEPROM.begin(EEPROM_SIZE);
  ledState = EEPROM.read(0);

  // set the LED to the last stored state
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(LED_PIN, LEDC_CHANNEL_0);

  // smooth start
  for(int rise=0; rise <= ledState; rise++){
    ledcAnalogWrite(LEDC_CHANNEL_0, rise);
    delay(4);
  }
  
  // Connect to Wi-Fi network with SSID and password
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  server.begin();

//  ESP_BT.begin("uguLed1");  // BT

  execOTA();  // Execute OTA Update

  domoUpdate(1, 14);
}

void loop(){
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
            
            // turns the GPIOs pwm. Use variables like off, fade, rise or pwm=N, where N: 1-255
            if (header.indexOf("GET /?off") >= 0) {
              ledcAnalogWrite(LEDC_CHANNEL_0, 0);
            }
            if (header.indexOf("GET /?fade") >= 0) {
              for (int i=ledState; i >= fadeState; i-=1){
                Serial.println(i);
                ledcAnalogWrite(LEDC_CHANNEL_0, i);
                delay(10);
              }
            }
            if (header.indexOf("GET /?rise") >= 0) {
              ledcAnalogWrite(LEDC_CHANNEL_0, ledState);
            }
            if (header.indexOf("?pwm=") >= 0) {
              header.replace("GET /?pwm=", "");
              header.replace(" HTTP/1.1", "");
              int pwmval = atoi(header.c_str());
              if (pwmval >= 255) {
                pwmval = 255;
              }
              if (pwmval <= 255 && pwmval > 0) {
                ledcAnalogWrite(LEDC_CHANNEL_0, pwmval);
                ledState = pwmval;
                EEPROM.write(0, pwmval);
                EEPROM.commit();
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
  // BT
/*  while(ESP_BT.available()){
    incoming = "";
    for(int i = 0; i < 4; i++) {
      incoming.concat(ESP_BT.read());
    }
    if (incoming == "10297100101"){ // fade
      for (int i=ledState; i >= fadeState; i-=1){
        ledcAnalogWrite(LEDC_CHANNEL_0, i);
        delay(10);
      }
    }
    if (incoming == "114105115101"){ // rise
      ledcAnalogWrite(LEDC_CHANNEL_0, ledState);
    }
  }*/
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

void domoUpdate(int dataVar, int idxVar){
  if (client.connect(domoserver.c_str(), domoport)) {
    client.print("GET /json.htm?type=command&param=udevice&idx=");
    client.print(idxVar);
    client.print("&nvalue=");
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
