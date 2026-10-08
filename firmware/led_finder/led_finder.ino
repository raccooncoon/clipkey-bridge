// LED 핀 탐색용 일회성 스케치. 후보 핀을 2초씩 깜빡이고, 그동안 BOOT 를 누르면 현재 핀을 시리얼로 보고한다.
// 빌드: USB-OTG + CDC On Boot (firmware/tools/upload.sh led_finder). 결과 확인 후 clipkey_bridge 를 다시 올린다.
#include <Arduino.h>

constexpr uint8_t BUTTON_PIN = 0;
// USB(19/20), UART(43/44), 스트랩(3/45/46) 제외. 48 은 단순 GPIO 와 WS2812 두 방식으로 시험한다.
constexpr int CANDIDATES[] = {48, 21, 2, 8, 10, 13, 47, 38, 39, 40, 41, 42, 1, 4, 5, 6, 7, 9, 11, 12, 14, 15, 16, 17, 18};
constexpr uint32_t PHASE_MS = 2000;

enum class Mode { GPIO_HIGH, GPIO_LOW, WS2812 };

struct Phase { int pin; Mode mode; };
Phase phases[sizeof(CANDIDATES) / sizeof(CANDIDATES[0]) * 2 + 1];
size_t phaseCount = 0, current = 0;
uint32_t phaseStart = 0;
bool reported = false;

const char* name(Mode m) { return m == Mode::GPIO_HIGH ? "HIGH=켜짐" : m == Mode::GPIO_LOW ? "LOW=켜짐" : "WS2812"; }

void apply(const Phase& p, bool on) {
  if (p.mode == Mode::WS2812) { rgbLedWrite(p.pin, on ? 0 : 0, 0, on ? 64 : 0); return; }
  pinMode(p.pin, OUTPUT);
  digitalWrite(p.pin, (p.mode == Mode::GPIO_HIGH) == on ? HIGH : LOW);
}

void release(const Phase& p) {
  if (p.mode == Mode::WS2812) { rgbLedWrite(p.pin, 0, 0, 0); }
  pinMode(p.pin, INPUT);
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  phases[phaseCount++] = {48, Mode::WS2812};
  for (int pin : CANDIDATES) { phases[phaseCount++] = {pin, Mode::GPIO_HIGH}; phases[phaseCount++] = {pin, Mode::GPIO_LOW}; }
  delay(1500);
  Serial.println("[led] 파란 LED 가 깜빡일 때 BOOT 를 누르세요");
  phaseStart = millis();
}

void loop() {
  const Phase& p = phases[current];
  const uint32_t t = millis() - phaseStart;
  apply(p, (t / 250) % 2 == 0);  // 250ms 간격 깜빡임
  if (!reported && digitalRead(BUTTON_PIN) == LOW) {
    reported = true;
    Serial.printf("[led] RESULT pin=%d mode=%s\n", p.pin, name(p.mode));
  }
  if (t >= PHASE_MS) {
    release(p);
    current = (current + 1) % phaseCount;
    phaseStart = millis();
    reported = false;
    Serial.printf("[led] phase %u/%u pin=%d %s\n", (unsigned)current + 1, (unsigned)phaseCount, phases[current].pin, name(phases[current].mode));
  }
  delay(5);
}
