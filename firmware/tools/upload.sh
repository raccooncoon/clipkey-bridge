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
P=$(port)
[ -n "$P" ] || { echo "보드 포트 없음. BOOT 누른 채 RESET 후 다시 실행" >&2; exit 1; }

if ! arduino-cli upload --fqbn "$FQBN" --port "$P" --input-dir "$BUILD" "$SKETCH"; then
  echo "자동 진입 실패. 1200bps touch 후 재시도" >&2
  python3 - "$P" <<'EOF' || true
import os, sys, termios
fd = os.open(sys.argv[1], os.O_RDWR | os.O_NONBLOCK | os.O_NOCTTY)
a = termios.tcgetattr(fd); a[4] = a[5] = termios.B1200; termios.tcsetattr(fd, termios.TCSANOW, a)
EOF
  for _ in $(seq 1 20); do [ -e /dev/cu.usbmodem1101 ] && break; sleep 0.5; done
  arduino-cli upload --fqbn "$FQBN" --port /dev/cu.usbmodem1101 --input-dir "$BUILD" "$SKETCH"
fi
echo "업로드 완료. 수동(BOOT+RESET)으로 진입했었다면 RESET 을 한 번 더 누른다."
