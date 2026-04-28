#include "LayoutElement.hpp"
#include "Helpers/ConvertVec.hpp"
#include "../GUIManager.hpp"

namespace GUIStuff {

LayoutElement::LayoutElement(GUIManager& gui): Element(gui) {}

void LayoutElement::layout(const Clay_ElementId& id, const std::function<void(LayoutElement*, const Clay_ElementId&)>& layout, const Callbacks& c) {
    this->c = c;
    layout(this, id);
}

void LayoutElement::input_mouse_button_callback(const InputManager::MouseButtonCallbackArgs& button) {
    if(c.mouseButton) c.mouseButton(this, button);
    if(c.onClick) c.onClick(this, button);
}

void LayoutElement::input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) {
    if(c.mouseMotion) c.mouseMotion(this, motion);
}

void LayoutElement::input_mouse_wheel_callback(const InputManager::MouseWheelCallbackArgs& wheel) {
    if(c.mouseWheel) c.mouseWheel(this, wheel);
}

void LayoutElement::input_finger_touch_callback(const InputManager::FingerTouchCallbackArgs& touch) {
    if(c.fingerTouch) c.fingerTouch(this, touch);
    if(c.onClick) {
        c.onClick(this, {
            .deviceType = InputManager::MouseDeviceType::TOUCH,
            .button = InputManager::MouseButton::LEFT,
            .down = touch.down,
            .clicks = static_cast<uint8_t>(touch.fingerTapCount),
            .pos = touch.pos
        });
    }
}

void LayoutElement::input_finger_motion_callback(const InputManager::FingerMotionCallbackArgs& motion) {
    if(c.fingerMotion) c.fingerMotion(this, motion);
}

void LayoutElement::input_key_callback(const InputManager::KeyCallbackArgs& key) {
    if(c.key) c.key(this, key);
}

}
