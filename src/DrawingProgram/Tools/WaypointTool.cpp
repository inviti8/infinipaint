#include "WaypointTool.hpp"

#include "../DrawingProgram.hpp"
#include "../../MainProgram.hpp"
#include "../../World.hpp"
#include "../../CanvasComponents/WaypointCanvasComponent.hpp"
#include "../../CanvasComponents/CanvasComponentContainer.hpp"
#include "../../Waypoints/Waypoint.hpp"
#include "../../Waypoints/WaypointGraph.hpp"
#include "../../GUIStuff/ElementHelpers/TextLabelHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/TextBoxHelpers.hpp"
#include "Helpers/NetworkingObjects/NetObjTemporaryPtr.decl.hpp"
#include "Helpers/SCollision.hpp"

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>

WaypointTool::WaypointTool(DrawingProgram& initDrawP)
    : DrawingProgramToolBase(initDrawP) {}

DrawingProgramToolType WaypointTool::get_type() {
    return DrawingProgramToolType::WAYPOINT;
}

void WaypointTool::switch_tool(DrawingProgramToolType) {
    hasSelection = false;
}

void WaypointTool::erase_component(CanvasComponentContainer::ObjInfo*) {
    // The selected waypoint's marker getting erased doesn't necessarily
    // erase the waypoint itself (the canvas component is just a visual
    // proxy), so don't drop selection here. M5 doesn't actually wire
    // erase to delete the Waypoint anyway — that's a follow-up.
}

void WaypointTool::tool_update() {}

void WaypointTool::draw(SkCanvas* canvas, const DrawData& drawData) {
    // Render the framing-rect outline for the selected waypoint, in
    // current cam-space. The 4 corners are the waypoint viewport's
    // (0,0)..(W,H) rectangle, transformed waypoint-local -> world ->
    // current-cam-space via CoordSpaceHelper::from_this_to_cam_space.
    if (!hasSelection) return;
    auto wpRef = drawP.world.netObjMan.get_obj_temporary_ref_from_id<Waypoint>(selectedWaypointId);
    if (!wpRef) return;
    const auto& coords = wpRef->get_coords();
    const Vector<int32_t, 2> ws = wpRef->get_window_size();
    if (ws.x() <= 0 || ws.y() <= 0) return;

    const Vector2f corners[4] = {
        coords.from_this_to_cam_space(drawP.world, Vector2f(0.0f, 0.0f)),
        coords.from_this_to_cam_space(drawP.world, Vector2f(static_cast<float>(ws.x()), 0.0f)),
        coords.from_this_to_cam_space(drawP.world, Vector2f(static_cast<float>(ws.x()), static_cast<float>(ws.y()))),
        coords.from_this_to_cam_space(drawP.world, Vector2f(0.0f, static_cast<float>(ws.y()))),
    };

    SkPathBuilder pb;
    pb.moveTo(corners[0].x(), corners[0].y());
    for (int i = 1; i < 4; ++i) pb.lineTo(corners[i].x(), corners[i].y());
    pb.close();

    SkPaint outline;
    outline.setAntiAlias(drawData.skiaAA);
    outline.setStyle(SkPaint::kStroke_Style);
    outline.setStrokeWidth(0.0f);
    outline.setColor4f({0.88f, 0.69f, 0.25f, 1.0f});  // matches the marker fill — same gold
    canvas->drawPath(pb.detach(), outline);
}

void WaypointTool::gui_toolbox(Toolbar&) {
    using namespace GUIStuff;
    using namespace ElementHelpers;
    auto& gui = drawP.world.main.g.gui;
    gui.new_id("waypoint tool", [&] {
        text_label_centered(gui, "Waypoint");
        if (hasSelection) {
            auto wpRef = drawP.world.netObjMan.get_obj_temporary_ref_from_id<Waypoint>(selectedWaypointId);
            if (wpRef) {
                input_text_field(gui, "label", "Label", &wpRef->mutable_label());
                return;
            }
        }
        text_label(gui, "Click an existing marker, or click empty canvas to drop one.");
    });
}

void WaypointTool::gui_phone_toolbox(PhoneDrawingProgramScreen&) {
    using namespace GUIStuff;
    using namespace ElementHelpers;
    auto& gui = drawP.world.main.g.gui;
    gui.new_id("waypoint tool phone", [&] {
        if (hasSelection) {
            auto wpRef = drawP.world.netObjMan.get_obj_temporary_ref_from_id<Waypoint>(selectedWaypointId);
            if (wpRef)
                input_text_field(gui, "label", "Label", &wpRef->mutable_label());
        }
    });
}

