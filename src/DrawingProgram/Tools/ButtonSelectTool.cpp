#include "ButtonSelectTool.hpp"

#include "../DrawingProgram.hpp"
#include "../../MainProgram.hpp"
#include "../../World.hpp"
#include "../../Waypoints/Waypoint.hpp"
#include "../../Waypoints/WaypointGraph.hpp"
#include "../../GUIStuff/ElementHelpers/TextLabelHelpers.hpp"
#include "Helpers/NetworkingObjects/NetObjTemporaryPtr.decl.hpp"

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathEffect.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkDashPathEffect.h>

#include <algorithm>
#include <cmath>
#include <vector>

ButtonSelectTool::ButtonSelectTool(DrawingProgram& initDrawP)
    : DrawingProgramToolBase(initDrawP) {}

DrawingProgramToolType ButtonSelectTool::get_type() {
    return DrawingProgramToolType::BUTTONSELECT;
}

void ButtonSelectTool::switch_tool(DrawingProgramToolType) {
    dragging = false;
}

void ButtonSelectTool::erase_component(CanvasComponentContainer::ObjInfo*) {}
void ButtonSelectTool::tool_update() {}
void ButtonSelectTool::right_click_popup_gui(Toolbar&, Vector2f) {}
bool ButtonSelectTool::prevent_undo_or_redo() { return false; }

void ButtonSelectTool::gui_toolbox(Toolbar&) {
    using namespace GUIStuff;
    using namespace ElementHelpers;
    auto& gui = drawP.world.main.g.gui;
    gui.new_id("button select tool", [&] {
        text_label_centered(gui, "Button Select");
        if (!drawP.world.wpGraph.has_selection())
            text_label(gui, "Select a waypoint first; drag a rect on canvas to capture its skin.");
        else
            text_label(gui, "Drag a rect on canvas to capture this waypoint's skin.");
    });
}

void ButtonSelectTool::gui_phone_toolbox(PhoneDrawingProgramScreen&) {
    gui_toolbox(*static_cast<Toolbar*>(nullptr));  // shared content; safe because gui_toolbox doesn't deref t
}

void ButtonSelectTool::draw(SkCanvas* canvas, const DrawData& drawData) {
    if (!dragging) return;
    // Dashed rect outline in cam-space.
    const float x1 = std::min(dragStart.x(), dragCurrent.x());
    const float y1 = std::min(dragStart.y(), dragCurrent.y());
    const float x2 = std::max(dragStart.x(), dragCurrent.x());
    const float y2 = std::max(dragStart.y(), dragCurrent.y());

    SkPaint p;
    p.setAntiAlias(drawData.skiaAA);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(0.0f);
    const SkScalar intervals[] = {6.0f, 4.0f};
    p.setPathEffect(SkDashPathEffect::Make(intervals, 0.0f));

    p.setColor4f({0.0f, 0.0f, 0.0f, 0.8f});
    canvas->drawRect(SkRect::MakeLTRB(x1, y1, x2, y2), p);
    p.setColor4f({1.0f, 1.0f, 1.0f, 0.8f});
    p.setPathEffect(SkDashPathEffect::Make(intervals, 5.0f));
    canvas->drawRect(SkRect::MakeLTRB(x1, y1, x2, y2), p);
}

void ButtonSelectTool::input_mouse_button_on_canvas_callback(const InputManager::MouseButtonCallbackArgs& button) {
    if (button.button != InputManager::MouseButton::LEFT) return;
    if (button.down) {
        if (drawP.world.main.g.gui.cursor_obstructed()) return;
        dragging = true;
        dragStart = button.pos;
        dragCurrent = button.pos;
    } else if (dragging) {
        dragging = false;
        const Vector2f camMin(std::min(dragStart.x(), button.pos.x()),
                              std::min(dragStart.y(), button.pos.y()));
        const Vector2f camMax(std::max(dragStart.x(), button.pos.x()),
                              std::max(dragStart.y(), button.pos.y()));
        // Skip degenerate (single-click or near-zero) rects.
        if ((camMax - camMin).x() < 4.0f || (camMax - camMin).y() < 4.0f) return;
        if (!drawP.world.wpGraph.has_selection()) return;
        capture_skin_to_selected(camMin, camMax);
    }
}

