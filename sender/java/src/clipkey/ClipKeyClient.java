package clipkey;

import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.UUID;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** v0.1 API 클라이언트 (docs/API.md). 외부 라이브러리 없이 평면 JSON 만 다룬다. */
public final class ClipKeyClient {

    public record Job(String requestId, String state, int typed, int total, String error) {
        public boolean active() { return state.equals("WAITING") || state.equals("TYPING"); }

        static Job parse(String json) {
            return new Job(Json.string(json, "requestId"), Json.string(json, "state"),
                    Json.integer(json, "typedCharacters"), Json.integer(json, "totalCharacters"),
                    Json.string(json, "error"));
        }
    }

    public record Status(String firmwareVersion, boolean usbReady, String state) {
        static Status parse(String json) {
            return new Status(Json.string(json, "firmwareVersion"), "true".equals(Json.raw(json, "usbReady")),
                    Json.string(json, "state"));
        }
    }

    /** 보드가 4xx/5xx 로 거절한 경우. 네트워크 오류는 IOException 그대로. */
    public static final class ApiException extends IOException {
        private static final long serialVersionUID = 1L;
        public final int status;
        public final String code;

        ApiException(int status, String code) {
            super("HTTP " + status + (code == null ? "" : " " + code));
            this.status = status;
            this.code = code;
        }
    }

    private final HttpClient http = HttpClient.newBuilder()
            .connectTimeout(Duration.ofSeconds(5)).followRedirects(HttpClient.Redirect.NEVER).build();
    private final URI base;
    private final String token;

    public ClipKeyClient(String baseUrl, String token) {
        this.base = parseBase(baseUrl);
        if (token == null || token.isBlank()) throw new IllegalArgumentException("토큰이 비어 있습니다");
        this.token = token;
    }

    static URI parseBase(String baseUrl) {
        URI u;
        try { u = URI.create(baseUrl.trim()); }
        catch (IllegalArgumentException e) { throw new IllegalArgumentException("주소 형식 오류: " + baseUrl); }
        boolean ok = ("http".equals(u.getScheme()) || "https".equals(u.getScheme()))
                && u.getHost() != null && u.getUserInfo() == null && u.getQuery() == null && u.getFragment() == null
                && (u.getPath().isEmpty() || "/".equals(u.getPath()));
        if (!ok) throw new IllegalArgumentException("주소는 http(s)://host[:port] 형식");
        return u;
    }

    public Job submit(String text, boolean appendEnter, int delayMs) throws IOException, InterruptedException {
        return submit(UUID.randomUUID().toString(), text, appendEnter, delayMs);
    }

    public Job submit(String requestId, String text, boolean appendEnter, int delayMs)
            throws IOException, InterruptedException {
        HttpRequest req = request("/api/v1/type?appendEnter=" + appendEnter + "&delayMs=" + delayMs)
                .header("Content-Type", "text/plain; charset=utf-8")
                .header("X-Request-Id", requestId)
                .POST(HttpRequest.BodyPublishers.ofString(text, StandardCharsets.UTF_8)).build();
        return Job.parse(send(req, 202));
    }

    public Job job(String requestId) throws IOException, InterruptedException {
        return Job.parse(send(request("/api/v1/jobs/" + requestId).GET().build(), 200));
    }

    public Job cancel(String requestId) throws IOException, InterruptedException {
        return Job.parse(send(request("/api/v1/jobs/" + requestId + "/cancel")
                .POST(HttpRequest.BodyPublishers.noBody()).build(), 200));
    }

    public Status status() throws IOException, InterruptedException {
        return Status.parse(send(request("/api/v1/status").GET().build(), 200));
    }

    private HttpRequest.Builder request(String path) {
        return HttpRequest.newBuilder(base.resolve(path))
                .timeout(Duration.ofSeconds(10))
                .header("Authorization", "Bearer " + token);
    }

    private String send(HttpRequest req, int expected) throws IOException, InterruptedException {
        HttpResponse<String> res = http.send(req, HttpResponse.BodyHandlers.ofString(StandardCharsets.UTF_8));
        if (res.statusCode() != expected) throw new ApiException(res.statusCode(), Json.string(res.body(), "error"));
        return res.body();
    }

    /** 평면 JSON 객체에서 값 하나를 꺼낸다. 중첩·이스케이프는 이 API 에 없다. */
    static final class Json {
        private Json() {}

        static String raw(String json, String key) {
            Matcher m = Pattern.compile("\"" + Pattern.quote(key) + "\"\\s*:\\s*(\"([^\"]*)\"|[^,}\\s]+)").matcher(json);
            if (!m.find()) return null;
            return m.group(2) != null ? m.group(2) : m.group(1);
        }

        static String string(String json, String key) {
            String v = raw(json, key);
            return v == null || "null".equals(v) ? null : v;
        }

        static int integer(String json, String key) {
            String v = raw(json, key);
            return v == null ? 0 : Integer.parseInt(v);
        }
    }
}
