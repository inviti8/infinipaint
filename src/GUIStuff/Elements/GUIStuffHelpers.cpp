#include "Element.hpp"
#include "Helpers/MathExtras.hpp"
#include <chrono>

namespace GUIStuff {
    SCollision::AABB<float> clay_bounding_box_to_aabb(const Clay_BoundingBox& bb) {
        return {{bb.x, bb.y}, {bb.x + bb.width, bb.y + bb.height}};
    }

    SkFont get_setup_skfont() {
        SkFont font;
        font.setLinearMetrics(true);
        font.setHinting(SkFontHinting::kNormal);
        //font.setForceAutoHinting(true);
        font.setSubpixel(true);
        font.setBaselineSnap(true);
        font.setEdging(SkFont::Edging::kSubpixelAntiAlias);
        //paint.setAntiAlias(true);
        return font;
    }

    SkFont UpdateInputData::get_font(float fSize) const {
        SkFont f = get_setup_skfont();
        f.setTypeface(textTypeface);
        f.setSize(fSize);
        return f;
    }

    std::shared_ptr<Theme> get_default_dark_mode() {
        // Colors match the "Default" dark-mode preset in the HEAVYMETA portal
        // (heavymeta_collective/static/theme_presets.json) — purple accent +
        // gold link on near-black, so Inkternity reads as the same product
        // family as the portal it ships from.
        std::shared_ptr<Theme> theme(std::make_shared<Theme>());
        theme->fillColor1 = {0.659f, 0.478f, 1.000f, 1.0f}; // #a87aff portal accent (purple)
        theme->fillColor2 = {0.831f, 0.659f, 0.263f, 1.0f}; // #d4a843 portal link (gold)
        theme->backColor0 = {0.102f, 0.102f, 0.102f, 1.0f}; // #1a1a1a portal bg
        theme->backColor1 = {0.165f, 0.165f, 0.165f, 1.0f}; // #2a2a2a portal card
        theme->backColor2 = {0.267f, 0.267f, 0.267f, 1.0f}; // #444444 portal border
        theme->frontColor1 = {0.941f, 0.941f, 0.941f, 1.0f}; // #f0f0f0 portal text
        theme->frontColor2 = {0.659f, 0.659f, 0.659f, 1.0f}; // derived: 70% of frontColor1
        return theme;
    }
    
    void SelectionHelper::update(bool isHovering, bool isLeftClick, bool isLeftHeld, const Vector2f& cursorPos) {
        hovered = isHovering;
    
        clicked = false;
        tapped = false;
        justUnselected = false;
        bool oldSelected = selected;
    
        if(isLeftClick) {
            if(hovered) {
                held = true;
                clicked = true;
                selected = true;
                timeClicked = std::chrono::steady_clock::now();
                clickPos = cursorPos;
            }
            else
                selected = false;
        }
        else if(!isLeftHeld) {
            if(held && hovered && (std::chrono::steady_clock::now() - timeClicked) < std::chrono::milliseconds(250) && vec_distance_sqrd(clickPos, cursorPos) < (25 * 25))
                tapped = true;
            held = false;
        }

        if(!selected && oldSelected)
            justUnselected = true;
    }
}
