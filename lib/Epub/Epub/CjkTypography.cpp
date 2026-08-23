#include "CjkTypography.h"

#include <GfxRenderer.h>

#include <cstdint>

namespace {
constexpr char IDEOGRAPHIC_SPACE_UTF8[] = "\xe3\x80\x80";
constexpr uint32_t IDEOGRAPHIC_SPACE_CODEPOINT = 0x3000;

// U+3000 is never one of a paragraph's words, so it misses the SD font's prewarmed
// advance table and every measurement costs an open/seek/read/close of the .cpfont
// file. Layout asks for it once per paragraph, so hold the last answer. A fontId is a
// hash of typeface plus point size, so a different font or a different reader font size
// is a different key and recomputes; one entry is enough because layout stays on one
// font for a whole chapter. Only a positive measurement is cached: a zero means the
// .cpfont read failed, and latching that would drop the CJK indent for the whole book
// and bake it into every section cache built afterwards.
struct CjkEmWidthCache {
  const GfxRenderer* renderer = nullptr;
  int fontId = 0;
  int width = 0;
  bool valid = false;
};
CjkEmWidthCache g_cjkEmWidthCache;
}  // namespace

int cjkEmWidth(const GfxRenderer& renderer, const int fontId) {
  if (g_cjkEmWidthCache.valid && g_cjkEmWidthCache.renderer == &renderer && g_cjkEmWidthCache.fontId == fontId) {
    return g_cjkEmWidthCache.width;
  }

  // Coverage must be checked before measuring. An SD .cpfont whose subset omits
  // U+3000 substitutes the replacement box and reports its advance as a success, so
  // the measurement alone cannot tell a real full-width em from a missing glyph.
  // Not reachable from the native test target, which links no GfxRenderer.
  if (!renderer.fontCoversCodepoint(fontId, IDEOGRAPHIC_SPACE_CODEPOINT)) {
    return 0;
  }

  const int measured = renderer.getTextWidth(fontId, IDEOGRAPHIC_SPACE_UTF8);
  if (measured <= 0) {
    return 0;
  }
  g_cjkEmWidthCache = {&renderer, fontId, measured, true};
  return measured;
}
