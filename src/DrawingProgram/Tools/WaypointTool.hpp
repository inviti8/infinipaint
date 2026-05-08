#pragma once
#include "DrawingProgramToolBase.hpp"
#include "../../CanvasComponents/CanvasComponentContainer.hpp"

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
        // (and the camera was smooth-moved to it). Otherwise the caller
        // should fall through to the drop-a-new-waypoint path.
        bool try_focus_existing_waypoint(const Vector2f& clickPos);

        // Drop a new Waypoint into WaypointGraph and a WaypointCanvasComponent
        // into the active layer.
        void drop_waypoint(const Vector2f& clickPos);
};
