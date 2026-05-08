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
    // Hard edges with very slight pressure variance — much subtler than
    // Fine inker (-0.3..+0.2) or Brush pen (-1.5..+0.8). Just enough to
    // give the line some life without losing the technical-pen feel.
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, 1.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_HARDNESS, 1.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_OPAQUE, 1.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_BASIC_RADIUS, 6.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS, 6.0f);
    set_linear_pressure_mapping(b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, -2.0f, 1.0f);
}

void apply_fine_inker(MyPaintBrush* b) {
    reset_base_values(b);
    // Crisper than the technical pen (max hardness, no opacity falloff)
    // with about half the pressure variance. Same line-weight purpose,
    // less hand-drawn looseness.
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, 1.5f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_HARDNESS, 1.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_OPAQUE, 1.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_BASIC_RADIUS, 5.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS, 5.0f);
    set_linear_pressure_mapping(b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, -1.0f, 0.5f);
    // Lighter pressure also drops opacity a bit, so faint pen-touches
    // leave a faint mark. Range chosen so a 0.5 base opacity still has
    // a visible variance and a 1.0 base only fades on the lightest taps.
    set_linear_pressure_mapping(b, MYPAINT_BRUSH_SETTING_OPAQUE, -0.4f, 0.0f);
}

void apply_brush_pen(MyPaintBrush* b) {
    reset_base_values(b);
    // Heavy pressure-tapered: low pressure = very thin, high = thick.
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, 2.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_HARDNESS, 0.85f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_OPAQUE, 1.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_BASIC_RADIUS, 4.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS, 4.0f);
    // Extreme radius pressure response — log-radius range of 5.3 means
    // ~40x diameter variance from lightest tap to full pressure. Lightest
    // touches are sub-pixel hairlines, heavy presses go fat-brush.
    set_linear_pressure_mapping(b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, -3.5f, 1.8f);
}

void apply_fine_marker(MyPaintBrush* b) {
    reset_base_values(b);
    // Soft marker tip with light tactile pressure response — pressing
    // harder squeezes out more pigment and flexes the tip slightly,
    // so it feels less mechanical without going wet/blendy (which
    // PHASE1.md §4 explicitly excludes).
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, 1.6f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_HARDNESS, 0.6f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_OPAQUE, 0.7f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_BASIC_RADIUS, 4.0f);
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS, 4.0f);
    set_linear_pressure_mapping(b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, -0.4f, 0.3f);
    set_linear_pressure_mapping(b, MYPAINT_BRUSH_SETTING_OPAQUE, -0.3f, 0.0f);
    // Slight per-dab radius jitter for a textured felt-tip edge.
    mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_RADIUS_BY_RANDOM, 0.15f);
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

// defaults must mirror the values set by each apply_* function below; the
// slider UI uses these to seed the per-preset overrides on first use.
const std::vector<BrushPreset> kPresets = {
    { "Technical pen", "data/icons/technical-pen.svg", { 1.0f, 1.00f, 1.0f }, apply_technical_pen },
    { "Fine inker",    "data/icons/fine-inker.svg",    { 1.5f, 1.00f, 1.0f }, apply_fine_inker    },
    { "Brush pen",     "data/icons/brush-pen.svg",     { 2.0f, 0.85f, 1.0f }, apply_brush_pen     },
    { "Fine marker",   "data/icons/fine-marker.svg",   { 1.6f, 0.60f, 0.7f }, apply_fine_marker   },
    { "Broad marker",  "data/icons/broad-marker.svg",  { 3.0f, 0.35f, 0.4f }, apply_broad_marker  },
    { "Reserved",      "data/icons/reserved.svg",      { 1.5f, 1.00f, 1.0f }, apply_reserved      },
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

void apply_tunable_overrides(MyPaintBrush* brush, float diameter, float hardness, float opacity) {
    if (!brush) return;
    mypaint_brush_set_base_value(brush, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, diameter);
    mypaint_brush_set_base_value(brush, MYPAINT_BRUSH_SETTING_HARDNESS, hardness);
    mypaint_brush_set_base_value(brush, MYPAINT_BRUSH_SETTING_OPAQUE, opacity);
}

}  // namespace HVYM::Brushes

#endif  // HVYM_HAS_LIBMYPAINT
