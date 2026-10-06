# GitHub에 저장소 만들기

권장 이름: `clipkey-bridge` / 초기 공개 범위: Private.

이 패키지를 풀고 폴더에서 실행한다. GitHub CLI 설치와 로그인이 되어 있어야 한다.

```bash
cd clipkey-bridge
git init -b main
git add .
git commit -m "chore: scaffold ClipKey Bridge project"
gh repo create clipkey-bridge --private --source=. --remote=origin --push
```

Git 사용자 정보가 없다는 오류가 나오면 본인의 이름과 GitHub 등록 이메일을 해당 저장소의 `git config user.name`, `git config user.email`로 설정한 후 커밋한다.

CLI 없이 진행하려면 GitHub 웹에서 이름이 clipkey-bridge인 빈 Private 저장소를 생성한다. README/.gitignore/라이선스 자동 생성은 선택하지 않는다. GitHub가 보여주는 실제 주소를 origin으로 지정한 뒤 push한다.

## 저장소 설명

Wi-Fi clipboard to USB HID keyboard bridge powered by ESP32-S3, with a Java sender.

## 첫 작업

1. ESP32-S3 Native USB/보드 핀 확인
2. 버튼 트리거 USB HID Hello World
3. Wi-Fi 작업 API와 버튼 승인
4. Java 클립보드 CLI 통합 검증

라이선스 결정 전에는 공개 배포 라이선스를 자동으로 추가하지 않는다.
