package clipkey;

/** 전송 텍스트 정규화와 검증. 펌웨어(docs/API.md)와 같은 규칙. */
public final class ClipKeyText {
    public static final int MAX_CHARS = 4096;

    private ClipKeyText() {}

    public static String normalize(String text) {
        return text.replace("\r\n", "\n").replace('\r', '\n');
    }

    /** 유효하면 null, 아니면 사용자에게 보여줄 사유. */
    public static String problem(String text) {
        if (text.isEmpty()) return "텍스트가 비어 있습니다";
        if (text.length() > MAX_CHARS) return "최대 " + MAX_CHARS + "자 (" + text.length() + "자)";
        for (int i = 0; i < text.length(); i++) {
            char c = text.charAt(i);
            if ((c < 32 || c > 126) && c != '\n' && c != '\t') {
                return "영문 ASCII, LF, Tab만 허용 (위치 " + i + ": '" + c + "')";
            }
        }
        return null;
    }

    public static String preview(String text) {
        return text.replace("\t", "[TAB]");
    }
}
