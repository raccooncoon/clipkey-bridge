package clipkey;

import java.awt.Toolkit;
import java.awt.datatransfer.DataFlavor;
import java.io.IOException;

/**
 * CLI. 실제 전송은 --send 로 명시한다. 주소/토큰은 CLIPKEY_URL / CLIPKEY_TOKEN 환경변수, 없으면 앱 설정값.
 * macOS 에서는 로컬 네트워크 권한 때문에 .app 번들로 실행해야 한다 (sender/macos/build-app.sh).
 */
public final class ClipKeySender {
    public static void main(String[] args) {
        try { run(args); }
        catch (Exception e) {
            System.err.println("실패: " + e.getMessage());
            System.exit(1);
        }
    }

    private static void run(String[] args) throws Exception {
        boolean send = false, enter = Settings.appendEnter(), auto = Settings.autoStart();
        int delay = Settings.delayMs();
        String ime = Settings.imeToggle();
        String text = null;
        for (int i = 0; i < args.length; i++) {
            switch (args[i]) {
                case "--send" -> send = true;
                case "--enter" -> enter = true;
                case "--auto" -> auto = true;
                case "--ime" -> {
                    if (++i >= args.length) throw new IllegalArgumentException("--ime 값 필요 (lang1|ralt)");
                    ime = args[i];
                }
                case "--text" -> {
                    if (++i >= args.length) throw new IllegalArgumentException("--text 값 필요");
                    text = args[i];
                }
                case "--delay-ms" -> {
                    if (++i >= args.length) throw new IllegalArgumentException("--delay-ms 값 필요");
                    delay = Integer.parseInt(args[i]);
                }
                case "--help" -> {
                    System.out.println("ClipKeySender [--text TEXT] [--send] [--enter] [--auto] [--delay-ms 10..100] [--ime lang1|ralt]");
                    return;
                }
                default -> throw new IllegalArgumentException("알 수 없는 옵션: " + args[i]);
            }
        }
        if (delay < 10 || delay > 100) throw new IllegalArgumentException("간격은 10~100ms");
        if (text == null) {
            text = (String) Toolkit.getDefaultToolkit().getSystemClipboard().getData(DataFlavor.stringFlavor);
        }
        text = ClipKeyText.normalize(text);
        String problem = ClipKeyText.problem(text);
        if (problem != null) throw new IllegalArgumentException(problem);

        System.out.println("문자: " + ClipKeyText.chars(text) + ", 추가 Enter: " + enter + ", 자동 시작: " + auto + ", 간격: " + delay + "ms, 한/영: " + ime);
        System.out.println("--- 미리보기 ---\n" + ClipKeyText.preview(text) + "\n--- 끝 ---");
        if (!send) {
            System.out.println("미리보기만 완료. 전송하려면 --send 사용");
            return;
        }
        String base = envOr("CLIPKEY_URL", Settings.url());
        String token = envOr("CLIPKEY_TOKEN", Settings.token());
        if (base.isBlank() || token.isBlank()) {
            throw new IllegalArgumentException("CLIPKEY_URL / CLIPKEY_TOKEN 환경변수 또는 앱 설정 필요");
        }
        ClipKeyClient client = new ClipKeyClient(base, token);
        try {
            ClipKeyClient.Job job = client.submit(text, enter, auto, delay, ime);
            System.out.println("요청 ID: " + job.requestId());
            System.out.println(auto ? "작업 등록 완료. 2초 뒤 자동 입력합니다. 그 사이 보드 버튼은 취소입니다."
                                    : "작업 등록 완료. 보드 버튼을 눌러 입력 승인하세요. 입력 완료 응답은 아닙니다.");
        } catch (ClipKeyClient.ApiException e) {
            throw new IllegalStateException(e.getMessage() + ". 자동 재시도하지 않습니다.");
        } catch (IOException e) {
            throw new IllegalStateException("응답 확인 불가 (" + e.getClass().getSimpleName() + ": " + e.getMessage()
                    + "). 자동 재시도하지 않습니다. 대상 입력과 요청 ID를 확인하세요.");
        }
    }

    private static String envOr(String name, String fallback) {
        String v = System.getenv(name);
        return v == null || v.isBlank() ? fallback : v;
    }
}
