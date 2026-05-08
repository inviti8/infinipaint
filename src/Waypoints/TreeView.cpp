#include "TreeView.hpp"
#include "../World.hpp"
#include "../MainProgram.hpp"
#include "../GUIStuff/GUIManager.hpp"
#include "../GUIStuff/Elements/LayoutElement.hpp"
#include "../GUIStuff/ElementHelpers/TextLabelHelpers.hpp"
#include "Helpers/ConvertVec.hpp"

TreeView::TreeView(World& w)
    : world(w) {}

void TreeView::gui(GUIStuff::GUIManager& gui) {
    if (!visible) return;
    using namespace GUIStuff;
    using namespace GUIStuff::ElementHelpers;
    auto& io = gui.io;

    // Panel is a fixed-width column on the right side of the canvas.
    // Width chosen empirically — wide enough to read short labels and
    // see a few nodes without overwhelming the canvas.
    constexpr float PANEL_WIDTH = 360.0f;

    gui.element<LayoutElement>("tree view panel", [&] (LayoutElement*, const Clay_ElementId& lId) {
        CLAY(lId, {
            .layout = {
                .sizing = {.width = CLAY_SIZING_FIXED(PANEL_WIDTH), .height = CLAY_SIZING_GROW(0)},
                .padding = CLAY_PADDING_ALL(static_cast<uint16_t>(io.theme->padding1)),
                .childGap = static_cast<uint16_t>(io.theme->childGap1),
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP},
                .layoutDirection = CLAY_TOP_TO_BOTTOM
            },
            .backgroundColor = convert_vec4<Clay_Color>(io.theme->backColor1),
            .cornerRadius = CLAY_CORNER_RADIUS(io.theme->windowCorners1)
        }) {
            text_label_centered(gui, "Tree");
            text_label(gui, "(graph view — M6-b)");
        }
    });
}
