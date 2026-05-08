#include "MyPaintBrushTool.hpp"

#include "../DrawingProgram.hpp"
#include "../../MainProgram.hpp"
#include "../../CanvasComponents/MyPaintLayerCanvasComponent.hpp"
#include "../../CanvasComponents/CanvasComponentContainer.hpp"
#include "Helpers/NetworkingObjects/NetObjTemporaryPtr.decl.hpp"

#include "../../GUIStuff/ElementHelpers/TextLabelHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/RadioButtonHelpers.hpp"

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>

#ifdef HVYM_HAS_LIBMYPAINT
#include "../../Brushes/BrushPresets.hpp"
extern "C" {
#include <mypaint-brush-settings.h>
#include <mypaint-surface.h>
}

namespace {
// libmypaint takes color as HSV base values, but the InfiniPaint palette
// hands us RGBA. Standard piecewise RGB->HSV; alpha is ignored (the brush
// preset's OPAQUE/OPAQUE_MULTIPLY already controls per-dab transparency).
void apply_foreground_color_to_brush(MyPaintBrush* brush, const Vector4f& rgba) {
    const float r = rgba.x();
    const float g = rgba.y();
    const float b = rgba.z();
    const float maxC = std::max({r, g, b});
    const float minC = std::min({r, g, b});
    const float delta = maxC - minC;
    float h = 0.0f;
    if (delta > 1e-6f) {
        if (maxC == r)      h = std::fmod(((g - b) / delta) + 6.0f, 6.0f);
        else if (maxC == g) h = ((b - r) / delta) + 2.0f;
        else                h = ((r - g) / delta) + 4.0f;
        h /= 6.0f;  // 0..1
    }
    const float s = (maxC > 1e-6f) ? (delta / maxC) : 0.0f;
    const float v = maxC;
    mypaint_brush_set_base_value(brush, MYPAINT_BRUSH_SETTING_COLOR_H, h);
    mypaint_brush_set_base_value(brush, MYPAINT_BRUSH_SETTING_COLOR_S, s);
    mypaint_brush_set_base_value(brush, MYPAINT_BRUSH_SETTING_COLOR_V, v);
}
}  // namespace
#endif

MyPaintBrushTool::MyPaintBrushTool(DrawingProgram& initDrawP)
    : DrawingProgramToolBase(initDrawP) {
#ifdef HVYM_HAS_LIBMYPAINT
    brush_ = mypaint_brush_new();
    // Apply the persisted preset choice immediately so the tool is usable
    // before the first stroke (e.g. cursor-radius preview pulls from the
    // brush state). begin_stroke re-applies in case the user picks a
    // different preset between strokes.
    HVYM::Brushes::apply_preset(brush_, drawP.world.main.toolConfig.myPaintBrush.activePresetIndex);
#endif
}

MyPaintBrushTool::~MyPaintBrushTool() {
#ifdef HVYM_HAS_LIBMYPAINT
    if (brush_) mypaint_brush_unref(brush_);
#endif
}

DrawingProgramToolType MyPaintBrushTool::get_type() {
    return DrawingProgramToolType::MYPAINTBRUSH;
}

void MyPaintBrushTool::switch_tool(DrawingProgramToolType) {
    end_stroke();
}

void MyPaintBrushTool::erase_component(CanvasComponentContainer::ObjInfo* erasedComp) {
    if (objInfoBeingEdited == erasedComp) objInfoBeingEdited = nullptr;
}

void MyPaintBrushTool::tool_update() {
    if (!drawP.world.main.g.gui.cursor_obstructed())
        drawP.world.main.input.hideCursor = true;
}

