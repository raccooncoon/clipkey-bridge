# ClipKey Bridge

**Copy here. Type there.**

내 Mac/PC의 클립보드 텍스트를 Wi-Fi로 ESP32-S3에 전송하고, USB로 연결된 다른 PC에 키보드 입력처럼 전달하는 무선 클립보드 동글 프로젝트.

- 저장소명: `clipkey-bridge`
- 대상 보드: ESP32-S3 Super Mini — 실제 보드의 Native USB 연결 확인 필요
- 송신기: Java 17 이상, Swing 창 + CLI (macOS 는 `.app` 번들)
- 펌웨어: Arduino-ESP32 / TinyUSB 기반으로 개발 예정
- 입력 대상: USB HID 키보드를 허용하는 PC, 별도 수신 프로그램 불필요

## 동작

송신 PC 클립보드 → Java 송신기 → Wi-Fi HTTP → ESP32-S3 → USB HID → 대상 PC의 현재 입력창.

USB 키보드 동작은 ESP32-S3 공식 USB API 지원 범위에 해당한다. 보드 판매명만으로 USB 배선까지 보장되지는 않는다.

## v0.1 목표

- 영문 대소문자, 숫자, ASCII 특수문자, Space, LF(Enter), Tab, **한글**(2벌식 자모 키 + 한/영 전환, Windows 우선)
- US 키보드 배열 기준. 대상 PC는 영문 입력 상태, Caps Lock OFF
- 최대 4,096바이트(한글 3바이트), CRLF/CR은 LF로 정규화, NFC 정규화, 그 외 Unicode(이모지·한자 등)는 거절
- 전송 전 미리보기, 자동 마지막 Enter는 기본 OFF
- 글자 간격 기본 20ms, 10~100ms 조절 — 실측 후 확정
- 토큰 인증, 물리 버튼으로 실행 승인, 입력 도중 같은 버튼으로 중단. 보드에서 허용하면 버튼 없이 2초 뒤 자동 입력(그 2초는 취소 창)
- 한 번에 작업 하나. 요청 수신 성공과 실제 입력 완료를 구분

## 현재 상태

기획, API 계약, 로컬 Git 저장소, Java 전송 CLI 초안이 포함된 시작용 저장소다.
펌웨어는 M1(버튼 → `Hello World!` 입력) 스케치가 있으며 실물 보드에서 동작을 확인했다. 보드 실물의 USB 인식, 버튼 핀, 타이핑 품질은 검증 전이다.
Java CLI의 HTTP 전송은 아래 API를 구현한 펌웨어가 있어야 동작한다.

## 필요한 준비물

ESP32-S3 Super Mini 1개, USB 데이터 케이블, 송신 PC와 보드를 연결할 Wi-Fi.
별도 배터리 없이 대상 PC USB에서 전원을 받는 구성이다. 케이스는 선택 사항.

## 송신기 실행

### 어느 기기에서나: 보드 웹페이지

같은 Wi-Fi 에서 브라우저로 `http://clipkey.local/`(또는 보드 IP)을 연다. iPhone·Android·다른 PC 모두 설치 없이 된다. 토큰을 한 번 저장하면 브라우저에 남고, 붙여넣기·편집·검증·옵션·보내기·진행률·취소를 제공한다. iPhone 은 Safari 공유 → 홈 화면에 추가하면 앱처럼 쓴다. Chrome 은 `http://` 를 명시해야 할 수 있다.

### Mac 앱 / CLI

JDK 17 이상. 외부 Java 라이브러리는 없다. 소스는 `sender/java/src/clipkey/`, 한 바이너리가 두 모드로 동작한다.

- **창 모드** (인자 없음): 클립보드 미리보기(편집 가능, 수정 시 재검증), 검증 결과, 마지막 Enter·자동 입력·글자 간격 옵션, 보내기, 진행률(typed/total), 취소. 설정 창에서 장치 주소와 토큰을 저장한다(macOS 사용자 설정, Git 밖).
- **CLI 모드** (인자 있음): `--text TEXT` `--send` `--enter` `--auto` `--delay-ms 10..100`. 주소/토큰은 `CLIPKEY_URL`/`CLIPKEY_TOKEN` 환경변수, 없으면 창 모드에서 저장한 설정값.

### macOS

macOS 15+에서 `java` 명령으로 직접 실행하면 보드 연결이 `No route to host`로 실패한다(실측, JDK 7종 모두). JDK 런처가 로컬 네트워크 허용 알림을 띄우지 못해 조용히 거부되기 때문이다. 그래서 `.app` 번들로 실행한다.

```bash
sh sender/macos/build-app.sh        # ~/Applications/ClipKeySender.app 생성 + jar 설치. 이후엔 jar 만 갱신
open ~/Applications/ClipKeySender.app                                       # 창 모드
~/Applications/ClipKeySender.app/Contents/MacOS/ClipKeySender --send        # CLI 모드
```

첫 실행 때 로컬 네트워크 허용 알림이 뜨면 허용한다. 놓쳤으면 시스템 설정 → 개인정보 보호 및 보안 → 로컬 네트워크에서 `ClipKeySender`를 켠다. jar 는 번들 밖(`~/Library/Application Support/ClipKey`)에 두므로 소스를 고쳐 스크립트를 다시 실행해도 허용이 유지된다. `--rebuild` 로 번들을 다시 만들면 허용을 다시 받는다.

미리보기만 할 때는 JDK 22+ 의 소스 실행으로도 된다: `java sender/java/src/clipkey/ClipKeySender.java`

### 전송 결과 해석

`202`는 대기 작업 등록이며 입력 완료가 아니다. 보드 버튼 승인 후 시작하고, 창 모드는 완료까지 진행률을 보여준다.
통신 실패 시 자동 재전송하지 않는다. 같은 요청 ID 재전송은 보드가 재입력하지 않지만 CLI 재실행은 새 ID 이므로 대상 PC 를 먼저 확인한다.

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
