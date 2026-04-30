#pragma once
#include "Element.hpp"
#include "Helpers/ConvertVec.hpp"

namespace GUIStuff {

class MutableTextLabel : public Element {
    public:
        MutableTextLabel(GUIManager& gui);
        void layout(const Clay_ElementId& id, const std::string& text, const Clay_TextElementConfig& textConfig);
    private:
        std::string oldText;
};

}
