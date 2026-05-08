#pragma once
#include <Helpers/NetworkingObjects/NetObjID.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class World;
namespace GUIStuff { class GUIManager; }

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
        // At a branch point (2+ outgoing edges) this still picks the
        // first edge silently — the branch-choice overlay (M7-c) is
        // the proper UI for that case, but keyboard nav stays usable.
        void forward();

        // Back: pops the most recent visit off the history stack and
        // navigates to it. No-op at the start.
        void back();
        bool has_history() const { return !history.empty(); }

        // Outgoing edges from the current waypoint, in graph order.
        // Each entry is (target waypoint id, optional edge label).
        // Empty when current is null or has no outgoing edges.
        std::vector<std::pair<NetworkingObjects::NetObjID, std::optional<std::string>>> outgoing_choices() const;

        // Convenience: 2+ outgoing edges from current — i.e. the
        // branch-choice overlay should render.
        bool is_branch_point() const;

        // Direct navigation: pushes current onto history, sets new
        // current, snaps the camera. Does nothing if reader mode
        // isn't active.
        void navigate_to(NetworkingObjects::NetObjID id);

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

// Renders the branch-choice overlay when reader mode is active and
// the current waypoint has 2+ outgoing edges. Lays out a horizontal
// row of buttons, each clickable; each button shows the *target*
// waypoint's skin (or a generic labeled rect when no skin is set).
// Lives outside the class so it can sit alongside the layout code
// in the active screen without adding a circular include.
void render_reader_branch_overlay(World& world, GUIStuff::GUIManager& gui);
