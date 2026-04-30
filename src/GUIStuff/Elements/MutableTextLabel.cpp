#include "MutableTextLabel.hpp"
#include "../GUIManager.hpp"

namespace GUIStuff {

MutableTextLabel::MutableTextLabel(GUIManager& gui): Element(gui) {}

void MutableTextLabel::layout(const Clay_ElementId& id, const std::string& text, const Clay_TextElementConfig& textConfig) {
    if(text != oldText) {
        oldText = text;
        gui.invalidate_draw_element(this);
    }
    CLAY(id, {
        .layout = {.sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)}}
    }) {
        CLAY_TEXT(gui.strArena.std_str_to_clay_str(text), textConfig);
    }
}

}
