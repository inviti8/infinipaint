#pragma once
#include "ColorPicker.decl.hpp"
#include "../GUIManager.hpp"
#include "Helpers/ConvertVec.hpp"
#include "SaturationValueSquare.hpp"
#include "HueVerticalSlider.hpp"
#include "AlphaSlider.hpp"

namespace GUIStuff {

template <typename T> ColorPicker<T>::ColorPicker(GUIManager& gui): Element(gui) {}

template <typename T> void ColorPicker<T>::layout(const Clay_ElementId& id, T* data, bool selectAlpha, const ColorPickerData& conf) {
    this->data = data;
    if(!oldData.has_value() || oldData.value() != *data) {
        savedHsv = rgb_to_hsv<Vector3f, T>(*data);
        oldData = *data;
    }

    config = conf;

    auto onChange = [&] {
        set_rgb_from_hsv(savedHsv);
        if(config.onChange) config.onChange();
    };

    if(selectAlpha) {
        CLAY(id, {
            .layout = {
                .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
                .childGap = BAR_GAP,
                .layoutDirection = CLAY_TOP_TO_BOTTOM
            }
        }) {
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .childGap = BAR_GAP,
                    .layoutDirection = CLAY_LEFT_TO_RIGHT
                },
            }) {
                gui.element<SaturationValueSquare>("sv square", SaturationValueSquareData{
                    .saturation = &savedHsv.y(),
                    .value = &savedHsv.z(),
                    .hue = &savedHsv.x(),
                    .onChange = onChange,
                    .onHold = config.onHold,
                    .onRelease = config.onRelease
                });
                CLAY_AUTO_ID({
                    .layout = {
                        .sizing = {.width = CLAY_SIZING_GROW(0, BAR_WIDTH), .height = CLAY_SIZING_GROW(0)}
                    },
                }) {
                    gui.element<HueVerticalSlider>("hue slider", HueVerticalSliderData{
                        .hue = &savedHsv.x(),
                        .onChange = onChange,
                        .onHold = config.onHold,
                        .onRelease = config.onRelease
                    });
                }
            }
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0, BAR_WIDTH)}
                },
            }) {
                gui.element<AlphaSlider>("alpha slider", AlphaSliderData{
                    .alpha = &(*data)[3],
                    .r = &(*data)[0],
                    .g = &(*data)[1],
                    .b = &(*data)[2],
                    .onChange = onChange,
                    .onHold = config.onHold,
                    .onRelease = config.onRelease
                });
            }
        }
    }
    else {
        CLAY(id, {
            .layout = {
                .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                .childGap = BAR_GAP,
                .layoutDirection = CLAY_LEFT_TO_RIGHT
            },
        }) {
            gui.element<SaturationValueSquare>("sv square", SaturationValueSquareData{
                .saturation = &savedHsv.y(),
                .value = &savedHsv.z(),
                .hue = &savedHsv.x(),
                .onChange = onChange,
                .onHold = config.onHold,
                .onRelease = config.onRelease
            });
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = {.width = CLAY_SIZING_FIT(BAR_WIDTH), .height = CLAY_SIZING_GROW(0)},
                    .layoutDirection = CLAY_LEFT_TO_RIGHT
                },
            }) {
                gui.element<HueVerticalSlider>("hue slider", HueVerticalSliderData{
                    .hue = &savedHsv.x(),
                    .onChange = onChange,
                    .onHold = config.onHold,
                    .onRelease = config.onRelease
                });
            }
        }
    }
}

template <typename T> void ColorPicker<T>::set_rgb_from_hsv(const Vector3f& hsv) {
    Vector3f a = hsv_to_rgb<Vector3f>(hsv);
    (*data)[0] = a.x();
    (*data)[1] = a.y();
    (*data)[2] = a.z();
    oldData = *data;
}

}
