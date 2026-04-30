#include "PhoneDrawingProgramScreen.hpp"
#include "../MainProgram.hpp"
#include "DrawingProgramScreen.hpp"
#include "Helpers/ConvertVec.hpp"
#include "../GUIStuff/Elements/GridScrollArea.hpp"
#include "../GUIStuff/ElementHelpers/ButtonHelpers.hpp"
#include "../GUIStuff/ElementHelpers/LayoutHelpers.hpp"
#include "../GUIStuff/ElementHelpers/TextLabelHelpers.hpp"
#include "../GUIStuff/ElementHelpers/ScrollAreaHelpers.hpp"
#include "FileSelectScreen.hpp"
#include <Helpers/Logger.hpp>

using namespace GUIStuff;
using namespace ElementHelpers;

PhoneDrawingProgramScreen::PhoneDrawingProgramScreen(MainProgram& m):
    DrawingProgramScreen(m)
{}

void PhoneDrawingProgramScreen::update() {
    DrawingProgramScreen::update();
}

void PhoneDrawingProgramScreen::gui_layout_run() {
    main_display();
}

void PhoneDrawingProgramScreen::main_display() {
    CLAY_AUTO_ID({
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
            .childGap = main.g.gui.io.theme->childGap1,
            .childAlignment = {.x = CLAY_ALIGN_X_CENTER},
            .layoutDirection = CLAY_TOP_TO_BOTTOM
        },
    }) {
        top_toolbar();
        CLAY_AUTO_ID({
            .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)}}
        }) {}
        bottom_toolbar();
    }
}

void PhoneDrawingProgramScreen::top_toolbar() {
    auto& gui = main.g.gui;
    auto& io = gui.io;

    gui.element<LayoutElement>("top toolbar", [&](LayoutElement*, const Clay_ElementId& lId) {
        CLAY(lId, {}) {
            window_fill_side_bar(gui, {
                .dir = WindowFillSideBarConfig::Direction::TOP,
                .backgroundColor = io.theme->backColor0,
                .border = {
                    .color = convert_vec4<Clay_Color>(io.theme->frontColor1),
                    .width = {.bottom = 1}
                }
            }, [&] {
                CLAY_AUTO_ID({
                    .layout = {
                        .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
                        .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
                        .layoutDirection = CLAY_LEFT_TO_RIGHT
                    }
                }) {
                    svg_icon_button(gui, "back exit button", "data/icons/RemixIcon/arrow-left-s-line.svg", {
                        .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
                        .onClick = [&] {
                            main.world->save_to_file(main.world->filePath);
                            main.set_tab_to_close(main.world.get());
                            main.set_screen([&] (std::unique_ptr<Screen>) { return std::make_unique<FileSelectScreen>(main); });
                        }
                    });
                    text_label(gui, main.world->name);
                }
            });
        }
    });
}

