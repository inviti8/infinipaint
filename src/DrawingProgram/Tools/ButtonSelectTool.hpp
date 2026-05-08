#pragma once
#include "DrawingProgramToolBase.hpp"
#include "../../CanvasComponents/CanvasComponentContainer.hpp"
#include <optional>

class DrawingProgram;
struct DrawData;

// PHASE1.md §5a — captures a rect of canvas pixels into the currently-
// selected Waypoint's `skin`, which then drives:
//   - the tree-view node visual (replaces the gold rounded rect),
//   - the canvas marker tint (signals "this destination has artwork"),
//   - the reader-mode branch-choice button image.
//
// Drag a rect on the canvas. On release: render the selected world area
// into an SkImage (capped at 512x512), assign it to the selected
// waypoint's skin via Waypoint::set_skin. The original canvas pixels
// are NOT removed — the artist removes them by hand if they don't want
// the button visible inside the comic page itself.
//
// Requires a waypoint to be selected; otherwise the drag is a no-op.
class ButtonSelectTool : public DrawingProgramToolBase {
    public:
        // Maximum side length (px) of the captured skin image. Larger
        // selections are downscaled preserving aspect ratio. Keeps file
        // sizes sane while staying crisp at typical reader-mode scales.
        static constexpr int MAX_SKIN_SIDE_PX = 512;

        ButtonSelectTool(DrawingProgram& initDrawP);

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
        virtual void input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) override;

    private:
        // Renders the drag rect's world content into an SkImage and
        // assigns it to the selected waypoint's skin. Caller checks
        // a waypoint is actually selected first.
        void capture_skin_to_selected(const Vector2f& camMin, const Vector2f& camMax);

        bool dragging = false;
        Vector2f dragStart{0.0f, 0.0f};
        Vector2f dragCurrent{0.0f, 0.0f};
};
