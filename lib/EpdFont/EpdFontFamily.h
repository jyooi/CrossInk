#pragma once
#include "EpdFont.h"

class EpdFontFamily {
 public:
  // Bitmask of text style flags carried per-word through layout and serialized in page cache.
  // Bits 0-1 select the font variant; higher bits are render-time overlays.
  enum Style : uint8_t {
    REGULAR = 0,
    BOLD = 1,
    ITALIC = 2,
    BOLD_ITALIC = 3,
    UNDERLINE = 4,
    STRIKETHROUGH = 8,
    SUP = 16,
    SUB = 32,
    SMALL_CAPS = 64,
    RUBY_CONTINUE = 128,
  };
  struct GlyphData {
    const EpdFontData* fontData;
    const EpdGlyph* glyph;
  };

  explicit EpdFontFamily(const EpdFont* regular, const EpdFont* bold = nullptr, const EpdFont* italic = nullptr,
                         const EpdFont* boldItalic = nullptr)
      : regular(regular), bold(bold), italic(italic), boldItalic(boldItalic) {}
  ~EpdFontFamily() = default;
  void getTextDimensions(const char* string, int* w, int* h, Style style = REGULAR) const;
  const EpdFontData* getData(Style style = REGULAR) const;
  GlyphData findGlyphData(uint32_t cp, Style style = REGULAR) const;
  /// Same resolution order as getGlyph(), but returns the EpdFontData of the
  /// face that supplied the glyph. Callers that read glyph bitmaps must use
  /// this pair: a styled face can resolve `cp` through `regular`, and the
  /// bitmap offsets are only valid against the face the glyph came from.
  GlyphData getGlyphData(uint32_t cp, Style style = REGULAR) const;
  const EpdGlyph* getGlyph(uint32_t cp, Style style = REGULAR) const;
  uint32_t getFallbackCodepoint(uint32_t cp, Style style = REGULAR) const;
  /// Returns true if the resolved style's font can render `cp` directly
  /// (interval coverage only — see EpdFont::hasCodepoint).
  bool hasCodepoint(uint32_t cp, Style style = REGULAR) const;
  /// Coverage probe that mirrors getGlyphData()'s face order: the styled face
  /// first, then `regular`. Use this wherever the answer decides whether a
  /// codepoint can be drawn at all, so the probe cannot report a miss for a
  /// glyph the resolver would have found on the regular face.
  bool hasCodepointInFamily(uint32_t cp, Style style = REGULAR) const;
  int8_t getKerning(uint32_t leftCp, uint32_t rightCp, Style style = REGULAR) const;
  uint32_t applyLigatures(uint32_t cp, const char*& text, Style style = REGULAR) const;

 private:
  const EpdFont* regular;
  const EpdFont* bold;
  const EpdFont* italic;
  const EpdFont* boldItalic;

  const EpdFont* getFont(Style style) const;
};
