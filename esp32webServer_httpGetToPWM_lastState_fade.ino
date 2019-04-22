#include <WiFi.h>
#include <EEPROM.h>

// Replace with your network credentials
const char* ssid     = "";
const char* password = "";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// For ledPWM
#define LEDC_CHANNEL_0     0
#define LEDC_TIMER_13_BIT  13
#define LEDC_BASE_FREQ     5000
#define LED_PIN            2
#define EEPROM_SIZE 1
int ledState = 255;
int brightness = 0;    // how bright the LED is
int fadeState = 10;

void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255) {
  // calculate duty, 8191 from 2 ^ 13 - 1
  uint32_t duty = (8191 / valueMax) * min(value, valueMax);

  // write duty to LEDC
  ledcWrite(channel, duty);
}

void setup() {
  Serial.begin(115200);

  // read the last LED state from flash memory
  EEPROM.begin(EEPROM_SIZE);
  ledState = EEPROM.read(0);

  // set the LED to the last stored state
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(LED_PIN, LEDC_CHANNEL_0);
  ledcAnalogWrite(LEDC_CHANNEL_0, ledState);
  Serial.println("Led Last State = " +ledState);
  
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
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}

void loop(){
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
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // turns the GPIOs pwm. Use variables like off, N
            if (header.indexOf("GET /?off") >= 0) {
              Serial.println("Light off");
              ledcAnalogWrite(LEDC_CHANNEL_0, 0);
            }
            if (header.indexOf("GET /?fade") >= 0) {
              Serial.println("Light fading");
              for (int i=ledState; i >= fadeState; i-=1){
                Serial.println(i);
                ledcAnalogWrite(LEDC_CHANNEL_0, i);
                delay(10);
              }
            }
            if (header.indexOf("GET /?rise") >= 0) {
              Serial.println("Light rising");
              ledcAnalogWrite(LEDC_CHANNEL_0, ledState);
            }
            if (header.indexOf("?pwm=") >= 0) {
              header.replace("GET /?pwm=", "");
              header.replace(" HTTP/1.1", "");
              Serial.println(header);
              int pwmval = atoi(header.c_str());
              if (pwmval >= 255) {
                pwmval = 255;
              }
              if (pwmval <= 255 && pwmval > 0) {
                Serial.println(pwmval);
                ledcAnalogWrite(LEDC_CHANNEL_0, pwmval);
                ledState = pwmval;
                Serial.println("State changed");
                EEPROM.write(0, pwmval);
                EEPROM.commit();
                Serial.println("State saved in flash memory");
              } else {
                Serial.println("PWM string not recognised");
              }
            }
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
           
            // Web Page Heading
            client.println("<body><h1>ESP32 Web Server</h1>");
            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
//    Serial.println("Client disconnected.");
//    Serial.println("");
  }
} 
