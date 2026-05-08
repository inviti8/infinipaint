#include "ReaderMode.hpp"
#include "../World.hpp"
#include "../MainProgram.hpp"
#include "../Waypoints/Waypoint.hpp"
#include "../Waypoints/Edge.hpp"
#include "../Waypoints/WaypointGraph.hpp"
#include <Helpers/NetworkingObjects/NetObjTemporaryPtr.decl.hpp>

ReaderMode::ReaderMode(World& w) : world(w) {}

void ReaderMode::toggle() {
    set_active(!active);
}

void ReaderMode::set_active(bool a) {
    if (active == a) return;
    active = a;
    if (active) {
        // Pick a starting waypoint: existing selection if any, else the
        // first node in the graph. Reader mode with zero waypoints is a
        // valid (if empty) state — currentId stays nullopt.
        std::optional<NetworkingObjects::NetObjID> startId;
        if (world.wpGraph.has_selection())
            startId = world.wpGraph.get_selected();
        else if (world.wpGraph.get_nodes() && !world.wpGraph.get_nodes()->empty())
            startId = world.wpGraph.get_nodes()->begin()->obj.get_net_id();
        currentId = startId;
        history.clear();
        if (currentId.has_value())
            snap_camera_to_current();
    }
}

void ReaderMode::set_current(NetworkingObjects::NetObjID id) {
    currentId = id;
    if (active) snap_camera_to_current();
}

void ReaderMode::forward() {
    if (!active || !currentId.has_value()) return;
    auto& edges = world.wpGraph.get_edges();
    if (!edges) return;
    // First outgoing edge wins (per-edge ordering inside the list is
    // stable; it's the order edges were created in). M7-c will replace
    // this with a branch-choice UI when there are 2+ outgoing edges.
    for (auto& info : *edges) {
        if (info.obj->get_from() != currentId.value()) continue;
        history.push_back(currentId.value());
        currentId = info.obj->get_to();
        snap_camera_to_current();
        return;
    }
    // No outgoing edge — dead end. M7-d will surface a "the end"
    // affordance here.
}

void ReaderMode::back() {
    if (!active || history.empty()) return;
    currentId = history.back();
    history.pop_back();
    snap_camera_to_current();
}

void ReaderMode::snap_camera_to_current() {
    if (!currentId.has_value()) return;
    auto wpRef = world.netObjMan.get_obj_temporary_ref_from_id<Waypoint>(currentId.value());
    if (!wpRef) return;
    world.drawData.cam.smooth_move_to(world, wpRef->get_coords(),
                                      wpRef->get_window_size().cast<float>());
}
