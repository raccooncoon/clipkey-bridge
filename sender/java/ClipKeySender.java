import java.awt.Toolkit;
import java.awt.datatransfer.DataFlavor;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.UUID;

/** Java 17+, dependencies 없음. 실제 전송은 --send로 명시한다. */
public class ClipKeySender {
    public static void main(String[] args) {
        try { run(args); }
        catch (Exception e) {
            System.err.println("실패: " + e.getMessage());
            System.exit(1);
        }
    }

    private static void run(String[] args) throws Exception {
        boolean send = false, enter = false;
        int delay = 20;
        String text = null;
        for (int i = 0; i < args.length; i++) {
            switch (args[i]) {
                case "--send" -> send = true;
                case "--enter" -> enter = true;
                case "--text" -> {
                    if (++i >= args.length) throw new IllegalArgumentException("--text 값 필요");
                    text = args[i];
                }
                case "--delay-ms" -> {
                    if (++i >= args.length) throw new IllegalArgumentException("--delay-ms 값 필요");
                    delay = Integer.parseInt(args[i]);
                }
                case "--help" -> {
                    System.out.println("java ClipKeySender.java [--text TEXT] [--send] [--enter] [--delay-ms 10..100]");
                    return;
                }
                default -> throw new IllegalArgumentException("알 수 없는 옵션: " + args[i]);
            }
        }
        if (delay < 10 || delay > 100) throw new IllegalArgumentException("간격은 10~100ms");
        if (text == null) {
            text = (String) Toolkit.getDefaultToolkit().getSystemClipboard().getData(DataFlavor.stringFlavor);
        }
        text = text.replace("\r\n", "\n").replace('\r', '\n');
        if (text.isEmpty() || text.length() > 4096) throw new IllegalArgumentException("텍스트는 1~4096자");
        for (int i = 0; i < text.length(); i++) {
            char c = text.charAt(i);
            if ((c < 32 || c > 126) && c != '\n' && c != '\t') {
                throw new IllegalArgumentException("영문 ASCII, LF, Tab만 허용 (위치 " + i + ")");
            }
        }
        System.out.println("문자: " + text.length() + ", 추가 Enter: " + enter + ", 간격: " + delay + "ms");
        System.out.println("--- 미리보기 ---\n" + text.replace("\t", "[TAB]") + "\n--- 끝 ---");
        if (!send) {
            System.out.println("미리보기만 완료. 전송하려면 --send 사용");
            return;
        }
        String base = System.getenv("CLIPKEY_URL");
        String token = System.getenv("CLIPKEY_TOKEN");
        if (base == null || token == null || token.isBlank()) {
            throw new IllegalArgumentException("CLIPKEY_URL / CLIPKEY_TOKEN 환경변수 필요");
        }
        URI device = URI.create(base);
        if (!("http".equals(device.getScheme()) || "https".equals(device.getScheme()))
                || device.getHost() == null || device.getUserInfo() != null
                || device.getQuery() != null || device.getFragment() != null
                || !(device.getPath().isEmpty() || "/".equals(device.getPath()))) {
            throw new IllegalArgumentException("CLIPKEY_URL은 http(s)://host[:port] 형식");
        }
        String id = UUID.randomUUID().toString();
        URI endpoint = device.resolve("/api/v1/type?appendEnter=" + enter + "&delayMs=" + delay);
        HttpRequest request = HttpRequest.newBuilder(endpoint)
                .timeout(Duration.ofSeconds(10))
                .header("Authorization", "Bearer " + token)
                .header("Content-Type", "text/plain; charset=utf-8")
                .header("X-Request-Id", id)
                .POST(HttpRequest.BodyPublishers.ofString(text, StandardCharsets.UTF_8)).build();
        System.out.println("요청 ID: " + id);
        try {
            HttpResponse<Void> response = HttpClient.newBuilder()
                    .connectTimeout(Duration.ofSeconds(5)).followRedirects(HttpClient.Redirect.NEVER)
                    .build().send(request, HttpResponse.BodyHandlers.discarding());
            if (response.statusCode() != 202) {
                throw new IllegalStateException("HTTP " + response.statusCode() + ". 자동 재시도하지 않습니다.");
            }
            System.out.println("작업 등록 완료. 보드 버튼을 눌러 입력 승인하세요. 입력 완료 응답은 아닙니다.");
        } catch (java.io.IOException e) {
            throw new IllegalStateException("응답 확인 불가 (" + e.getClass().getSimpleName() + ": " + e.getMessage()
                    + "). 자동 재시도하지 않습니다. 대상 입력과 요청 ID를 확인하세요.");
        }
    }
}
