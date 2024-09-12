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
const char* password = "a123456789";
// RFID 핀 설정
int RST_PIN = 6;
int SS_PIN = 10;
MFRC522 mfrc(SS_PIN, RST_PIN);
// 전송할 데이터 구조체
typedef struct {
  char userId[32];
} myStruct;
myStruct myData;
esp_now_peer_info_t dest;
int isSuccess = 0;
// 릴레이 모듈 제어 핀 설정
int relayPin = 0;
int relayValue = 0;
// 헬멧 감지 및 서버 전송 관련 변수
const int idx = 1;
char isDetected = 'N';
void setup() {
  // 시리얼 통신 및 RFID 설정
  Serial.begin(115200);
  SPI.begin(9, 7, 8);
  mfrc.PCD_Init();
  isSuccess = 0;
  // WiFi 설정 및 연결
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to the WiFi network");
  // 릴레이 핀 설정
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);
}
void loop() {
  // RFID 카드 감지 및 서버로 데이터 전송 로직
  helmetDetectionAndSend();
  // 시리얼을 통해 릴레이 제어 명령을 수신하고 처리하는 로직
  relayControl();
  delay(5500);
}
// 헬멧 감지 및 서버 전송 함수
void relayControl() {
  // 서버에 데이터 전송 및 응답 처리
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://192.168.219.59:3000/board/read-verified");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String postdata = "scooterIdx=" + String(idx);
    int httpCode = http.POST(postdata);
    Serial.println(httpCode);
    if (httpCode == 200) {
      String result = http.getString();
      Serial.println("서버 응답: " + result);
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, result);
      if (!error) {
        const char* message = doc["message"];
        const int isVerified = doc["isVerified"];
        // 여기서 받아오는 result안에 음주감지통과 여부를 받아오는거임
        // 그래서 일단 인증번호 입력 성공하면 릴레이 HIGH로 바꾸고?
        // 헬멧 쓰고 음주측정해서 통과하면 다시 LOW로 바꾸는거임!!!!
        // 중요한건 사용자가 헬멧을 꺼내고 뚜껑을 닫아야 원하는대로 동작한다는것
        Serial.println(message);
        if (isVerified) {
          digitalWrite(relayPin, LOW);
        } else {
          digitalWrite(relayPin, HIGH);
        }
      } else {
        Serial.print("JSON 파싱 실패: ");
        Serial.println(error.f_str());
      }
    } else {
      Serial.println("HTTP 요청 실패: " + String(httpCode));
    }
    http.end();
  } else {
    Serial.println("WiFi 연결 오류");
  }
}

// 헬멧 감지 상태 업데이트
void helmetDetectionAndSend() {
    // 카드 감지 및 처리 로직
  for (int i = 0; i < 5; i++) {
    if (mfrc.PICC_IsNewCardPresent()) {
      break;
    }
    delay(100);
  }
  if (mfrc.PICC_ReadCardSerial()) {
    Serial.println("헬멧 감지됨");
    isDetected = 'Y';
  } else {
    Serial.println("헬멧 보관 안됨!!");
    isDetected = 'N';
  }
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://192.168.219.59:3000/board/update-detect");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String postdata = "scooterIdx=" + String(idx) + "&detected=" + String(isDetected);
    int httpCode = http.POST(postdata);
    Serial.println(httpCode);
    if (httpCode == 200) {
      String result = http.getString();
      Serial.println("서버 응답: " + result);
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, result);
      if (!error) {
        const char* message = doc["message"];
        Serial.println(message);
      } else {
        Serial.print("JSON 파싱 실패: ");
        Serial.println(error.f_str());
      }
    } else {
      Serial.println("HTTP 요청 실패: " + String(httpCode));
    }
    http.end();
  } else {
    Serial.println("WiFi 연결 오류");
  }
}