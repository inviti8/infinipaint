#pragma once
#include "DrawingProgramToolBase.hpp"
#include <Helpers/SCollision.hpp>
#include <filesystem>
#include "../../InputManager.hpp"
#include "../../CoordSpaceHelper.hpp"
#include "../../World.hpp"

class DrawingProgram;

class ScreenshotTool : public DrawingProgramToolBase {
    public:
        ScreenshotTool(DrawingProgram& initDrawP);
        virtual DrawingProgramToolType get_type() override;
        virtual void gui_toolbox(Toolbar& t) override;
        virtual void right_click_popup_gui(Toolbar& t, Vector2f popupPos) override;
        virtual void erase_component(CanvasComponentContainer::ObjInfo* erasedComp) override;
        virtual void tool_update() override;
        virtual void draw(SkCanvas* canvas, const DrawData& drawData) override;
        virtual void switch_tool(DrawingProgramToolType newTool) override;
        virtual bool prevent_undo_or_redo() override;
        virtual void input_mouse_button_on_canvas_callback(const InputManager::MouseButtonCallbackArgs& button) override;
        virtual void input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) override;
    private:
        void commit_rect();
        void take_screenshot(const std::filesystem::path& filePath, WorldScreenshotInfo::ScreenshotType screenshotType);

        struct ScreenshotControls {
            CoordSpaceHelper translateBeginCoords;
            WorldVec translateBeginPos;

            CoordSpaceHelper coords;
            float rectX1;
            float rectX2;
            float rectY1;
            float rectY2;
            enum class SelectionMode {
                NO_SELECTION,
                DRAGGING_BORDER,
                SELECTION_EXISTS,
                DRAGGING_AREA
            } selectionMode = SelectionMode::NO_SELECTION;
            int dragType = 0;
            Vector2i imageSize = {0, 0};
            std::array<SCollision::Circle<float>, 8> circles;

            bool displayGrid = true;
            bool transparentBackground = false;

            std::vector<std::string> typeSelections = {".jpg", ".png", ".webp", ".svg"};

            std::atomic<bool> setToTakeScreenshot = false;
            std::filesystem::path screenshotSavePath;
            WorldScreenshotInfo::ScreenshotType screenshotSaveType;
        } controls;
        bool dragging_border_update(const Vector2f& camCursorPos);
        bool selection_exists_update();
        bool dragging_area_update(const Vector2f& camCursorPos);
};
