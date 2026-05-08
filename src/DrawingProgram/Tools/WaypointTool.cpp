#include "WaypointTool.hpp"

#include "../DrawingProgram.hpp"
#include "../../MainProgram.hpp"
#include "../../World.hpp"
#include "../../CanvasComponents/WaypointCanvasComponent.hpp"
#include "../../CanvasComponents/CanvasComponentContainer.hpp"
#include "../../Waypoints/Waypoint.hpp"
#include "../../Waypoints/WaypointGraph.hpp"
#include "Helpers/NetworkingObjects/NetObjTemporaryPtr.decl.hpp"
#include "Helpers/SCollision.hpp"

WaypointTool::WaypointTool(DrawingProgram& initDrawP)
    : DrawingProgramToolBase(initDrawP) {}

DrawingProgramToolType WaypointTool::get_type() {
    return DrawingProgramToolType::WAYPOINT;
}

void WaypointTool::switch_tool(DrawingProgramToolType) {}

void WaypointTool::erase_component(CanvasComponentContainer::ObjInfo*) {}

void WaypointTool::tool_update() {}

void WaypointTool::draw(SkCanvas*, const DrawData&) {}

void WaypointTool::gui_toolbox(Toolbar&) {}
void WaypointTool::gui_phone_toolbox(PhoneDrawingProgramScreen&) {}
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
}
