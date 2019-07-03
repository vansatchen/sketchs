#include <WiFi.h>
#include <WiFiClientSecure.h>

WiFiClientSecure secureClient;

#define ssid      ""
#define password  ""

void setup() {
  Serial.begin(115200);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  pushbullet((String)"Hello from esp32");
}

void loop() {
  // put your main code here, to run repeatedly:

}

bool pushbullet(const String &message) {
  const char* PushBulletAPIKEY = "";
  const uint16_t timeout = 2000;
  const char*  host = "api.pushbullet.com";
  String messagebody = R"({"type": "note", "title": "Push from ESP32", "body": ")" + message + R"("})";
  uint32_t sendezeit = millis();
  if (!secureClient.connect(host, 443)) {
    return false;
  }
  else {
    secureClient.printf("POST /v2/pushes HTTP/1.1\r\nHost: %s\r\nAuthorization: Bearer %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n%s\r\n"\
                        , host, PushBulletAPIKEY, messagebody.length(), messagebody.c_str());
  }
  while (!secureClient.available()) {
    if (millis() - sendezeit > timeout) {
      secureClient.stop();
      return false;
    }
  }
  while (secureClient.available()) {
    String line = secureClient.readStringUntil('\n');
    if (line.startsWith("HTTP/1.1 200 OK")) {
      secureClient.stop();
      return true;
    }
  }
  return false;
}
