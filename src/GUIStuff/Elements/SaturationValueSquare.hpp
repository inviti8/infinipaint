#pragma once
#include "Element.hpp"

namespace GUIStuff {

struct SaturationValueSquareData {
    float* saturation = nullptr;
    float* value = nullptr;
    float* hue = nullptr;
    std::function<void()> onChange;
    std::function<void()> onHold;
    std::function<void()> onRelease;
};

class SaturationValueSquare : public Element {
    public:
        SaturationValueSquare(GUIManager& gui);

        void layout(const Clay_ElementId& id, const SaturationValueSquareData& opts);
        virtual void update() override;
        virtual void input_mouse_button_callback(const InputManager::MouseButtonCallbackArgs& button) override;
        virtual void input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) override;
        virtual void clay_draw(SkCanvas* canvas, UpdateInputData& io, Clay_RenderCommand* command, bool skiaAA) override;

    private:
        void update_sv(Vector2f p);
        SaturationValueSquareData o;
        float currentHue = -1.0f;
        float currentSaturation = -1.0f;
        float currentValue = -1.0f;
        bool isHeld = false;
};

}
