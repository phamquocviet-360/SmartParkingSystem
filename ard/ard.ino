#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <DHT.h>                // Thư viện DHT

// ================= WIFI =================
#define WIFI_SSID "vietpham"
#define WIFI_PASSWORD "123456789"

// ================= FIREBASE =================
#define API_KEY "AIzaSyC_2DztT_WBrN-FgalEOc0CI-kKd_lYxCY"
#define DATABASE_URL "https://biensoxe-e00fa-default-rtdb.firebaseio.com/"

// ================= RFID =================
#define SS_PIN 5
#define RST_PIN 4

// ================= SENSOR =================
#define MQ2_DO     25
#define FLAME_PIN  27
#define IR1        34
#define IR2        35
#define IR3        39                // Cảm biến IR thứ 3
#define LDR_PIN    26
#define DHTPIN     32                // Chân dữ liệu DHT11
#define DHTTYPE    DHT11

// ================= OUTPUT =================
#define LED_RED     14      // cảnh báo
#define LED_YELLOW  12      // đèn trời tối
#define LED_GREEN   16      // báo cổng mở
#define BUZZER      13
#define SERVO_PIN   33

// ================= NÚT NHẤN =================
#define BUTTON_PIN 15   // Nút nhấn mở cổng thủ công

Servo servo;
MFRC522 mfrc522(SS_PIN, RST_PIN);
DHT dht(DHTPIN, DHTTYPE);             // Đối tượng DHT

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

WiFiUDP udp;
NTPClient timeClient(udp, "pool.ntp.org", 7 * 3600, 60000); // UTC+7

bool gateBusy = false;
unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 500;
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 5000;

// Thêm biến cho DHT
unsigned long lastDHTRead = 0;
const unsigned long dhtInterval = 2000;   // đọc DHT mỗi 2 giây

// ================= BIẾN CHO NÚT NHẤN =================
volatile bool buttonPressed = false;
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 50;

// ================= BIẾN CHO VÍ ĐIỆN TỬ =================
String pendingExitUid = "";      // UID đang chờ xác nhận ra
bool exitRequestProcessed = false;

// ================= TIME FORMAT =================
String getFormattedDate() {
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  setTime(epochTime);

  String formatted = String(year()) + "-" 
                   + (month()<10?"0":"") + String(month()) + "-" 
                   + (day()<10?"0":"") + String(day()) + " ";
  formatted += (hour()<10?"0":"") + String(hour()) + ":"
            + (minute()<10?"0":"") + String(minute()) + ":"
            + (second()<10?"0":"") + String(second());
  return formatted;
}

// ================= HÀM NGẮT CHO NÚT NHẤN =================
void IRAM_ATTR handleButtonPress() {
  buttonPressed = true;
}

// ================= LOG =================
void logToFirebase(String uid, String message) {
  String path = "/Logs/" + String(millis());
  FirebaseJson json;
  json.set("timestamp", getFormattedDate());
  json.set("uid", uid);
  json.set("message", message);
  
  if (Firebase.RTDB.setJSON(&fbdo, path, &json)) {
    Serial.println("✅ Đã ghi log");
  } else {
    Serial.println("❌ Lỗi ghi log: " + fbdo.errorReason());
  }
}

// ================= UPDATE WARNINGS =================
void updateWarnings(bool flame, bool gas, bool light) {
  FirebaseJson warningJson;
  warningJson.set("flame", flame);
  warningJson.set("gas", gas);
  warningJson.set("light", light);
  warningJson.set("timestamp", getFormattedDate());
  
  Firebase.RTDB.setJSON(&fbdo, "/Warnings", &warningJson);
}

// ================= UPDATE GATE STATUS =================
void updateGateStatus(String status) {
  FirebaseJson gateJson;
  gateJson.set("status", status);
  gateJson.set("timestamp", getFormattedDate());
  gateJson.set("last_action", millis());
  
  Firebase.RTDB.setJSON(&fbdo, "/Gate/status", &gateJson);
}

