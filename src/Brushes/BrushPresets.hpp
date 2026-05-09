#pragma once

#include <string>
#include <vector>

#ifdef HVYM_HAS_LIBMYPAINT

extern "C" {
#include <mypaint-brush.h>
}

namespace HVYM::Brushes {

// PHASE1.md §4 — curated ink/marker preset list. Wet/oily/watercolor/smudge/
// charcoal/bristle presets are explicitly out of scope for Phase 1; only the
// six entries below ever appear in the brush picker.
//
// M3 minimum: each preset's `apply` configures a MyPaintBrush via base
// values + (where useful) pressure-input mapping points. Loading the same
// configurations from external .myb JSON files is a follow-up — the public
// shape (BrushPreset list with name + apply function) is stable across
// that swap.
// User-tunable subset of MyPaintBrush settings, surfaced as sliders in the
// brush picker. Each preset declares its canonical defaults; the tool layer
// stores per-preset overrides separately and writes them on top of apply()
// at stroke start (see MyPaintBrushTool::begin_stroke).
struct BrushPresetDefaults {
    float diameter;  // MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC
    float hardness;  // MYPAINT_BRUSH_SETTING_HARDNESS
    float opacity;   // MYPAINT_BRUSH_SETTING_OPAQUE
};

// PHASE2 M3 follow-up: brush family. SHARP brushes (technical pen, fine
// inker, brush pen) render as crisp, uniform-color lines and convert
// cleanly to vector via StrokeVectorizeTool. TEXTURED brushes (fine
// marker, broad marker, wet ink) carry the wet/blotchy/spatter look as
// emergent properties of per-dab randomness — the vector representation
// can't reproduce that, so recording is intentionally suppressed for
// TEXTURED strokes (has_valid_recording stays false; vectorize tool
// skips them).
enum class BrushCategory : uint8_t {
    SHARP    = 0,
    TEXTURED = 1
};

struct BrushPreset {
    std::string name;
    std::string iconPath;  // svg under data/icons/, e.g. "data/icons/technical-pen.svg"
    BrushPresetDefaults defaults;
    void (*apply)(MyPaintBrush*);
    BrushCategory category;
};

const std::vector<BrushPreset>& curated_presets();

// Apply preset at presetIndex. Out-of-range index falls back to preset 0.
void apply_preset(MyPaintBrush* brush, int presetIndex);

// Write the three tunable values onto an already-apply()'d brush. Used by
// the tool to layer per-preset slider overrides on top of canonical
// defaults at stroke start.
void apply_tunable_overrides(MyPaintBrush* brush, float diameter, float hardness, float opacity);

}  // namespace HVYM::Brushes

#endif  // HVYM_HAS_LIBMYPAINT
