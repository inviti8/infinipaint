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
#include "../../GUIStuff/ElementHelpers/NumberSliderHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/LayoutHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/CheckBoxHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/ButtonHelpers.hpp"
#include "../../GUIStuff/Elements/DropDown.hpp"
#include "Helpers/NetworkingObjects/NetObjTemporaryPtr.decl.hpp"
#include "Helpers/SCollision.hpp"

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>

#include <SDL3/SDL_keyboard.h>

WaypointTool::WaypointTool(DrawingProgram& initDrawP)
    : DrawingProgramToolBase(initDrawP) {}

DrawingProgramToolType WaypointTool::get_type() {
    return DrawingProgramToolType::WAYPOINT;
}

void WaypointTool::switch_tool(DrawingProgramToolType) {
    drawP.world.wpGraph.clear_selection();
}

void WaypointTool::erase_component(CanvasComponentContainer::ObjInfo*) {
    // The selected waypoint's marker getting erased doesn't necessarily
    // erase the waypoint itself (the canvas component is just a visual
    // proxy), so don't drop selection here. M5 doesn't actually wire
    // erase to delete the Waypoint anyway — that's a follow-up.
}

void WaypointTool::tool_update() {}

void WaypointTool::draw(SkCanvas* canvas, const DrawData& drawData) {
    // Anchor for an edge endpoint or a framing-rect overlay: the screen
    // position of the waypoint's framing-rect center in current cam-space.
    const auto wp_anchor_in_cam_space = [&](const Waypoint& wp) -> Vector2f {
        const Vector<int32_t, 2> ws = wp.get_window_size();
        const Vector2f localCenter(static_cast<float>(ws.x()) * 0.5f,
                                   static_cast<float>(ws.y()) * 0.5f);
        return wp.get_coords().from_this_to_cam_space(drawP.world, localCenter);
    };

    // Faint outgoing-edge previews — PHASE1.md §5 author-mode chrome.
    // Drawn for every edge in the graph, not just those touching the
    // current selection: the visual is "the directed graph as-is".
    auto& edges = drawP.world.wpGraph.get_edges();
    if (edges) {
        SkPaint edgePaint;
        edgePaint.setAntiAlias(drawData.skiaAA);
        edgePaint.setStyle(SkPaint::kStroke_Style);
        edgePaint.setStrokeWidth(0.0f);
        edgePaint.setColor4f({0.88f, 0.69f, 0.25f, 0.45f});  // muted gold, semi-transparent
        for (auto& info : *edges) {
            auto fromRef = drawP.world.netObjMan.get_obj_temporary_ref_from_id<Waypoint>(info.obj->get_from());
            auto toRef   = drawP.world.netObjMan.get_obj_temporary_ref_from_id<Waypoint>(info.obj->get_to());
            if (!fromRef || !toRef) continue;
            const Vector2f a = wp_anchor_in_cam_space(*fromRef);
            const Vector2f b = wp_anchor_in_cam_space(*toRef);
            canvas->drawLine(a.x(), a.y(), b.x(), b.y(), edgePaint);
        }
    }

    // Selected waypoint's framing-rect outline.
    if (!drawP.world.wpGraph.has_selection()) return;
    auto wpRef = drawP.world.netObjMan.get_obj_temporary_ref_from_id<Waypoint>(drawP.world.wpGraph.get_selected());
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
        if (drawP.world.wpGraph.has_selection()) {
            auto wpRef = drawP.world.netObjMan.get_obj_temporary_ref_from_id<Waypoint>(drawP.world.wpGraph.get_selected());
            if (wpRef) {
                input_text_field(gui, "label", "Label", &wpRef->mutable_label());
                // PHASE2 M4: per-waypoint reader-mode transition speed.
                // Range 0.1× (slow) to 2× (fast); default 1× = global speed.
                slider_scalar_field<float>(gui, "transition speed", "Transition speed",
                    &wpRef->mutable_transition_speed_multiplier(),
                    Waypoint::TRANSITION_SPEED_MIN, Waypoint::TRANSITION_SPEED_MAX,
                    { .decimalPrecision = 2 });
                // PHASE2 M5: easing dropdown. The DropDown<T> widget reads
                // *(size_t*)d as the selection index, so we cast through
                // a uint8 alias of the enum (TransitionEasing values
                // 0..4 correspond to dropdown options in order).
                left_to_right_line_layout(gui, [&]() {
                    text_label(gui, "Easing");
                    gui.element<DropDown<uint8_t>>("easing dropdown",
                        reinterpret_cast<uint8_t*>(&wpRef->mutable_transition_easing()),
                        transition_easing_display_names());
                });
                // TRANSITIONS.md — transition-point flag + stop-time
                // slider (slider only renders when the flag is on).
                // Toggling the flag is just a data flip here; T6 will
                // add the multi-out confirm guard around it.
                checkbox_boolean_field(gui, "is transition", "Transition point",
                    &wpRef->mutable_is_transition(),
                    [this] { invalidate_marker_caches(); });
                if (wpRef->is_transition()) {
                    slider_scalar_field<float>(gui, "stop time", "Stop time (s)",
                        &wpRef->mutable_stop_time(),
                        Waypoint::TRANSITION_STOP_TIME_MIN, Waypoint::TRANSITION_STOP_TIME_MAX,
                        { .decimalPrecision = 1 });
                    // T6: invariant-violation prompt. Shown whenever a
                    // transition-flagged waypoint has 2+ outgoing edges
                    // (most commonly right after the user just toggled
                    // the flag on). Stateless — re-evaluates each frame
                    // from the live graph state, so navigating away and
                    // back leaves the prompt in place until acted on.
                    const auto selId = drawP.world.wpGraph.get_selected();
                    const size_t outCount = drawP.world.wpGraph.count_outgoing_edges_from(selId);
                    if (outCount >= 2) {
                        text_label(gui, "Has " + std::to_string(outCount) + " outgoing edges; transitions allow only 1.");
                        text_button(gui, "prune outgoing", "Keep first edge",
                            { .onClick = [this, selId] {
                                drawP.world.wpGraph.prune_outgoing_edges_to_first(selId);
                            }});
                        text_button(gui, "cancel transition", "Cancel transition",
                            { .onClick = [this, selId] {
                                auto ref = drawP.world.netObjMan.get_obj_temporary_ref_from_id<Waypoint>(selId);
                                if (ref) ref->set_is_transition(false);
                                invalidate_marker_caches();
                            }});
                    }
                }
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
        if (drawP.world.wpGraph.has_selection()) {
            auto wpRef = drawP.world.netObjMan.get_obj_temporary_ref_from_id<Waypoint>(drawP.world.wpGraph.get_selected());
            if (wpRef) {
                input_text_field(gui, "label", "Label", &wpRef->mutable_label());
                slider_scalar_field<float>(gui, "transition speed", "Transition speed",
                    &wpRef->mutable_transition_speed_multiplier(),
                    Waypoint::TRANSITION_SPEED_MIN, Waypoint::TRANSITION_SPEED_MAX,
                    { .decimalPrecision = 2 });
                // PHASE2 M5: easing dropdown. The DropDown<T> widget reads
                // *(size_t*)d as the selection index, so we cast through
                // a uint8 alias of the enum (TransitionEasing values
                // 0..4 correspond to dropdown options in order).
                left_to_right_line_layout(gui, [&]() {
                    text_label(gui, "Easing");
                    gui.element<DropDown<uint8_t>>("easing dropdown",
                        reinterpret_cast<uint8_t*>(&wpRef->mutable_transition_easing()),
                        transition_easing_display_names());
                });
                checkbox_boolean_field(gui, "is transition", "Transition point",
                    &wpRef->mutable_is_transition(),
                    [this] { invalidate_marker_caches(); });
                if (wpRef->is_transition()) {
                    slider_scalar_field<float>(gui, "stop time", "Stop time (s)",
                        &wpRef->mutable_stop_time(),
                        Waypoint::TRANSITION_STOP_TIME_MIN, Waypoint::TRANSITION_STOP_TIME_MAX,
                        { .decimalPrecision = 1 });
                    // T6 — see desktop variant for full notes; identical logic.
                    const auto selId = drawP.world.wpGraph.get_selected();
                    const size_t outCount = drawP.world.wpGraph.count_outgoing_edges_from(selId);
                    if (outCount >= 2) {
                        text_label(gui, "Has " + std::to_string(outCount) + " outgoing edges; transitions allow only 1.");
                        text_button(gui, "prune outgoing", "Keep first edge",
                            { .onClick = [this, selId] {
                                drawP.world.wpGraph.prune_outgoing_edges_to_first(selId);
                            }});
                        text_button(gui, "cancel transition", "Cancel transition",
                            { .onClick = [this, selId] {
                                auto ref = drawP.world.netObjMan.get_obj_temporary_ref_from_id<Waypoint>(selId);
                                if (ref) ref->set_is_transition(false);
                                invalidate_marker_caches();
                            }});
                    }
                }
            }
        }
    });
}

