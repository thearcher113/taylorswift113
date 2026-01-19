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
const char* ssid = "Paris Gateaux 5";
const char* password = "parisgateaux05";

// ====== SERVER ======
const char* server_ip = "192.168.31.109";
const int server_port = 5683;
const char* resource_path = "mpu";

// ====== BIẾN ======
Adafruit_MPU6050 mpu;
WiFiUDP udp;
Coap coap(udp);
unsigned long msg_id = 0; 

// 2. Khởi tạo LCD (Địa chỉ 0x27, 16x2)
// SDA nối chân 21, SCL nối chân 22 trên ESP32
LiquidCrystal_I2C lcd(0x27, 16, 2);

void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println(">>> Server phản hồi OK");
}

void setup() {
  Serial.begin(115200);
  // === Cấu hình chân LED ===
  pinMode(LED_DONG_DAT, OUTPUT);
  pinMode(LED_SAT_LO, OUTPUT);
  pinMode(2, OUTPUT);

  // 3. Khởi tạo LCD và I2C
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("DANG KET NOI... ");

  // ====== KẾT NỐI WIFI ======
  Serial.println("Đang kết nối WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(2, HIGH);
    delay(200);
    digitalWrite(2, LOW);
    delay(200);
    Serial.print(".");
  }

  digitalWrite(2, HIGH);
  Serial.println("\nWiFi đã kết nối!");
  lcd.setCursor(0, 1);
  lcd.print("WiFi: OK       ");
  delay(1000);

  // ====== KHỞI ĐỘNG MPU6050 ======
  if (!mpu.begin()) {
    Serial.println("!!! Không tìm thấy MPU6050");
    lcd.clear();
    lcd.print("LOI: MPU6050");
    while (1) {
      digitalWrite(2, HIGH); delay(100);
      digitalWrite(2, LOW); delay(100);
    }
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

  // --- QUẢN LÝ ID 4 CHỮ SỐ (0000 - 9999) ---
  char id_str[5];
  sprintf(id_str, "%04d", msg_id); 

  // Lưu lại ID hiện tại để dùng cho gói tin
  String current_id = String(id_str);

  msg_id++;
  if (msg_id > 9999) msg_id = 0; 
  
  // --- TÍNH TOÁN CẤP ĐỘ THỰC TẾ ---
  float ax = accel.acceleration.x;
  float ay = accel.acceleration.y;
  float az = accel.acceleration.z;
  float totalAcc = sqrt(ax*ax + ay*ay + az*az);
  float diff = abs(totalAcc - 9.81);

  int n = 0;
  String dong_dat = "none";

  if (diff > 1) {
    dong_dat = "co_dong_dat";
    if (diff < 2.5) n = 1;
    else if (diff < 4.5) n = 2;
    else if (diff < 7.0) n = 3;
    else if (diff < 10.0) n = 4;
    else if (diff < 15.0) n = 5;
    else if (diff < 22.0) n = 6;
    else if (diff < 33.0) n = 7;
    else if (diff < 40.0) n = 8;
    else n = 9;
  }

  String sat_lo = (abs(az) < 8.5 || abs(ax) > 4.0 || abs(ay) > 4.0) ? "1" : "none";

  // ====== HIỂN THỊ LÊN LCD ======
  lcd.setCursor(0, 0);
  if (n > 0) {
    lcd.print("DONG DAT: CAP ");
    lcd.print(n);
  } else {
    lcd.print("DONG DAT: KHONG ");
  }

  lcd.setCursor(0, 1);
  if (sat_lo == "1") {
    lcd.print("CO SAT LO!    ");
  } else {
    lcd.print("SAT LO: KHONG     ");

    lcd.print(current_id);
  }

  // ====== ĐIỀU KHIỂN LED NHÁY ======
  // Nháy LED Động đất (Chân 4)
  if (n > 0) {
    digitalWrite(LED_DONG_DAT, HIGH);
  } else {
    digitalWrite(LED_DONG_DAT, LOW);
  }

  // Nháy LED Sạt lở (Chân 16)
  if (sat_lo == "1") {
    digitalWrite(LED_SAT_LO, HIGH);
  } else {
    digitalWrite(LED_SAT_LO, LOW);
  }

  // ====== GỬI GÓI TIN (SERIAL + COAP) ======
  String payload1 = "{\"t\":1,\"id\":\"" + current_id + "\",\"dong_dat\":\"" + dong_dat + "\",\"cap_do\":" + String(n) + ",\"sat_lo\":\"" + sat_lo + "\"}";
  String payload2 = "{\"t\":2,\"id\":\"" + current_id + "\",\"ax\":" + String(ax, 2) + ",\"ay\":" + String(ay, 2) + ",\"az\":" + String(az, 2) + 
                    ",\"gx\":" + String(gyro.gyro.x, 2) + ",\"gy\":" + String(gyro.gyro.y, 2) + ",\"gz\":" + String(gyro.gyro.z, 2) + "}";

  IPAddress ipAddr;
  ipAddr.fromString(server_ip);

  // In Serial Monitor
  Serial.println("Gói 1: " + payload1);
  coap.send(ipAddr, server_port, resource_path, COAP_CON, COAP_POST, NULL, 0, (const uint8_t*)payload1.c_str(), payload1.length());
  
  delay(150); 

  Serial.println("Gói 2: " + payload2);
  coap.send(ipAddr, server_port, resource_path, COAP_CON, COAP_POST, NULL, 0, (const uint8_t*)payload2.c_str(), payload2.length());

  coap.loop();
  delay(500); 
  digitalWrite(LED_DONG_DAT, LOW);
  digitalWrite(LED_SAT_LO, LOW);
  delay(500);
}