#pragma once
#include "DrawingProgramToolBase.hpp"
#include "../../CanvasComponents/CanvasComponentContainer.hpp"
#include <Helpers/NetworkingObjects/NetObjID.hpp>

class DrawingProgram;
struct DrawData;

// PHASE1.md §5 — canvas-side waypoint editor.
//
// M5-a behaviour:
//   - Click on empty canvas: drop a new Waypoint into WaypointGraph and
//     a WaypointCanvasComponent into the active layer at the click point.
//     The Waypoint snapshots the current camera (CoordSpaceHelper +
//     window size), which is what "click to focus" replays later.
//   - Click on an existing waypoint marker: smooth-move the camera to
//     that waypoint's stored framing.
//
// Deferred: framing-rect handles for the selected waypoint (M5-b),
// faint outgoing-edge previews (M5-c), reader-mode chrome toggle (M7).
class WaypointTool : public DrawingProgramToolBase {
    public:
        WaypointTool(DrawingProgram& initDrawP);

        virtual DrawingProgramToolType get_type() override;
        virtual void gui_toolbox(Toolbar& t) override;
        virtual void gui_phone_toolbox(PhoneDrawingProgramScreen& t) override;
        virtual void tool_update() override;
        virtual void right_click_popup_gui(Toolbar& t, Vector2f popupPos) override;
        virtual void erase_component(CanvasComponentContainer::ObjInfo* erasedComp) override;
        virtual void switch_tool(DrawingProgramToolType newTool) override;
        virtual void draw(SkCanvas* canvas, const DrawData& drawData) override;
        virtual bool prevent_undo_or_redo() override;
        virtual void input_mouse_button_on_canvas_callback(const InputManager::MouseButtonCallbackArgs& button) override;

    private:
        // Returns true if the click landed on an existing waypoint
        // (and the camera was smooth-moved to it, and selection updated).
        // Otherwise the caller falls through to drop-a-new-waypoint.
        bool try_focus_existing_waypoint(const Vector2f& clickPos);

        // Drop a new Waypoint into WaypointGraph and a WaypointCanvasComponent
        // into the active layer. Selects the new waypoint.
        void drop_waypoint(const Vector2f& clickPos);

        // Shift+click handler: if the click hit an existing waypoint and
        // a different waypoint is selected, append an Edge from selection
        // to the clicked one. Returns true if an edge was created.
        bool try_create_edge_to_clicked(const Vector2f& clickPos);

        // Selection state lives on WaypointGraph as the single source of
        // truth shared between this tool and the TreeView panel — a
        // click in either updates the other's chrome.
};
