#include "ImageDisplay.hpp"
#include "../GUIManager.hpp"
#include <include/codec/SkJpegDecoder.h>
#include <include/core/SkRRect.h>

namespace GUIStuff {

ImageDisplay::ImageDisplay(GUIManager& gui): Element(gui) {}

void ImageDisplay::layout(const Clay_ElementId& id, const Data& data) {
    if(data != d) {
        d = data;
        std::string imgData;
        try {
            imgData = read_file_to_string(data.imgPath);
            auto codec = SkCodec::MakeFromData(SkData::MakeWithoutCopy(imgData.c_str(), imgData.size()), {
                SkJpegDecoder::Decoder()
            });
            img = std::get<0>(codec->getImage());
        } catch(...) {
            img = nullptr;
        }
        gui.invalidate_draw_element(this);
    }

    CLAY(id, {
        .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)}},
        .custom = {.customData = this}
    }) {
    }
}

void ImageDisplay::clay_draw(SkCanvas* canvas, UpdateInputData& io, Clay_RenderCommand* command, bool skiaAA) {
    if(img) {
        SkPaint p;
        p.setAntiAlias(skiaAA);
        if(d.radius != 0.0f) {
            canvas->save();
            SkRRect r = SkRRect::MakeRectXY(boundingBox.value().get_sk_rect(), d.radius, d.radius);
            canvas->clipRRect(r, skiaAA);
            canvas->drawImageRect(img, boundingBox.value().get_sk_rect(), {SkFilterMode::kLinear, SkMipmapMode::kNone}, &p);
            canvas->restore();
        }
        else
            canvas->drawImageRect(img, boundingBox.value().get_sk_rect(), {SkFilterMode::kLinear, SkMipmapMode::kNone}, &p);
    }
    else {
        SkPaint p{io.theme->frontColor1};
        p.setAntiAlias(skiaAA);
        SkRRect r = SkRRect::MakeRectXY(boundingBox.value().get_sk_rect(), d.radius, d.radius);
        canvas->drawRRect(r, p);
    }
}

}