void ButtonSelectTool::input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) {
    if (dragging) dragCurrent = motion.pos;
}

void ButtonSelectTool::capture_skin_to_selected(const Vector2f& camMin, const Vector2f& camMax) {
    const Vector2f rectSize = camMax - camMin;

    // Output size: rect dims, scaled down to fit MAX_SKIN_SIDE_PX while
    // preserving aspect ratio. Round to int.
    const float maxSide = std::max(rectSize.x(), rectSize.y());
    const float scale = (maxSide > MAX_SKIN_SIDE_PX) ? (static_cast<float>(MAX_SKIN_SIDE_PX) / maxSide) : 1.0f;
    const int outW = std::max(1, static_cast<int>(std::round(rectSize.x() * scale)));
    const int outH = std::max(1, static_cast<int>(std::round(rectSize.y() * scale)));

    // Render world into an offscreen raster surface. Camera is set up
    // with bounds equal to the drag rect's world projection, viewing
    // area equal to the output size — same pattern as
    // WorldScreenshot's take_screenshot_area_hw inner loop.
    SkImageInfo info = SkImageInfo::Make(outW, outH, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
    if (!surface) return;
    SkCanvas* offCanvas = surface->getCanvas();

    const auto& cam = drawP.world.drawData.cam;
    const CoordSpaceHelper& cameraCoords = cam.c;

    const WorldVec topLeft     = cameraCoords.from_space({camMin.x(), camMin.y()});
    const WorldVec topRight    = cameraCoords.from_space({camMax.x(), camMin.y()});
    const WorldVec bottomLeft  = cameraCoords.from_space({camMin.x(), camMax.y()});
    const WorldVec bottomRight = cameraCoords.from_space({camMax.x(), camMax.y()});
    const WorldVec camCenter = (topLeft + bottomRight) / WorldScalar(2);

    const WorldScalar distX = (camCenter - (topLeft + bottomLeft) * WorldScalar(0.5)).norm();
    const WorldScalar distY = (camCenter - (topLeft + topRight) * WorldScalar(0.5)).norm();
    const WorldVec vectorZoom{distX / WorldScalar(outW * 0.5), distY / WorldScalar(outH * 0.5)};
    const WorldScalar newInverseScale = (vectorZoom.x() + vectorZoom.y()) * WorldScalar(0.5);

    DrawData captureDD = drawP.world.drawData;
    captureDD.cam.set_based_on_properties(drawP.world, topLeft, newInverseScale, cameraCoords.rotation);
    captureDD.cam.set_viewing_area(Vector2f(static_cast<float>(outW), static_cast<float>(outH)));
    captureDD.takingScreenshot = true;
    captureDD.transparentBackground = true;
    captureDD.refresh_draw_optimizing_values();
    drawP.world.main.draw_world(offCanvas, drawP.world.main.world, captureDD);

    // Snapshot to SkImage. makeRasterImage forces a CPU copy so we own
    // the pixels — assigning the live surface's image would tie the
    // skin's lifetime to the surface.
    sk_sp<SkImage> snapshot = surface->makeImageSnapshot();
    if (!snapshot) return;
    sk_sp<SkImage> cpuImage = snapshot->makeRasterImage(nullptr);
    if (!cpuImage) cpuImage = snapshot;

    auto wpRef = drawP.world.netObjMan.get_obj_temporary_ref_from_id<Waypoint>(drawP.world.wpGraph.get_selected());
    if (!wpRef) return;
    wpRef->set_skin(cpuImage);
    // P0.5-LIVE-SYNC: push the new skin to already-connected
    // subscribers so the branch-overlay button artwork updates live.
    // Larger payload than the scalar fields but fires only on this
    // explicit capture action, never on continuous interaction.
    Waypoint::publish_skin_update(wpRef);
}
