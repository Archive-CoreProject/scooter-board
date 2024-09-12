#include <esp_now.h>
#include <WiFi.h>
#include <SPI.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TM1637Display.h>

#define CLK 5
#define DIO 6
#define POWER_PIN 3  // 킥보드에 전력 공급할 핀 번호
#define LED 9 // 음주측정 통과 시 LED에 불 들어옴

TM1637Display display(CLK, DIO);

// MAC 주소 및 WiFi 정보
byte broadcastAddress[] = { 0x3C, 0x84, 0x27, 0xA5, 0x52, 0xEC };
byte helmetAddress[] = { 0x3C, 0x84, 0x27, 0xA7, 0xE2, 0xC0 };
const char* ssid = "SHRDI_501B";
const char* password = "a123456789";

// 전송할 데이터 구조체
typedef struct {
  char userId[32];
} myStruct;

myStruct myData;
esp_now_peer_info_t dest;
const int idx = 1;

void setup() {
  Serial.begin(115200);
  SPI.begin(9, 7, 8);
  pinMode(POWER_PIN, OUTPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(POWER_PIN, LOW);
  digitalWrite(LED, LOW);

  setupWiFi();
  setupEspNow();
  setupDisplay();
}

void setupWiFi() {
  Serial.println("Starting WiFi connection...");
  WiFi.disconnect(true);  // 이전에 저장된 WiFi 설정 제거
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {  // 15초 동안 시도
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to the WiFi network");
    Serial.print("Local IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());
  } else {
    Serial.println("\nFailed to connect to WiFi. Please check your SSID and password.");
    while (true)
      ;  // WiFi 연결 실패 시 멈춤
  }
}

void setupEspNow() {
  Serial.println("Initializing ESP-NOW...");
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW initialization failed");
    while (true)
      ;  // ESP-NOW 초기화 실패 시 멈춤
  }

  // 피어 설정 및 추가
  memcpy(dest.peer_addr, helmetAddress, 6);
  dest.channel = 0;
  dest.encrypt = false;

  if (esp_now_add_peer(&dest) != ESP_OK) {
    Serial.println("Failed to add peer");
    while (true)
      ;  // 피어 추가 실패 시 멈춤
  }

  // 송신 및 수신 콜백 설정
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
}

void setupDisplay() {
  display.setBrightness(5, false);
}

void loop() {
  getVerifyStatus();
  getAlcoholData();
  delay(5500);
}

void getVerifyStatus() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi 연결 오류");
    return;
  }

  HTTPClient http;
  http.begin("http://192.168.219.59:3000/board/read-code");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String postdata = "scooterIdx=" + String(idx);
  int httpCode = http.POST(postdata);
  Serial.print("HTTP 응답 코드: ");
  Serial.println(httpCode);

  if (httpCode == 200) {
    String result = http.getString();
    Serial.println("서버 응답: " + result);
    processVerifyResponse(result);
  } else {
    handleFailedHttpRequest(httpCode);
  }
  http.end();
}

void processVerifyResponse(const String& response) {
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, response);

  if (error) {
    Serial.print("JSON 파싱 실패: ");
    Serial.println(error.f_str());
    return;
  }

  const char* status = doc["userId"];
  const char* authCode = doc["authCode"];
  const int isVerified = doc["isVerified"];

  Serial.print("유저 아이디: ");
  Serial.println(status);
  Serial.print("인증번호: ");
  Serial.println(authCode);
  Serial.print("인증완료: ");
  Serial.println(isVerified);

  int codeNumber = (authCode != nullptr) ? atoi(authCode) : 0;
  updateDisplay(codeNumber, isVerified);

  strncpy(myData.userId, status, sizeof(myData.userId));
  myData.userId[sizeof(myData.userId) - 1] = '\0';  // null 종료 보장

  if (isVerified) {
    Serial.println("인증성공했어요");
    esp_now_send(dest.peer_addr, (uint8_t*)&myData, sizeof(myData));
  }
}

void updateDisplay(int codeNumber, bool isVerified) {
  if (isVerified) {
    display.setBrightness(5, false);
    display.showNumberDec(0);
  } else {
    display.setBrightness(5, true);
    display.showNumberDec(codeNumber);
  }
}

void handleFailedHttpRequest(int httpCode) {
  Serial.println("HTTP 요청 실패: " + String(httpCode));
  display.setBrightness(5, false);
  display.showNumberDec(0);

  strncpy(myData.userId, "", sizeof(myData.userId));
  myData.userId[sizeof(myData.userId) - 1] = '\0';
  esp_now_send(dest.peer_addr, (uint8_t*)&myData, sizeof(myData));
}

void getAlcoholData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi 연결 오류");
    return;
  }

  HTTPClient http;
  String getData = "http://192.168.219.59:3000/board/read-alcohol?idx=" + String(idx) + "&userId=" + String(myData.userId);
  http.begin(getData);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int httpCode = http.GET();
  Serial.print("HTTP 응답 코드: ");
  Serial.println(httpCode);

  if (httpCode == 200) {
    String result = http.getString();
    Serial.println("서버 응답: " + result);
    processAlcoholResponse(result);
  } else {
    Serial.println("HTTP 요청 실패: " + String(httpCode));
  }
  http.end();
}

void processAlcoholResponse(const String& response) {
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, response);

  if (error) {
    Serial.print("JSON 파싱 실패: ");
    Serial.println(error.f_str());
    return;
  }

  const char* message = doc["message"];
  const int accept = doc["accept"];
  Serial.println(message);
  Serial.print("음주 측정 결과: ");
  Serial.println(accept);

  digitalWrite(POWER_PIN, accept ? HIGH : LOW);
  digitalWrite(LED, accept);
}

void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Sent successfully" : "전송 실패");
}

void OnDataRecv(const esp_now_recv_info* info, const uint8_t* data, int len) {
  if (len == sizeof(myData)) {
    memcpy(&myData, data, len);
    Serial.print("유저 아이디: ");
    Serial.println(myData.userId);
  } else {
    Serial.println("수신 데이터 길이 불일치");
  }
}