void WaypointTool::invalidate_marker_caches() {
    drawP.drawCache.clear_own_cached_surfaces();
}

void WaypointTool::right_click_popup_gui(Toolbar&, Vector2f) {}
bool WaypointTool::prevent_undo_or_redo() { return false; }

void WaypointTool::input_mouse_button_on_canvas_callback(const InputManager::MouseButtonCallbackArgs& button) {
    if (button.button != InputManager::MouseButton::LEFT) return;
    if (!button.down) return;
    if (drawP.world.main.g.gui.cursor_obstructed()) return;
    if (!drawP.layerMan.is_a_layer_being_edited()) return;

    // Shift+click on a waypoint creates an edge from the currently selected
    // waypoint to the clicked one. Provides a way to test edges before M6
    // lands the tree-window edge editor; remains a useful shortcut after.
    const bool shiftHeld = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
    if (shiftHeld && drawP.world.wpGraph.has_selection()) {
        if (try_create_edge_to_clicked(button.pos)) return;
    }

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
    drawP.world.wpGraph.select(hitId);
    return true;
}

bool WaypointTool::try_create_edge_to_clicked(const Vector2f& clickPos) {
    // Reuse the focus-path's hit-test (single-point wide-line collider)
    // but DON'T focus the camera. Just find the hit waypoint id and add
    // an edge.
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
            hitId = static_cast<const WaypointCanvasComponent&>(c->obj->get_comp()).get_waypoint_id();
            hit = true;
        });
        return !hit;
    });
    if (!hit) return false;
    const auto sel = drawP.world.wpGraph.get_selected();
    if (hitId == sel) return false;  // self-edge: skip

    drawP.world.wpGraph.add_edge_enforcing_invariant(sel, hitId, std::optional<std::string>{});
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

    drawP.world.wpGraph.select(newWaypointId);
}