void MyPaintBrushTool::draw(SkCanvas* canvas, const DrawData& drawData) {
    // Mirror BrushTool's cursor circle so the user gets the same brush-tool
    // affordance. Hardcoded radius for the M3 minimum — wires to the
    // libmypaint RADIUS_LOGARITHMIC setting in the next M3 commit.
    if (drawP.world.main.input.isTouchDevice || drawData.main->g.gui.cursor_obstructed())
        return;
    const float screenRadius = 8.0f;
    Vector2f pos = drawData.main->input.mouse.pos;
    SkPaint linePaint;
    linePaint.setAntiAlias(drawData.skiaAA);
    linePaint.setStyle(SkPaint::kStroke_Style);
    linePaint.setStrokeCap(SkPaint::kRound_Cap);
    linePaint.setStrokeWidth(0.0f);
    linePaint.setColor4f({1.0f, 1.0f, 1.0f, 1.0f});
    canvas->drawPath(SkPath::Circle(pos.x(), pos.y(), screenRadius), linePaint);
    linePaint.setColor4f({0.0f, 0.0f, 0.0f, 1.0f});
    canvas->drawPath(SkPath::Circle(pos.x(), pos.y(), screenRadius - 1.0f), linePaint);
}

void MyPaintBrushTool::gui_toolbox(Toolbar&) {
#ifdef HVYM_HAS_LIBMYPAINT
    using namespace GUIStuff;
    using namespace ElementHelpers;
    auto& gui = drawP.world.main.g.gui;
    auto& cfg = drawP.world.main.toolConfig.myPaintBrush;

    gui.new_id("mypaint brush tool", [&] {
        text_label_centered(gui, "MyPaint Brush");
        std::vector<std::pair<std::string_view, int>> options;
        const auto& presets = HVYM::Brushes::curated_presets();
        options.reserve(presets.size());
        for (int i = 0; i < static_cast<int>(presets.size()); ++i)
            options.emplace_back(std::string_view(presets[i].name), i);
        radio_button_selector<int>(gui, "preset", &cfg.activePresetIndex, options, [this, &cfg] {
            HVYM::Brushes::apply_preset(brush_, cfg.activePresetIndex);
        });
    });
#endif
}
void MyPaintBrushTool::gui_phone_toolbox(PhoneDrawingProgramScreen&) {
#ifdef HVYM_HAS_LIBMYPAINT
    using namespace GUIStuff;
    using namespace ElementHelpers;
    auto& gui = drawP.world.main.g.gui;
    auto& cfg = drawP.world.main.toolConfig.myPaintBrush;

    gui.new_id("mypaint brush tool phone", [&] {
        std::vector<std::pair<std::string_view, int>> options;
        const auto& presets = HVYM::Brushes::curated_presets();
        options.reserve(presets.size());
        for (int i = 0; i < static_cast<int>(presets.size()); ++i)
            options.emplace_back(std::string_view(presets[i].name), i);
        radio_button_selector<int>(gui, "preset", &cfg.activePresetIndex, options, [this, &cfg] {
            HVYM::Brushes::apply_preset(brush_, cfg.activePresetIndex);
        });
    });
#endif
}
void MyPaintBrushTool::right_click_popup_gui(Toolbar&, Vector2f) {}
bool MyPaintBrushTool::prevent_undo_or_redo() { return objInfoBeingEdited != nullptr; }

void MyPaintBrushTool::input_mouse_button_on_canvas_callback(const InputManager::MouseButtonCallbackArgs& button) {
    if (button.button != InputManager::MouseButton::LEFT) return;
    if (button.down && drawP.layerMan.is_a_layer_being_edited() && !objInfoBeingEdited
        && !drawP.world.main.g.gui.cursor_obstructed()) {
        if (drawP.world.main.input.pen.isDown && drawP.world.main.input.pen.pressure != 0.0f)
            currentPressure = drawP.world.main.input.pen.pressure;
        else
            currentPressure = 1.0f;
        begin_stroke(button.pos, currentPressure);
    } else if (!button.down && objInfoBeingEdited) {
        end_stroke();
    }
}

void MyPaintBrushTool::input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) {
    if (!objInfoBeingEdited) return;
    continue_stroke(motion.pos, currentPressure);
}

void MyPaintBrushTool::input_pen_axis_callback(const InputManager::PenAxisCallbackArgs& axis) {
    if (axis.axis == SDL_PEN_AXIS_PRESSURE) currentPressure = axis.value;
}

