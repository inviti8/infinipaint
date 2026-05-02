#pragma once
#include "Element.hpp"

namespace GUIStuff {

struct AlphaSliderData {
    float* alpha = nullptr;
    float* r = nullptr;
    float* g = nullptr;
    float* b = nullptr;
    std::function<void()> onChange;
    std::function<void()> onHold;
    std::function<void()> onRelease;
};

class AlphaSlider : public Element {
    public:
        AlphaSlider(GUIManager& gui);

        void layout(const Clay_ElementId& id, const AlphaSliderData& opts);
        virtual void update() override;
        virtual void input_mouse_button_callback(const InputManager::MouseButtonCallbackArgs& button) override;
        virtual void input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) override;
        virtual void clay_draw(SkCanvas* canvas, UpdateInputData& io, Clay_RenderCommand* command, bool skiaAA) override;

    private:
        void update_alpha(Vector2f p);
        AlphaSliderData o;
        float currentAlpha = -1.0f;
        float currentR = -1.0f;
        float currentG = -1.0f;
        float currentB = -1.0f;
        bool isHeld = false;
};

}
