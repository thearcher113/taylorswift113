#include <WiFi.h>
#include <WiFiUdp.h>
#include <coap-simple.h>

// ===== WIFI =====
const char* ssid = "LONG-RE7LTP";
const char* password = "LongLong";

// ===== SERVER =====
const char* server_ip = "192.168.137.1";
const int server_port = 5683;
const char* resource_path = "api/records/upload";

// ===== COAP =====
WiFiUDP udp;
Coap coap(udp);

void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println(">>> Server đã phản hồi");
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Đang kết nối WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(200);
  }
  Serial.println("\nWiFi OK!");

  coap.response(callback_response);
  coap.start();
}

void loop() {

  // Gói tin cố định để test server
  String payload = 
      "{\"id\":\"9999\",\"ts\":0000,"
      "\"ax\":0.1,\"ay\":0.2,\"az\":9.8,"
      "\"gx\":0.01,\"gy\":0.02,\"gz\":0.03}";

  IPAddress ipAddr;
  ipAddr.fromString(server_ip);

  Serial.println("Sending: " + payload);

  coap.send(
    ipAddr,
    server_port,
    resource_path,
    COAP_CON,
    COAP_POST,
    NULL,
    0,
    (uint8_t*)payload.c_str(),
    payload.length()
  );

  coap.loop();
  delay(1000); // gửi mỗi 1s
}
