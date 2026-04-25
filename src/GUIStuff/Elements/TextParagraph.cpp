#include "TextParagraph.hpp"
#include <include/core/SkPath.h>
#include <iostream>
#include "../GUIManager.hpp"
#include "Helpers/ConvertVec.hpp"

namespace GUIStuff {

TextParagraph::TextParagraph(GUIManager& gui):
    Element(gui) {}

void TextParagraph::layout(const Clay_ElementId& id, const Data& newData) {
    bool redraw = false;

    if(!textbox) {
        d = newData;
        textbox = std::make_unique<RichText::TextBox>();
        textbox->set_rich_text_data(d.text);
        textbox->set_ellipsis(newData.ellipsis);
        textbox->set_font_data(gui.io.fonts);
        textbox->set_allow_newlines(newData.allowNewlines);
        textbox->set_max_width(newData.maxGrowX);
        textbox->set_max_height(newData.maxGrowY);
        skia::textlayout::TextStyle tStyle;
        tStyle.setFontFamilies(gui.io.fonts->get_default_font_families());
        tStyle.setFontSize(gui.io.fontSize);
        textbox->set_initial_text_style(tStyle);
        fontSize = gui.io.fontSize;
        redraw = true;
    }

    if(d.allowNewlines != newData.allowNewlines) {
        d.allowNewlines = newData.allowNewlines;
        textbox->set_allow_newlines(d.allowNewlines);
        redraw = true;
    }

    if(gui.io.fontSize != fontSize) {
        fontSize = gui.io.fontSize;
        skia::textlayout::TextStyle tStyle;
        tStyle.setFontFamilies(gui.io.fonts->get_default_font_families());
        tStyle.setFontSize(gui.io.fontSize);
        textbox->set_initial_text_style(tStyle);
        redraw = true;
    }

    if(d.text != newData.text) {
        d.text = newData.text;
        textbox->set_rich_text_data(d.text);
        redraw = true;
    }

    if(d.maxGrowX != newData.maxGrowX) {
        d.maxGrowX = newData.maxGrowX;
        textbox->set_max_width(d.maxGrowX);
        redraw = true;
    }
    if(d.maxGrowY != newData.maxGrowY) {
        d.maxGrowY = newData.maxGrowY;
        textbox->set_max_height(d.maxGrowY);
        redraw = true;
    }

    if(d.ellipsis != newData.ellipsis) {
        d.ellipsis = newData.ellipsis;
        textbox->set_ellipsis(d.ellipsis);
        redraw = true;
    }

    if(redraw)
        gui.invalidate_draw_element(this);

    CLAY(id, {
        .layout = {
            .sizing = {.width = CLAY_SIZING_GROW(0, textbox->get_width()), .height = CLAY_SIZING_FIXED(textbox->get_height())}
        },
        .custom = { .customData = this }
    }) {}
}

void TextParagraph::clay_draw(SkCanvas* canvas, UpdateInputData& io, Clay_RenderCommand* command, bool skiaAA) {
    auto& bb = boundingBox.value();
    canvas->save();
    canvas->translate(bb.min.x(), bb.min.y());
    skia::textlayout::TextStyle tStyle;
    tStyle.setFontFamilies(gui.io.fonts->get_default_font_families());
    tStyle.setFontSize(fontSize);
    tStyle.setForegroundPaint(SkPaint{io.theme->frontColor1});
    textbox->set_initial_text_style(tStyle);
    textbox->paint(canvas, { .skiaAA = skiaAA });
    canvas->restore();
}

}
