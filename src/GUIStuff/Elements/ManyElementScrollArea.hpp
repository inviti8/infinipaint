#pragma once
#include "Element.hpp"
#include "Helpers/ConvertVec.hpp"
#include "ScrollArea.hpp"

namespace GUIStuff {

class ManyElementScrollArea : public Element {
    public:
        ManyElementScrollArea(GUIManager& gui);

        struct Options {
            float entryHeight;
            size_t entryCount;
            bool clipHorizontal = false;
            Clay_LayoutAlignmentX xAlign = CLAY_ALIGN_X_LEFT;
            Clay_SizingAxis xElementSize = CLAY_SIZING_GROW(0);
            ScrollArea::ScrollbarType scrollbar = ScrollArea::ScrollbarType::NORMAL;
            std::function<void(size_t elementIndex)> elementContent;
            std::function<void(const ScrollArea::InnerContentParameters&)> innerContentExtraCallback;
        };

        void layout(const Clay_ElementId& id, const Options& opts);

        ScrollArea* scrollArea = nullptr;
    private:
        Options options;
};

}