// ================= OPEN GATE =================
void openGate(String uid) {
  if (gateBusy) return;
  gateBusy = true;

  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_RED, LOW);
  Serial.println("🔓 Đang mở cổng...");
  
  updateGateStatus("opening");
  logToFirebase(uid, "🔓 Cổng đang mở");

  // Mở cổng
  for (int angle = 0; angle <= 90; angle++) {
    servo.write(angle);
    delay(5);
  }

  Serial.println("🔓 Cổng đã mở");
  updateGateStatus("open");
  logToFirebase(uid, "🔓 Cổng đã mở");

  delay(2000);

  // Đóng cổng
  for (int angle = 90; angle >= 0; angle--) {
    servo.write(angle);
    delay(5);
  }

  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, HIGH);
  Serial.println("🔒 Cổng đã đóng");
  updateGateStatus("closed");
  logToFirebase(uid, "🔒 Cổng đã đóng");

  gateBusy = false;
}

// ================= XỬ LÝ NÚT NHẤN =================
void processButtonPress() {
  if (buttonPressed) {
    buttonPressed = false;
    
    if (millis() - lastButtonPress > debounceDelay) {
      Serial.println("\n🔘 NÚT NHẤN ĐƯỢC NHẤN - Mở cổng thủ công");
      
      if (!gateBusy) {
        logToFirebase("BUTTON", "🔘 Mở cổng bằng nút nhấn thủ công");
        openGate("MANUAL_BUTTON");
      } else {
        Serial.println("⚠️ Cổng đang bận, không thể mở bằng nút nhấn");
        logToFirebase("BUTTON", "⚠️ Thử mở cổng nhưng cổng đang bận");
        
        for (int i = 0; i < 3; i++) {
          digitalWrite(LED_RED, HIGH);
          delay(50);
          digitalWrite(LED_RED, LOW);
          delay(50);
        }
      }
      
      lastButtonPress = millis();
    }
  }
}

// ================= FIRE ALERT =================
void fireAlert() {
  Serial.println("🔥 CO LUA! - Kích hoạt cảnh báo");
  
  digitalWrite(LED_RED, HIGH);
  digitalWrite(BUZZER, HIGH);
  servo.write(90);
  
  updateWarnings(true, digitalRead(MQ2_DO) == LOW, digitalRead(LDR_PIN) == HIGH);
  
  static unsigned long lastFireLog = 0;
  if (millis() - lastFireLog > 5000) {
    logToFirebase("SYSTEM", "🔥 CẢNH BÁO: Phát hiện lửa!");
    lastFireLog = millis();
  }
}

// ================= GAS ALERT =================
void gasAlert() {
  Serial.println("⚠️ CO GAS! - Kích hoạt cảnh báo");
  
  digitalWrite(LED_RED, HIGH);
  digitalWrite(BUZZER, HIGH);
  
  updateWarnings(digitalRead(FLAME_PIN) == LOW, true, digitalRead(LDR_PIN) == HIGH);
  
  static unsigned long lastGasLog = 0;
  if (millis() - lastGasLog > 5000) {
    logToFirebase("SYSTEM", "⚠️ CẢNH BÁO: Phát hiện gas!");
    lastGasLog = millis();
  }
}

// ================= ĐỌC DHT VÀ GỬI LÊN FIREBASE =================
void readDHTAndSend() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("⚠️ Lỗi đọc DHT11");
    return;
  }

  FirebaseJson envJson;
  envJson.set("temperature", t);
  envJson.set("humidity", h);
  envJson.set("timestamp", getFormattedDate());

  if (Firebase.RTDB.setJSON(&fbdo, "/Environment", &envJson)) {
    Serial.printf("🌡️ Nhiệt độ: %.1f°C, Độ ẩm: %.1f%%\n", t, h);
  } else {
    Serial.println("❌ Lỗi gửi DHT: " + fbdo.errorReason());
  }
}

