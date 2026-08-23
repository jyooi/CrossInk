#include "CjkTypography.h"

#include <GfxRenderer.h>

namespace {
constexpr char IDEOGRAPHIC_SPACE_UTF8[] = "\xe3\x80\x80";

// U+3000 is never one of a paragraph's words, so it misses the SD font's prewarmed
// advance table and every measurement costs an open/seek/read/close of the .cpfont
// file. Layout asks for it once per paragraph, so hold the last answer. A fontId is a
// hash of typeface plus point size, so a different font or a different reader font size
// is a different key and recomputes; one entry is enough because layout stays on one
// font for a whole chapter.
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

  const int measured = renderer.getTextWidth(fontId, IDEOGRAPHIC_SPACE_UTF8);
  const int width = measured > 0 ? measured : 0;
  g_cjkEmWidthCache = {&renderer, fontId, width, true};
  return width;
}
