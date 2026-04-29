#pragma once
#include "Element.hpp"

namespace GUIStuff {

class LayoutElement : public Element {
    public:
        LayoutElement(GUIManager& gui);
        struct Callbacks {
            std::function<void(LayoutElement* l, const InputManager::MouseButtonCallbackArgs& button)> mouseButton;
            std::function<void(LayoutElement* l, const InputManager::MouseMotionCallbackArgs& motion)> mouseMotion;
            std::function<void(LayoutElement* l, const InputManager::MouseWheelCallbackArgs& wheel)> mouseWheel;
            std::function<void(LayoutElement* l, const InputManager::FingerTouchCallbackArgs& touch)> fingerTouch;
            std::function<void(LayoutElement* l, const InputManager::FingerMotionCallbackArgs& motion)> fingerMotion;
            std::function<void(LayoutElement* l, const InputManager::MouseButtonCallbackArgs& button)> onClick;
            std::function<void(LayoutElement* l, const InputManager::MouseMotionCallbackArgs& motion)> onMotion;
            std::function<void(LayoutElement* l, const InputManager::KeyCallbackArgs& key)> key;
        };
        void layout(const Clay_ElementId& id, const std::function<void(LayoutElement*, const Clay_ElementId&)>& layout, const Callbacks& c = {});
        virtual void input_mouse_button_callback(const InputManager::MouseButtonCallbackArgs& button) override;
        virtual void input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) override;
        virtual void input_mouse_wheel_callback(const InputManager::MouseWheelCallbackArgs& wheel) override;
        virtual void input_finger_touch_callback(const InputManager::FingerTouchCallbackArgs& touch) override;
        virtual void input_finger_motion_callback(const InputManager::FingerMotionCallbackArgs& motion) override;
        virtual void input_key_callback(const InputManager::KeyCallbackArgs& key) override;
    private:
        Callbacks c;
};

}
