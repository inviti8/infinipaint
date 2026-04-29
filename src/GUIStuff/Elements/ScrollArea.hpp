#pragma once
#include "Element.hpp"
#include "Helpers/ConvertVec.hpp"

namespace GUIStuff {

class ScrollArea : public Element {
    public:
        ScrollArea(GUIManager& gui);

        struct InnerContentParameters {
            Vector2f contentDimensions;
            Vector2f containerDimensions;
            Vector2f* scrollOffset;
        };

        struct Options {
            bool scrollVertical = false;
            bool scrollHorizontal = false;
            bool clipHorizontal = false;
            bool clipVertical = false;
            bool showScrollbarX = false;
            bool showScrollbarY = false;
            Clay_LayoutDirection layoutDirection = CLAY_TOP_TO_BOTTOM;
            Clay_LayoutAlignmentX xAlign = CLAY_ALIGN_X_LEFT;
            Clay_LayoutAlignmentY yAlign = CLAY_ALIGN_Y_TOP;
            std::function<void(const InnerContentParameters&)> innerContent;
        };

        void layout(const Clay_ElementId& id, const Options& options);
        void reset_scroll();


        virtual void update() override;
        virtual void input_mouse_wheel_callback(const InputManager::MouseWheelCallbackArgs& wheel) override;
        virtual void input_mouse_button_callback(const InputManager::MouseButtonCallbackArgs& button) override;
        virtual void input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) override;
        virtual void input_finger_touch_callback(const InputManager::FingerTouchCallbackArgs& touch) override;
        virtual void input_finger_motion_callback(const InputManager::FingerMotionCallbackArgs& motion) override;
    private:
        Options opts;
        bool scrollAreaHeld = false;
        Vector2f scrollAreaMotion = {0.0f, 0.0f};
        Vector2f scrollAreaMotionMax = {0.0f, 0.0f};

        bool isScrollbarHeldY = false;
        bool isScrollbarHoveredY = false;
        bool isHoveringOverScrollerY = false;
        Element* scrollerY = nullptr;
        float scrollerStartPosY;
        float mouseStartPosY;
        void y_scroll_bar();

        Vector2f contentDimensions = {0.0f, 0.0f};
        Vector2f scrollOffset = {0.0f, 0.0f};
        void clamp_scroll();
};

}
