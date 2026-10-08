# ESP32-S3 펌웨어 시작 안내

첫 구현은 공식 Arduino-ESP32 USB HID 예제를 바탕으로 한다.

## LED 표시 (`clipkey_bridge/`, M4.3)

보드 실측(2026-10-08): 빨간 LED 는 전원 표시, 작은 파란 LED 는 GPIO13(LOW 켜짐), 큰 LED 는 GPIO48 WS2812 RGB. 이 RGB LED 는 R/G 채널 순서가 라이브러리 기본과 반대라 `setRgb()` 에서 바꿔 보낸다. 핀을 모르는 보드는 `firmware/led_finder` 를 올리고 파란 LED 가 깜빡일 때 BOOT 를 누르면 시리얼로 핀과 극성을 보고한다.

| 상태 | RGB | 작은 파란 LED |
|---|---|---|
| Wi-Fi 미연결 | 빨강 느린 점멸 | 꺼짐 |
| 대기(연결됨) | 초록 아주 어둡게 | 꺼짐 |
| WAITING | 파랑 점멸 | 점멸 |
| TYPING | 파랑 켜짐 | 켜짐 |
| 완료 | 초록 1.5초 | 꺼짐 |
| 취소/실패/만료 | 빨강 1.5초 | 꺼짐 |

밝기는 `RGB_LEVEL`(기본 40/255), 대기 초록은 `DIM_GREEN`(4/255)으로 조정한다.

## M2: 무선 작업 수신 (`clipkey_bridge/`) — 완료

M1 검증 후 본 펌웨어는 `clipkey_bridge/`에서 이어 간다. `clipkey_hello/`는 M1 산출물로 유지한다.

### M2.2 작업 등록 → 버튼 승인 → 타이핑

- `POST /api/v1/type` 로 등록된 작업은 WAITING 으로 60초 대기한다. BOOT 를 누르면 TYPING, 다시 누르면 CANCELLED.
- 타이핑은 `loop()` 에서 한 글자씩 진행하므로 입력 중에도 상태 조회·취소 요청이 처리된다.
- 완료·취소·만료·USB 끊김 모두 `releaseAll()` 로 끝난다. 원문은 작업이 끝나는 즉시 메모리에서 지운다.
- 최근 16개 요청 ID 와 요청 해시를 10분 보관한다. 같은 요청 재전송은 기존 작업을 돌려주고(202), 같은 ID 에 다른 내용은 409.
- 실측(2026-10-08, curl): 등록 202 / 조회 200 / 중복 202 / ID 재사용 409 / 실행 중 등록 409 / 취소 200 / UUID·빈 본문·한글·옵션 오류 400 / 4097B 413 / 4096B 202 / 미존재 404 / 60초 만료 CANCELLED(expired). 실제 타이핑(2026-10-08): 80자(대소문자·숫자·반복·Tab·LF·전 ASCII 기호·추가 Enter) TextEdit 에 정확히 입력, COMPLETED 80/80. 버튼 중단: 2,819자 작업을 TYPING 중 BOOT 로 중단 → CANCELLED(button), typed=701 이 화면 출력과 정확히 일치. 간격별 누락 검사: 4,095자@20ms, 2,549자@10ms, 329자@100ms 모두 화면과 바이트 일치.
- 업로드는 `firmware/tools/upload.sh clipkey_bridge` 로 한다. 앱(CDC) 포트면 1200bps touch 로 다운로드 모드에 넣고 ROM 포트(`usbmodem1101`)에 올린다. 플래시 후 리셋 단계의 포트 오류는 무시한다(모든 구간 검증이 성공 기준).

### M2.1 Wi-Fi 다중 SSID + mDNS + `/api/v1/status` + 토큰 인증

- `secrets.h.example`을 `secrets.h`로 복사해 Wi-Fi 목록(집/회사/폰 핫스팟 등, 2.4GHz)과 토큰을 넣는다. `secrets.h`는 커밋되지 않는다.
- 부팅·재접속 시 목록을 스캔해 신호가 가장 좋은 SSID에 붙는다. 새 장소를 추가하려면 `secrets.h` 수정 후 재업로드한다(런타임 설정은 M4).
- mDNS로 `http://clipkey.local`에 응답한다. IP를 몰라도 된다. mDNS가 막힌 네트워크에서는 시리얼에 찍힌 IP를 쓴다.
- 토큰은 `openssl rand -hex 16` 같은 랜덤 문자열이면 된다. 보드 `secrets.h`와 송신 PC `CLIPKEY_TOKEN` 환경변수에 같은 값을 둔다.
- 개발 빌드는 **USB CDC On Boot = Enabled**. HID 키보드 + 시리얼 복합 장치로 동작해 IP를 시리얼로 확인할 수 있고, 재업로드 시 BOOT+RESET이 필요 없다.
- `/api/v1/` 아래는 모든 경로에서 `Authorization: Bearer <token>`을 먼저 검사한다. 토큰이 틀리면 경로 존재 여부와 무관하게 401.
- 링크로컬 IPv6를 켠다. 없으면 mDNS AAAA 질의가 응답 없이 타임아웃돼 macOS/Java의 `clipkey.local` 해석이 5초 걸리고 Java CLI는 연결 타임아웃으로 실패한다(실측). 켜면 0.01초.
- 이 단계는 작업 등록·타이핑을 하지 않는다. 버튼도 아직 사용하지 않는다.

실측 결과 (2026-10-08): 저장된 SSID 접속·시리얼 IP 출력, `clipkey.local` 해석(IPv4/IPv6), 토큰 없음 401, 틀린 토큰 401, 올바른 토큰 200, 미존재 경로 401(무토큰)/404(토큰), HID 키보드 유지 — 모두 통과.

```bash
cp firmware/clipkey_bridge/secrets.h.example firmware/clipkey_bridge/secrets.h   # 값 채우기
arduino-cli compile --fqbn esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=cdc firmware/clipkey_bridge
arduino-cli upload  --fqbn esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=cdc --port /dev/cu.usbmodem1101 firmware/clipkey_bridge
arduino-cli monitor -p /dev/cu.usbmodem1101 -c baudrate=115200           # [wifi] connected ssid=... ip=... 확인
curl -i http://clipkey.local/api/v1/status -H "Authorization: Bearer <token>"   # 200 JSON
curl -i http://clipkey.local/api/v1/status                                       # 401
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

### 업로드와 리셋 동작 (실측)

- CDC Off 빌드(M1)는 업로드 후 시리얼 포트가 사라진다. 재업로드는 BOOT를 누른 채 RESET으로 다운로드 모드에 진입한 뒤 진행한다.
- **수동(BOOT+RESET)으로 다운로드 모드에 들어가 플래시하면 esptool의 자동 리셋이 듣지 않아 보드가 다운로드 모드에 남는다.** 업로드 후 RESET을 한 번 눌러야 앱이 뜬다. Mac에서 장치 이름이 `USB JTAG_serial debug unit`이면 아직 부트로더, `ESP32S3_DEV`면 앱이다.
- CDC On 빌드(M2~)는 `arduino-cli upload`가 1200bps touch로 자동 진입한다. `No serial data received`가 나오면 포트가 바뀌는 사이 실패한 것이다. 포트에 1200bps를 한 번 설정해 다운로드 모드로 보낸 뒤 나타나는 `/dev/cu.usbmodem1101`로 다시 업로드하면 된다. 이 경우 자동 리셋은 정상 동작한다.
- 시리얼 로그는 부팅 직후 한 번 찍힌다. 포트가 재열거되면 리더가 끊기므로, 리셋 전후를 보려면 포트를 다시 여는 리더가 필요하다.

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
