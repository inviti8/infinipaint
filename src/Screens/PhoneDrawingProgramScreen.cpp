#include "PhoneDrawingProgramScreen.hpp"
#include "../MainProgram.hpp"
#include "DrawingProgramScreen.hpp"
#include "Helpers/ConvertVec.hpp"
#include "../GUIStuff/Elements/LayoutElement.hpp"
#include "../GUIStuff/ElementHelpers/ButtonHelpers.hpp"
#include "../GUIStuff/ElementHelpers/LayoutHelpers.hpp"
#include "FileSelectScreen.hpp"

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
    }
}

void PhoneDrawingProgramScreen::top_toolbar() {
    auto& gui = main.g.gui;
    auto& io = gui.io;

    window_fill_side_bar(gui, "top toolbar", {
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
                    main.set_tab_to_close(main.world.get());
                    main.screen = std::make_unique<FileSelectScreen>(main);
                }
            });
        }
    });
}
