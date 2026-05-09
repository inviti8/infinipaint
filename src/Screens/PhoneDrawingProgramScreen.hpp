#pragma once
#include "DrawingProgramScreen.hpp"
#include "../Toolbar.hpp"
#include "../GUIStuff/Elements/TreeListing.hpp"

class PhoneDrawingProgramScreen : public DrawingProgramScreen {
    public:
        PhoneDrawingProgramScreen(MainProgram& m);
        virtual void update() override;
        virtual void gui_layout_run() override;
        virtual void input_global_back_button_callback() override;
        virtual void input_app_about_to_go_to_background_callback() override;
    private:
        void bottom_toolbar_gui();
        void bottom_extra_toolbar_gui();
        void save_files();
        void top_toolbar();
        void bottom_toolbar();
        void main_display();
        void tool_settings_popup();
        void color_settings_popup(Vector4f* color);
        enum class SettingsMenuPopup {
            NONE,
            SETTINGS,
            FG_COLOR,
            BG_COLOR
        } settingsMenuPopup = SettingsMenuPopup::NONE;
        // PHASE2 C4: top-bar layer dropdown selection index.
        // 0=Sketch, 1=Color, 2=Ink. Synced from the active layer's
        // kind each frame; default Ink (matches the new-world default
        // editingLayer).
        size_t layerDropdownIndex = 2;
        // PHASE2 (M6 followup): layer-manager popup state. Phone layout
        // didn't carry a layer-manager surface pre-Phase-2; without
        // this the Sketch/Color/Ink visibility/alpha/blend controls
        // would be unreachable from the phone toolbar.
        bool layerMenuPopupOpen = false;
        void layer_menu_popup(GUIStuff::Element* triggerButton);
        struct ColorPickerPopupData {
            enum class ScreenType {
                NORMAL,
                EXTRA,
                PALETTES
            } screenType = ScreenType::NORMAL;
            size_t selectedPalette = 0;
            std::string newPaletteStr;
            std::set<GUIStuff::TreeListingObjIndexList> paletteListSelection;
        } colorPickerPopupData;
        void reset_color_picker_popup_data();
};
