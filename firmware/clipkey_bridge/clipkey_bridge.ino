// ClipKey Bridge — M2.2: 작업 등록(POST /api/v1/type) → 버튼 승인 → USB HID 타이핑, 상태 조회/취소
//
// 빌드 조건 (Arduino-ESP32 3.x)
//   - Board: ESP32S3 Dev Module
//   - Tools > USB Mode: USB-OTG (TinyUSB)
//   - Tools > USB CDC On Boot: Enabled  (HID 키보드 + 시리얼 복합 장치. IP 확인과 재업로드용)
//
// 설정은 secrets.h 에 둔다 (secrets.h.example 참고). Git 에 포함하지 않는다.
// 계약은 docs/API.md. 요청 원문과 토큰은 시리얼에 남기지 않는다.

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
#include <uri/UriBraces.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "secrets.h"

namespace {

constexpr char FIRMWARE_VERSION[] = "0.1.0-m2.2";
constexpr char HOSTNAME[] = "clipkey";
constexpr uint16_t HTTP_PORT = 80;
constexpr char API_PREFIX[] = "/api/v1/";
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 8000;
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000;

constexpr uint8_t BUTTON_PIN = 0;  // BOOT. 런타임 입력 동작은 실물 확인됨
constexpr uint32_t DEBOUNCE_MS = 30;

constexpr size_t MAX_TEXT_BYTES = 4096;
constexpr uint32_t WAIT_TIMEOUT_MS = 60000;      // 승인 대기 만료
constexpr uint32_t JOB_RETENTION_MS = 600000;    // 최근 요청 보관 (중복 방지·상태 조회)
constexpr size_t JOB_HISTORY_SIZE = 16;
constexpr int DEFAULT_DELAY_MS = 20, MIN_DELAY_MS = 10, MAX_DELAY_MS = 100;

enum class JobState : uint8_t { WAITING, TYPING, COMPLETED, CANCELLED, FAILED };

const char* name(JobState s) {
  switch (s) {
    case JobState::WAITING: return "WAITING";
    case JobState::TYPING: return "TYPING";
    case JobState::COMPLETED: return "COMPLETED";
    case JobState::CANCELLED: return "CANCELLED";
    case JobState::FAILED: return "FAILED";
  }
  return "UNKNOWN";
}

bool isActive(JobState s) { return s == JobState::WAITING || s == JobState::TYPING; }

// 요청 ID 별 결과 기록. 원문은 현재 작업에만 있고 기록에는 해시만 남긴다.
struct JobRecord {
  String requestId;
  uint32_t hash = 0;
  JobState state = JobState::COMPLETED;
  size_t typed = 0, total = 0;
  const char* error = nullptr;
  uint32_t updatedAt = 0;
  bool used = false;
};

// 현재 실행 중인 유일한 작업
struct Job {
  String requestId;
  String text;
  bool appendEnter = false;
  int delayMs = DEFAULT_DELAY_MS;
  JobState state = JobState::COMPLETED;
  size_t typed = 0;
  size_t total = 0;
  uint32_t createdAt = 0, lastKeyAt = 0;
  bool active() const { return isActive(state); }
};

struct Debounce {
  bool stable, lastRead;
  uint32_t changedAt;
};

USBHIDKeyboard keyboard;
WebServer server(HTTP_PORT);
WiFiMulti wifiMulti;
std::atomic<bool> usbMounted{false};
bool mdnsStarted = false;
uint32_t lastWifiAttemptAt = 0;

Job job;
JobRecord history[JOB_HISTORY_SIZE];
Debounce button{true, true, 0};  // 부팅 직후를 '눌림'으로 가정: 떼어진 뒤의 누름만 에지

// ---------- USB / Wi-Fi / mDNS ----------

void onUsbEvent(void*, esp_event_base_t base, int32_t id, void*) {
  if (base != ARDUINO_USB_EVENTS) return;
  switch (id) {
    case ARDUINO_USB_STARTED_EVENT:
    case ARDUINO_USB_RESUME_EVENT: usbMounted = true; break;
    case ARDUINO_USB_STOPPED_EVENT:
    case ARDUINO_USB_SUSPEND_EVENT: usbMounted = false; break;
  }
}

void onWifiEvent(WiFiEvent_t event) {
  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    Serial.printf("[wifi] connected ssid=%s ip=%s host=%s.local\n",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), HOSTNAME);
  } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    Serial.println("[wifi] disconnected");
  }
}

void maintainWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  const uint32_t now = millis();
  if (lastWifiAttemptAt != 0 && now - lastWifiAttemptAt < WIFI_RETRY_INTERVAL_MS) return;
  lastWifiAttemptAt = now;
  Serial.println("[wifi] scanning for known networks");
  wifiMulti.run(WIFI_CONNECT_TIMEOUT_MS);
}

void maintainMdns() {
  if (mdnsStarted || WiFi.status() != WL_CONNECTED) return;
  mdnsStarted = MDNS.begin(HOSTNAME);
  if (!mdnsStarted) return;
  MDNS.addService("http", "tcp", HTTP_PORT);
  Serial.printf("[mdns] http://%s.local\n", HOSTNAME);
}

// ---------- 작업 기록 ----------

uint32_t fnv1a(const String& s, bool appendEnter, int delayMs) {
  uint32_t h = 2166136261u;
  auto mix = [&](uint8_t b) { h = (h ^ b) * 16777619u; };
  for (size_t i = 0; i < s.length(); ++i) mix(s[i]);
  mix(appendEnter);
  mix(delayMs);
  return h;
}

JobRecord* findRecord(const String& id) {
  const uint32_t now = millis();
  for (JobRecord& r : history) {
    if (r.used && now - r.updatedAt <= JOB_RETENTION_MS && r.requestId == id) return &r;
  }
  return nullptr;
}

// 비어 있거나 만료된 칸, 없으면 가장 오래된 완료 칸. 실행 중인 작업은 밀어내지 않는다.
JobRecord* allocRecord() {
  const uint32_t now = millis();
  JobRecord* oldest = nullptr;
  for (JobRecord& r : history) {
    if (!r.used || now - r.updatedAt > JOB_RETENTION_MS) return &r;
    if (!isActive(r.state) && (!oldest || r.updatedAt < oldest->updatedAt)) oldest = &r;
  }
  return oldest;
}

void syncRecord(const char* error = nullptr) {
  JobRecord* r = findRecord(job.requestId);
  if (!r) return;
  r->state = job.state;
  r->typed = job.typed;
  r->total = job.total;
  if (error) r->error = error;
  r->updatedAt = millis();
}

// ---------- 작업 상태 전이 ----------

void finishJob(JobState state, const char* error = nullptr) {
  keyboard.releaseAll();
  job.state = state;
  syncRecord(error);
  job.text = String();  // 원문은 끝나는 즉시 버린다
  Serial.printf("[job] %s typed=%u/%u%s%s\n", name(state), (unsigned)job.typed, (unsigned)job.total,
                error ? " error=" : "", error ? error : "");
}

void startTyping() {
  job.state = JobState::TYPING;
  job.lastKeyAt = 0;
  syncRecord();
  Serial.println("[job] TYPING");
}

// 한 글자씩. delayMs 는 press/release 후 다음 글자까지의 추가 간격. 본문 뒤의 한 글자는 추가 Enter.
void typeNextChar() {
  const uint32_t now = millis();
  if (job.lastKeyAt != 0 && now - job.lastKeyAt < (uint32_t)job.delayMs) return;
  const char c = job.typed < job.text.length() ? job.text[job.typed] : '\n';
  keyboard.write(static_cast<uint8_t>(c));
  job.typed++;
  job.lastKeyAt = now;
  if (job.typed >= job.total) finishJob(JobState::COMPLETED);
}