void PhoneDrawingProgramScreen::bottom_toolbar() {
    auto& gui = main.g.gui;
    auto& io = gui.io;
    window_fill_side_bar(gui, {
        .dir = WindowFillSideBarConfig::Direction::BOTTOM,
    }, [&] {
        CLAY_AUTO_ID({
            .layout = {
                .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
                .childGap = io.theme->childGap1,
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}
            }
        }) {
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0) },
                    .childGap = io.theme->childGap1,
                    .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_BOTTOM},
                    .layoutDirection = CLAY_TOP_TO_BOTTOM
                }
            }) {
                CLAY_AUTO_ID({
                    .layout = {
                        .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
                        .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_BOTTOM},
                        .layoutDirection = CLAY_LEFT_TO_RIGHT
                    }
                }) {
                    CLAY_AUTO_ID({
                        .layout = {
                            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
                        }
                    }) {}
                    switch(settingsMenuPopup) {
                        case SettingsMenuPopup::NONE:
                            break;
                        case SettingsMenuPopup::SETTINGS:
                            tool_settings_popup();
                            break;
                        case SettingsMenuPopup::FG_COLOR:
                            SCROLL_AREA_BUG_WORKAROUND();
                            color_settings_popup(&main.toolConfig.globalConf.foregroundColor);
                            break;
                        case SettingsMenuPopup::BG_COLOR:
                            SCROLL_AREA_BUG_WORKAROUND();
                            color_settings_popup(&main.toolConfig.globalConf.backgroundColor);
                            break;
                    }
                }

                CLAY_AUTO_ID({
                    .layout = {
                        .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
                        .childGap = io.theme->childGap1,
                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                        .layoutDirection = CLAY_LEFT_TO_RIGHT
                    }
                }) {
                    gui.element<LayoutElement>("bottom toolbar", [&](LayoutElement*, const Clay_ElementId& lId) {
                        CLAY(lId, {
                            .layout = {
                                .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)}
                            },
                            .backgroundColor = convert_vec4<Clay_Color>(io.theme->backColor1),
                            .cornerRadius = CLAY_CORNER_RADIUS(io.theme->windowCorners1)
                        }) {
                            gui.clipping_element<ScrollArea>("tools scroll", ScrollArea::Options{
                                .scrollHorizontal = true,
                                .clipHorizontal = true,
                                .scrollbarX = ScrollArea::ScrollbarType::NONE,
                                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                .xAlign = CLAY_ALIGN_X_LEFT,
                                .yAlign = CLAY_ALIGN_Y_CENTER,
                                .innerContent = [&](auto&) {
                                    bottom_toolbar_gui();
                                }
                            });
                        }
                    });
                    gui.element<LayoutElement>("bottom extra toolbar", [&](LayoutElement*, const Clay_ElementId& lId) {
                        CLAY(lId, {
                            .layout = {
                                .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
                                .layoutDirection = CLAY_LEFT_TO_RIGHT
                            },
                            .backgroundColor = convert_vec4<Clay_Color>(io.theme->backColor1),
                            .cornerRadius = CLAY_CORNER_RADIUS(io.theme->windowCorners1)
                        }) {
                            bottom_extra_toolbar_gui();
                        }
                    });
                }
            }
        }
    });
}

void PhoneDrawingProgramScreen::bottom_toolbar_gui() {
    GUIManager& gui = main.g.gui;
    auto& drawP = main.world->drawProg;

    auto tool_button = [&](const char* id, const std::string& svgPath, DrawingProgramToolType toolType) {
        svg_icon_button(gui, id, svgPath, {
            .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
            .isSelected = drawP.drawTool->get_type() == toolType,
            .onClick = [&, toolType] {
                drawP.switch_to_tool(toolType);
            }
        });
    };

    tool_button("Brush Toolbar Button", "data/icons/brush.svg", DrawingProgramToolType::BRUSH);
    tool_button("Eraser Toolbar Button", "data/icons/eraser.svg", DrawingProgramToolType::ERASER);
    tool_button("Line Toolbar Button", "data/icons/line.svg", DrawingProgramToolType::LINE);
    tool_button("Text Toolbar Button", "data/icons/text.svg", DrawingProgramToolType::TEXTBOX);
    tool_button("Ellipse Toolbar Button", "data/icons/circle.svg", DrawingProgramToolType::ELLIPSE);
    tool_button("Rect Toolbar Button", "data/icons/rectangle.svg", DrawingProgramToolType::RECTANGLE);
    tool_button("RectSelect Toolbar Button", "data/icons/rectselect.svg", DrawingProgramToolType::RECTSELECT);
    tool_button("LassoSelect Toolbar Button", "data/icons/lassoselect.svg", DrawingProgramToolType::LASSOSELECT);
    tool_button("Edit Toolbar Button", "data/icons/cursor.svg", DrawingProgramToolType::EDIT);
    tool_button("Eyedropper Toolbar Button", "data/icons/eyedropper.svg", DrawingProgramToolType::EYEDROPPER);
    tool_button("Zoom Canvas Toolbar Button", "data/icons/zoom.svg", DrawingProgramToolType::ZOOM);
    tool_button("Pan Canvas Toolbar Button", "data/icons/hand.svg", DrawingProgramToolType::PAN);
}

