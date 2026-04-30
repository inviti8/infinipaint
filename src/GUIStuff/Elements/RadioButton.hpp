#pragma once
#include "Element.hpp"

namespace GUIStuff {

class RadioButton : public Element {
    public:
        RadioButton(GUIManager& gui);
        virtual void clay_draw(SkCanvas* canvas, UpdateInputData& io, Clay_RenderCommand* command, bool skiaAA) override;
        virtual void input_mouse_button_callback(const InputManager::MouseButtonCallbackArgs& button) override;
        virtual void input_finger_touch_callback(const InputManager::FingerTouchCallbackArgs& touch) override;
        virtual void update() override;

        void layout(const Clay_ElementId& id, const std::function<bool()>& isTicked, const std::function<void()>& onClick);
    private:
        static constexpr float RADIOBUTTON_ANIMATION_TIME = 0.3;
        bool oldIsTicked = false;
        bool isHeld = false;
        bool is_hovering_animation();
        float hoverAnimation = 0.0;
        std::function<bool()> isTicked;
        std::function<void()> onClick;
};

}
