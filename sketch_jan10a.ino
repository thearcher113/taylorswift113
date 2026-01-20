#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFiUdp.h>
#include <coap-simple.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

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

// ====== BIẾN ======
Adafruit_MPU6050 mpu;
WiFiUDP udp;
Coap coap(udp);

// Biến bổ sung cho điều kiện thực tế (để tính toán không bị kẹt)
float lastTotalAcc = 9.81; 
float base_roll = 0, base_pitch = 0;
int stability_count = 0;

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ====== HÀM TẠO TIMESTAMP ======
String getTimestampFast() {
  time_t now = time(NULL);
  struct tm t;
  localtime_r(&now, &t);

  char buf[20];
  sprintf(buf, "%02d%02d%04d%02d%02d%02d",
          t.tm_mday,
          t.tm_mon + 1,
          t.tm_year + 1900,
          t.tm_hour,
          t.tm_min,
          t.tm_sec);

  return String(buf);
}

// Callback CoAP
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println(">>> Server phản hồi OK");
}

void setup() {
  Serial.begin(115200);

  // LED
  pinMode(LED_DONG_DAT, OUTPUT);
  pinMode(LED_SAT_LO, OUTPUT);
  pinMode(2, OUTPUT);

  // LCD
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

  // ====== ĐỒNG BỘ THỜI GIAN NTP (NON-BLOCKING) ======
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  unsigned long startSync = millis();
  while (time(NULL) < 1000000000 && millis() - startSync < 5000) {
    delay(100);
  }

  Serial.println("Thoi gian NTP: " + getTimestampFast());

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
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ); // Lọc nhiễu tần số cao

  // Lấy mốc cân bằng ban đầu (Calibration)
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  base_roll = atan2(a.acceleration.y, a.acceleration.z) * 57.3;
  base_pitch = atan2(-a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 57.3;

  coap.response(callback_response);
  coap.start();

  lcd.clear();
}

void loop() {
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  // Dữ liệu sensor
  float ax = accel.acceleration.x;
  float ay = accel.acceleration.y;
  float az = accel.acceleration.z;

  // --- THUẬT TOÁN ĐỘNG ĐẤT (VIBRATION) ---
  float totalAcc = sqrt(ax*ax + ay*ay + az*az);
  float vibration = abs(totalAcc - lastTotalAcc);
  lastTotalAcc = totalAcc; 

  int n = 0;
  String dong_dat = "none";

  if (vibration > 0.1) { 
    dong_dat = "co_dong_dat";
    if (vibration < 0.5) n = 1;
    else if (vibration < 1) n = 2;
    else if (vibration < 1.5) n = 3;
    else if (vibration < 3.0) n = 4;
    else if (vibration < 5.0) n = 5;
    else if (vibration < 7.0) n = 6;
    else if (vibration < 12.0) n = 7;
    else if (vibration < 15.0) n = 8;
    else n = 9;
  }

  // --- THUẬT TOÁN SẠT LỞ (TILT) ---
  float current_roll = atan2(ay, az) * 57.3;
  float current_pitch = atan2(-ax, sqrt(ay*ay + az*az)) * 57.3;

  float diff_roll = abs(current_roll - base_roll);
  float diff_pitch = abs(current_pitch - base_pitch);

  String sat_lo = "none";
  // Ngưỡng nghiêng 15 độ so với mốc cân bằng
  if (diff_roll > 15.0 || diff_pitch > 15.0) {
    sat_lo = "1";
    
    // Thoát kẹt: Nếu nghiêng rồi nhưng nằm yên (vibration thấp) thì sau 5s cập nhật base mới
    if (vibration < 0.1) {
      stability_count++;
      if (stability_count > 5) { 
        base_roll = current_roll;
        base_pitch = current_pitch;
        stability_count = 0;
      }
    } else {
      stability_count = 0; 
    }
  } else {
    stability_count = 0;
  }

  // LCD hiển thị
  lcd.setCursor(0, 0);
  if (n > 0) {
    lcd.print("DONG DAT: CAP ");
    lcd.print(n);
    lcd.print(" ");
  } else {
    lcd.print("DONG DAT: KHONG ");
  }

  lcd.setCursor(0, 1);
  if (sat_lo == "1") {
    lcd.print("CO SAT LO!      ");
  } else {
    lcd.print("SAT LO: KHONG   ");
  }

  digitalWrite(LED_DONG_DAT, n > 0);
  digitalWrite(LED_SAT_LO, sat_lo == "1");

  // ====== LẤY TIMESTAMP NHANH ======
  String ts = getTimestampFast();
  Serial.println("Timestamp: " + ts);

  // ====== PAYLOAD ======
String payload = "{";
payload += "\"id\":\"ESP32-01\","; // Trường 'id' bắt buộc
payload += "\"ts\":" + String(ts) + ",";  
payload += "\"ax\":" + String(ax, 2) + ",";  // Trường 'ax' bắt buộc
payload += "\"ay\":" + String(ay, 2) + ",";  // Trường 'ay' bắt buộc
payload += "\"az\":" + String(az, 2) + ",";  // Trường 'az' bắt buộc
payload += "\"gx\":" + String(gyro.gyro.x, 2) + ","; // Trường 'gx' bắt buộc
payload += "\"gy\":" + String(gyro.gyro.y, 2) + ","; // Trường 'gy' bắt buộc
payload += "\"gz\":" + String(gyro.gyro.z, 2);       // Trường 'gz' bắt buộc.
payload += "}";

  IPAddress ipAddr;
  ipAddr.fromString(server_ip);

  Serial.println("Đã gửi: " + payload);
  coap.send(ipAddr, server_port, resource_path, COAP_CON, COAP_POST, NULL, 0,
            (const uint8_t*)payload.c_str(), payload.length());

  coap.loop();
  delay(250);
}
