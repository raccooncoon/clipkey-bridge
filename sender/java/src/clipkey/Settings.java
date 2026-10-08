package clipkey;

import java.util.prefs.Preferences;

/** 장치 주소·토큰·옵션. macOS 는 ~/Library/Preferences 의 사용자 설정에 저장되며 Git 과 무관하다. */
public final class Settings {
    private static final Preferences PREFS = Preferences.userRoot().node("clipkey-bridge");

    private Settings() {}

    public static String url() { return PREFS.get("url", "http://clipkey.local"); }
    public static String token() { return PREFS.get("token", ""); }
    public static int delayMs() { return PREFS.getInt("delayMs", 20); }
    public static boolean appendEnter() { return PREFS.getBoolean("appendEnter", false); }
    public static boolean autoStart() { return PREFS.getBoolean("autoStart", false); }
    /** 한/영 전환 키: lang1/ralt(Windows), capslock/ctrlspace(macOS). 대상 PC 설정에 따른다. */
    public static String imeToggle() { return PREFS.get("imeToggle", "lang1"); }

    public static void save(String url, String token, int delayMs, boolean appendEnter, boolean autoStart, String imeToggle) {
        PREFS.put("url", url.trim());
        PREFS.put("token", token.trim());
        PREFS.putInt("delayMs", delayMs);
        PREFS.putBoolean("appendEnter", appendEnter);
        PREFS.putBoolean("autoStart", autoStart);
        PREFS.put("imeToggle", imeToggle);
    }

    public static boolean configured() {
        return !url().isBlank() && !token().isBlank();
    }
}
