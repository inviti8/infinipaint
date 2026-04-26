#include "LayoutHelpers.hpp"
#include "../Elements/LayoutElement.hpp"
#include "Helpers/ConvertVec.hpp"

namespace GUIStuff { namespace ElementHelpers {

void top_to_bottom_window_popup_layout(GUIManager& gui, const char* id, Clay_SizingAxis x, Clay_SizingAxis y, const std::function<void(LayoutElement*)>& innerContent, const LayoutElement::Callbacks& callbacks) {
    gui.element<LayoutElement>(id, [&] (LayoutElement* l, const Clay_ElementId& lId) {
        CLAY(lId, {
            .layout = { 
                .sizing = {.width = x, .height = y },
                .padding = CLAY_PADDING_ALL(gui.io.theme->padding1),
                .childGap = gui.io.theme->childGap1,
                .layoutDirection = CLAY_TOP_TO_BOTTOM
            },
            .backgroundColor = convert_vec4<Clay_Color>(gui.io.theme->backColor1),
            .cornerRadius = CLAY_CORNER_RADIUS(10),
            .floating = { .zIndex = gui.get_z_index(), .attachTo = CLAY_ATTACH_TO_PARENT },
            .border = {.color = convert_vec4<Clay_Color>(gui.io.theme->fillColor2), .width = CLAY_BORDER_OUTSIDE(1)}
        }) {
            innerContent(l);
        }
    }, callbacks);
}

void left_to_right_layout(GUIManager& gui, Clay_SizingAxis x, Clay_SizingAxis y, const std::function<void()>& innerContent) {
    CLAY_AUTO_ID({.layout = { 
          .sizing = {.width = x, .height = y },
          .childGap = gui.io.theme->childGap1,
          .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
          .layoutDirection = CLAY_LEFT_TO_RIGHT,
    }
    }) {
        innerContent();
    }
}

void left_to_right_line_layout(GUIManager& gui, const std::function<void()>& innerContent) {
    left_to_right_layout(gui, CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0), innerContent);
}

void left_to_right_line_centered_layout(GUIManager& gui, const std::function<void()>& innerContent) {
    CLAY_AUTO_ID({.layout = { 
          .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
          .childGap = gui.io.theme->childGap1,
          .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
          .layoutDirection = CLAY_LEFT_TO_RIGHT,
    }
    }) {
        innerContent();
    }
}

void window_fill_side_bar(GUIManager& gui, const char* id, const WindowFillSideBarConfig& config, const std::function<void()>& innerContent) {
    gui.element<LayoutElement>(id, [&](LayoutElement* l, const Clay_ElementId& lId) {
        switch(config.dir) {
            case WindowFillSideBarConfig::Direction::TOP: {
                CLAY(lId, {
                    .layout = {
                        .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
                        .layoutDirection = CLAY_TOP_TO_BOTTOM
                    },
                    .backgroundColor = convert_vec4<Clay_Color>(config.backgroundColor),
                    .border = config.border
                }) {
                    CLAY_AUTO_ID({
                        .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(gui.io.safeWindowRect.min.y())}}
                    }) {}
                    CLAY_AUTO_ID({
                        .layout = {
                            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
                            .layoutDirection = CLAY_LEFT_TO_RIGHT
                        }
                    }) {
                        float paddingElementWidth = (gui.io.windowSize.x() - gui.io.safeWindowRect.width()) / 2.0f;
                        CLAY_AUTO_ID({
                            .layout = {.sizing = {.width = CLAY_SIZING_FIXED(paddingElementWidth), .height = CLAY_SIZING_FIT(0)}}
                        }) {}
                        innerContent();
                        CLAY_AUTO_ID({
                            .layout = {.sizing = {.width = CLAY_SIZING_FIXED(paddingElementWidth), .height = CLAY_SIZING_FIT(0)}}
                        }) {}
                    }
                }
                break;
            }
            case WindowFillSideBarConfig::Direction::BOTTOM: {
                CLAY(lId, {
                    .layout = {
                        .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
                        .layoutDirection = CLAY_TOP_TO_BOTTOM
                    },
                    .backgroundColor = convert_vec4<Clay_Color>(config.backgroundColor),
                    .border = config.border
                }) {
                    CLAY_AUTO_ID({
                        .layout = {
                            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
                            .layoutDirection = CLAY_LEFT_TO_RIGHT
                        }
                    }) {
                        float paddingElementWidth = (gui.io.windowSize.x() - gui.io.safeWindowRect.width()) / 2.0f;
                        CLAY_AUTO_ID({
                            .layout = {.sizing = {.width = CLAY_SIZING_FIXED(paddingElementWidth), .height = CLAY_SIZING_FIT(0)}}
                        }) {}
                        innerContent();
                        CLAY_AUTO_ID({
                            .layout = {.sizing = {.width = CLAY_SIZING_FIXED(paddingElementWidth), .height = CLAY_SIZING_FIT(0)}}
                        }) {}
                    }
                    CLAY_AUTO_ID({
                        .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(gui.io.windowSize.y() - gui.io.safeWindowRect.max.y())}}
                    }) {}
                }
                break;
            }
            case WindowFillSideBarConfig::Direction::LEFT: {
                CLAY(lId, {
                    .layout = {
                        .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_GROW(0) },
                        .layoutDirection = CLAY_LEFT_TO_RIGHT
                    },
                    .backgroundColor = convert_vec4<Clay_Color>(config.backgroundColor),
                    .border = config.border
                }) {
                    CLAY_AUTO_ID({
                        .layout = {.sizing = {.width = CLAY_SIZING_FIXED(gui.io.safeWindowRect.min.x()), .height = CLAY_SIZING_GROW(0)}}
                    }) {}
                    CLAY_AUTO_ID({
                        .layout = {
                            .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_GROW(0) },
                            .layoutDirection = CLAY_TOP_TO_BOTTOM
                        }
                    }) {
                        float paddingElementHeight = (gui.io.windowSize.y() - gui.io.safeWindowRect.height()) / 2.0f;
                        CLAY_AUTO_ID({
                            .layout = {.sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIXED(paddingElementHeight)}}
                        }) {}
                        innerContent();
                        CLAY_AUTO_ID({
                            .layout = {.sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIXED(paddingElementHeight)}}
                        }) {}
                    }
                }
                break;
            }
            case WindowFillSideBarConfig::Direction::RIGHT: {
                CLAY(lId, {
                    .layout = {
                        .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_GROW(0) },
                        .layoutDirection = CLAY_LEFT_TO_RIGHT
                    },
                    .backgroundColor = convert_vec4<Clay_Color>(config.backgroundColor),
                    .border = config.border
                }) {
                    CLAY_AUTO_ID({
                        .layout = {
                            .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_GROW(0) },
                            .layoutDirection = CLAY_TOP_TO_BOTTOM
                        }
                    }) {
                        float paddingElementHeight = (gui.io.windowSize.y() - gui.io.safeWindowRect.height()) / 2.0f;
                        CLAY_AUTO_ID({
                            .layout = {.sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIXED(paddingElementHeight)}}
                        }) {}
                        innerContent();
                        CLAY_AUTO_ID({
                            .layout = {.sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIXED(paddingElementHeight)}}
                        }) {}
                    }
                    CLAY_AUTO_ID({
                        .layout = {.sizing = {.width = CLAY_SIZING_FIXED(gui.io.windowSize.x() - gui.io.safeWindowRect.max.x()), .height = CLAY_SIZING_GROW(0)}}
                    }) {}
                }
                break;
            }
        }
    });
}

}}
