#include "GridScrollArea.hpp"
#include "../ElementHelpers/ScrollAreaHelpers.hpp"
#include "Helpers/ConvertVec.hpp"
#include "../GUIManager.hpp"

namespace GUIStuff {

GridScrollArea::GridScrollArea(GUIManager& gui): Element(gui) {}

void GridScrollArea::layout(const Clay_ElementId& id, const Options& o) {
    using namespace ElementHelpers;

    options = o;

    ScrollBarManyEntriesOptions opts;
    opts.clipHorizontal = true;
    opts.entryHeight = options.entryHeight;
    opts.innerContentExtraCallback = [&, extraCallback = options.innerContentExtraCallback] (const ScrollArea::InnerContentParameters& contParams) {
        rowWidth = std::fabs(contParams.containerDimensions.x());
        if(extraCallback) extraCallback(contParams);
    };
    size_t entriesPerRow = std::max<size_t>(rowWidth / options.entryWidth, 1);
    size_t rowCount = (options.entryCount + entriesPerRow - 1) / entriesPerRow;
    opts.entryCount = rowCount;
    uint16_t xPadding = 0;
    if(rowCount > 1 || (rowCount == 1 && ((options.entryCount + entriesPerRow) / entriesPerRow) > 1)) // More than 1 row or 1st row is full
        xPadding = static_cast<float>(std::max<float>(rowWidth - (entriesPerRow * options.entryWidth), 0) / (entriesPerRow * 2));

    opts.elementContent = [&] (size_t rowIndex) {
        gui.new_id("grid row", [&] {
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .padding = {.left = xPadding, .right = xPadding},
                    .childGap = xPadding,
                    .childAlignment = { .x = options.childAlignmentX}
                }
            }) {
                size_t startIndex = rowIndex * entriesPerRow;
                size_t endIndex = std::min(rowIndex * entriesPerRow + entriesPerRow, o.entryCount);
                for(size_t i = startIndex; i < endIndex; i++) {
                    gui.new_id(static_cast<int64_t>(i - startIndex), [&] {
                        CLAY_AUTO_ID({
                            .layout = {
                                .sizing = {.width = CLAY_SIZING_FIXED(options.entryWidth), .height = CLAY_SIZING_FIXED(options.entryHeight)},
                            }
                        }) {
                            options.elementContent(i);
                        }
                    });
                }
            }
        });
    };
    CLAY(id, {
        .layout = {
            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)}
        }
    }) {
        scroll_area_many_entries(gui, "scroll many area", opts);
    }
}

}
