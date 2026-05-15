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

        // P0-C0 (DISTRIBUTION-PHASE0.md): networking menu state. The
        // phone top-bar previously had no Host/Connect surface — only
        // the desktop Toolbar exposed it. Without this, no Phase 0
        // testing is possible on touch devices (which is the only screen
        // FileSelectScreen instantiates today).
        bool mainMenuPopupOpen = false;
        enum class PhoneNetMenu : uint8_t {
            NONE,
            HOST,         // pre-host: showing the lobby address + Host/Cancel
            CONNECT,      // pre-connect: paste address, Connect/Cancel
            LOBBY_INFO    // already hosting/connected: show address + copy
        } phoneNetMenu = PhoneNetMenu::NONE;
        // Mirror of Toolbar's serverToConnectTo / serverLocalID. Held
        // here so HOST flow can pre-generate the address before the user
        // confirms (matching desktop UX in Toolbar.cpp:577-580).
        std::string phoneNetLobbyAddress;
        std::string phoneNetLocalID;
        // P0-D1: subscriber-paste field for CONNECT mode. Empty for
        // vanilla collab joins; non-empty when joining a published
        // canvas as a subscriber.
        std::string phoneNetSubscriberToken;
        // Selected hosting mode for the HOST menu. Set when the menu
        // opens (SUBSCRIPTION if canvas has portal metadata, COLLAB
        // otherwise) and passed into World::start_hosting on confirm.
        HostMode phoneHostMode = HostMode::COLLAB;
        void main_menu_popup(GUIStuff::Element* triggerButton);
        void network_menu_popup();

        // DISTRIBUTION-PHASE1.md §4 polish — inline canvas rename for
        // the phone top toolbar. Same pattern as Toolbar::canvasNameInput
        // on desktop: per-frame sync from filePath.stem() unless the
        // input is currently focused, commit on Enter via
        // World::rename_on_disk.
        std::string canvasNameInput;
        bool canvasNameInputFocused = false;
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