void WaypointTool::right_click_popup_gui(Toolbar&, Vector2f) {}
bool WaypointTool::prevent_undo_or_redo() { return false; }

void WaypointTool::input_mouse_button_on_canvas_callback(const InputManager::MouseButtonCallbackArgs& button) {
    if (button.button != InputManager::MouseButton::LEFT) return;
    if (!button.down) return;
    if (drawP.world.main.g.gui.cursor_obstructed()) return;
    if (!drawP.layerMan.is_a_layer_being_edited()) return;

    // Hit-test first; clicking on an existing waypoint focuses the camera
    // there. Only fall through to dropping a new waypoint if the click
    // missed every existing marker.
    if (try_focus_existing_waypoint(button.pos)) return;
    drop_waypoint(button.pos);
}

bool WaypointTool::try_focus_existing_waypoint(const Vector2f& clickPos) {
    // Build a small click collider around the cursor — same wide-line
    // pattern EraserTool uses, with start == end so it reduces to a
    // disc. Width is matched to the marker so a click anywhere within
    // the marker's footprint registers.
    SCollision::ColliderCollection<float> cC;
    SCollision::generate_wide_line(cC, clickPos, clickPos,
                                   WaypointCanvasComponent::MARKER_RADIUS_PX * 2.0f, true);
    const auto cCWorld = drawP.world.drawData.cam.c.collider_to_world<SCollision::ColliderCollection<WorldScalar>, SCollision::ColliderCollection<float>>(cC);

    NetworkingObjects::NetObjID hitId{};
    bool hit = false;
    drawP.drawCache.traverse_bvh_run_function(cCWorld.bounds, [&](const auto& bvhNode) {
        drawP.drawCache.node_loop_components(bvhNode, [&](auto c) {
            if (hit) return;
            if (c->obj->get_comp().get_type() != CanvasComponentType::WAYPOINT) return;
            if (!c->obj->collides_with(drawP.world.drawData.cam.c, cCWorld, cC)) return;
            const auto& wpc = static_cast<const WaypointCanvasComponent&>(c->obj->get_comp());
            hitId = wpc.get_waypoint_id();
            hit = true;
        });
        return !hit;
    });
    if (!hit) return false;

    auto wpRef = drawP.world.netObjMan.get_obj_temporary_ref_from_id<Waypoint>(hitId);
    if (!wpRef) return false;  // dangling ref — treat as miss
    drawP.world.drawData.cam.smooth_move_to(drawP.world, wpRef->get_coords(), wpRef->get_window_size().cast<float>());
    selectedWaypointId = hitId;
    hasSelection = true;
    return true;
}

void WaypointTool::drop_waypoint(const Vector2f& clickPos) {
    using namespace NetworkingObjects;

    // Snapshot the current camera into a Waypoint and drop it into the
    // graph. Label is empty for now — M5-b will add a label-edit affordance.
    const CoordSpaceHelper currentCam = drawP.world.drawData.cam.c;
    const Vector<int32_t, 2> windowSize = drawP.world.main.window.size.cast<int32_t>();
    auto& nodes = drawP.world.wpGraph.get_nodes();
    auto wpIt = nodes->emplace_back_direct(nodes, std::string{}, currentCam, windowSize);
    const NetObjID newWaypointId = wpIt->obj.get_net_id();

    // Container.coords mirrors the camera at click time, so clickPos
    // (cam-space) lands at component-local clickPos as the marker
    // position — same convention as BrushStrokeCanvasComponent's
    // first point.
    auto* container = new CanvasComponentContainer(drawP.world.netObjMan, CanvasComponentType::WAYPOINT);
    container->coords = currentCam;
    auto& wpc = static_cast<WaypointCanvasComponent&>(container->get_comp());
    wpc.set_data(newWaypointId, clickPos);

    auto* objInfo = drawP.layerMan.add_component_to_layer_being_edited(container);
    container->commit_update(drawP);
    container->send_comp_update(drawP, true);
    if (container->get_world_bounds().has_value())
        drawP.layerMan.add_undo_place_component(objInfo);

    selectedWaypointId = newWaypointId;
    hasSelection = true;
}