void MyPaintBrushTool::begin_stroke(const Vector2f& canvasPos, float pressure) {
#ifdef HVYM_HAS_LIBMYPAINT
    auto* container = new CanvasComponentContainer(drawP.world.netObjMan, CanvasComponentType::MYPAINTLAYER);
    container->coords = drawP.world.drawData.cam.c;
    objInfoBeingEdited = drawP.layerMan.add_component_to_layer_being_edited(container);

    // Two-step reset: mypaint_brush_reset queues a full state clear (incl.
    // accumulated position) that takes effect on the next stroke_to;
    // mypaint_brush_new_stroke clears stroke-local timers. Without _reset,
    // a new stroke linearly interpolates from the previous stroke's last
    // dab position, painting a connecting line across canvas gaps.
    // Re-apply the current preset before each stroke so a picker change
    // between strokes takes effect even if the picker callback didn't run
    // (e.g. config loaded from disk, multi-window edits). Palette color
    // overrides the preset's COLOR_H/S/V so the user's chosen foreground
    // wins regardless of which preset is active.
    HVYM::Brushes::apply_preset(brush_, drawP.world.main.toolConfig.myPaintBrush.activePresetIndex);
    apply_foreground_color_to_brush(brush_, drawP.world.main.toolConfig.globalConf.foregroundColor);

    mypaint_brush_reset(brush_);
    mypaint_brush_new_stroke(brush_);

    auto& layer = static_cast<MyPaintLayerCanvasComponent&>(container->get_comp());
    MyPaintSurface* surf = layer.surface().surface();

    mypaint_surface_begin_atomic(surf);
    mypaint_brush_stroke_to(brush_, surf,
                            canvasPos.x(), canvasPos.y(),
                            pressure, 0.0f, 0.0f,
                            0.0,  // dtime — first event of stroke (just seeds position post-reset)
                            1.0f, 0.0f, 0.0f, FALSE);
    mypaint_surface_end_atomic(surf, nullptr);
    layer.mark_dirty();
    container->commit_update(drawP);

    lastEventTime = std::chrono::steady_clock::now();
#else
    (void)canvasPos; (void)pressure;
#endif
}

void MyPaintBrushTool::continue_stroke(const Vector2f& canvasPos, float pressure) {
#ifdef HVYM_HAS_LIBMYPAINT
    auto& container = *objInfoBeingEdited->obj;
    auto& layer = static_cast<MyPaintLayerCanvasComponent&>(container.get_comp());
    const Vector2f localPos = container.coords.from_cam_space_to_this(drawP.world, canvasPos);

    const auto now = std::chrono::steady_clock::now();
    const double dtime = std::chrono::duration<double>(now - lastEventTime).count();
    lastEventTime = now;

    MyPaintSurface* surf = layer.surface().surface();
    mypaint_surface_begin_atomic(surf);
    mypaint_brush_stroke_to(brush_, surf,
                            localPos.x(), localPos.y(),
                            pressure, 0.0f, 0.0f,
                            dtime,
                            1.0f, 0.0f, 0.0f, FALSE);
    mypaint_surface_end_atomic(surf, nullptr);
    layer.mark_dirty();
    container.commit_update(drawP);
#else
    (void)canvasPos; (void)pressure;
#endif
}

void MyPaintBrushTool::end_stroke() {
    if (!objInfoBeingEdited) return;
#ifdef HVYM_HAS_LIBMYPAINT
    auto& containerPtr = objInfoBeingEdited->obj;
    containerPtr->commit_update(drawP);
    containerPtr->send_comp_update(drawP, true);
    if (containerPtr->get_world_bounds().has_value()) {
        drawP.layerMan.add_undo_place_component(objInfoBeingEdited);
    } else {
        // Stroke produced no pixels (e.g. clicked-without-dragging and the
        // first seed event drew nothing). Drop it so we don't leave a
        // zero-bounds layer in the undo stack.
        auto& components = containerPtr->parentLayer->get_layer().components;
        components->erase(components, containerPtr->objInfo);
    }
#endif
    objInfoBeingEdited = nullptr;
}
