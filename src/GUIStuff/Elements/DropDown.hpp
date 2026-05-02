#pragma once
#include "Element.hpp"
#include "Helpers/ConvertVec.hpp"
#include "ManyElementScrollArea.hpp"
#include "SelectableButton.hpp"
#include "../ElementHelpers/LayoutHelpers.hpp"
#include "../ElementHelpers/TextLabelHelpers.hpp"
#include "../ElementHelpers/ButtonHelpers.hpp"
#include "SVGIcon.hpp"

namespace GUIStuff {

struct DropdownOptions {
    std::function<void()> onClick;
};

template <typename T> class DropDown : public Element {
    public:
        static constexpr float DROPDOWN_OFFSET = 4.0f;

        DropDown(GUIManager& gui): Element(gui) {}
        void layout(const Clay_ElementId& id, T* data, const std::vector<std::string>& selections, const DropdownOptions& options = {}) {
            using namespace ElementHelpers;
            opts = options;
            d = data;


            CLAY(id, {
                .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)}}
            }) {
                dropdownButton = gui.element<SelectableButton>("dropdown", SelectableButton::Data{
                    .drawType = SelectableButton::DrawType::FILLED,
                    .isSelected = isOpen,
                    .onClick = [&] {
                        isOpen = !isOpen;
                        gui.set_to_layout();
                    },
                    .innerContent = [&](const SelectableButton::InnerContentCallbackParameters&) {
                        CLAY_AUTO_ID({
                            .layout = {
                                .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
                                .padding = {.left = 4, .right = 4},
                                .childGap = gui.io.theme->childGap1,
                                .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
                                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                            }
                        }) {
                            text_label(gui, selections[static_cast<size_t>(*data)]);
                            CLAY_AUTO_ID({.layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)}}}) {}
                            CLAY_AUTO_ID({
                                .layout = {
                                    .sizing = {.width = CLAY_SIZING_FIT(ElementHelpers::SMALL_BUTTON_SIZE), .height = CLAY_SIZING_FIT(ElementHelpers::SMALL_BUTTON_SIZE)}
                                }
                            }) {
                                gui.element<SVGIcon>("dropico", "data/icons/droparrow.svg", isOpen);
                            }
                        }
                    }
                });

                if(isOpen && boundingBox.has_value()) {
                    auto& buttonBB = boundingBox.value();
                    float ENTRY_HEIGHT = buttonBB.height();
                    gui.set_z_index(gui.get_z_index() + 1, [&] {
                        gui.element<LayoutElement>("DROPDOWN", [&](LayoutElement* l, const Clay_ElementId& lId) {
                            float dropdownHeight = ENTRY_HEIGHT * selections.size() + DROPDOWN_OFFSET;
                            Clay_FloatingElementConfig floatConfig;
                            float maxHeight = gui.io.windowSize.y();
                            if(buttonBB.max.y() + dropdownHeight > gui.io.windowSize.y()) {
                                float rightSideWidth = gui.io.windowSize.x() - buttonBB.max.x();
                                float leftSideWidth = buttonBB.min.x();
                                if(rightSideWidth > buttonBB.width() || leftSideWidth > buttonBB.width())
                                    vertical_offset_setup(dropdownHeight, floatConfig, maxHeight, rightSideWidth >= leftSideWidth);
                                else {
                                    if(gui.io.windowSize.y() - buttonBB.max.y() >= buttonBB.min.y())
                                        bottom_offset_setup(dropdownHeight, floatConfig, maxHeight);
                                    else
                                        top_offset_setup(dropdownHeight, floatConfig, maxHeight);
                                }
                            }
                            else
                                bottom_offset_setup(dropdownHeight, floatConfig, maxHeight);
                            CLAY(lId, {
                                .layout = {
                                    .sizing = {.width = CLAY_SIZING_FIXED(buttonBB.width()), .height = CLAY_SIZING_FIT(0, maxHeight)},
                                    .childGap = 0
                                },
                                .backgroundColor = convert_vec4<Clay_Color>(gui.io.theme->backColor1),
                                .cornerRadius = CLAY_CORNER_RADIUS(4),
                                .floating = floatConfig,
                                .border = {
                                    .color = convert_vec4<Clay_Color>(gui.io.theme->fillColor2),
                                    .width = CLAY_BORDER_OUTSIDE(1)
                                }
                            }) {
                                gui.element<ManyElementScrollArea>("dropdown scroll area", ManyElementScrollArea::Options{
                                    .entryHeight = ENTRY_HEIGHT,
                                    .entryCount = selections.size(),
                                    .clipHorizontal = true,
                                    .elementContent = [&](size_t i) {
                                        text_transparent_option_button("option", selections[i].c_str(), [&, i] {
                                            *d = static_cast<T>(i);
                                            if(opts.onClick) opts.onClick();
                                            isOpen = false;
                                        });
                                    }
                                });
                            }
                        }, LayoutElement::Callbacks {
                            .onClick = [&](LayoutElement* l, const InputManager::MouseButtonCallbackArgs& m) {
                                if(!l->mouseHovering && !l->childMouseHovering && m.down && !dropdownButton->mouseHovering) {
                                    isOpen = false;
                                    gui.set_to_layout();
                                }
                            }
                        });
                    });
                }
            }
        }
    private:
        void bottom_offset_setup(float dropdownHeight, Clay_FloatingElementConfig& floatConfig, float& maxHeight) {
            floatConfig = {
                .offset = {
                    .y = DROPDOWN_OFFSET
                },
                .zIndex = gui.get_z_index(),
                .attachPoints = {
                    .element = CLAY_ATTACH_POINT_LEFT_TOP,
                    .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM
                },
                .attachTo = CLAY_ATTACH_TO_PARENT
            };
            // This function might be used even when boundingBox isnt determined yet
            maxHeight = boundingBox.has_value() ? std::max(gui.io.windowSize.y() - boundingBox.value().max.y() - DROPDOWN_OFFSET, 0.0f) : 0.0f;
        }

        void top_offset_setup(float dropdownHeight, Clay_FloatingElementConfig& floatConfig, float& maxHeight) {
            floatConfig = {
                .offset = {
                    .y = -DROPDOWN_OFFSET
                },
                .zIndex = gui.get_z_index(),
                .attachPoints = {
                    .element = CLAY_ATTACH_POINT_LEFT_BOTTOM,
                    .parent = CLAY_ATTACH_POINT_LEFT_TOP
                },
                .attachTo = CLAY_ATTACH_TO_PARENT
            };
            maxHeight = std::max(boundingBox.value().min.y() - DROPDOWN_OFFSET, 0.0f);
        }

        void vertical_offset_setup(float dropdownHeight, Clay_FloatingElementConfig& floatConfig, float& maxHeight, bool isRightSide) {
            auto& bb = boundingBox.value();
            float offsetY = bb.center().y() - dropdownHeight * 0.5f;
            if(dropdownHeight >= gui.io.windowSize.y() || offsetY < 0.0f)
                offsetY = 0.0f;
            else if(offsetY + dropdownHeight > gui.io.windowSize.y())
                offsetY -= (offsetY + dropdownHeight) - gui.io.windowSize.y();
            floatConfig = {
                .offset = {
                    .x = isRightSide ? bb.max.x() + DROPDOWN_OFFSET : bb.min.x() - bb.width() - DROPDOWN_OFFSET,
                    .y = offsetY
                },
                .zIndex = gui.get_z_index(),
                .attachPoints = {
                    .element = CLAY_ATTACH_POINT_LEFT_TOP,
                    .parent = CLAY_ATTACH_POINT_LEFT_TOP
                },
                .attachTo = CLAY_ATTACH_TO_ROOT
            };
            maxHeight = gui.io.windowSize.y();
        }

        void text_transparent_option_button(const char* id, const char* text, const std::function<void()>& onClick) {
            using namespace ElementHelpers;
            text_button(gui, id, text, {
                .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
                .wide = true,
                .centered = false,
                .onClick = onClick
            });
        }
        T* d;
        SelectableButton* dropdownButton = nullptr;
        DropdownOptions opts;
        bool isOpen = false;
};

}
