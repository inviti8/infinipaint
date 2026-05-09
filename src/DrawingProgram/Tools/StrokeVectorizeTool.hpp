#pragma once
#include "DrawingProgramToolBase.hpp"
#include "../../CanvasComponents/CanvasComponentContainer.hpp"

class DrawingProgram;
struct DrawData;

// PHASE2 Workstream A — converts recorded libmypaint strokes within a
// rect-drag selection on the INK layer into vector BrushStrokeCanvas
// components. Each MyPaintLayerCanvasComponent that:
//   - lives on the INK-kind layer,
//   - has a valid recorded stroke (pre-Phase-2 layers and eraser-touched
//     strokes don't), and
//   - has bounds intersecting the selection rect,
// is replaced with a BrushStrokeCanvasComponent fitted from its recorded
// dab path via Schneider 1990 bezier-fit + dense resampling.
//
// First-cut scope:
//   - No preview overlay; converts immediately on rect release. Undo
//     reverts.
//   - Two undo entries per drag (place new + erase old). Bundling into
//     a single undo is a follow-up.
//   - INK-only; SKETCH and COLOR layers are skipped per design doc.
//
// Per the design doc, partial-rect-intersect strokes are converted in
// full, not split — the rect picks "this stroke or not", at the
// granularity of the recorded log.
class StrokeVectorizeTool : public DrawingProgramToolBase {
    public:
        StrokeVectorizeTool(DrawingProgram& initDrawP);

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
        // Selection rect dispatcher. Walks the INK layer, converts every
        // eligible recorded stroke into a vector BrushStrokeCanvasComponent
        // and replaces the source MyPaintLayerCanvasComponent.
        void convert_selection(const Vector2f& camMin, const Vector2f& camMax);

        bool dragging = false;
        Vector2f dragStart{0.0f, 0.0f};
        Vector2f dragCurrent{0.0f, 0.0f};
};
