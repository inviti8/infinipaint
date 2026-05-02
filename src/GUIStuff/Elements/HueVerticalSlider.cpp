#include "HueVerticalSlider.hpp"
#include "../GUIManager.hpp"
#include "ColorPicker.hpp"

namespace GUIStuff {

HueVerticalSlider::HueVerticalSlider(GUIManager& gui): Element(gui) {}

void HueVerticalSlider::layout(const Clay_ElementId& id, const HueVerticalSliderData& opts) {
    o = opts;
    update();

    CLAY(id, {
        .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)}},
        .custom = {.customData = this}
    }) {
    }
}

void HueVerticalSlider::update() {
    if(currentHue != *o.hue) {
        currentHue = *o.hue;
        gui.invalidate_draw_element(this);
    }
}

void HueVerticalSlider::input_mouse_button_callback(const InputManager::MouseButtonCallbackArgs& button) {
    if(mouseHovering && button.down && button.button == InputManager::MouseButton::LEFT) {
        if(o.onHold) o.onHold();
        update_hue(button.pos);
        if(o.onChange) o.onChange();
        isHeld = true;
    }
    else if(isHeld && !button.down && button.button == InputManager::MouseButton::LEFT) {
        if(o.onRelease) o.onRelease();
        isHeld = false;
    }
}

void HueVerticalSlider::input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) {
    if(isHeld) {
        update_hue(motion.pos);
        if(o.onChange) o.onChange();
    }
}

void HueVerticalSlider::update_hue(Vector2f p) {
    *o.hue = (1.0f - std::clamp((p.y() - boundingBox.value().min.y()) / boundingBox.value().height(), 0.0f, 1.0f)) * 360.0f;
}

void HueVerticalSlider::clay_draw(SkCanvas* canvas, UpdateInputData& io, Clay_RenderCommand* command, bool skiaAA) {
    auto& bb = boundingBox.value();

    float normalizedHue = currentHue / 360.0f;

    canvas->save();
    canvas->translate(bb.min.x(), bb.min.y());
    canvas->scale(bb.width(), bb.height());
    canvas->clipRect(SkRect::MakeXYWH(0.0f, 0.0f, 1.0f, 1.0f), skiaAA);
    SkPaint hueBarPaint;
    hueBarPaint.setShader(ColorPickerShaders::get_hue_shader());
    canvas->drawPaint(hueBarPaint);
    canvas->restore();

    canvas->save();
    canvas->translate(bb.min.x(), bb.min.y());
    SkPaint selectionLinePaint({1.0f, 1.0f, 1.0f, 1.0f});
    selectionLinePaint.setAntiAlias(skiaAA);
    selectionLinePaint.setStrokeWidth(1.0f);
    float hueY = bb.height() * (1.0f - normalizedHue);
    canvas->drawLine(0.0f, hueY, bb.width(), hueY, selectionLinePaint);
    canvas->restore();
}

}
