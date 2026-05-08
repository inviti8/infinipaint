#pragma once

class World;
namespace GUIStuff { class GUIManager; }

// PHASE1.md §6 (adapted) — graph view of WaypointGraph rendered as a
// collapsible side panel inside the main window, instead of the
// originally-spec'd second OS window. The doc rejected side-panel for
// multi-monitor reasons; this fork is single-user / single-monitor so
// the simplification is acceptable. If multi-monitor matters later
// the rendering code is reusable in a real second window.
//
// M6-a: skeleton — visibility toggle, empty Clay panel reservation.
// M6-b: render WaypointGraph nodes + edges (read-only) using Skia,
//       inside the panel's screen rect, with positions from
//       WaypointGraph::layout (auto-placed if no entry yet).
// M6-c: drag-reposition, drag-from-port to create edge,
//       double-click → focus canvas on the corresponding waypoint.
class TreeView {
    public:
        explicit TreeView(World& w);

        bool is_visible() const { return visible; }
        void toggle()           { visible = !visible; }
        void set_visible(bool v){ visible = v; }

        // Lays out the panel in the current Clay layout (called from
        // Toolbar::drawing_program_gui). When hidden the panel
        // contributes zero width to the layout.
        void gui(GUIStuff::GUIManager& gui);

    private:
        World& world;
        bool visible = false;
};
