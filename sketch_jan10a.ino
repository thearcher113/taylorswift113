#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFiUdp.h>
#include <coap-simple.h>
#include <LiquidCrystal_I2C.h>

// ====== CẤU HÌNH PIN LED ======
const int LED_DONG_DAT = 4;
const int LED_SAT_LO = 16;

// ====== CẤU HÌNH WIFI ======
const char* ssid = "LONG-RE7LTP";
const char* password = "LongLong";

// ====== SERVER ======
const char* server_ip = "192.168.137.1";
const int server_port = 5683;
const char* resource_path = "api/records/upload";

Adafruit_MPU6050 mpu;
WiFiUDP udp;
Coap coap(udp);
unsigned long msg_id = 0;

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println(">>> Server phản hồi OK");
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_DONG_DAT, OUTPUT);
  pinMode(LED_SAT_LO, OUTPUT);
  pinMode(2, OUTPUT);

  Wire.begin(21,22);
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("DANG KET NOI...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(2, HIGH); delay(200);
    digitalWrite(2, LOW); delay(200);
    Serial.print(".");
  }
  Serial.println("\nWiFi OK!");
  lcd.setCursor(0,1);
  lcd.print("WiFi: OK");

  if (!mpu.begin()) {
    Serial.println("Loi MPU6050!");
    while(1);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_10_HZ);

  coap.response(callback_response);
  coap.start();
  lcd.clear();
}

void loop() {
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  // Tạo ID dạng 4 chữ số
  char id_str[5];
  sprintf(id_str, "%04d", msg_id);
  String current_id = String(id_str);

  msg_id++;
  if (msg_id > 9999) msg_id = 0;

  // Lấy timestamp
  unsigned long ts = millis() / 1000;

  // Chuẩn bị JSON theo định dạng mới
  String payload = "{";
  payload += "\"id\":\"" + current_id + "\",";
  payload += "\"ts\":" + String(ts) + ",";
  payload += "\"ax\":" + String(accel.acceleration.x, 2) + ",";
  payload += "\"ay\":" + String(accel.acceleration.y, 2) + ",";
  payload += "\"az\":" + String(accel.acceleration.z, 2) + ",";
  payload += "\"gx\":" + String(gyro.gyro.x, 2) + ",";
  payload += "\"gy\":" + String(gyro.gyro.y, 2) + ",";
  payload += "\"gz\":" + String(gyro.gyro.z, 2);
  payload += "}";

  Serial.println("Gửi JSON: ");
  Serial.println(payload);

  IPAddress ipAddr;
  ipAddr.fromString(server_ip);

  coap.send(
    ipAddr,
    server_port,
    resource_path,
    COAP_CON,
    COAP_POST,
    NULL,
    0,
    (const uint8_t*)payload.c_str(),
    payload.length(), COAP_APPLICATION_JSON
  );

  coap.loop();

  delay(500);
}
