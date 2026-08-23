#include "Utf8.h"

#include "Utf8ComposeTable.h"

namespace {
// Look up the canonical composition of (base + combining mark), or 0 if none.
uint32_t utf8ComposePair(const uint32_t base, const uint32_t mark) {
  if (base > 0xFFFF || mark > 0xFFFF) return 0;
  int lo = 0;
  int hi = kUtf8ComposeTableSize - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const Utf8ComposeEntry& e = kUtf8ComposeTable[mid];
    if (e.base < base || (e.base == base && e.mark < mark)) {
      lo = mid + 1;
    } else if (e.base > base || (e.base == base && e.mark > mark)) {
      hi = mid - 1;
    } else {
      return e.composed;
    }
  }
  return 0;
}

bool isLookupBoundary(const uint32_t cp) {
  if (cp <= 0x7F) {
    return !((cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z'));
  }

  // Controls and Unicode whitespace.
  if ((cp >= 0x80 && cp <= 0xA0) || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A) || (cp >= 0x2028 && cp <= 0x202F) ||
      cp == 0x205F || cp == 0x3000) {
    return true;
  }

  // Latin-1 punctuation and symbols sit immediately before accented Latin.
  if (cp >= 0x00A1 && cp <= 0x00BF) return true;

  // Common punctuation and symbol blocks. Keeping this as range checks avoids
  // pulling a full Unicode-category library into constrained C3 firmware.
  if ((cp >= 0x2000 && cp <= 0x2BFF) || (cp >= 0x2E00 && cp <= 0x2E7F) || (cp >= 0x3000 && cp <= 0x303F) ||
      (cp >= 0xFE10 && cp <= 0xFE6F) || (cp >= 0x1F000 && cp <= 0x1FAFF)) {
    return true;
  }

  // Fullwidth punctuation/symbols, excluding fullwidth letters and digits.
  if ((cp >= 0xFF01 && cp <= 0xFF0F) || (cp >= 0xFF1A && cp <= 0xFF20) || (cp >= 0xFF3B && cp <= 0xFF40) ||
      (cp >= 0xFF5B && cp <= 0xFF65) || (cp >= 0xFFE0 && cp <= 0xFFEE)) {
    return true;
  }

  // Script-specific punctuation that lives beside letters rather than in a
  // dedicated punctuation block.
  return cp == 0x037E || cp == 0x0387 || (cp >= 0x055A && cp <= 0x055F) || cp == 0x0589 || cp == 0x058A ||
         cp == 0x05BE || cp == 0x05C0 || cp == 0x05C3 || cp == 0x05C6 || (cp >= 0x0609 && cp <= 0x060D) ||
         cp == 0x061B || (cp >= 0x061D && cp <= 0x061F) || (cp >= 0x066A && cp <= 0x066D) || cp == 0x06D4 ||
         (cp >= 0x0700 && cp <= 0x070D) || cp == 0x0964 || cp == 0x0965;
}

bool isLookupCoreCharacter(const uint32_t cp) {
  return cp != 0 && cp != REPLACEMENT_GLYPH && !utf8IsCombiningMark(cp) && !isLookupBoundary(cp);
}

// Below this many letters a paragraph cannot be judged by content alone: a bracketed
// one-word line of dialogue carries too little signal either way.
constexpr uint32_t kMinLettersForCjkMajority = 4;

bool isSmallKana(const uint32_t cp) {
  if (cp >= 0xFF67 && cp <= 0xFF6F) return true;  // halfwidth small katakana ｧ-ｯ
  switch (cp) {
    case 0x3041:  // ぁ
    case 0x3043:  // ぃ
    case 0x3045:  // ぅ
    case 0x3047:  // ぇ
    case 0x3049:  // ぉ
    case 0x3063:  // っ
    case 0x3083:  // ゃ
    case 0x3085:  // ゅ
    case 0x3087:  // ょ
    case 0x308E:  // ゎ
    case 0x3095:  // ゕ
    case 0x3096:  // ゖ
    case 0x30A1:  // ァ
    case 0x30A3:  // ィ
    case 0x30A5:  // ゥ
    case 0x30A7:  // ェ
    case 0x30A9:  // ォ
    case 0x30C3:  // ッ
    case 0x30E3:  // ャ
    case 0x30E5:  // ュ
    case 0x30E7:  // ョ
    case 0x30EE:  // ヮ
    case 0x30F5:  // ヵ
    case 0x30F6:  // ヶ
      return true;
    default:
      return false;
  }
}
}  // namespace

