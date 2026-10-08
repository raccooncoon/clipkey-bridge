// 한글 → 2벌식 키 입력 변환. 대상 PC 의 IME 가 한글 모드일 때 이 키들을 치면 음절이 조합된다.
// 키 문자열은 US 배열 ASCII 이며 대문자는 Shift (ㄲ=R, ㅒ=O 등). 복모음·겹받침은 두 키로 푼다 (ㅘ=hk, ㄳ=rt).
#pragma once
#include <Arduino.h>

namespace hangul {

constexpr uint32_t SYLLABLE_BASE = 0xAC00, SYLLABLE_END = 0xD7A3;
constexpr uint32_t COMPAT_BASE = 0x3131, COMPAT_END = 0x3163;  // ㄱ..ㅣ 호환 자모

inline bool isSyllable(uint32_t cp) { return cp >= SYLLABLE_BASE && cp <= SYLLABLE_END; }
inline bool isCompatJamo(uint32_t cp) { return cp >= COMPAT_BASE && cp <= COMPAT_END; }
inline bool isHangul(uint32_t cp) { return isSyllable(cp) || isCompatJamo(cp); }

// 초성 19: ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ
static const char* const CHOSEONG[19] = {"r", "R", "s", "e", "E", "f", "a", "q", "Q", "t", "T", "d", "w", "W", "c", "z", "x", "v", "g"};
// 중성 21: ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ
static const char* const JUNGSEONG[21] = {"k", "o", "i", "O", "j", "p", "u", "P", "h", "hk", "ho", "hl", "y", "n", "nj", "np", "nl", "b", "m", "ml", "l"};
// 종성 28: (없음)ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ
static const char* const JONGSEONG[28] = {"", "r", "R", "rt", "s", "sw", "sg", "e", "f", "fr", "fa", "fq", "ft", "fx", "fv", "fg", "a", "q", "qt", "t", "T", "d", "w", "c", "z", "x", "v", "g"};
// 호환 자모 U+3131..U+3163: ㄱㄲㄳㄴㄵㄶㄷㄸㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅃㅄㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ
static const char* const COMPAT[51] = {
    "r", "R", "rt", "s", "sw", "sg", "e", "E", "f", "fr", "fa", "fq", "ft", "fx", "fv", "fg", "a", "q", "Q", "qt", "t", "T", "d", "w", "W", "c", "z", "x", "v", "g",
    "k", "o", "i", "O", "j", "p", "u", "P", "h", "hk", "ho", "hl", "y", "n", "nj", "np", "nl", "b", "m", "ml", "l"};

// 한글 코드포인트 하나를 키 문자열로. 한글이 아니면 빈 문자열.
inline String keys(uint32_t cp) {
  if (isSyllable(cp)) {
    const uint32_t i = cp - SYLLABLE_BASE;
    return String(CHOSEONG[i / 588]) + JUNGSEONG[(i % 588) / 28] + JONGSEONG[i % 28];
  }
  if (isCompatJamo(cp)) return String(COMPAT[cp - COMPAT_BASE]);
  return String();
}

// UTF-8 한 글자를 디코드하고 i 를 다음 글자로 옮긴다. 잘못된 바이트열이면 0xFFFD.
inline uint32_t decodeUtf8(const String& s, size_t& i) {
  const uint8_t b0 = s[i];
  auto cont = [&](size_t k) { return i + k < s.length() && (static_cast<uint8_t>(s[i + k]) & 0xC0) == 0x80; };
  if (b0 < 0x80) { i += 1; return b0; }
  if ((b0 & 0xE0) == 0xC0 && cont(1)) { uint32_t cp = ((b0 & 0x1F) << 6) | (s[i + 1] & 0x3F); i += 2; return cp; }
  if ((b0 & 0xF0) == 0xE0 && cont(1) && cont(2)) {
    uint32_t cp = ((b0 & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F);
    i += 3;
    return cp;
  }
  if ((b0 & 0xF8) == 0xF0 && cont(1) && cont(2) && cont(3)) { i += 4; return 0xFFFD; }
  i += 1;
  return 0xFFFD;
}

}  // namespace hangul
