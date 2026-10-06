// ClipKey Bridge — M1: 버튼을 누르면 USB HID 키보드로 "Hello World!"를 한 번 입력한다.
//
// 빌드 조건 (Arduino-ESP32 3.x)
//   - Board: ESP32S3 Dev Module (또는 실제 보드에 맞는 정의)
//   - Tools > USB Mode: USB-OTG (TinyUSB)
//   - Tools > USB CDC On Boot: Disabled 권장
//
// 안전 규칙
//   - 전원 연결/재부팅만으로는 입력하지 않는다. 버튼이 떼어진 상태를 먼저 확인한 뒤, 다음 누름에서만 실행한다.
//   - 누르고 있는 동안 반복 입력하지 않는다 (누름 에지 1회 = 입력 1회).
//   - USB가 호스트에 연결되지 않았거나 중간에 끊기면 입력하지 않거나 중단하고 모든 키를 해제한다.

#ifndef ARDUINO_USB_MODE
#error "Native USB를 지원하는 ESP32-S2/S3 보드를 선택하세요."
#elif ARDUINO_USB_MODE == 1
#error "Tools > USB Mode 를 'USB-OTG (TinyUSB)' 로 설정하세요."
#else

#include <atomic>
#include "USB.h"
#include "USBHIDKeyboard.h"

namespace {

// BOOT 버튼(GPIO0). 부팅 시 스트랩 핀이므로 누른 채 재부팅하면 다운로드 모드로 진입한다.
// 실제 보드 배선 확인 후 다른 버튼을 쓰면 이 값만 바꾼다.
constexpr uint8_t BUTTON_PIN = 0;
constexpr uint32_t DEBOUNCE_MS = 30;
constexpr uint32_t KEY_DELAY_MS = 20;  // docs/API.md delayMs 기본값과 동일
constexpr char MESSAGE[] = "Hello World!";

USBHIDKeyboard keyboard;
std::atomic<bool> usbMounted{false};

struct Debounce {
  bool stable;      // 확정된 상태 (true = 눌림)
  bool lastRead;    // 마지막 원시 입력
  uint32_t changedAt;
};

// 부팅 직후를 '눌림'으로 가정한다. 실제로 떼어진 상태가 확인돼야 다음 누름이 에지가 된다.
Debounce button{true, true, 0};

bool readPressed() { return digitalRead(BUTTON_PIN) == LOW; }

// 확정 상태가 '떼어짐 → 눌림'으로 바뀐 순간에만 true.
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

// 문자마다 press/release 후 간격을 둔다. USB가 끊기면 즉시 중단하고 키를 모두 해제한다.
void typeText(const char* text) {
  for (const char* p = text; *p && usbMounted; ++p) {
    keyboard.write(static_cast<uint8_t>(*p));
    delay(KEY_DELAY_MS);
  }
  keyboard.releaseAll();
}

}  // namespace

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  USB.onEvent(onUsbEvent);
  keyboard.begin();
  USB.begin();
}

void loop() {
  if (pressedEdge(button, readPressed(), millis()) && usbMounted) {
    typeText(MESSAGE);
  }
  delay(1);
}

#endif  // ARDUINO_USB_MODE