// A break must not sit just before …, — or ～.
// This keeps a doubled pair such as …… or —— together, since the second mark could
// otherwise start a line on its own.
bool utf8IsNoLineStartMark(const uint32_t cp) {
  if (isSmallKana(cp)) return true;
  switch (cp) {
    case '.':
    case ',':
    case ':':
    case ';':
    case '!':
    case '?':
    case ')':
    case ']':
    case '}':
    case 0x00BB:  // »
    case 0x2019:  // ’
    case 0x201D:  // ”
    case 0x3001:  // 、
    case 0x3002:  // 。
    case 0x3009:  // 〉
    case 0x300B:  // 》
    case 0x300D:  // 」
    case 0x300F:  // 』
    case 0x3011:  // 】
    case 0x3015:  // 〕
    case 0x3017:  // 〗
    case 0x3019:  // 〙
    case 0x301B:  // 〛
    case 0xFF01:  // ！
    case 0xFF09:  // ）
    case 0xFF0C:  // ，
    case 0xFF0E:  // ．
    case 0xFF1A:  // ：
    case 0xFF1B:  // ；
    case 0xFF1F:  // ？
    case 0xFF3D:  // ］
    case 0xFF5D:  // ｝
    case 0x2026:  // …
    case 0x2014:  // —
    case 0x301C:  // 〜
    case 0xFF5E:  // ～
    case 0x00B7:  // ·
    case 0x30FB:  // ・
    case 0x3005:  // 々
    case 0x30FC:  // ー
    case 0x301E:  // 〞
    case 0x301F:  // 〟
    case 0xFF61:  // ｡
    case 0xFF63:  // ｣
    case 0xFF64:  // ､
      return true;
    default:
      return false;
  }
}

bool utf8IsNoLineEndMark(const uint32_t cp) {
  switch (cp) {
    case '(':
    case '[':
    case '{':
    case 0x00AB:  // «
    case 0x2018:  // ‘
    case 0x201C:  // “
    case 0x3008:  // 〈
    case 0x300A:  // 《
    case 0x300C:  // 「
    case 0x300E:  // 『
    case 0x3010:  // 【
    case 0x3014:  // 〔
    case 0x3016:  // 〖
    case 0x3018:  // 〘
    case 0x301A:  // 〚
    case 0xFF08:  // （
    case 0xFF3B:  // ［
    case 0xFF5B:  // ｛
    case 0x301D:  // 〝
    case 0xFF62:  // ｢
      return true;
    default:
      return false;
  }
}

bool utf8HasCjkBreakOpportunityBetween(const uint32_t leftCp, const uint32_t rightCp) {
  if (!utf8IsCjkBreakable(leftCp) && !utf8IsCjkBreakable(rightCp)) return false;
  if (utf8IsNoLineEndMark(leftCp) || utf8IsNoLineStartMark(rightCp)) return false;
  if (utf8IsCombiningMark(rightCp) || utf8IsVariationSelector(rightCp)) return false;
  return true;
}

std::vector<size_t> utf8CjkCharacterBreakByteOffsets(const std::string& text) {
  struct CodepointBoundary {
    uint32_t cp;
    size_t endOffset;
  };

  std::vector<CodepointBoundary> codepoints;
  codepoints.reserve(text.size());
  bool hasCjkBreakable = false;

  const auto* ptr = reinterpret_cast<const unsigned char*>(text.c_str());
  const auto* const start = ptr;
  while (*ptr) {
    const uint32_t cp = utf8NextCodepoint(&ptr);
    if (cp == 0) break;
    if (utf8IsCjkBreakable(cp)) {
      hasCjkBreakable = true;
    }
    codepoints.push_back({cp, static_cast<size_t>(ptr - start)});
  }

  if (!hasCjkBreakable || codepoints.size() < 2) return {};

  std::vector<size_t> allowedOffsets;
  allowedOffsets.reserve(codepoints.size() - 1);
  for (size_t i = 0; i + 1 < codepoints.size(); ++i) {
    if (!utf8HasCjkBreakOpportunityBetween(codepoints[i].cp, codepoints[i + 1].cp)) continue;
    allowedOffsets.push_back(codepoints[i].endOffset);
  }
  return allowedOffsets;
}

