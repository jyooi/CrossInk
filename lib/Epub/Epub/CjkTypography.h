#pragma once

class GfxRenderer;

// Width in pixels of one CJK em, meaning one full-width character. Measured from
// U+3000 IDEOGRAPHIC SPACE, which every CJK font gives the same advance as any other
// full-width glyph. A Latin "em" derived from the font ascender is narrower, so the
// two indent paths must use this instead when the book is CJK.
// Returns 0 when neither the font nor its CJK fallback covers U+3000, and when the
// measurement fails; callers must then keep their non-CJK fallback rather than indent
// by nothing.
int cjkEmWidth(const GfxRenderer& renderer, int fontId);
