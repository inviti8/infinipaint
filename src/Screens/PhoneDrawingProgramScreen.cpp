#include "PhoneDrawingProgramScreen.hpp"
#include "../MainProgram.hpp"
#include "DrawingProgramScreen.hpp"
#include "Helpers/ConvertVec.hpp"
#include "../GUIStuff/ElementHelpers/ButtonHelpers.hpp"
#include "../GUIStuff/ElementHelpers/LayoutHelpers.hpp"
#include "../GUIStuff/ElementHelpers/TextLabelHelpers.hpp"
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
    auto& drawProg = main.world->drawProg;
    window_fill_side_bar(gui, {
        .dir = WindowFillSideBarConfig::Direction::BOTTOM,
    }, [&] {
        CLAY_AUTO_ID({
            .layout = {
                .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                .layoutDirection = CLAY_LEFT_TO_RIGHT
            }
        }) {
            gui.element<LayoutElement>("bottom toolbar", [&](LayoutElement*, const Clay_ElementId& lId) {
                CLAY(lId, {
                    .layout = {
                        .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0) },
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
                            drawProg.phone_bottom_toolbar_gui(*this);
                        }
                    });
                }
            });
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
