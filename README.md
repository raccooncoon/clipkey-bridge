# ClipKey Bridge

**Copy here. Type there.**

내 Mac/PC의 클립보드 텍스트를 Wi-Fi로 ESP32-S3에 전송하고, USB로 연결된 다른 PC에 키보드 입력처럼 전달하는 무선 클립보드 동글 프로젝트.

- 저장소명: `clipkey-bridge`
- 대상 보드: ESP32-S3 Super Mini — 실제 보드의 Native USB 연결 확인 필요
- 송신기: Java 17 이상, 최초 CLI → 이후 데스크톱 UI
- 펌웨어: Arduino-ESP32 / TinyUSB 기반으로 개발 예정
- 입력 대상: USB HID 키보드를 허용하는 PC, 별도 수신 프로그램 불필요

## 동작

송신 PC 클립보드 → Java 송신기 → Wi-Fi HTTP → ESP32-S3 → USB HID → 대상 PC의 현재 입력창.

USB 키보드 동작은 ESP32-S3 공식 USB API 지원 범위에 해당한다. 보드 판매명만으로 USB 배선까지 보장되지는 않는다.

## v0.1 목표

- 영문 대소문자, 숫자, ASCII 특수문자, Space, LF(Enter), Tab
- US 키보드 배열 기준. 대상 PC는 영문 입력 상태, Caps Lock OFF
- 최대 4,096바이트, CRLF/CR은 LF로 정규화, 한글/기타 Unicode는 거절
- 전송 전 미리보기, 자동 마지막 Enter는 기본 OFF
- 글자 간격 기본 20ms, 10~100ms 조절 — 실측 후 확정
- 토큰 인증, 물리 버튼으로 실행 승인, 입력 도중 같은 버튼으로 중단
- 한 번에 작업 하나. 요청 수신 성공과 실제 입력 완료를 구분

## 현재 상태

기획, API 계약, 로컬 Git 저장소, Java 전송 CLI 초안이 포함된 시작용 저장소다.
펌웨어는 M1(버튼 → `Hello World!` 입력) 스케치가 있으며 실물 보드에서 동작을 확인했다. 보드 실물의 USB 인식, 버튼 핀, 타이핑 품질은 검증 전이다.
Java CLI의 HTTP 전송은 아래 API를 구현한 펌웨어가 있어야 동작한다.

## 필요한 준비물

ESP32-S3 Super Mini 1개, USB 데이터 케이블, 송신 PC와 보드를 연결할 Wi-Fi.
별도 배터리 없이 대상 PC USB에서 전원을 받는 구성이다. 케이스는 선택 사항.

## 송신기 실행

JDK 17 이상 설치 후 저장소 루트에서 실행한다. 외부 Java 라이브러리는 필요 없다.

```bash
java sender/java/ClipKeySender.java --text 'Hello World!'
java sender/java/ClipKeySender.java
```

첫 명령은 직접 지정한 텍스트, 두 번째는 클립보드를 읽는다. 기본은 미리보기만 한다.
실제 전송 시 보드 주소와 펌웨어에 설정한 동일한 토큰이 필요하다.

```bash
export CLIPKEY_URL='http://clipkey.local'   # mDNS. 안 되면 보드 IP
export CLIPKEY_TOKEN='replace-with-device-token'
java sender/java/ClipKeySender.java --send
java sender/java/ClipKeySender.java --send --enter --delay-ms 30
```

`202` 응답은 대기 작업 등록이며 입력 완료가 아니다. 보드 버튼 승인 후 시작한다.
통신 실패 시 자동 재전송하지 않는다. 대상 PC에서 입력 상태를 확인한다.

## 문서

- [아키텍처와 범위](docs/ARCHITECTURE.md)
- [API 계약](docs/API.md)
- [개발 단계와 완료 기준](docs/ROADMAP.md)
- [보드 확인과 펌웨어 시작](firmware/README.md)
- [GitHub 저장소 생성](docs/REPOSITORY.md)

## 참고 자료

- [Espressif Arduino USB API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/usb.html)
- [Espressif USB 예제](https://github.com/espressif/arduino-esp32/tree/master/libraries/USB/examples)

BLE, OTA, 한글, 마우스/단축키 입력, 파일 전송은 후속 범위다. 배포 라이선스는 아직 지정하지 않았다.
