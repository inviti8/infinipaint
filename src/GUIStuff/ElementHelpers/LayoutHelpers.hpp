#pragma once
#include "../GUIManager.hpp"
#include "../Elements/LayoutElement.hpp"

namespace GUIStuff { namespace ElementHelpers {

void top_to_bottom_window_popup_layout(GUIManager& gui, const char* id, Clay_SizingAxis x, Clay_SizingAxis y, const std::function<void(LayoutElement*)>& innerContent, const LayoutElement::Callbacks& callbacks);
void left_to_right_layout(GUIManager& gui, Clay_SizingAxis x, Clay_SizingAxis y, const std::function<void()>& innerContent);
void left_to_right_line_layout(GUIManager& gui, const std::function<void()>& innerContent);
void left_to_right_line_centered_layout(GUIManager& gui, const std::function<void()>& innerContent);

struct WindowFillSideBarConfig {
    enum class Direction {
        TOP,
        BOTTOM,
        LEFT,
        RIGHT
    } dir = Direction::TOP;
    SkColor4f backgroundColor = {0.0f, 0.0f, 0.0f, 0.0f};
    Clay_BorderElementConfig border;
};

void window_fill_side_bar(GUIManager& gui, const char* id, const WindowFillSideBarConfig& config, const std::function<void()>& innerContent);
void window_gap_side_bar(GUIManager& gui, const char* id, const WindowFillSideBarConfig::Direction& dir);

}}
