#include <esp_now.h>
#include <WiFi.h>
#include <HTTPClient.h>
// WiFi 설정
const char* ssid = "SHRDI_501B";      // WIFI ID
const char* password = "a123456789";  // WIFI PW
// Server 요청 주소
const char* serverUrl = "http://192.168.219.59:3000/helmet/set-data";
esp_now_peer_info_t dest;
String userId = "";
struct struct_id {
  char userId[32];  // 최대 31자 + null terminator
} incomingData;

12345678910111213141516171819202122232425262728
#include <esp_now.h>
#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>  // 최신 ArduinoJson 라이브러리 포함
// MAC 주소 및 WiFi 정보
byte broadcastAddress[] = { 0x3C, 0x84, 0x27, 0xA4, 0x5B, 0xA8 };
byte helmetAddress[] = { 0x3C, 0x84, 0x27, 0xA7, 0xE2, 0xC0 };
const char* ssid = "SHRDI_501B";

Message (Enter to send message to 'Geekble Mini ESP32-C3' on 'COM7')
Both NL & CR
115200 baud
헬멧 보관 안됨!!
-1
HTTP 요청 실패: -1
-11
HTTP 요청 실패: -11
Connecting to WiFi...
Connected to the WiFi network
헬멧 보관 안됨!!
-1
HTTP 요청 실패: -1
200
서버 응답: {"code":200,"message":"킥보드 반납(잠금)","isVerified":0}
킥보드 반납(잠금)
헬멧 보관 안됨!!
-11
HTTP 요청 실패: -11
HTTPClient http;  // 통신 객체
// 센서 및 핀 설정
const int analogSensorPin = A0;   // MQ-3 센서의 아날로그 출력 핀 (A0)
const int touchsensor = 7;        // 터치센서 SIG 핀 (D7)
int analogValue = 0;              // 아날로그 센서 값
bool flagTouchON = false;         // 터치 상태 플래그
void OnDataRecv(const esp_now_recv_info* mac, const uint8_t* data, int len) {
  // len이 구조체 크기와 일치하는지 확인
  if (len == sizeof(incomingData)) {
    memcpy(&incomingData, data, len);
    // 안전하게 문자열 종료
    incomingData.userId[sizeof(incomingData.userId) - 1] = '\0';
    // Serial 출력 시 C++의 String 객체를 사용하여 안전하게 처리
    Serial.print("userId: ");
    Serial.println(incomingData.userId);
    // C 스타일 문자열을 C++의 String 객체로 변환
    userId = String(incomingData.userId);
  } else {
    Serial.println("수신 데이터 길이 불일치");
  }
}
void setup() {
  Serial.begin(115200);
  pinMode(analogSensorPin, INPUT);  // 아날로그 핀을 입력으로 설정
  pinMode(touchsensor, INPUT);      // 터치센서 핀을 입력으로 설정
  // WiFi 연결 설정
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.println(WiFi.macAddress()); // MAC 주소 출력
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  dest.channel = 0;
  dest.encrypt = false;
  esp_now_register_recv_cb(OnDataRecv);  // 데이터 수신 콜백 등록
}
void loop() {
  // 센서 값 읽기
  analogValue = analogRead(analogSensorPin);
  bool touchState = digitalRead(touchsensor);
  // 터치센서 상태 확인
  if (touchState == HIGH && !flagTouchON) {
    flagTouchON = true;
    Serial.println("TOUCH ON");
  } else if (touchState == LOW && flagTouchON) {
    flagTouchON = false;
    Serial.println("TOUCH OFF");
  }
  // WiFi를 통해 서버로 데이터 전송
  Serial.println(userId);
  if (WiFi.status() == WL_CONNECTED && flagTouchON && userId != "") {
    String postData;
    // snprintf(postData, sizeof(postData), "alcohol=%d&helmetIdx=1&userId=%s", analogValue, userId);
    postData = "alcohol="+String(analogValue) + "&helmetIdx=1&userId="+userId;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpCode = http.POST(postData);
    if (httpCode > 0) {
      Serial.printf("HTTP 응답 코드: %d\n", httpCode);
      String result = http.getString(); // 결과를 String 변수로 선언 및 사용
      Serial.println("서버 응답: " + result);
    } else {
      Serial.printf("HTTP 요청 실패: %d\n", httpCode);
    }
    http.end();
  } else if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi 연결 오류");
  }
  delay(3000);
}