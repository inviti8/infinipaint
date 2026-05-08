#pragma once
#include <Helpers/NetworkingObjects/NetObjID.hpp>
#include <optional>
#include <vector>

class World;

// PHASE1.md §7 — reader mode toggle + traversal state.
//
// M7-a: just the toggle + "current waypoint" anchor. Editor chrome
// (tree-view panel, waypoint markers) hides while active; the camera
// smooth-moves to the selected waypoint on entry. The user-facing
// keyboard navigation, branch UI, and dead-end affordance land in
// M7-b/c/d.
class ReaderMode {
    public:
        explicit ReaderMode(World& w);

        bool is_active() const { return active; }
        void toggle();
        void set_active(bool a);

        // Currently displayed waypoint while in reader mode. Set on entry,
        // updated by forward/back navigation.
        bool has_current() const                       { return currentId.has_value(); }
        NetworkingObjects::NetObjID get_current() const { return currentId.value_or(NetworkingObjects::NetObjID{}); }
        void set_current(NetworkingObjects::NetObjID id);

        // Forward: follows the current waypoint's first outgoing edge.
        // (Multi-edge branch UI lands in M7-c; for now "forward" picks
        // the first edge silently.) No-op at dead-ends.
        void forward();

        // Back: pops the most recent visit off the history stack and
        // navigates to it. No-op at the start.
        void back();

    private:
        // Anchors the camera to currentId's stored framing.
        void snap_camera_to_current();

        World& world;
        bool active = false;
        std::optional<NetworkingObjects::NetObjID> currentId;
        // History of previously-visited waypoint ids, in order — back()
        // pops one. Forward navigation pushes the prior currentId here
        // before swapping current.
        std::vector<NetworkingObjects::NetObjID> history;
};
