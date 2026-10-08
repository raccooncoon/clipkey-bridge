package clipkey;

import java.nio.charset.StandardCharsets;
import java.text.Normalizer;

/** 전송 텍스트 정규화와 검증. 펌웨어(docs/API.md)와 같은 규칙: ASCII·LF·Tab·한글, UTF-8 4,096바이트. */
public final class ClipKeyText {
    public static final int MAX_BYTES = 4096;

    private ClipKeyText() {}

    /** CRLF→LF, NFC(macOS 파일명 등의 분해 자모를 음절로). */
    public static String normalize(String text) {
        return Normalizer.normalize(text.replace("\r\n", "\n").replace('\r', '\n'), Normalizer.Form.NFC);
    }

    public static boolean isHangul(int cp) {
        return (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0x3131 && cp <= 0x3163);
    }

    public static boolean isTypable(int cp) {
        return (cp >= 32 && cp <= 126) || cp == '\n' || cp == '\t' || isHangul(cp);
    }

    public static int chars(String text) { return text.codePointCount(0, text.length()); }

    /** 유효하면 null, 아니면 사용자에게 보여줄 사유. */
    public static String problem(String text) {
        if (text.isEmpty()) return "텍스트가 비어 있습니다";
        int bytes = text.getBytes(StandardCharsets.UTF_8).length;
        if (bytes > MAX_BYTES) return "최대 " + MAX_BYTES + "바이트 (" + bytes + "바이트, 한글은 3바이트)";
        int[] cps = text.codePoints().toArray();
        for (int i = 0; i < cps.length; i++) {
            if (!isTypable(cps[i])) {
                return "영문·숫자·기호, 줄바꿈, Tab, 한글만 허용 (위치 " + i + ": '" + new String(Character.toChars(cps[i])) + "')";
            }
        }
        return null;
    }

    public static String preview(String text) {
        return text.replace("\t", "[TAB]");
    }
}
