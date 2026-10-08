#!/bin/sh
# macOS 용 송신기 .app 빌드·설치.
#
# 왜 .app 인가: macOS 15+ 는 로컬 네트워크 접근에 사용자 허용이 필요하다. JDK 의 java 런처는 번들 ID 는 있지만
# NSLocalNetworkUsageDescription 이 없어 허용 알림을 띄우지 못하고 조용히 거부된다(No route to host).
# jpackage 로 .app 을 만들고 그 키를 넣으면 첫 실행 때 알림이 뜨고, 허용하면 이후 CLI 처럼 직접 실행해도 된다.
# 임시 폴더의 ad-hoc 앱은 허용 상태가 반영되지 않으므로 ~/Applications 에 설치한다.
set -eu

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
WORK=$(mktemp -d)
APP_NAME=ClipKeySender
DEST="${HOME}/Applications"
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$WORK/classes" "$WORK/input"
javac -d "$WORK/classes" "$ROOT/sender/java/ClipKeySender.java"
jar cfe "$WORK/input/clipkey-sender.jar" ClipKeySender -C "$WORK/classes" .
jpackage --type app-image --name "$APP_NAME" --input "$WORK/input" \
  --main-jar clipkey-sender.jar --main-class ClipKeySender --dest "$WORK/out"

APP="$WORK/out/$APP_NAME.app"
plutil -insert NSLocalNetworkUsageDescription \
  -string "같은 Wi-Fi의 ClipKey 보드(clipkey.local)에 클립보드 텍스트를 전송합니다." "$APP/Contents/Info.plist"

mkdir -p "$DEST"
rm -rf "$DEST/$APP_NAME.app"
cp -R "$APP" "$DEST/"
codesign --force --deep --sign - "$DEST/$APP_NAME.app"

cat <<EOF
설치됨: $DEST/$APP_NAME.app

실행 (첫 실행 때 로컬 네트워크 허용 알림이 뜨면 허용):
  export CLIPKEY_URL=http://clipkey.local CLIPKEY_TOKEN=<token>
  $DEST/$APP_NAME.app/Contents/MacOS/$APP_NAME --send
EOF
