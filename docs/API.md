# v0.1 API 계약 — 구현 예정

기본 주소는 `CLIPKEY_URL`. `/api/v1/` 아래 모든 요청에 `Authorization: Bearer <token>`을 요구한다.

## 작업 등록

`POST /api/v1/type?appendEnter=false&delayMs=20`

- `Content-Type: text/plain; charset=utf-8`
- `X-Request-Id`: UUID, 동일 요청의 중복 입력 방지
- 본문: 검증된 ASCII 텍스트, 1~4,096바이트
- `appendEnter`: true/false, 생략 시 false
- `delayMs`: 10~100 정수, 생략 시 20. 키 press/release 후 다음 문자까지의 추가 간격이며 정확한 초당 문자 수를 보장하지 않는다.
- CR은 받지 않는다. 송신기에서 LF로 정규화한다.

정상 등록 시 `202 Accepted`:

```json
{"requestId":"uuid","state":"WAITING","expiresInSeconds":60}
```

60초 안에 물리 버튼을 누르면 타이핑 시작. 마지막 Enter 옵션은 본문에 이미 있는 LF와 별개로 추가 Enter 하나를 의미한다.

## 상태와 취소

`GET /api/v1/jobs/{requestId}`: `requestId`, `state`, `typedCharacters`, `totalCharacters`, `error` 반환. `error`는 없으면 `null`, 있으면 `expired`(대기 만료), `button`(버튼 중단), `cancelled`(API 취소), `usb_disconnected`.
완료는 HID 보고서 전송 완료를 뜻하며 대상 앱에 정확히 표시되었음을 보장하지 않는다.

`POST /api/v1/jobs/{requestId}/cancel`: 작업 취소, 키 해제. 이미 입력된 문자를 되돌리지는 않는다.

`GET /api/v1/status`: `firmwareVersion`, `usbReady`, `state`, `activeRequestId` 반환. 작업이 없으면 `state`는 `IDLE`, `activeRequestId`는 `null`. 원문/토큰은 반환하지 않는다.

`/api/v1/` 아래 존재하지 않는 경로도 인증을 먼저 검사한다. 토큰이 없거나 틀리면 404 대신 401을 반환한다.

## 실패와 중복

오류 응답은 `{"error":"<code>"}` 형식이다.

| 응답 | 의미 |
|---|---|
| 400 | `invalid_request_id`, `empty_body`, `unsupported_character`, `invalid_option` |
| 401 | `unauthorized` |
| 404 | `not_found` — 작업이 없거나 상태 보관 기간 만료 |
| 409 | `busy` — 다른 작업 실행/대기 중, `request_id_reused` — 동일 ID에 다른 내용 |
| 413 | `too_large` — 4,096바이트 초과 |
| 503 | `usb_not_ready` — USB HID 준비 안 됨, `history_full` — 보관 공간 부족 |

같은 ID와 같은 본문/옵션의 요청은 기존 작업을 반환하고 다시 입력하지 않는다. 최근 최대 16개 ID와 요청 해시를 RAM에 10분 보관한다. 재부팅하면 사라지므로 영구적인 exactly-once 보장은 없다. 이미 입력 중인 작업은 보존하고 보관 공간이 부족하면 새 요청을 503으로 거절한다.

응답을 받지 못했을 때 Java 송신기는 자동 재시도하지 않는다. 재실행하면 새로운 ID이므로 중복 입력될 수 있다. 대상 PC와 작업 상태를 먼저 확인한다.