void maintainJob() {
  if (!job.active()) return;
  if (!usbMounted) {
    finishJob(JobState::FAILED, "usb_disconnected");
  } else if (job.state == JobState::WAITING && millis() - job.createdAt > WAIT_TIMEOUT_MS) {
    finishJob(JobState::CANCELLED, "expired");
  } else if (job.state == JobState::TYPING) {
    typeNextChar();
  }
}

// ---------- 버튼 ----------

bool pressedEdge(Debounce& d, bool reading, uint32_t now) {
  if (reading != d.lastRead) {
    d.lastRead = reading;
    d.changedAt = now;
    return false;
  }
  if (reading == d.stable || now - d.changedAt < DEBOUNCE_MS) return false;
  d.stable = reading;
  return d.stable;
}

void maintainButton() {
  if (!pressedEdge(button, digitalRead(BUTTON_PIN) == LOW, millis())) return;
  if (job.state == JobState::WAITING) startTyping();
  else if (job.state == JobState::TYPING) finishJob(JobState::CANCELLED, "button");
}

// ---------- HTTP 공통 ----------

bool constantTimeEquals(const String& a, const String& b) {
  uint8_t diff = a.length() ^ b.length();
  for (size_t i = 0; i < a.length(); ++i) diff |= a[i] ^ b[i < b.length() ? i : 0];
  return diff == 0;
}

void sendJson(int code, const String& body) { server.send(code, "application/json", body); }
void sendError(int code, const char* error) { sendJson(code, String("{\"error\":\"") + error + "\"}"); }

bool requireAuth() {
  if (constantTimeEquals(server.header("Authorization"), String("Bearer ") + CLIPKEY_TOKEN)) return true;
  server.sendHeader("WWW-Authenticate", "Bearer");
  sendError(401, "unauthorized");
  return false;
}

String jobJson(const JobRecord& r, bool withExpiry = false) {
  String s = "{\"requestId\":\"" + r.requestId + "\",\"state\":\"" + name(r.state)
             + "\",\"typedCharacters\":" + r.typed + ",\"totalCharacters\":" + r.total
             + ",\"error\":" + (r.error ? String("\"") + r.error + "\"" : String("null"));
  if (withExpiry) s += ",\"expiresInSeconds\":" + String(WAIT_TIMEOUT_MS / 1000);
  return s + "}";
}

// ---------- 요청 검증 ----------

bool isUuid(const String& s) {
  if (s.length() != 36) return false;
  for (size_t i = 0; i < 36; ++i) {
    const char c = s[i];
    const bool dash = (i == 8 || i == 13 || i == 18 || i == 23);
    if (dash ? c != '-' : !isxdigit(c)) return false;
  }
  return true;
}

bool isTypable(const String& s) {
  for (size_t i = 0; i < s.length(); ++i) {
    const char c = s[i];
    if ((c < 0x20 || c > 0x7E) && c != '\n' && c != '\t') return false;
  }
  return true;
}

bool parseBool(const String& v, bool& out) {
  if (v.isEmpty() || v == "false") { out = false; return true; }
  if (v == "true") { out = true; return true; }
  return false;
}

bool parseDelay(const String& v, int& out) {
  if (v.isEmpty()) { out = DEFAULT_DELAY_MS; return true; }
  for (size_t i = 0; i < v.length(); ++i) if (!isdigit(v[i])) return false;
  out = v.toInt();
  return out >= MIN_DELAY_MS && out <= MAX_DELAY_MS;
}

// ---------- 핸들러 ----------

void handleStatus() {
  if (!requireAuth()) return;
  const bool active = job.active();
  sendJson(200, String("{\"firmwareVersion\":\"") + FIRMWARE_VERSION
                  + "\",\"usbReady\":" + (usbMounted ? "true" : "false")
                  + ",\"state\":\"" + (active ? name(job.state) : "IDLE")
                  + "\",\"activeRequestId\":" + (active ? "\"" + job.requestId + "\"" : String("null")) + "}");
}

