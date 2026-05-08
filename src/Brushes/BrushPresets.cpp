#include "BrushPresets.hpp"

#ifdef HVYM_HAS_LIBMYPAINT

extern "C" {
#include <mypaint-brush-settings.h>
}

namespace HVYM::Brushes {

namespace {

// Reset every base value we touch to a known starting point so a preset
// switch on the same MyPaintBrush instance can't inherit stale state from
// the previous preset.
void reset_base_values(MyPaintBrush* b) {
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_OPAQUE, 1.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_OPAQUE_LINEARIZE, 0.9f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_OPAQUE_MULTIPLY, 1.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_HARDNESS, 0.85f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, 1.5f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_RADIUS_BY_RANDOM, 0.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_BASIC_RADIUS, 4.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS, 4.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_COLOR_H, 0.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_COLOR_S, 0.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_COLOR_V, 0.0f);
    // Wipe any pressure-input mapping curves leftover from a previous preset.
    mypaint_brush_set_mapping_n(b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, MYPAINT_BRUSH_INPUT_PRESSURE, 0);
    mypaint_brush_set_mapping_n(b, MYPAINT_BRUSH_SETTING_OPAQUE,             MYPAINT_BRUSH_INPUT_PRESSURE, 0);
    mypaint_brush_set_mapping_n(b, MYPAINT_BRUSH_SETTING_HARDNESS,           MYPAINT_BRUSH_INPUT_PRESSURE, 0);
}

// Linear pressure -> output curve. (x0,y0) at pressure 0, (x1,y1) at pressure 1.
// libmypaint adds the mapped output to the base value at draw time.
void set_linear_pressure_mapping(MyPaintBrush* b, MyPaintBrushSetting setting,
                                 float lowOffset, float highOffset) {
    mypaint_brush_set_mapping_n(b, setting, MYPAINT_BRUSH_INPUT_PRESSURE, 2);
    mypaint_brush_set_mapping_point(b, setting, MYPAINT_BRUSH_INPUT_PRESSURE, 0, 0.0f, lowOffset);
    mypaint_brush_set_mapping_point(b, setting, MYPAINT_BRUSH_INPUT_PRESSURE, 1, 1.0f, highOffset);
}

void apply_technical_pen(MyPaintBrush* b) {
    reset_base_values(b);
    // Uniform width, hard edges. No pressure variance.
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, 1.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_HARDNESS, 1.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_OPAQUE, 1.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_BASIC_RADIUS, 6.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS, 6.0f);
}

void apply_fine_inker(MyPaintBrush* b) {
    reset_base_values(b);
    // Slight pressure response in line weight; crisp edges.
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, 1.5f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_HARDNESS, 0.95f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_OPAQUE, 1.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_BASIC_RADIUS, 5.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS, 5.0f);
    set_linear_pressure_mapping(b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, -0.3f, 0.2f);
}

void apply_brush_pen(MyPaintBrush* b) {
    reset_base_values(b);
    // Heavy pressure-tapered: low pressure = very thin, high = thick.
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, 2.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_HARDNESS, 0.85f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_OPAQUE, 1.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_BASIC_RADIUS, 4.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS, 4.0f);
    set_linear_pressure_mapping(b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, -1.5f, 0.8f);
}

void apply_fine_marker(MyPaintBrush* b) {
    reset_base_values(b);
    // Sub-opaque so overlapping passes build up; soft-ish edges.
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, 1.6f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_HARDNESS, 0.6f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_OPAQUE, 0.5f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_BASIC_RADIUS, 4.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS, 4.0f);
}

void apply_broad_marker(MyPaintBrush* b) {
    reset_base_values(b);
    // Large soft brush; lower opacity for marker-style buildup.
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, 3.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_HARDNESS, 0.35f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_OPAQUE, 0.4f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_BASIC_RADIUS, 3.5f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS, 3.5f);
}

void apply_reserved(MyPaintBrush* b) {
    // PHASE1.md §4 reserves one slot for "tuning during integration".
    // Defaults to fine-inker until a real preset claims the slot.
    apply_fine_inker(b);
}

const std::vector<BrushPreset> kPresets = {
    { "Technical pen", apply_technical_pen },
    { "Fine inker",    apply_fine_inker    },
    { "Brush pen",     apply_brush_pen     },
    { "Fine marker",   apply_fine_marker   },
    { "Broad marker",  apply_broad_marker  },
    { "Reserved",      apply_reserved      },
};

}  // namespace

const std::vector<BrushPreset>& curated_presets() {
    return kPresets;
}

void apply_preset(MyPaintBrush* brush, int presetIndex) {
    if (!brush) return;
    const auto& list = curated_presets();
    if (list.empty()) return;
    const int safe = (presetIndex >= 0 && presetIndex < static_cast<int>(list.size())) ? presetIndex : 0;
    list[safe].apply(brush);
}

}  // namespace HVYM::Brushes

#endif  // HVYM_HAS_LIBMYPAINT
