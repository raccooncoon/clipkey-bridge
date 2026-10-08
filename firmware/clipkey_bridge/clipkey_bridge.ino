// ClipKey Bridge — M2.1: Wi-Fi 다중 SSID 접속 + mDNS(clipkey.local) + GET /api/v1/status + Bearer 토큰 인증
//
// 빌드 조건 (Arduino-ESP32 3.x)
//   - Board: ESP32S3 Dev Module
//   - Tools > USB Mode: USB-OTG (TinyUSB)
//   - Tools > USB CDC On Boot: Enabled  (HID 키보드 + 시리얼 복합 장치. IP 확인과 재업로드용)
//
// 설정은 secrets.h 에 둔다 (secrets.h.example 참고). Git 에 포함하지 않는다.
// 이 단계에서는 작업 등록/타이핑을 하지 않는다. 키보드 인터페이스는 usbReady 보고를 위해 열어 둔다.

#ifndef ARDUINO_USB_MODE
#error "Native USB를 지원하는 ESP32-S2/S3 보드를 선택하세요."
#elif ARDUINO_USB_MODE == 1
#error "Tools > USB Mode 를 'USB-OTG (TinyUSB)' 로 설정하세요."
#else

#include <atomic>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "secrets.h"

namespace {

constexpr char FIRMWARE_VERSION[] = "0.1.0-m2.1";
constexpr char HOSTNAME[] = "clipkey";
constexpr uint16_t HTTP_PORT = 80;
constexpr char API_PREFIX[] = "/api/v1/";
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 8000;  // 한 번의 스캔+접속 시도 상한
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000;  // 실패 후 다음 시도까지 간격

USBHIDKeyboard keyboard;
WebServer server(HTTP_PORT);
WiFiMulti wifiMulti;
std::atomic<bool> usbMounted{false};
bool mdnsStarted = false;
uint32_t lastWifiAttemptAt = 0;

void onUsbEvent(void*, esp_event_base_t base, int32_t id, void*) {
  if (base != ARDUINO_USB_EVENTS) return;
  switch (id) {
    case ARDUINO_USB_STARTED_EVENT:
    case ARDUINO_USB_RESUME_EVENT:
      usbMounted = true;
      break;
    case ARDUINO_USB_STOPPED_EVENT:
    case ARDUINO_USB_SUSPEND_EVENT:
      usbMounted = false;
      break;
  }
}

void onWifiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[wifi] connected ssid=%s ip=%s host=%s.local\n",
                    WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), HOSTNAME);
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("[wifi] disconnected");
      break;
    default:
      break;
  }
}

// 길이가 달라도 같은 시간이 걸리도록 비교한다. 토큰 비교 시 타이밍 차이를 줄이기 위함.
bool constantTimeEquals(const String& a, const String& b) {
  uint8_t diff = a.length() ^ b.length();
  for (size_t i = 0; i < a.length(); ++i) {
    diff |= a[i] ^ b[i < b.length() ? i : 0];
  }
  return diff == 0;
}

bool authorized() {
  return constantTimeEquals(server.header("Authorization"), String("Bearer ") + CLIPKEY_TOKEN);
}

void sendJson(int code, const String& body) {
  server.send(code, "application/json", body);
}

void sendError(int code, const char* error) {
  sendJson(code, String("{\"error\":\"") + error + "\"}");
}

bool requireAuth() {
  if (authorized()) return true;
  server.sendHeader("WWW-Authenticate", "Bearer");
  sendError(401, "unauthorized");
  return false;
}

void handleStatus() {
  if (!requireAuth()) return;
  sendJson(200, String("{\"firmwareVersion\":\"") + FIRMWARE_VERSION
                  + "\",\"usbReady\":" + (usbMounted ? "true" : "false")
                  + ",\"state\":\"IDLE\",\"activeRequestId\":null}");
}

// /api/v1/ 아래는 경로가 틀려도 인증을 먼저 요구한다. 토큰 없이 엔드포인트 존재 여부를 알 수 없게 한다.
void handleNotFound() {
  if (server.uri().startsWith(API_PREFIX) && !requireAuth()) return;
  sendError(404, "not_found");
}

void setupUsb() {
  USB.onEvent(onUsbEvent);
  keyboard.begin();
  USB.begin();
}

void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  // 링크로컬 IPv6 를 켜서 mDNS 가 AAAA 에도 즉시 답하게 한다. 없으면 macOS/Java 가 AAAA 타임아웃(약 5초)까지 기다린다.
  WiFi.enableIPv6();
  WiFi.onEvent(onWifiEvent);
  for (const WifiNetwork& n : WIFI_NETWORKS) wifiMulti.addAP(n.ssid, n.password);
}

// 끊겨 있으면 일정 간격으로 저장된 SSID 를 스캔해 가장 신호가 좋은 곳에 붙는다. 스캔 중에는 HTTP 처리가 잠시 멈춘다.
void maintainWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  const uint32_t now = millis();
  if (lastWifiAttemptAt != 0 && now - lastWifiAttemptAt < WIFI_RETRY_INTERVAL_MS) return;
  lastWifiAttemptAt = now;
  Serial.println("[wifi] scanning for known networks");
  wifiMulti.run(WIFI_CONNECT_TIMEOUT_MS);
}

// 접속 후 한 번만 시작한다. 이후 IP 가 바뀌어도 mDNS 가 새 주소로 응답한다.
void maintainMdns() {
  if (mdnsStarted || WiFi.status() != WL_CONNECTED) return;
  mdnsStarted = MDNS.begin(HOSTNAME);
  if (!mdnsStarted) {
    Serial.println("[mdns] start failed");
    return;
  }
  MDNS.addService("http", "tcp", HTTP_PORT);
  Serial.printf("[mdns] http://%s.local\n", HOSTNAME);
}

void setupHttp() {
  static const char* collected[] = {"Authorization"};
  server.collectHeaders(collected, 1);
  server.on("/api/v1/status", HTTP_GET, handleStatus);
  server.onNotFound(handleNotFound);
  server.begin();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  setupUsb();
  setupWifi();
  setupHttp();
  Serial.printf("[boot] clipkey-bridge %s\n", FIRMWARE_VERSION);
}

void loop() {
  maintainWifi();
  maintainMdns();
  server.handleClient();
  delay(1);
}

#endif  // ARDUINO_USB_MODE
