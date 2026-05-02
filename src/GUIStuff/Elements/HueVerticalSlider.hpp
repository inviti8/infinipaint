#pragma once
#include "Element.hpp"

namespace GUIStuff {

struct HueVerticalSliderData {
    float* hue = nullptr;
    std::function<void()> onChange;
    std::function<void()> onHold;
    std::function<void()> onRelease;
};

class HueVerticalSlider : public Element {
    public:
        HueVerticalSlider(GUIManager& gui);

        void layout(const Clay_ElementId& id, const HueVerticalSliderData& opts);
        virtual void update() override;
        virtual void input_mouse_button_callback(const InputManager::MouseButtonCallbackArgs& button) override;
        virtual void input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) override;
        virtual void clay_draw(SkCanvas* canvas, UpdateInputData& io, Clay_RenderCommand* command, bool skiaAA) override;

    private:
        void update_hue(Vector2f p);
        HueVerticalSliderData o;
        float currentHue = -1.0f;
        bool isHeld = false;
};

}