void handleType() {
  if (!requireAuth()) return;
  const String id = server.header("X-Request-Id");
  const String text = server.arg("plain");
  bool appendEnter = false;
  int delayMs = DEFAULT_DELAY_MS;
  if (!isUuid(id)) return sendError(400, "invalid_request_id");
  if (!parseBool(server.arg("appendEnter"), appendEnter) || !parseDelay(server.arg("delayMs"), delayMs)) {
    return sendError(400, "invalid_option");
  }
  if (text.length() > MAX_TEXT_BYTES) return sendError(413, "too_large");
  if (text.isEmpty()) return sendError(400, "empty_body");
  if (!isTypable(text)) return sendError(400, "unsupported_character");

  const uint32_t hash = fnv1a(text, appendEnter, delayMs);
  if (JobRecord* dup = findRecord(id)) {
    if (dup->hash != hash) return sendError(409, "request_id_reused");
    return sendJson(202, jobJson(*dup, dup->state == JobState::WAITING));  // 같은 요청 재전송: 다시 입력하지 않음
  }
  if (job.active()) return sendError(409, "busy");
  if (!usbMounted) return sendError(503, "usb_not_ready");
  JobRecord* rec = allocRecord();
  if (!rec) return sendError(503, "history_full");

  job = Job{};
  job.requestId = id;
  job.text = text;
  job.appendEnter = appendEnter;
  job.delayMs = delayMs;
  job.total = text.length() + (appendEnter ? 1 : 0);
  job.state = JobState::WAITING;
  job.createdAt = millis();
  *rec = JobRecord{id, hash, JobState::WAITING, 0, job.total, nullptr, job.createdAt, true};
  Serial.printf("[job] WAITING chars=%u enter=%d delay=%d\n", (unsigned)job.total, appendEnter, delayMs);
  sendJson(202, jobJson(*rec, true));
}

void handleJob() {
  if (!requireAuth()) return;
  const JobRecord* r = findRecord(server.pathArg(0));
  if (!r) return sendError(404, "not_found");
  sendJson(200, jobJson(*r));
}

void handleCancel() {
  if (!requireAuth()) return;
  const String id = server.pathArg(0);
  JobRecord* r = findRecord(id);
  if (!r) return sendError(404, "not_found");
  if (job.active() && job.requestId == id) finishJob(JobState::CANCELLED, "cancelled");
  sendJson(200, jobJson(*r));
}

void handleNotFound() {
  if (server.uri().startsWith(API_PREFIX) && !requireAuth()) return;
  sendError(404, "not_found");
}

// ---------- setup ----------

void setupUsb() {
  USB.onEvent(onUsbEvent);
  keyboard.begin();
  USB.begin();
}

void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.enableIPv6();  // mDNS 가 AAAA 에 즉시 답하게 한다 (없으면 macOS/Java 해석 5초 지연)
  WiFi.onEvent(onWifiEvent);
  for (const WifiNetwork& n : WIFI_NETWORKS) wifiMulti.addAP(n.ssid, n.password);
}

void setupHttp() {
  static const char* collected[] = {"Authorization", "X-Request-Id"};
  server.collectHeaders(collected, 2);
  server.on("/api/v1/status", HTTP_GET, handleStatus);
  server.on("/api/v1/type", HTTP_POST, handleType);
  server.on(UriBraces("/api/v1/jobs/{}"), HTTP_GET, handleJob);
  server.on(UriBraces("/api/v1/jobs/{}/cancel"), HTTP_POST, handleCancel);
  server.onNotFound(handleNotFound);
  server.begin();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  setupUsb();
  setupWifi();
  setupHttp();
  Serial.printf("[boot] clipkey-bridge %s\n", FIRMWARE_VERSION);
}

void loop() {
  maintainWifi();
  maintainMdns();
  server.handleClient();
  maintainButton();
  maintainJob();
  delay(1);
}

#endif  // ARDUINO_USB_MODE
