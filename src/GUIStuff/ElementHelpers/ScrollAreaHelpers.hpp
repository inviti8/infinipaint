#pragma once
#include "../Elements/ScrollArea.hpp"
#include "Helpers/ConvertVec.hpp"

namespace GUIStuff { namespace ElementHelpers {

struct ScrollBarManyEntriesOptions {
    float entryHeight;
    size_t entryCount;
    bool clipHorizontal = false;
    bool growing = false; // If set to false, this element has a fixed size. Otherwise, is a growing element.
    Clay_LayoutAlignmentX xAlign = CLAY_ALIGN_X_LEFT;
    Clay_SizingAxis xElementSize = CLAY_SIZING_GROW(0);
    ScrollArea::ScrollbarType scrollbar = ScrollArea::ScrollbarType::NORMAL;
    std::function<void(size_t elementIndex)> elementContent;
    std::function<void(const ScrollArea::InnerContentParameters&)> innerContentExtraCallback;
};

ScrollArea* scroll_area_many_entries(GUIManager& gui, const char* id, const ScrollBarManyEntriesOptions& options);

}}