// ================= READ SENSORS (cập nhật thêm IR3) =================
void readSensors() {
  static int lastFlameState = HIGH;
  static int lastGasState = HIGH;
  static int lastLightState = LOW;
  
  int gasState = digitalRead(MQ2_DO);
  int flameState = digitalRead(FLAME_PIN);
  int slot1 = digitalRead(IR1);
  int slot2 = digitalRead(IR2);
  int slot3 = digitalRead(IR3);               // Đọc IR3
  int lightState = digitalRead(LDR_PIN);

  Serial.println("\n======= TRẠNG THÁI HỆ THỐNG =======");

  // XỬ LÝ CẢNH BÁO
  if (flameState == LOW) {
    fireAlert();
  } else if (gasState == LOW) {
    gasAlert();
  } else {
    digitalWrite(LED_RED, LOW);
    digitalWrite(BUZZER, LOW);
    if (!gateBusy) servo.write(0);
    
    if (lastFlameState != flameState || lastGasState != gasState || lastLightState != lightState) {
      updateWarnings(false, false, lightState == HIGH);
    }
  }

  // CẢM BIẾN ÁNH SÁNG
  if (lightState == HIGH) {
    Serial.println("🌚 Trời tối - Bật đèn vàng");
    digitalWrite(LED_YELLOW, HIGH);
  } else {
    Serial.println("🌞 Trời sáng - Tắt đèn vàng");
    digitalWrite(LED_YELLOW, LOW);
  }

  // CẢM BIẾN BÃI ĐỖ XE (3 slot)
  Serial.print("🚗 Slot 1: ");
  Serial.print(slot1 == LOW ? "Có xe" : "Trống");
  Serial.print(" | Slot 2: ");
  Serial.print(slot2 == LOW ? "Có xe" : "Trống");
  Serial.print(" | Slot 3: ");
  Serial.println(slot3 == LOW ? "Có xe" : "Trống");

  FirebaseJson parkingJson;
  parkingJson.set("slot1", slot1 == LOW ? "occupied" : "empty");
  parkingJson.set("slot2", slot2 == LOW ? "occupied" : "empty");
  parkingJson.set("slot3", slot3 == LOW ? "occupied" : "empty");   // Thêm slot3
  parkingJson.set("timestamp", getFormattedDate());
  
  Firebase.RTDB.setJSON(&fbdo, "/Parking/status", &parkingJson);

  lastFlameState = flameState;
  lastGasState = gasState;
  lastLightState = lightState;

  Serial.println("===================================\n");
}

// ================= SEND HEARTBEAT =================
void sendHeartbeat() {
  FirebaseJson heartbeatJson;
  heartbeatJson.set("last_seen", getFormattedDate());
  heartbeatJson.set("timestamp", millis());
  heartbeatJson.set("rssi", WiFi.RSSI());
  heartbeatJson.set("ip", WiFi.localIP().toString());
  
  Firebase.RTDB.setJSON(&fbdo, "/System/ESP32/heartbeat", &heartbeatJson);
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== KHỞI ĐỘNG HỆ THỐNG ===");

  // Cấu hình chân INPUT
  pinMode(MQ2_DO, INPUT);
  pinMode(FLAME_PIN, INPUT);
  pinMode(IR1, INPUT);
  pinMode(IR2, INPUT);
  pinMode(IR3, INPUT);          // Thêm IR3
  pinMode(LDR_PIN, INPUT);
  
  // Cấu hình nút nhấn với ngắt
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, FALLING);

  // Cấu hình chân OUTPUT
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  // Tắt tất cả đèn và còi
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(BUZZER, HIGH);
  delay(100);
  digitalWrite(BUZZER, LOW);

  // Khởi tạo Servo
  servo.attach(SERVO_PIN);
  servo.write(0);
  Serial.println("✅ Servo sẵn sàng");

  // Khởi tạo DHT
  dht.begin();
  Serial.println("✅ DHT11 sẵn sàng");

  Serial.println("✅ Nút nhấn D15 sẵn sàng");

  // Kết nối WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Đang kết nối WiFi");
  
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 20) {
    Serial.print(".");
    delay(500);
    wifiTimeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi đã kết nối");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Không thể kết nối WiFi");
  }

  // FIREBASE SETUP
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("✅ Anonymous SignUp OK");
  } else {
    Serial.printf("❌ SignUp failed: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (Firebase.ready()) {
    Serial.println("🔥 Firebase READY");
  } else {
    Serial.println("❌ Firebase NOT READY");
  }

  // Khởi tạo RFID
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("✅ RFID sẵn sàng");

  // Khởi tạo NTP
  timeClient.begin();
  timeClient.update();
  Serial.println("✅ NTP sẵn sàng");

  Serial.println("=== HỆ THỐNG SẴN SÀNG ===\n");
  
  logToFirebase("SYSTEM", "🚀 Hệ thống khởi động");
  updateGateStatus("closed");
  sendHeartbeat();
}

