#include "LayoutElement.hpp"
#include "Helpers/ConvertVec.hpp"
#include "../GUIManager.hpp"

namespace GUIStuff {

LayoutElement::LayoutElement(GUIManager& gui): Element(gui) {}

void LayoutElement::layout(const Clay_ElementId& id, const std::function<void(LayoutElement*, const Clay_ElementId&)>& layout, const Callbacks& c) {
    this->c = c;
    layout(this, id);
}

void LayoutElement::update() {
    for(auto& [id, animation] : animations) {
        switch(animation.state) {
            case InternalFloatAnimation::State::RUN: {
                animation.currentTime += gui.io.deltaTime;
                if(animation.currentTime >= animation.anim.duration) {
                    animation.currentTime = animation.anim.duration;
                    animation.state = InternalFloatAnimation::State::DONE;
                }
                gui.set_to_layout();
                break;
            }
            case InternalFloatAnimation::State::RUN_REVERSE: {
                animation.currentTime -= gui.io.deltaTime;
                if(animation.currentTime <= 0.0f) {
                    animation.currentTime = 0.0f;
                    animation.state = InternalFloatAnimation::State::DONE;
                }
                gui.set_to_layout();
                break;
            }
            case InternalFloatAnimation::State::DONE:
                break;
        }
    }
}

float LayoutElement::get_float_animation(const char* animationID, const FloatAnimation& animation) {
    InternalFloatAnimation internalAnim;
    internalAnim.anim = animation;
    auto [it, placed] = animations.emplace(animationID, internalAnim);
    const InternalFloatAnimation& actualInternalAnim = it->second;
    return std::lerp(actualInternalAnim.anim.startVal, actualInternalAnim.anim.endVal, actualInternalAnim.anim.easing(it->second.currentTime / actualInternalAnim.anim.duration));
}

void LayoutElement::animation_trigger(const char* animationID) {
    auto it = animations.find(animationID);
    if(it != animations.end()) {
        InternalFloatAnimation& animation = it->second;
        if(animation.currentTime < animation.anim.duration)
            animation.state = InternalFloatAnimation::State::RUN;
    }
}

void LayoutElement::animation_trigger_reverse(const char* animationID) {
    auto it = animations.find(animationID);
    if(it != animations.end()) {
        InternalFloatAnimation& animation = it->second;
        if(animation.currentTime > 0.0f)
            animation.state = InternalFloatAnimation::State::RUN_REVERSE;
    }
}

void LayoutElement::input_mouse_button_callback(const InputManager::MouseButtonCallbackArgs& button) {
    if(c.mouseButton) c.mouseButton(this, button);
}

void LayoutElement::input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) {
    if(c.mouseMotion) c.mouseMotion(this, motion);
}

void LayoutElement::input_mouse_wheel_callback(const InputManager::MouseWheelCallbackArgs& wheel) {
    if(c.mouseWheel) c.mouseWheel(this, wheel);
}

void LayoutElement::input_key_callback(const InputManager::KeyCallbackArgs& key) {
    if(c.key) c.key(this, key);
}

}
