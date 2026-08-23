#include "CjkTypography.h"

#include <GfxRenderer.h>

namespace {
constexpr char IDEOGRAPHIC_SPACE_UTF8[] = "\xe3\x80\x80";
}  // namespace

int cjkEmWidth(const GfxRenderer& renderer, const int fontId) {
  const int width = renderer.getTextWidth(fontId, IDEOGRAPHIC_SPACE_UTF8);
  return width > 0 ? width : 0;
}