// ================= LOOP =================
void loop() {
  timeClient.update();
  
  // XỬ LÝ NÚT NHẤN
  processButtonPress();
  
  // Đọc cảm biến (IR, gas, flame, light) theo khoảng thời gian
  if (millis() - lastSensorRead >= sensorInterval) {
    readSensors();
    lastSensorRead = millis();
  }
  
  // Đọc DHT định kỳ
  if (millis() - lastDHTRead >= dhtInterval) {
    readDHTAndSend();
    lastDHTRead = millis();
  }
  
  // Gửi heartbeat định kỳ
  if (millis() - lastHeartbeat >= heartbeatInterval) {
    sendHeartbeat();
    lastHeartbeat = millis();
  }

  // ================= KIỂM TRA MỞ CỔNG XE RA (SAU KHI THANH TOÁN) =================
  if (pendingExitUid != "" && !exitRequestProcessed) {
    if (Firebase.RTDB.getBool(&fbdo, "/Gate/exit_open")) {
      if (fbdo.boolData()) {
        // Đọc UID được phép ra
        if (Firebase.RTDB.getString(&fbdo, "/Gate/exit_uid")) {
          String allowedUid = fbdo.stringData();
          if (allowedUid == pendingExitUid) {
            Serial.println("✅ Thanh toán thành công, mở cổng cho UID: " + pendingExitUid);
            openGate(pendingExitUid);
            
            // Reset cờ
            Firebase.RTDB.setBool(&fbdo, "/Gate/exit_open", false);
            Firebase.RTDB.setString(&fbdo, "/Gate/exit_uid", "");
            pendingExitUid = "";
            exitRequestProcessed = true;
          }
        }
      }
    }
  }

  // ================= KIỂM TRA RFID =================
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial() && !gateBusy) {
    
    String UID = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      if (mfrc522.uid.uidByte[i] < 0x10) UID += "0";
      UID += String(mfrc522.uid.uidByte[i], HEX);
    }
    UID.toUpperCase();
    
    Serial.println("\n📱 PHÁT HIỆN THẺ RFID");
    Serial.println("UID: " + UID);
    
    String path = "/ParkingSystem/RFID/" + UID;
    
    // Kiểm tra thẻ đã tồn tại chưa
    if (Firebase.RTDB.getInt(&fbdo, path + "/status")) {
      int status = fbdo.intData();
      
      if (status == 0) {
        // Xe đang trong bãi -> chuẩn bị ra
        Firebase.RTDB.setBool(&fbdo, path + "/confirm_plate", true);
        logToFirebase(UID, "🚗 Xe chuẩn bị ra - Chờ xác nhận biển số");
        pendingExitUid = UID;
        exitRequestProcessed = false;
        Serial.println("🚗 Xe đang trong bãi - Chờ xác nhận biển số ra");
      } else {
        // Thẻ đã tồn tại nhưng đang ở trạng thái đã ra (status=1) hoặc lỗi
        // Lấy số dư hiện tại để giữ nguyên
        float currentBalance = 0;
        if (Firebase.RTDB.getFloat(&fbdo, path + "/balance")) {
          currentBalance = fbdo.floatData();
        }
        
        // Cập nhật thông tin cho lần vào mới
        FirebaseJson json;
        json.set("uid", UID);
        json.set("status", 0);
        json.set("waiting_plate", true);
        json.set("date_in", getFormattedDate());
        json.set("balance", currentBalance);  // Giữ nguyên số dư cũ
        json.set("waiting_payment", false);
        json.set("fee_due", 0);
        
        Firebase.RTDB.setJSON(&fbdo, path, &json);
        logToFirebase(UID, "🚗 Xe vào - Chờ nhận diện biển số (giữ số dư cũ)");
        Serial.println("🚗 Xe vào - Chờ nhận diện biển số (giữ số dư cũ)");
        
        openGate(UID);
      }
    } else {
      // Thẻ chưa tồn tại: tạo mới với balance = 0
      FirebaseJson json;
      json.set("uid", UID);
      json.set("status", 0);
      json.set("waiting_plate", true);
      json.set("date_in", getFormattedDate());
      json.set("balance", 0);
      json.set("waiting_payment", false);
      json.set("fee_due", 0);
      
      Firebase.RTDB.setJSON(&fbdo, path, &json);
      logToFirebase(UID, "🚗 Xe mới vào - Chờ nhận diện biển số");
      Serial.println("🚗 Xe mới vào - Chờ nhận diện biển số");
      
      openGate(UID);
    }
    
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(1000);
  }
  
  delay(10);
}