void PhoneDrawingProgramScreen::tool_settings_popup() {
    auto& drawP = main.world->drawProg;
    auto& gui = main.g.gui;
    auto& io = gui.io;
 
    gui.element<LayoutElement>("tool settings popup", [&] (LayoutElement*, const Clay_ElementId& lId) {
        CLAY(lId, {
            .layout = {
                .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
            },
            .backgroundColor = convert_vec4<Clay_Color>(io.theme->backColor1),
            .cornerRadius = CLAY_CORNER_RADIUS(io.theme->windowCorners1)
        }) {
            gui.clipping_element<ScrollArea>("toolbox scroll area", ScrollArea::Options{
                .scrollVertical = true,
                .clipVertical = true,
                .scrollbarY = ScrollArea::ScrollbarType::NORMAL,
                .innerContent = [&](auto&) {
                    CLAY_AUTO_ID({
                        .layout = {
                            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                            .padding = CLAY_PADDING_ALL(io.theme->padding1),
                            .childGap = io.theme->childGap1,
                            .layoutDirection = CLAY_TOP_TO_BOTTOM
                        },
                    }) {
                        drawP.drawTool->gui_phone_toolbox(*this);
                    }
                }
            });
        }
    });
}

void PhoneDrawingProgramScreen::color_settings_popup(Vector4f* color) {
    auto& gui = main.g.gui;
    auto& palette = main.conf.palettes[paletteData.selectedPalette];
    gui.element<LayoutElement>("color settings popup", [&] (LayoutElement*, const Clay_ElementId& lId) {
        CLAY(lId, {
            .layout = {
                .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)},
            },
            .backgroundColor = convert_vec4<Clay_Color>(gui.io.theme->backColor1),
            .cornerRadius = CLAY_CORNER_RADIUS(gui.io.theme->windowCorners1)
        }) {
            gui.element<GridScrollArea>("color selector grid", GridScrollArea::Options{
                .entryWidth = BIG_BUTTON_SIZE,
                .childAlignmentX = CLAY_ALIGN_X_CENTER,
                .entryHeight = BIG_BUTTON_SIZE,
                .entryCount = palette.colors.size(),
                .scrollbar = ScrollArea::ScrollbarType::NORMAL,
                .elementContent = [&](size_t i) {
                    auto newC = std::make_shared<Vector3f>(palette.colors[i].x(), palette.colors[i].y(), palette.colors[i].z());
                    color_button(gui, "c", newC.get(), {
                        .isSelected = newC->x() == color->x() && newC->y() == color->y() && newC->z() == color->z(),
                        .hasAlpha = false,
                        .onClick = [newC, color] {
                            // We want to keep the old color's alpha
                            color->x() = newC->x();
                            color->y() = newC->y();
                            color->z() = newC->z();
                        }
                    });
                }
            });
        }
    });
}

void PhoneDrawingProgramScreen::bottom_extra_toolbar_gui() {
    GUIManager& gui = main.g.gui;

    svg_icon_button(gui, "tool settings", "data/icons/RemixIcon/settings-3-line.svg", {
        .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
        .isSelected = settingsMenuPopup == SettingsMenuPopup::SETTINGS,
        .onClick = [&] {
            settingsMenuPopup = (settingsMenuPopup == SettingsMenuPopup::SETTINGS) ? SettingsMenuPopup::NONE : SettingsMenuPopup::SETTINGS;
        }
    });

    color_button(gui, "foreground color", &main.toolConfig.globalConf.foregroundColor, {
        .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
        .isSelected = settingsMenuPopup == SettingsMenuPopup::FG_COLOR,
        .hasAlpha = true,
        .onClick = [&] {
            settingsMenuPopup = (settingsMenuPopup == SettingsMenuPopup::FG_COLOR) ? SettingsMenuPopup::NONE : SettingsMenuPopup::FG_COLOR;
        }
    });

    color_button(gui, "background color", &main.toolConfig.globalConf.backgroundColor, {
        .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
        .isSelected = settingsMenuPopup == SettingsMenuPopup::BG_COLOR,
        .hasAlpha = true,
        .onClick = [&] {
            settingsMenuPopup = (settingsMenuPopup == SettingsMenuPopup::BG_COLOR) ? SettingsMenuPopup::NONE : SettingsMenuPopup::BG_COLOR;
        }
    });
}

void PhoneDrawingProgramScreen::input_global_back_button_callback() {
    main.world->save_to_file(main.world->filePath);
    main.set_tab_to_close(main.world.get());
    main.g.gui.set_to_layout();
    main.set_screen([&] (std::unique_ptr<Screen>) { return std::make_unique<FileSelectScreen>(main); });
}

void PhoneDrawingProgramScreen::input_app_about_to_go_to_background_callback() {
    main.world->save_to_file(main.world->filePath);
}
