#pragma once

#include "Element.hpp"

namespace GUIStuff {

class ImageDisplay : public Element {
    public:
        ImageDisplay(GUIManager& gui);
        struct Data {
            std::filesystem::path imgPath;
            float radius = 0.0f;
            bool operator==(const Data& d) const = default;
        };
        void layout(const Clay_ElementId& id, const Data& data);
        virtual void clay_draw(SkCanvas* canvas, UpdateInputData& io, Clay_RenderCommand* command, bool skiaAA) override;

    private:
        Data d;
        sk_sp<SkImage> img;
};

}
