#pragma once
#include <cstdint>

// PHASE2 Workstream C — semantic role of a layer in the comic-production
// pipeline. Each World auto-creates one of each named kind on first run;
// legacy files lazy-create them on load. DEFAULT preserves backward compat
// for layers from InfiniPaint-format and pre-Phase-2 Inkternity files.
//
// Stack order (bottom-to-top): SKETCH → COLOR → INK. The ink overlay's
// clean line work visually masks color bleed underneath, which is why
// the COLOR layer can stay raster forever (no vectorization needed) —
// the eye reads ink boundaries as the shape boundaries.
//
// Reader-mode visibility: SKETCH hidden; COLOR + INK + DEFAULT visible.
//
// Tool restrictions:
//   - SKETCH accepts only raster (libmypaint) strokes; vector tools refuse it.
//   - COLOR is permissive (raster or vector).
//   - INK is permissive but is the only target for stroke vectorization
//     (recorded libmypaint strokes here can be converted to vector paths).
enum class LayerKind : uint8_t {
    DEFAULT = 0,
    SKETCH  = 1,
    COLOR   = 2,
    INK     = 3
};
