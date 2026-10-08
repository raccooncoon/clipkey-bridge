#!/bin/sh
# macOS 용 송신기 빌드·설치.
#
# 왜 .app 인가: macOS 15+ 는 로컬 네트워크 접근에 사용자 허용이 필요하다. JDK 의 java 런처는 번들 ID 는 있지만
# NSLocalNetworkUsageDescription 이 없어 허용 알림을 띄우지 못하고 조용히 거부된다(No route to host).
# jpackage 로 .app 을 만들고 그 키를 넣으면 첫 실행 때 알림이 뜨고, 허용하면 이후 CLI 처럼 직접 실행해도 된다.
#
# 왜 jar 를 번들 밖에 두는가: ad-hoc 서명 앱의 정체성은 실행 파일 해시이고 번들 봉인에 jar 가 포함되므로,
# jar 를 번들 안에 두면 코드를 고칠 때마다 정체성이 바뀌어 허용이 풀린다(실측). 번들은 한 번만 만들고
# jar 는 ~/Library/Application Support/ClipKey 에 두어 갱신한다. 번들을 다시 만들면 허용을 다시 받아야 한다.
#
# 사용: sh sender/macos/build-app.sh            # jar 갱신 (+ 번들이 없으면 생성)
#       sh sender/macos/build-app.sh --rebuild  # 번들도 다시 생성 (허용 알림 다시 뜸)
set -eu

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
WORK=$(mktemp -d)
APP_NAME=ClipKeySender
APP="${HOME}/Applications/${APP_NAME}.app"
JAR_DIR="${HOME}/Library/Application Support/ClipKey"
JAR="${JAR_DIR}/clipkey-sender.jar"
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$WORK/classes" "$WORK/input" "$JAR_DIR"
javac --release 17 -d "$WORK/classes" "$ROOT"/sender/java/src/clipkey/*.java
jar cfe "$WORK/input/clipkey-sender.jar" clipkey.ClipKeyApp -C "$WORK/classes" .
cp "$WORK/input/clipkey-sender.jar" "$JAR"
echo "jar 갱신: $JAR"

if [ -d "$APP" ] && [ "${1:-}" != "--rebuild" ]; then
  echo "번들 유지: $APP (허용 상태 보존)"
else
  jpackage --type app-image --name "$APP_NAME" --input "$WORK/input" \
    --main-jar clipkey-sender.jar --main-class clipkey.ClipKeyApp --dest "$WORK/out" \
    --java-options "-Xdock:name=ClipKey"
  NEW="$WORK/out/$APP_NAME.app"
  # 번들 안의 jar 대신 외부 jar 를 쓰도록 런처 설정을 바꾼다
  sed -i '' "s|^app.classpath=.*|app.classpath=$JAR|" "$NEW/Contents/app/$APP_NAME.cfg"
  rm -f "$NEW/Contents/app/clipkey-sender.jar"
  plutil -insert NSLocalNetworkUsageDescription \
    -string "같은 Wi-Fi의 ClipKey 보드(clipkey.local)에 클립보드 텍스트를 전송합니다." "$NEW/Contents/Info.plist"
  mkdir -p "$(dirname "$APP")"
  rm -rf "$APP"
  cp -R "$NEW" "$APP"
  codesign --force --deep --sign - "$APP"
  echo "번들 생성: $APP (첫 실행 때 로컬 네트워크 허용 알림이 뜨면 허용)"
fi

cat <<EOF

실행:
  open "$APP"                                    # 창 모드 (설정 창에서 주소·토큰 저장)
  "$APP/Contents/MacOS/$APP_NAME" --send         # CLI 모드 (저장된 설정 또는 CLIPKEY_URL/CLIPKEY_TOKEN)
EOF
