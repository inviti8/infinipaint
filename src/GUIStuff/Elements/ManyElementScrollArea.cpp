#include "ManyElementScrollArea.hpp"
#include "../GUIManager.hpp"

namespace GUIStuff {

ManyElementScrollArea::ManyElementScrollArea(GUIManager& gui): Element(gui) {}

void ManyElementScrollArea::layout(const Clay_ElementId& id, const Options& opts) {
    options = opts;
    CLAY(id, {
        .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)}}
    }) {
        if(boundingBox.has_value()) {
            scrollArea = gui.clipping_element<ScrollArea>("scroll area", ScrollArea::Options{
                .scrollVertical = true,
                .clipHorizontal = options.clipHorizontal,
                .clipVertical = true,
                .scrollbarY = options.scrollbar,
                .xAlign = options.xAlign,
                .yAlign = CLAY_ALIGN_Y_TOP,
                .innerContent = [&](const ScrollArea::InnerContentParameters& params) {
                    if(options.innerContentExtraCallback) options.innerContentExtraCallback(params);

                    float absScrollAmount = std::fabs(params.scrollOffset->y());
                    size_t startPoint = absScrollAmount / options.entryHeight;
                    size_t elementsContainable = (boundingBox.value().height() / options.entryHeight) + 2; // Displaying 2 more elements ensures that there isn't any empty space in the container
                    size_t endPoint = std::min(options.entryCount, startPoint + elementsContainable);

                    if(startPoint != 0) {
                        CLAY_AUTO_ID({
                            .layout = {
                                .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(startPoint * options.entryHeight)},
                            }
                        }) { }
                    }

                    gui.new_id("many element list", [&] {
                        for(size_t i = startPoint; i < endPoint; i++) {
                            gui.new_id(i - startPoint, [&] {
                                CLAY_AUTO_ID({
                                    .layout = { .sizing = {.width = options.xElementSize, .height = CLAY_SIZING_FIXED(options.entryHeight)}}
                                }) {
                                    options.elementContent(i);
                                }
                            });
                        }
                    });

                    if(endPoint != options.entryCount) {
                        CLAY_AUTO_ID({
                            .layout = {
                                .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED((options.entryCount - endPoint) * options.entryHeight)},
                            }
                        }) { }
                    }
                }
            });
        }
    }
}

}
