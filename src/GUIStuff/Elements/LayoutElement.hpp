#pragma once
#include "Element.hpp"
#include "Helpers/BezierEasing.hpp"

namespace GUIStuff {

class LayoutElement : public Element {
    public:
        LayoutElement(GUIManager& gui);
        struct Callbacks {
            std::function<void(LayoutElement* l, const InputManager::MouseButtonCallbackArgs& button)> mouseButton;
            std::function<void(LayoutElement* l, const InputManager::MouseMotionCallbackArgs& motion)> mouseMotion;
            std::function<void(LayoutElement* l, const InputManager::MouseWheelCallbackArgs& wheel)> mouseWheel;
            std::function<void(LayoutElement* l, const InputManager::KeyCallbackArgs& key)> key;
        };
        struct FloatAnimation {
            float startVal = 0.0f;
            float endVal = 1.0f;
            float duration = 1.0f;
            BezierEasing easing = BezierEasing::linear;
        };
        void layout(const Clay_ElementId& id, const std::function<void(LayoutElement*, const Clay_ElementId&)>& layout, const Callbacks& c = {});
        virtual void update() override;
        float get_float_animation(const char* animationID, const FloatAnimation& animation);
        void animation_trigger(const char* animationID);
        void animation_trigger_reverse(const char* animationID);
        virtual void input_mouse_button_callback(const InputManager::MouseButtonCallbackArgs& button) override;
        virtual void input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) override;
        virtual void input_mouse_wheel_callback(const InputManager::MouseWheelCallbackArgs& wheel) override;
        virtual void input_key_callback(const InputManager::KeyCallbackArgs& key) override;
    private:
        struct InternalFloatAnimation {
            enum class State {
                RUN,
                RUN_REVERSE,
                DONE
            } state = State::DONE;
            float currentTime = 0.0f;
            FloatAnimation anim;
        };
        std::unordered_map<const char*, InternalFloatAnimation> animations;
        Callbacks c;
};

}
