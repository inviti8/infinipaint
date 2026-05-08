#include "ReaderMode.hpp"
#include "../World.hpp"
#include "../MainProgram.hpp"
#include "../Waypoints/Waypoint.hpp"
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
        if (currentId.has_value())
            snap_camera_to_current();
    }
}

void ReaderMode::set_current(NetworkingObjects::NetObjID id) {
    currentId = id;
    if (active) snap_camera_to_current();
}

void ReaderMode::snap_camera_to_current() {
    if (!currentId.has_value()) return;
    auto wpRef = world.netObjMan.get_obj_temporary_ref_from_id<Waypoint>(currentId.value());
    if (!wpRef) return;
    world.drawData.cam.smooth_move_to(world, wpRef->get_coords(),
                                      wpRef->get_window_size().cast<float>());
}
