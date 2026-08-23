#pragma once

class GfxRenderer;

// Width in pixels of one CJK em, meaning one full-width character. Measured from
// U+3000 IDEOGRAPHIC SPACE, which every CJK font gives the same advance as any other
// full-width glyph. A Latin "em" derived from the font ascender is narrower, so the
// two indent paths must use this instead when the book is CJK.
// Returns 0 when neither the font nor its CJK fallback covers U+3000, and when the
// measurement fails; callers must then keep their non-CJK fallback rather than indent
// by nothing.
// Known limitation: a zero does not always mean the font lacks CJK coverage. Only the
// SD-font path with a live advance table sums advances. Every other path falls through
// to EpdFontFamily::getTextDimensions, an ink-extent bounding box, and U+3000 has no
// ink, so it measures 0 even for a font with a perfectly good full-width glyph. That
// silently drops the CJK indent for a built-in CJK font, and for an SD font whose
// advance table failed to allocate. This is an accepted residual case and a candidate
// for a follow-up: measure a visible full-width character, or use an advance-based
// query such as getTextAdvanceX, which sums advances on both paths.
int cjkEmWidth(const GfxRenderer& renderer, int fontId);
