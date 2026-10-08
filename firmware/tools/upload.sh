#!/bin/sh
# 펌웨어 컴파일 + 업로드. 사용: firmware/tools/upload.sh clipkey_bridge [cdc|default]
#
# CDC 빌드가 올라가 있으면 arduino-cli 가 1200bps touch 로 다운로드 모드에 넣는다. 포트가 바뀌는 사이
# "No serial data received" 로 실패하는 경우가 있어, 그때는 직접 touch 한 뒤 나타나는 ROM 포트로 다시 올린다.
set -eu

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
SKETCH="$ROOT/firmware/${1:?sketch 이름 필요 (예: clipkey_bridge)}"
CDC=${2:-cdc}
FQBN="esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=$CDC"
BUILD="${TMPDIR:-/tmp}/clipkey-build-$1"

arduino-cli compile --fqbn "$FQBN" --build-path "$BUILD" "$SKETCH"

port() { ls /dev/cu.usbmodem* 2>/dev/null | head -1; }
ROM=/dev/cu.usbmodem1101   # ROM 다운로드 모드일 때의 포트 이름

# 플래시 후 보드가 리셋되며 포트가 사라지면 esptool 이 마지막 리셋 단계에서 실패로 끝나기도 한다.
# 모든 구간을 쓰고 검증했으면("Hard resetting") 성공으로 본다.
flash() {
  out=$(arduino-cli upload --fqbn "$FQBN" --port "$1" --input-dir "$BUILD" "$SKETCH" 2>&1)
  echo "$out" | grep -E "Hash of data verified|Hard resetting|rror|fatal"
  echo "$out" | grep -q "Hard resetting"
}

P=$(port)
[ -n "$P" ] || { echo "보드 포트 없음. BOOT 누른 채 RESET 후 다시 실행" >&2; exit 1; }
if [ "$P" != "$ROM" ]; then
  # 앱(CDC) 포트: 1200bps touch 로 다운로드 모드에 넣는다. arduino-cli 의 자동 touch 는 포트 교체 타이밍에 실패하는 일이 있어 직접 한다.
  python3 - "$P" <<'PY' 2>/dev/null || true
import os, sys, termios
fd = os.open(sys.argv[1], os.O_RDWR | os.O_NONBLOCK | os.O_NOCTTY)
a = termios.tcgetattr(fd); a[4] = a[5] = termios.B1200; termios.tcsetattr(fd, termios.TCSANOW, a)
PY
  for _ in $(seq 1 30); do [ -e "$ROM" ] && break; sleep 0.5; done
  [ -e "$ROM" ] || { echo "다운로드 모드 진입 실패. BOOT 누른 채 RESET 후 다시 실행" >&2; exit 1; }
fi
flash "$ROM"
echo "업로드 완료. 수동(BOOT+RESET)으로 진입했었다면 RESET 을 한 번 더 누른다."
