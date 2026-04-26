#include "PositionAdjustingPopupMenu.hpp"
#include "../GUIManager.hpp"
#include "Helpers/ConvertVec.hpp"

namespace GUIStuff {

PositionAdjustingPopupMenu::PositionAdjustingPopupMenu(GUIManager& gui): Element(gui) {}

void PositionAdjustingPopupMenu::layout(const Clay_ElementId& id, Vector2f popupPos, const std::function<void()>& innerContent, const LayoutElement::Callbacks& callbacks) {
    if(layoutElement && layoutElement->get_bb().has_value()) {
        auto& bb = layoutElement->get_bb().value();
        if((popupPos.y() + bb.height()) > gui.io.windowSize.y())
            popupPos.y() -= bb.height();
        if((popupPos.x() + bb.width()) > gui.io.windowSize.y())
            popupPos.x() -= bb.width();
    }

    layoutElement = gui.element<LayoutElement>("popup", [&] (LayoutElement*, const Clay_ElementId& lId) {
        CLAY(lId, {
            .floating = {
                .offset = {popupPos.x(), popupPos.y()},
                .zIndex = gui.get_z_index(),
                .attachTo = CLAY_ATTACH_TO_ROOT
            }
        }) {
            innerContent();
        }
    }, callbacks);
}

}
