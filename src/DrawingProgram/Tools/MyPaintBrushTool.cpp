#include "MyPaintBrushTool.hpp"

#include "../DrawingProgram.hpp"
#include "../../MainProgram.hpp"
#include "../../CanvasComponents/MyPaintLayerCanvasComponent.hpp"
#include "../../CanvasComponents/CanvasComponentContainer.hpp"
#include "Helpers/NetworkingObjects/NetObjTemporaryPtr.decl.hpp"

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>

#ifdef HVYM_HAS_LIBMYPAINT
extern "C" {
#include <mypaint-brush-settings.h>
#include <mypaint-surface.h>
}
#endif

MyPaintBrushTool::MyPaintBrushTool(DrawingProgram& initDrawP)
    : DrawingProgramToolBase(initDrawP) {
#ifdef HVYM_HAS_LIBMYPAINT
    brush_ = mypaint_brush_new();
    // M3 minimum: hardcoded ink-pen-ish settings instead of loading a .myb
    // preset. Curated preset registry + .myb loading + palette-color hookup
    // land with the brush picker UI in the next M3 commit.
    mypaint_brush_set_base_value(brush_, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, 2.5f);
    mypaint_brush_set_base_value(brush_, MYPAINT_BRUSH_SETTING_HARDNESS, 0.85f);
    mypaint_brush_set_base_value(brush_, MYPAINT_BRUSH_SETTING_OPAQUE, 1.0f);
    mypaint_brush_set_base_value(brush_, MYPAINT_BRUSH_SETTING_OPAQUE_LINEARIZE, 0.9f);
    mypaint_brush_set_base_value(brush_, MYPAINT_BRUSH_SETTING_OPAQUE_MULTIPLY, 1.0f);
    mypaint_brush_set_base_value(brush_, MYPAINT_BRUSH_SETTING_DABS_PER_BASIC_RADIUS, 4.0f);
    mypaint_brush_set_base_value(brush_, MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS, 4.0f);
    mypaint_brush_set_base_value(brush_, MYPAINT_BRUSH_SETTING_COLOR_H, 0.0f);
    mypaint_brush_set_base_value(brush_, MYPAINT_BRUSH_SETTING_COLOR_S, 0.0f);
    mypaint_brush_set_base_value(brush_, MYPAINT_BRUSH_SETTING_COLOR_V, 0.0f);
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

void MyPaintBrushTool::gui_toolbox(Toolbar&) {}
void MyPaintBrushTool::gui_phone_toolbox(PhoneDrawingProgramScreen&) {}
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
