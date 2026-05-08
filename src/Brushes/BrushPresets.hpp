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
struct BrushPreset {
    std::string name;
    void (*apply)(MyPaintBrush*);
};

const std::vector<BrushPreset>& curated_presets();

// Apply preset at presetIndex. Out-of-range index falls back to preset 0.
void apply_preset(MyPaintBrush* brush, int presetIndex);

}  // namespace HVYM::Brushes

#endif  // HVYM_HAS_LIBMYPAINT
