# ESP32-S3 펌웨어 시작 안내

첫 구현은 공식 Arduino-ESP32 USB HID 예제를 바탕으로 한다.

## M2: 무선 작업 수신 (`clipkey_bridge/`) — 진행 중

M1 검증 후 본 펌웨어는 `clipkey_bridge/`에서 이어 간다. `clipkey_hello/`는 M1 산출물로 유지한다.

### M2.1 Wi-Fi 접속 + `/api/v1/status` + 토큰 인증

- `secrets.h.example`을 `secrets.h`로 복사해 SSID/비밀번호/토큰을 넣는다. `secrets.h`는 커밋되지 않는다.
- 개발 빌드는 **USB CDC On Boot = Enabled**. HID 키보드 + 시리얼 복합 장치로 동작해 IP를 시리얼로 확인할 수 있고, 재업로드 시 BOOT+RESET이 필요 없다.
- `/api/v1/` 아래는 모든 경로에서 `Authorization: Bearer <token>`을 먼저 검사한다. 토큰이 틀리면 경로 존재 여부와 무관하게 401.
- 이 단계는 작업 등록·타이핑을 하지 않는다. 버튼도 아직 사용하지 않는다.

```bash
cp firmware/clipkey_bridge/secrets.h.example firmware/clipkey_bridge/secrets.h   # 값 채우기
arduino-cli compile --fqbn esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=cdc firmware/clipkey_bridge
arduino-cli upload  --fqbn esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=cdc --port /dev/cu.usbmodem1101 firmware/clipkey_bridge
arduino-cli monitor -p /dev/cu.usbmodem1101 -c baudrate=115200           # [wifi] connected, ip=... 확인
curl -i http://<ip>/api/v1/status -H "Authorization: Bearer <token>"      # 200 JSON
curl -i http://<ip>/api/v1/status                                          # 401
```

## M1: 버튼 → `Hello World!` (`clipkey_hello/`) — 완료

BOOT 버튼(GPIO0)을 누르면 USB HID 키보드로 `Hello World!`를 한 번 입력한다.

- 전원 연결·재부팅만으로는 입력하지 않는다. 버튼이 떼어진 상태를 확인한 뒤의 누름에서만 실행한다.
- 누름 에지 1회에 입력 1회. 30ms 디바운싱, 문자 간격 20ms.
- USB가 연결(mount)되지 않았거나 suspend 상태면 입력하지 않고, 입력 중 끊기면 중단 후 모든 키를 해제한다.
- 버튼 핀은 `BUTTON_PIN` 상수 하나로 바꾼다.

### Arduino IDE 설정

### 확인된 보드 정보 (2026-10-08, Mac 실측)

| 항목 | 값 |
|---|---|
| 칩 | ESP32-S3, 내장 Flash 4MB (Quad), 내장 PSRAM 2MB |
| USB 포트 | Native USB 직결. 펌웨어 전에는 `USB JTAG/serial debug unit`(0x303A:0x1001)로 인식 |
| 업로드 | `arduino-cli upload --port /dev/cu.usbmodemXXXX`로 추가 배선 없이 성공 |
| 업로드 후 | HID 키보드 `ESP32S3_DEV`로 재열거, 시리얼 포트 사라짐 |

| 항목 | 값 |
|---|---|
| 보드 패키지 | esp32 by Espressif 3.3.2 (컴파일·업로드 확인 버전) |
| Board | ESP32S3 Dev Module (실제 보드 확인 후 조정) |
| USB Mode | **USB-OTG (TinyUSB)** — 기본값 Hardware CDC면 `#error`로 빌드 중단 |
| USB CDC On Boot | Disabled |

`arduino-cli`로 빌드할 때:

```bash
arduino-cli core install esp32:esp32@3.3.2 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli compile --fqbn esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=default firmware/clipkey_hello
arduino-cli upload  --fqbn esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=default --port /dev/cu.usbmodem1101 firmware/clipkey_hello
```

포트 이름은 `ls /dev/cu.usbmodem*`로 확인한다.

USB-OTG 모드에서는 업로드 후 보드가 키보드로 재열거되어 시리얼 포트가 사라질 수 있다. 재업로드는 BOOT를 누른 채 RESET(또는 USB 재연결)으로 다운로드 모드에 진입한 뒤 진행한다.

### 실물 테스트 체크리스트

1. 대상 PC를 영문 입력/US 배열, Caps Lock OFF로 두고 메모장 등 텍스트 편집기에 포커스를 둔다.
2. 보드를 연결해 키보드로 인식되는지 확인한다. 이때 아무것도 입력되지 않아야 한다.
3. BOOT 버튼을 한 번 눌러 `Hello World!`가 정확히 한 번 입력되는지 확인한다.
4. 버튼을 길게 눌러도 한 번만 입력되는지, 재부팅 후 자동 입력이 없는지 확인한다.


1. 보드의 실물 칩과 판매자 회로도를 확인한다. USB 커넥터가 Native USB로 연결되는지 확인한다. USB-UART 포트만 있는 보드는 추가 배선이 필요할 수 있다.
2. 판매자 정보에 따라 정확한 보드/Flash/PSRAM 설정을 정한다. Super Mini라는 이름만으로 용량과 LED/버튼 핀을 추정하지 않는다.
3. Arduino IDE에 Espressif ESP32 보드 패키지를 설치한다. 최초 빌드에서 패키지 버전과 보드 설정을 이 문서에 기록한다.
4. ESP32-S3의 USB Mode를 USB-OTG(TinyUSB)로 설정하고 공식 USB 키보드 예제를 확인한다. 메뉴는 패키지와 보드 정의에 따라 달라질 수 있다.
5. 버튼을 누른 뒤 Hello World를 한 번 입력하는 것으로 첫 테스트를 한다 (`clipkey_hello/`). 전원 연결 직후 자동 입력하지 않는다.
6. 성공 후 docs/API.md의 작업 큐와 Wi-Fi 서버를 구현한다.

BOOT는 부팅 시 스트랩 역할이 있으므로 누른 채 재부팅하면 다운로드 모드로 들어갈 수 있다. 런타임 사용자 버튼으로 쓰려면 실제 배선과 동작을 확인한다. RESET 버튼은 일반 사용자 입력 버튼으로 사용할 수 없다.

## 향후 소스 구성

- USB HID 출력 모듈
- HTTP API / 토큰 검증
- 단일 작업 버퍼와 상태 머신
- 버튼/LED 제어
- 비공개 Wi-Fi/토큰 설정

Wi-Fi 비밀번호와 토큰은 Git에 포함하지 않는다. 설정 예제에는 실제 값을 넣지 않는다.