bool utf8IsCjkLanguageTag(const std::string& langTag) {
  std::string primary;
  primary.reserve(langTag.size());
  for (const char c : langTag) {
    if (c == '-' || c == '_') break;
    primary.push_back(static_cast<char>((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c));
  }
  // ISO 639-2 three-letter codes are valid BCP-47 and common in EPUB 2 metadata.
  return primary == "zh" || primary == "zho" || primary == "chi" || primary == "ja" || primary == "jpn";
}

void utf8AccumulateCjkTextStats(const std::string& text, Utf8CjkTextStats& stats) {
  const auto* ptr = reinterpret_cast<const unsigned char*>(text.c_str());
  while (*ptr) {
    const uint32_t cp = utf8NextCodepoint(&ptr);
    if (cp == 0) break;
    if (!isLookupCoreCharacter(cp)) continue;
    // A numeral says nothing about script, so a date must not dilute the ratio.
    // Known limitation: fullwidth digits U+FF10-FF19, the usual numeral form in CJK
    // body text, still count. Like an untagged book, this is an accepted residual
    // case and a candidate for a follow-up.
    if (cp >= '0' && cp <= '9') continue;
    ++stats.letters;
    if (utf8IsHanOrKana(cp)) ++stats.hanOrKana;
  }
}

Utf8CjkMajority utf8ClassifyCjkMajority(const Utf8CjkTextStats& stats) {
  if (stats.letters < kMinLettersForCjkMajority) return Utf8CjkMajority::Undetermined;
  return stats.hanOrKana * 2 > stats.letters ? Utf8CjkMajority::Cjk : Utf8CjkMajority::NotCjk;
}

bool utf8IsMajorityCjkText(const std::string& text) {
  Utf8CjkTextStats stats;
  utf8AccumulateCjkTextStats(text, stats);
  return utf8ClassifyCjkMajority(stats) == Utf8CjkMajority::Cjk;
}

bool utf8IsJustifyClosingPunctuation(const uint32_t cp) {
  switch (cp) {
    case '.':
    case ',':
    case ':':
    case ';':
    case '!':
    case '?':
    case ')':
    case ']':
    case '}':
    case 0x00BB:  // »
    case 0x2019:  // ’
    case 0x201D:  // ”
    case 0x3001:  // 、
    case 0x3002:  // 。
    case 0x3009:  // 〉
    case 0x300B:  // 》
    case 0x300D:  // 」
    case 0x300F:  // 』
    case 0x3011:  // 】
    case 0x3015:  // 〕
    case 0x3017:  // 〗
    case 0x3019:  // 〙
    case 0x301B:  // 〛
    case 0x301E:  // 〞
    case 0x301F:  // 〟
    case 0xFF01:  // ！
    case 0xFF09:  // ）
    case 0xFF0C:  // ，
    case 0xFF0E:  // ．
    case 0xFF1A:  // ：
    case 0xFF1B:  // ；
    case 0xFF1F:  // ？
    case 0xFF3D:  // ］
    case 0xFF5D:  // ｝
    case 0xFF61:  // ｡
    case 0xFF63:  // ｣
    case 0xFF64:  // ､
      return true;
    default:
      return false;
  }
}

std::string utf8ComposeNfc(const std::string& in) {
  // Fast path: NFC composition can only change text that contains a combining
  // diacritical mark U+0300-036F (UTF-8 lead byte 0xCC or 0xCD). Plain ASCII and
  // already-precomposed (NFC) text -- the vast majority of words -- have none, so
  // return them untouched without walking codepoints or allocating. A 0xCD that is
  // actually a non-combining codepoint just falls through to the full pass below.
  bool maybeHasMarks = false;
  for (const unsigned char c : in) {
    if (c == 0xCC || c == 0xCD) {
      maybeHasMarks = true;
      break;
    }
  }
  if (!maybeHasMarks) return in;

  std::string out;
  out.reserve(in.size());
  const unsigned char* p = reinterpret_cast<const unsigned char*>(in.c_str());
  uint32_t base = 0;
  bool haveBase = false;
  while (*p) {
    const uint32_t cp = utf8NextCodepoint(&p);
    if (cp == 0) break;
    if (utf8IsCombiningMark(cp)) {
      const uint32_t composed = haveBase ? utf8ComposePair(base, cp) : 0;
      if (composed) {
        base = composed;  // keep accumulating further marks onto the composed char
        continue;
      }
      // No composition: flush the pending base, then emit the mark unchanged.
      if (haveBase) {
        utf8AppendCodepoint(base, out);
        haveBase = false;
      }
      utf8AppendCodepoint(cp, out);
    } else {
      if (haveBase) utf8AppendCodepoint(base, out);
      base = cp;
      haveBase = true;
    }
  }
  if (haveBase) utf8AppendCodepoint(base, out);
  return out;
}

bool utf8ContainsLookupCharacter(const char* text) {
  if (!text) return false;
  const auto* cursor = reinterpret_cast<const unsigned char*>(text);
  while (*cursor) {
    if (isLookupCoreCharacter(utf8NextCodepoint(&cursor))) return true;
  }
  return false;
}

bool utf8ContainsLookupCharacter(const std::string& text) { return utf8ContainsLookupCharacter(text.c_str()); }

std::string utf8CleanLookupWord(const std::string& text) {
  const auto* begin = reinterpret_cast<const unsigned char*>(text.c_str());
  const auto* cursor = begin;
  size_t firstCore = std::string::npos;
  size_t lastKeptEnd = 0;

  while (*cursor) {
    const auto* cpStart = cursor;
    const uint32_t cp = utf8NextCodepoint(&cursor);
    if (isLookupCoreCharacter(cp)) {
      if (firstCore == std::string::npos) firstCore = static_cast<size_t>(cpStart - begin);
      lastKeptEnd = static_cast<size_t>(cursor - begin);
    } else if (firstCore != std::string::npos && utf8IsCombiningMark(cp) &&
               static_cast<size_t>(cpStart - begin) == lastKeptEnd) {
      // A trailing mark belongs to the preceding base character. If another
      // core character follows, punctuation between them remains internal.
      lastKeptEnd = static_cast<size_t>(cursor - begin);
    }
  }

  if (firstCore == std::string::npos) return {};
  return utf8ComposeNfc(text.substr(firstCore, lastKeptEnd - firstCore));
}

int utf8CodepointLen(const unsigned char c) {
  if (c < 0x80) return 1;          // 0xxxxxxx
  if ((c >> 5) == 0x6) return 2;   // 110xxxxx
  if ((c >> 4) == 0xE) return 3;   // 1110xxxx
  if ((c >> 3) == 0x1E) return 4;  // 11110xxx
  return 1;                        // fallback for invalid
}

uint32_t utf8NextCodepoint(const unsigned char** string) {
  if (**string == 0) {
    return 0;
  }

  const unsigned char lead = **string;
  const int bytes = utf8CodepointLen(lead);
  const uint8_t* chr = *string;

  // Invalid lead byte (stray continuation byte 0x80-0xBF, or 0xFE/0xFF)
  if (bytes == 1 && lead >= 0x80) {
    (*string)++;
    return REPLACEMENT_GLYPH;
  }

  if (bytes == 1) {
    (*string)++;
    return chr[0];
  }

  // Validate continuation bytes before consuming them
  for (int i = 1; i < bytes; i++) {
    if ((chr[i] & 0xC0) != 0x80) {
      // Missing or invalid continuation byte — skip all bytes consumed so far
      *string += i;
      return REPLACEMENT_GLYPH;
    }
  }

  uint32_t cp = chr[0] & ((1 << (7 - bytes)) - 1);  // mask header bits

  for (int i = 1; i < bytes; i++) {
    cp = (cp << 6) | (chr[i] & 0x3F);
  }

  // Reject overlong encodings, surrogates, and out-of-range values
  const bool overlong = (bytes == 2 && cp < 0x80) || (bytes == 3 && cp < 0x800) || (bytes == 4 && cp < 0x10000);
  const bool surrogate = (cp >= 0xD800 && cp <= 0xDFFF);
  if (overlong || surrogate || cp > 0x10FFFF) {
    (*string)++;
    return REPLACEMENT_GLYPH;
  }

  *string += bytes;

  return cp;
}

void utf8AppendCodepoint(uint32_t cp, std::string& out) {
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

int utf8SafeTruncateBuffer(const char* buf, int len) {
  if (len <= 0) return 0;

  // Walk back past continuation bytes (10xxxxxx) to find the lead byte
  int leadPos = len - 1;
  while (leadPos > 0 && (static_cast<uint8_t>(buf[leadPos]) & 0xC0) == 0x80) {
    leadPos--;
  }

  // Determine expected length of the sequence starting at leadPos
  int expectedLen = utf8CodepointLen(static_cast<unsigned char>(buf[leadPos]));
  int actualLen = len - leadPos;

  if (actualLen < expectedLen && leadPos > 0) {
    // Incomplete UTF-8 sequence at the end — exclude it
    return leadPos;
  }
  return len;
}

size_t utf8RemoveLastChar(std::string& str) {
  if (str.empty()) return 0;
  size_t pos = str.size() - 1;
  while (pos > 0 && (static_cast<unsigned char>(str[pos]) & 0xC0) == 0x80) {
    --pos;
  }
  str.resize(pos);
  return pos;
}

// Truncate string by removing N UTF-8 characters from the end
void utf8TruncateChars(std::string& str, const size_t numChars) {
  for (size_t i = 0; i < numChars && !str.empty(); ++i) {
    utf8RemoveLastChar(str);
  }
}
