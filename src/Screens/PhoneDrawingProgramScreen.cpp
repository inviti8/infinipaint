#include "PhoneDrawingProgramScreen.hpp"
#include "../MainProgram.hpp"
#include "../Waypoints/TreeView.hpp"
#include "../ReaderMode/ReaderMode.hpp"
#include "DrawingProgramScreen.hpp"
#include "Helpers/CanvasShareId.hpp"
#include "Helpers/ConvertVec.hpp"
#include "../GUIStuff/Elements/GridScrollArea.hpp"
#include "../GUIStuff/Elements/DropDown.hpp"
#include "../GUIStuff/Elements/ColorPicker.hpp"
#include "../GUIStuff/Elements/TreeListing.hpp"
#include "../GUIStuff/ElementHelpers/ButtonHelpers.hpp"
#include "../GUIStuff/ElementHelpers/LayoutHelpers.hpp"
#include "../GUIStuff/ElementHelpers/TextLabelHelpers.hpp"
#include "../GUIStuff/ElementHelpers/TextBoxHelpers.hpp"
#include "FileSelectScreen.hpp"
#include "../Brushes/BrushPresets.hpp"
#include "../CustomEvents.hpp"
#include <Helpers/Networking/NetLibrary.hpp>
#include <Helpers/Logger.hpp>

using namespace GUIStuff;
using namespace ElementHelpers;

PhoneDrawingProgramScreen::PhoneDrawingProgramScreen(MainProgram& m):
    DrawingProgramScreen(m)
{}

void PhoneDrawingProgramScreen::update() {
    DrawingProgramScreen::update();
}

void PhoneDrawingProgramScreen::gui_layout_run() {
    main_display();
}

void PhoneDrawingProgramScreen::main_display() {
    CLAY_AUTO_ID({
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
            .childGap = main.g.gui.io.theme->childGap1,
            .childAlignment = {.x = CLAY_ALIGN_X_CENTER},
            .layoutDirection = CLAY_TOP_TO_BOTTOM
        },
    }) {
        top_toolbar();
        // Horizontal split between the canvas-occupies-this-space stretch
        // and the optional tree-view side panel on the right.
        CLAY_AUTO_ID({
            .layout = {
                .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                .layoutDirection = CLAY_LEFT_TO_RIGHT
            }
        }) {
            CLAY_AUTO_ID({
                .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)}}
            }) {}
            main.world->treeView.gui(main.g.gui);
        }
        // Editor toolbar disappears in reader mode (PHASE1.md §7
        // "toolbar minimized"). The top toolbar stays so the eye-icon
        // toggle remains reachable.
        // P0-D5: viewers (subscribers joined via token) never see the
        // editing toolbar regardless of reader mode — they're read-only.
        const bool isViewer = main.world->ownClientData && main.world->ownClientData->is_viewer();
        if (!main.world->readerMode.is_active() && !isViewer)
            bottom_toolbar();
        // Branch-choice overlay is floating (attached to the screen's
        // bottom-center), so the canvas gets full vertical space
        // underneath while the buttons compose on top. Self-gates on
        // reader-mode + branch-point.
        render_reader_branch_overlay(*main.world, main.g.gui);
    }
}

void PhoneDrawingProgramScreen::top_toolbar() {
    auto& gui = main.g.gui;
    auto& io = gui.io;

    gui.element<LayoutElement>("top toolbar", [&](LayoutElement*, const Clay_ElementId& lId) {
        CLAY(lId, {}) {
            window_fill_side_bar(gui, {
                .dir = WindowFillSideBarConfig::Direction::TOP,
                .backgroundColor = io.theme->backColor0,
                .border = {
                    .color = convert_vec4<Clay_Color>(io.theme->frontColor1),
                    .width = {.bottom = 1}
                }
            }, [&] {
                CLAY_AUTO_ID({
                    .layout = {
                        .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
                        .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
                        .layoutDirection = CLAY_LEFT_TO_RIGHT
                    }
                }) {
                    svg_icon_button(gui, "back exit button", "data/icons/RemixIcon/arrow-left-s-line.svg", {
                        .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
                        .onClick = [&] {
                            // P0-D5: viewers don't save anything — they're
                            // read-only and shouldn't write a phantom local
                            // copy of the artist's canvas.
                            const bool ownIsViewer = main.world->ownClientData && main.world->ownClientData->is_viewer();
                            if (!ownIsViewer)
                                main.world->save_to_file(main.world->filePath);
                            main.set_tab_to_close(main.world.get());
                            main.set_screen([&] (std::unique_ptr<Screen>) { return std::make_unique<FileSelectScreen>(main); });
                        }
                    });
                    // P0-D6: viewer indicator — replace the file name with
                    // a "Viewing live: ..." string so the subscriber knows
                    // they're in read-only mode on someone else's canvas.
                    const bool ownIsViewer = main.world->ownClientData && main.world->ownClientData->is_viewer();
                    if (ownIsViewer)
                        text_label(gui, "Viewing live: " + main.world->name);
                    else
                        text_label(gui, main.world->name);
                    // Spacer that pushes the layer dropdown + reader toggle to the right edge.
                    CLAY_AUTO_ID({
                        .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)}}
                    }) {}
                    // P0-C0: networking menu trigger. The phone top-bar
                    // had no Host/Connect surface before this; tapping
                    // this opens a small popup with Host / Connect /
                    // Lobby Info actions, mirroring the desktop Toolbar
                    // menu but phone-native (centered modal sub-popups).
                    // P0-D5: hidden for viewers — they can only Disconnect
                    // (via the back button), not host/connect to other lobbies.
                    Element* mainMenuButton = nullptr;
                    if (!ownIsViewer) {
                        mainMenuButton = svg_icon_button(gui, "main menu button (phone)", "data/icons/RemixIcon/more-fill.svg", {
                            .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
                            .isSelected = mainMenuPopupOpen,
                            .onClick = [&] { mainMenuPopupOpen = !mainMenuPopupOpen; }
                        });
                    }
                    // PHASE2 C4: Sketch / Color / Ink layer dropdown — picks
                    // which named layer is the active edit target. Hidden in
                    // reader mode (no edit target meaningful with chrome off).
                    if (!main.world->readerMode.is_active() && !ownIsViewer) {
                        // Sync the dropdown's index from whichever named
                        // layer editingLayer currently points at. If the
                        // user has switched to a non-named (DEFAULT) layer
                        // via the layer-manager side panel, the index keeps
                        // its last value.
                        if (auto editLock = main.world->drawProg.layerMan.get_editing_layer().lock()) {
                            switch (editLock->get_kind()) {
                                case LayerKind::SKETCH:  layerDropdownIndex = 0; break;
                                case LayerKind::COLOR:   layerDropdownIndex = 1; break;
                                case LayerKind::INK:     layerDropdownIndex = 2; break;
                                case LayerKind::DEFAULT: break;
                            }
                        }
                        static const std::vector<std::string> layerNames{"Sketch", "Color", "Ink"};
                        CLAY_AUTO_ID({
                            .layout = {.sizing = {.width = CLAY_SIZING_FIXED(140), .height = CLAY_SIZING_FIT(0)},
                                       .padding = {.right = 4}}
                        }) {
                            gui.element<DropDown<size_t>>("layer kind dropdown", &layerDropdownIndex, layerNames, DropdownOptions{
                                .onClick = [&] {
                                    static constexpr LayerKind indexToKind[] = {
                                        LayerKind::SKETCH, LayerKind::COLOR, LayerKind::INK
                                    };
                                    auto target = main.world->drawProg.layerMan.get_named_layer(indexToKind[layerDropdownIndex]);
                                    if (!target.expired())
                                        main.world->drawProg.layerMan.set_editing_layer(target);
                                }
                            });
                        }
                    }
                    // PHASE2 (M6 followup): layer manager button. Phone
                    // layout's top toolbar predates Phase 2 and lacked
                    // any way to reach the layer-manager popup; without
                    // this the visibility/alpha/blend controls on the
                    // Sketch/Color/Ink layers (and any DEFAULT layers)
                    // are unreachable. Hidden in reader mode for parity
                    // with the dropdown.
                    Element* layerMenuButton = nullptr;
                    if (!main.world->readerMode.is_active() && !ownIsViewer) {
                        layerMenuButton = svg_icon_button(gui, "Layer Manager Button", "data/icons/layer.svg", {
                            .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
                            .isSelected = layerMenuPopupOpen,
                            .onClick = [&] { layerMenuPopupOpen = !layerMenuPopupOpen; }
                        });
                    }
                    // Reader-mode toggle: lives in the top toolbar (never
                    // hidden) so the user can exit reader mode after the
                    // bottom editor toolbar disappears.
                    svg_icon_button(gui, "Reader Mode Toggle Button (top)", "data/icons/eyeopen.svg", {
                        .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
                        .isSelected = main.world->readerMode.is_active(),
                        .onClick = [&] { main.world->readerMode.toggle(); }
                    });
                    if (layerMenuPopupOpen && layerMenuButton)
                        layer_menu_popup(layerMenuButton);
                    if (mainMenuPopupOpen && mainMenuButton)
                        main_menu_popup(mainMenuButton);
                }
            });
        }
    });
    // Sub-modal for Host / Connect / Lobby Info. Rendered at the screen
    // root so it overlays everything; gates internally on phoneNetMenu.
    if (phoneNetMenu != PhoneNetMenu::NONE)
        network_menu_popup();
}

void PhoneDrawingProgramScreen::bottom_toolbar() {
    auto& gui = main.g.gui;
    auto& io = gui.io;
    window_fill_side_bar(gui, {
        .dir = WindowFillSideBarConfig::Direction::BOTTOM,
    }, [&] {
        CLAY_AUTO_ID({
            .layout = {
                .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
                .childGap = io.theme->childGap1,
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}
            }
        }) {
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0) },
                    .childGap = io.theme->childGap1,
                    .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_BOTTOM},
                    .layoutDirection = CLAY_TOP_TO_BOTTOM
                }
            }) {
                CLAY_AUTO_ID({
                    .layout = {
                        .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
                        .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_BOTTOM},
                        .layoutDirection = CLAY_LEFT_TO_RIGHT
                    }
                }) {
                    switch(settingsMenuPopup) {
                        case SettingsMenuPopup::NONE:
                            reset_color_picker_popup_data();
                            break;
                        case SettingsMenuPopup::SETTINGS:
                            CLAY_AUTO_ID({
                                .layout = {
                                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
                                }
                            }) {}
                            reset_color_picker_popup_data();
                            tool_settings_popup();
                            break;
                        case SettingsMenuPopup::FG_COLOR:
                        case SettingsMenuPopup::BG_COLOR:
                            if(colorPickerPopupData.screenType == ColorPickerPopupData::ScreenType::NORMAL) {
                                CLAY_AUTO_ID({
                                    .layout = {
                                        .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
                                    }
                                }) {}
                            }
                            color_settings_popup(settingsMenuPopup == SettingsMenuPopup::FG_COLOR ? &main.toolConfig.globalConf.foregroundColor : &main.toolConfig.globalConf.backgroundColor);
                            break;
                    }
                }

                CLAY_AUTO_ID({
                    .layout = {
                        .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
                        .childGap = io.theme->childGap1,
                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                        .layoutDirection = CLAY_LEFT_TO_RIGHT
                    }
                }) {
                    gui.element<LayoutElement>("bottom toolbar", [&](LayoutElement*, const Clay_ElementId& lId) {
                        CLAY(lId, {
                            .layout = {
                                .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)}
                            },
                            .backgroundColor = convert_vec4<Clay_Color>(io.theme->backColor1),
                            .cornerRadius = CLAY_CORNER_RADIUS(io.theme->windowCorners1)
                        }) {
                            gui.clipping_element<ScrollArea>("tools scroll", ScrollArea::Options{
                                .scrollHorizontal = true,
                                .clipHorizontal = true,
                                .scrollbarX = ScrollArea::ScrollbarType::NONE,
                                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                .xAlign = CLAY_ALIGN_X_LEFT,
                                .yAlign = CLAY_ALIGN_Y_CENTER,
                                .innerContent = [&](auto&) {
                                    bottom_toolbar_gui();
                                }
                            });
                        }
                    });
                    gui.element<LayoutElement>("bottom extra toolbar", [&](LayoutElement*, const Clay_ElementId& lId) {
                        CLAY(lId, {
                            .layout = {
                                .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
                                .layoutDirection = CLAY_LEFT_TO_RIGHT
                            },
                            .backgroundColor = convert_vec4<Clay_Color>(io.theme->backColor1),
                            .cornerRadius = CLAY_CORNER_RADIUS(io.theme->windowCorners1)
                        }) {
                            bottom_extra_toolbar_gui();
                        }
                    });
                }
            }
        }
    });
}

void PhoneDrawingProgramScreen::main_menu_popup(Element* triggerButton) {
    auto& gui = main.g.gui;
    auto& io = gui.io;
    gui.set_z_index(gui.get_z_index() + 1, [&] {
        gui.element<LayoutElement>("phone main menu", [&] (LayoutElement*, const Clay_ElementId& lId) {
            CLAY(lId, {
                .layout = {
                    .sizing = {.width = CLAY_SIZING_FIT(220), .height = CLAY_SIZING_FIT(0) },
                    .padding = CLAY_PADDING_ALL(io.theme->padding1),
                    .childGap = io.theme->childGap1,
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP},
                    .layoutDirection = CLAY_TOP_TO_BOTTOM
                },
                .backgroundColor = convert_vec4<Clay_Color>(io.theme->backColor1),
                .cornerRadius = CLAY_CORNER_RADIUS(io.theme->windowCorners1),
                .floating = {
                    .offset = {.x = 0, .y = static_cast<float>(io.theme->padding1)},
                    .zIndex = gui.get_z_index(),
                    .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_TOP, .parent = CLAY_ATTACH_POINT_RIGHT_BOTTOM},
                    .attachTo = CLAY_ATTACH_TO_PARENT
                }
            }) {
                auto menu_item = [&](const char* id, std::string_view label, const std::function<void()>& onClick) {
                    text_button(gui, id, label, {
                        .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
                        .wide = true,
                        .centered = false,
                        .onClick = [&, onClick] {
                            onClick();
                            mainMenuPopupOpen = false;
                        }
                    });
                };
                if (main.world->netObjMan.is_connected()) {
                    menu_item("phone lobby info", "Lobby Info", [&] {
                        phoneNetMenu = PhoneNetMenu::LOBBY_INFO;
                    });
                }
                menu_item("phone host", "Host", [&] {
                    // Pre-generate the lobby address so the user sees it
                    // before confirming. Mirrors Toolbar.cpp host action.
                    phoneHostMode = main.world->has_subscription_metadata()
                        ? HostMode::SUBSCRIPTION : HostMode::COLLAB;
                    if (phoneHostMode == HostMode::SUBSCRIPTION) {
                        // DISTRIBUTION-PHASE0.md §12.5: stable share code
                        // derived from (app_secret, canvas_id).
                        const std::string& appSec = main.devKeys.app_secret();
                        std::string previewGlobal;
                        if (!appSec.empty()) {
                            previewGlobal = CanvasShareId::derive_global_id(appSec, main.world->canvasId);
                            phoneNetLocalID = CanvasShareId::derive_local_id(appSec, main.world->canvasId);
                        }
                        if (previewGlobal.empty() || phoneNetLocalID.empty()) {
                            previewGlobal = NetLibrary::get_global_id();
                            phoneNetLocalID = NetLibrary::deterministic_local_id_from_seed(main.world->canvasId);
                        }
                        phoneNetLobbyAddress = previewGlobal + phoneNetLocalID;
                    } else {
                        phoneNetLocalID = NetLibrary::get_random_server_local_id();
                        phoneNetLobbyAddress = NetLibrary::get_global_id() + phoneNetLocalID;
                    }
                    phoneNetMenu = PhoneNetMenu::HOST;
                });
                menu_item("phone connect", "Connect", [&] {
                    phoneNetLobbyAddress.clear();
                    phoneNetMenu = PhoneNetMenu::CONNECT;
                });
            }
        }, LayoutElement::Callbacks {
            .onClick = [&, triggerButton] (LayoutElement* l, const InputManager::MouseButtonCallbackArgs& button) {
                if(!l->mouseHovering && !l->childMouseHovering && !triggerButton->mouseHovering && button.down) {
                    mainMenuPopupOpen = false;
                    main.g.gui.set_to_layout();
                }
            }
        });
    });
}

void PhoneDrawingProgramScreen::network_menu_popup() {
    auto& gui = main.g.gui;
    auto& io = gui.io;
    // Centered modal — same shape as Toolbar::center_obstructing_window_gui
    // but inlined here so the phone screen doesn't need a Toolbar member.
    gui.set_z_index(100, [&] {
        gui.element<LayoutElement>("phone network menu modal", [&] (LayoutElement*, const Clay_ElementId& lId) {
            CLAY(lId, {
                .layout = {
                    .sizing = {.width = CLAY_SIZING_FIT(600), .height = CLAY_SIZING_FIT(0) },
                    .padding = CLAY_PADDING_ALL(io.theme->padding1),
                    .childGap = io.theme->childGap1,
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP},
                    .layoutDirection = CLAY_TOP_TO_BOTTOM
                },
                .backgroundColor = convert_vec4<Clay_Color>(io.theme->backColor1),
                .cornerRadius = CLAY_CORNER_RADIUS(io.theme->windowCorners1),
                .floating = {
                    .zIndex = gui.get_z_index(),
                    .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                    .attachTo = CLAY_ATTACH_TO_PARENT
                }
            }) {
                switch (phoneNetMenu) {
                    case PhoneNetMenu::HOST: {
                        text_label_centered(gui, "Host this canvas");
                        // Hosting-mode toggle. SUBSCRIPTION is greyed out
                        // unless the canvas has portal-issued metadata
                        // (same rule as the desktop host menu).
                        const bool subEligible = main.world->has_subscription_metadata();
                        text_label(gui, "Mode:");
                        left_to_right_line_layout(gui, [&]() {
                            text_button(gui, "phone host mode collab", "Collab", {
                                .isSelected = (phoneHostMode == HostMode::COLLAB),
                                .wide = true,
                                .onClick = [&] {
                                    if (phoneHostMode != HostMode::COLLAB) {
                                        phoneHostMode = HostMode::COLLAB;
                                        phoneNetLocalID = NetLibrary::get_random_server_local_id();
                                        phoneNetLobbyAddress = NetLibrary::get_global_id() + phoneNetLocalID;
                                    }
                                }
                            });
                            text_button(gui, "phone host mode sub", "Subscription", {
                                .drawType = subEligible
                                    ? SelectableButton::DrawType::FILLED
                                    : SelectableButton::DrawType::TRANSPARENT_ALL,
                                .isSelected = (phoneHostMode == HostMode::SUBSCRIPTION),
                                .wide = true,
                                .onClick = subEligible
                                    ? std::function<void()>([&] {
                                        if (phoneHostMode != HostMode::SUBSCRIPTION) {
                                            phoneHostMode = HostMode::SUBSCRIPTION;
                                            // DISTRIBUTION-PHASE0.md §12.5
                                            const std::string& appSec = main.devKeys.app_secret();
                                            std::string previewGlobal;
                                            if (!appSec.empty()) {
                                                previewGlobal = CanvasShareId::derive_global_id(appSec, main.world->canvasId);
                                                phoneNetLocalID = CanvasShareId::derive_local_id(appSec, main.world->canvasId);
                                            }
                                            if (previewGlobal.empty() || phoneNetLocalID.empty()) {
                                                previewGlobal = NetLibrary::get_global_id();
                                                phoneNetLocalID = NetLibrary::deterministic_local_id_from_seed(main.world->canvasId);
                                            }
                                            phoneNetLobbyAddress = previewGlobal + phoneNetLocalID;
                                        }
                                    })
                                    : std::function<void()>{}
                            });
                        });
                        if(!subEligible) {
                            text_label(gui, "(Publish via portal to enable Subscription)");
                        }
                        input_text_field(gui, "phone host lobby field", "Lobby", &phoneNetLobbyAddress);
                        left_to_right_line_layout(gui, [&]() {
                            text_button(gui, "phone host copy", "Copy Address", {
                                .wide = true,
                                .onClick = [&] { main.input.set_clipboard_str(phoneNetLobbyAddress); }
                            });
                            text_button(gui, "phone host confirm", "Host", {
                                .wide = true,
                                .onClick = [&] {
                                    main.world->start_hosting(phoneHostMode, phoneNetLobbyAddress, phoneNetLocalID);
                                    phoneNetMenu = PhoneNetMenu::NONE;
                                }
                            });
                            text_button(gui, "phone host cancel", "Cancel", {
                                .wide = true,
                                .onClick = [&] { phoneNetMenu = PhoneNetMenu::NONE; }
                            });
                        });
                        break;
                    }
                    case PhoneNetMenu::CONNECT: {
                        text_label_centered(gui, "Connect to a lobby");
                        input_text_field(gui, "phone connect lobby field", "Lobby", &phoneNetLobbyAddress);
                        // P0-D1: optional subscriber token. Leave empty
                        // for vanilla collab; paste a portal-issued
                        // (or dev-minted) token to join a published
                        // canvas as a viewer.
                        input_text_field(gui, "phone connect token field", "Token (optional, for subscribers)", &phoneNetSubscriberToken);
                        left_to_right_line_layout(gui, [&]() {
                            text_button(gui, "phone connect confirm", "Connect", {
                                .wide = true,
                                .onClick = [&] {
                                    if (phoneNetLobbyAddress.length() != (NetLibrary::LOCALID_LEN + NetLibrary::GLOBALID_LEN))
                                        Logger::get().log("USERINFO", "Connect issue: Incorrect address length");
                                    else if (phoneNetLobbyAddress.substr(0, NetLibrary::GLOBALID_LEN) == NetLibrary::get_global_id())
                                        Logger::get().log("USERINFO", "Connect issue: Can't connect to your own address");
                                    else {
                                        CustomEvents::emit_event<CustomEvents::OpenInfiniPaintFileEvent>({
                                            .isClient = true,
                                            .netSource = phoneNetLobbyAddress,
                                            .subscriberToken = phoneNetSubscriberToken
                                        });
                                        phoneNetMenu = PhoneNetMenu::NONE;
                                    }
                                }
                            });
                            text_button(gui, "phone connect cancel", "Cancel", {
                                .wide = true,
                                .onClick = [&] { phoneNetMenu = PhoneNetMenu::NONE; }
                            });
                        });
                        break;
                    }
                    case PhoneNetMenu::LOBBY_INFO: {
                        text_label_centered(gui, "Lobby info");
                        input_text_field(gui, "phone lobby info field", "Lobby", &main.world->netSource);
                        left_to_right_line_layout(gui, [&]() {
                            text_button(gui, "phone lobby info copy", "Copy Address", {
                                .wide = true,
                                .onClick = [&] { main.input.set_clipboard_str(main.world->netSource); }
                            });
                            text_button(gui, "phone lobby info close", "Close", {
                                .wide = true,
                                .onClick = [&] { phoneNetMenu = PhoneNetMenu::NONE; }
                            });
                        });
                        break;
                    }
                    case PhoneNetMenu::NONE:
                        break;
                }
            }
        });
    });
}

void PhoneDrawingProgramScreen::layer_menu_popup(Element* triggerButton) {
    auto& gui = main.g.gui;
    auto& io = gui.io;
    gui.set_z_index(gui.get_z_index() + 1, [&] {
        gui.element<LayoutElement>("phone layer menu", [&] (LayoutElement*, const Clay_ElementId& lId) {
            CLAY(lId, {
                .layout = {
                    .sizing = {.width = CLAY_SIZING_FIT(300), .height = CLAY_SIZING_FIT(0, 600) },
                    .padding = CLAY_PADDING_ALL(io.theme->padding1),
                    .childGap = io.theme->childGap1,
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP},
                    .layoutDirection = CLAY_TOP_TO_BOTTOM
                },
                .backgroundColor = convert_vec4<Clay_Color>(io.theme->backColor1),
                .cornerRadius = CLAY_CORNER_RADIUS(io.theme->windowCorners1),
                .floating = {
                    .offset = {.x = 0, .y = static_cast<float>(io.theme->padding1)},
                    .zIndex = gui.get_z_index(),
                    .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_TOP, .parent = CLAY_ATTACH_POINT_RIGHT_BOTTOM},
                    .attachTo = CLAY_ATTACH_TO_PARENT
                }
            }) {
                text_label_centered(gui, "Layers");
                main.world->drawProg.layerMan.listGUI.setup_list_gui();
            }
        }, LayoutElement::Callbacks {
            .onClick = [&, triggerButton] (LayoutElement* l, const InputManager::MouseButtonCallbackArgs& button) {
                if(!l->mouseHovering && !l->childMouseHovering && !triggerButton->mouseHovering && button.down) {
                    main.world->drawProg.layerMan.listGUI.refresh_gui_data();
                    layerMenuPopupOpen = false;
                    main.g.gui.set_to_layout();
                }
            }
        });
    });
}

void PhoneDrawingProgramScreen::bottom_toolbar_gui() {
    GUIManager& gui = main.g.gui;
    auto& drawP = main.world->drawProg;

    // PHASE2 C6: vector tools that produce non-MyPaint canvas
    // components are hidden from the toolbar when the active layer
    // is Sketch (raster-only). Greying-with-tooltip would be the
    // ideal UX but the SelectableButton primitive has no disabled
    // state yet; hiding is the simplest correct behavior. The
    // raster-friendly tools (MyPaintBrush, Eraser), the world-level
    // tools (Waypoint, ButtonSelect), the selection/edit/inspection
    // tools, and the camera tools all stay available.
    bool sketchActive = false;
    if(auto editLock = drawP.layerMan.get_editing_layer().lock())
        sketchActive = (editLock->get_kind() == LayerKind::SKETCH);
    auto is_vector_tool = [](DrawingProgramToolType t) {
        switch(t) {
            case DrawingProgramToolType::BRUSH:
            case DrawingProgramToolType::LINE:
            case DrawingProgramToolType::TEXTBOX:
            case DrawingProgramToolType::ELLIPSE:
            case DrawingProgramToolType::RECTANGLE:
                return true;
            default:
                return false;
        }
    };

    auto tool_button = [&](const char* id, const std::string& svgPath, DrawingProgramToolType toolType) {
        if(sketchActive && is_vector_tool(toolType)) return;
        svg_icon_button(gui, id, svgPath, {
            .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
            .isSelected = drawP.drawTool->get_type() == toolType,
            .onClick = [&, toolType] {
                drawP.switch_to_tool(toolType);
            }
        });
    };

    tool_button("Brush Toolbar Button", "data/icons/brush.svg", DrawingProgramToolType::BRUSH);

    // PHASE2 M3 follow-up: split the MyPaint brush button into two
    // category-aware buttons so the user can clearly see which family
    // of brushes they're picking from. Both invoke MYPAINTBRUSH; each
    // jumps to the first preset of its category if the current preset
    // is on the wrong side. The brush picker (in tool_settings_popup)
    // filters to the active category.
    //
    // libmypaint is disabled on Android (and Emscripten) — the
    // HVYM::Brushes namespace is gated on HVYM_HAS_LIBMYPAINT in
    // BrushPresets.hpp, so these MyPaint-specific buttons are hidden
    // when the engine isn't compiled in.
#ifdef HVYM_HAS_LIBMYPAINT
    {
        const auto& presets = HVYM::Brushes::curated_presets();
        auto& cfg = main.toolConfig.myPaintBrush;
        const HVYM::Brushes::BrushCategory activeCat =
            (cfg.activePresetIndex >= 0 && cfg.activePresetIndex < static_cast<int>(presets.size()))
                ? presets[cfg.activePresetIndex].category
                : HVYM::Brushes::BrushCategory::SHARP;
        const bool toolIsMyPaint = drawP.drawTool->get_type() == DrawingProgramToolType::MYPAINTBRUSH;

        auto activate_category = [&](HVYM::Brushes::BrushCategory cat) {
            drawP.switch_to_tool(DrawingProgramToolType::MYPAINTBRUSH);
            const auto& ps = HVYM::Brushes::curated_presets();
            auto& c = main.toolConfig.myPaintBrush;
            if (c.activePresetIndex < 0 || c.activePresetIndex >= static_cast<int>(ps.size())
                || ps[c.activePresetIndex].category != cat) {
                for (int i = 0; i < static_cast<int>(ps.size()); ++i) {
                    if (ps[i].category == cat) { c.activePresetIndex = i; break; }
                }
            }
        };

        // Sketch layer is raster-only; both MyPaint buttons are raster
        // (libmypaint), so neither is hidden there.
        svg_icon_button(gui, "Ink Brushes Toolbar Button", "data/icons/ink.svg", {
            .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
            .isSelected = toolIsMyPaint && activeCat == HVYM::Brushes::BrushCategory::SHARP,
            .onClick = [activate_category] { activate_category(HVYM::Brushes::BrushCategory::SHARP); }
        });
        svg_icon_button(gui, "Textured Brushes Toolbar Button", "data/icons/pencil.svg", {
            .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
            .isSelected = toolIsMyPaint && activeCat == HVYM::Brushes::BrushCategory::TEXTURED,
            .onClick = [activate_category] { activate_category(HVYM::Brushes::BrushCategory::TEXTURED); }
        });
    }
#endif // HVYM_HAS_LIBMYPAINT

    tool_button("Waypoint Toolbar Button", "data/icons/bookmark.svg", DrawingProgramToolType::WAYPOINT);
    tool_button("Button Select Toolbar Button", "data/icons/button-select.svg", DrawingProgramToolType::BUTTONSELECT);
    tool_button("Stroke Vectorize Toolbar Button", "data/icons/pixel-to-vector.svg", DrawingProgramToolType::STROKEVECTORIZE);
    tool_button("Eraser Toolbar Button", "data/icons/eraser.svg", DrawingProgramToolType::ERASER);
    tool_button("Line Toolbar Button", "data/icons/line.svg", DrawingProgramToolType::LINE);
    tool_button("Text Toolbar Button", "data/icons/text.svg", DrawingProgramToolType::TEXTBOX);
    tool_button("Ellipse Toolbar Button", "data/icons/circle.svg", DrawingProgramToolType::ELLIPSE);
    tool_button("Rect Toolbar Button", "data/icons/rectangle.svg", DrawingProgramToolType::RECTANGLE);
    tool_button("RectSelect Toolbar Button", "data/icons/rectselect.svg", DrawingProgramToolType::RECTSELECT);
    tool_button("LassoSelect Toolbar Button", "data/icons/lassoselect.svg", DrawingProgramToolType::LASSOSELECT);
    tool_button("Edit Toolbar Button", "data/icons/cursor.svg", DrawingProgramToolType::EDIT);
    tool_button("Eyedropper Toolbar Button", "data/icons/eyedropper.svg", DrawingProgramToolType::EYEDROPPER);
    tool_button("Zoom Canvas Toolbar Button", "data/icons/zoom.svg", DrawingProgramToolType::ZOOM);
    tool_button("Pan Canvas Toolbar Button", "data/icons/hand.svg", DrawingProgramToolType::PAN);
}

void PhoneDrawingProgramScreen::tool_settings_popup() {
    auto& drawP = main.world->drawProg;
    auto& gui = main.g.gui;
    auto& io = gui.io;
 
    gui.element<LayoutElement>("tool settings popup", [&] (LayoutElement*, const Clay_ElementId& lId) {
        CLAY(lId, {
            .layout = {
                .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
            },
            .backgroundColor = convert_vec4<Clay_Color>(io.theme->backColor1),
            .cornerRadius = CLAY_CORNER_RADIUS(io.theme->windowCorners1)
        }) {
            gui.clipping_element<ScrollArea>("toolbox scroll area", ScrollArea::Options{
                .scrollVertical = true,
                .clipVertical = true,
                .scrollbarY = ScrollArea::ScrollbarType::NORMAL,
                .innerContent = [&](auto&) {
                    CLAY_AUTO_ID({
                        .layout = {
                            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                            .padding = CLAY_PADDING_ALL(io.theme->padding1),
                            .childGap = io.theme->childGap1,
                            .layoutDirection = CLAY_TOP_TO_BOTTOM
                        },
                    }) {
                        drawP.drawTool->gui_phone_toolbox(*this);
                    }
                }
            });
        }
    });
}

void PhoneDrawingProgramScreen::reset_color_picker_popup_data() {
    colorPickerPopupData.screenType = ColorPickerPopupData::ScreenType::NORMAL;
    colorPickerPopupData.newPaletteStr.clear();
    colorPickerPopupData.paletteListSelection.clear();
}

void PhoneDrawingProgramScreen::color_settings_popup(Vector4f* color) {
    auto& gui = main.g.gui;
    auto& palette = main.conf.palettes[colorPickerPopupData.selectedPalette];

    bool extraSettingsOpen = colorPickerPopupData.screenType == ColorPickerPopupData::ScreenType::EXTRA;
    bool extraColorButtons = extraSettingsOpen && colorPickerPopupData.selectedPalette != 0;

    auto paletteColorPickerGridColorButton = [&](size_t i){
        auto newC = std::make_shared<Vector3f>(palette.colors[i].x(), palette.colors[i].y(), palette.colors[i].z());
        color_button(gui, "c", newC.get(), {
            .isSelected = newC->x() == color->x() && newC->y() == color->y() && newC->z() == color->z(),
            .hasAlpha = false,
            .onClick = [newC, color] {
                // We want to keep the old color's alpha
                color->x() = newC->x();
                color->y() = newC->y();
                color->z() = newC->z();
            }
        });
    };

    auto paletteColorPickerGrid = [&] {
        gui.element<GridScrollArea>("color selector grid", GridScrollArea::Options{
            .entryWidth = BIG_BUTTON_SIZE,
            .childAlignmentX = CLAY_ALIGN_X_CENTER,
            .entryHeight = BIG_BUTTON_SIZE,
            .entryCount = palette.colors.size() + (extraColorButtons ? 3 : 1),
            .scrollbar = ScrollArea::ScrollbarType::NORMAL,
            .elementContent = [&](size_t i) {
                if(colorPickerPopupData.screenType == ColorPickerPopupData::ScreenType::NORMAL) {
                    if(i < palette.colors.size())
                        paletteColorPickerGridColorButton(i);
                    else {
                        svg_icon_button(gui, "extra color settings", "data/icons/RemixIcon/more-fill.svg", {
                            .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
                            .onClick = [&] {
                                colorPickerPopupData.screenType = ColorPickerPopupData::ScreenType::EXTRA;
                            }
                        });
                    }
                }
                else {
                    if(i == 0) {
                        svg_icon_button(gui, "extra color settings", "data/icons/RemixIcon/arrow-left-s-line.svg", {
                            .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
                            .onClick = [&] {
                                colorPickerPopupData.screenType = ColorPickerPopupData::ScreenType::NORMAL;
                            }
                        });
                    }
                    else if((i - 1) < palette.colors.size())
                        paletteColorPickerGridColorButton(i - 1);
                    else if(extraColorButtons) {
                        switch(i - palette.colors.size() - 1) {
                            case 0:
                                svg_icon_button(gui, "addcolor", "data/icons/plus.svg", {
                                    .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
                                    .onClick = [&, color] {
                                        std::erase(palette.colors, Vector3f{color->x(), color->y(), color->z()});
                                        palette.colors.emplace_back(color->x(), color->y(), color->z());
                                    }
                                });
                                break;
                            case 1:
                                svg_icon_button(gui, "deletecolor", "data/icons/close.svg", {
                                    .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
                                    .onClick = [&, color] {
                                        std::erase(palette.colors, Vector3f{color->x(), color->y(), color->z()});
                                    }
                                });
                                break;
                        }
                    }
                }
            }
        });
    };

    auto paletteColorPickerExtras = [&] {
        CLAY_AUTO_ID({
            .layout = {
                .sizing = {.width = CLAY_SIZING_FIT(200), .height = CLAY_SIZING_GROW(0)},
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                .layoutDirection = CLAY_LEFT_TO_RIGHT
            },
            .aspectRatio = {1.0f}
        }) {
            gui.element<ColorPicker<Vector4f>>("color picker element", color, true, ColorPickerData{
                .onChange = [&] {
                    gui.set_to_layout();
                }
            });
        }
        CLAY_AUTO_ID({
            .layout = {
                .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)},
                .childGap = gui.io.theme->childGap1,
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                .layoutDirection = CLAY_LEFT_TO_RIGHT
            }
        }) {
            std::vector<std::string> paletteNames;
            for(auto& p : main.conf.palettes)
                paletteNames.emplace_back(p.name);
            gui.element<DropDown<size_t>>("paletteselector", &colorPickerPopupData.selectedPalette, paletteNames);
            svg_icon_button(gui, "paletteadd", "data/icons/pencil.svg", {
                .onClick = [&] {
                    reset_color_picker_popup_data();
                    colorPickerPopupData.screenType = ColorPickerPopupData::ScreenType::PALETTES;
                }
            });
        }
    };

    auto paletteGUI = [&] {
        CLAY_AUTO_ID({
            .layout = {
                .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                .childGap = gui.io.theme->childGap1,
                .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP},
                .layoutDirection = CLAY_TOP_TO_BOTTOM
            }
        }) {
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .childGap = gui.io.theme->childGap1,
                    .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
                    .layoutDirection = CLAY_LEFT_TO_RIGHT
                }
            }) {
                svg_icon_button(gui, "palette back", "data/icons/RemixIcon/arrow-left-s-line.svg", {
                    .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
                    .onClick = [&] {
                        colorPickerPopupData.screenType = ColorPickerPopupData::ScreenType::EXTRA;
                    }
                });
                text_label(gui, "Palettes");
            }

            gui.element<TreeListing>("palette list", TreeListing::Data{
                .selectedIndices = &colorPickerPopupData.paletteListSelection,
                .dirInfo = [&](const TreeListingObjIndexList& objIndex) {
                    std::optional<TreeListing::DirectoryInfo> d;
                    if(objIndex.empty()) {
                        d = TreeListing::DirectoryInfo();
                        d.value().isOpen = true;
                        d.value().dirSize = main.conf.palettes.size();
                    }
                    return d;
                },
                .drawNonDirectoryObjIconGUI = [&](const TreeListingObjIndexList& objIndex) {},
                .drawObjGUI = [&](const TreeListingObjIndexList& objIndex) {
                    CLAY_AUTO_ID({
                        .layout = {
                            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                            .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER}
                        }
                    }) {
                        text_label(gui, main.conf.palettes[objIndex.back()].name);
                    }
                    if(objIndex.back() != 0) {
                        gui.set_z_index_keep_clipping_region(gui.get_z_index() + 1, [&] {
                            svg_icon_button(gui, "delete button", "data/icons/trash.svg", {
                                .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
                                .size = TreeListing::ENTRY_HEIGHT,
                                .onClick = [&, objIndex] {
                                    colorPickerPopupData.selectedPalette = 0;
                                    main.conf.palettes.erase(main.conf.palettes.begin() + objIndex.back());
                                }
                            });
                        });
                    }
                },
                .moveObj = [&](const std::vector<TreeListingObjIndexList>& objIndices, const TreeListingObjIndexList& newObjIndex) {
                    if(newObjIndex.back() == 0)
                        return;
                    std::deque<GlobalConfig::Palette> movedPalettes;
                    size_t moveIndex = newObjIndex.back();
                    for(auto& p : objIndices | std::views::reverse) {
                        if(p.back() != 0) {
                            if(moveIndex > p.back())
                                moveIndex--;
                            movedPalettes.emplace_front(main.conf.palettes[p.back()]);
                            main.conf.palettes.erase(main.conf.palettes.begin() + p.back());
                        }
                    }
                    colorPickerPopupData.selectedPalette = 0;
                    main.conf.palettes.insert(main.conf.palettes.begin() + moveIndex, movedPalettes.begin(), movedPalettes.end());
                }
            });
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .childGap = gui.io.theme->childGap1,
                    .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
                    .layoutDirection = CLAY_LEFT_TO_RIGHT
                }
            }) {
                auto addPaletteFunc = [&] {
                    if(!colorPickerPopupData.newPaletteStr.empty()) {
                        main.conf.palettes.emplace_back();
                        main.conf.palettes.back().name = colorPickerPopupData.newPaletteStr;
                        colorPickerPopupData.selectedPalette = main.conf.palettes.size() - 1;
                        colorPickerPopupData.screenType = ColorPickerPopupData::ScreenType::EXTRA;
                        gui.set_to_layout();
                    }
                };
                input_text(gui, "paletteinputname", &colorPickerPopupData.newPaletteStr, {
                    .emptyText = "New Palette Name...",
                    .onEnter = addPaletteFunc
                });
                svg_icon_button(gui, "paletteadd", "data/icons/plus.svg", {
                    .onClick = addPaletteFunc
                });
            }
        }
    };

    gui.element<LayoutElement>("color settings popup", [&] (LayoutElement* l, const Clay_ElementId& lId) {
        CLAY(lId, {
            .layout = {
                .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)},
                .childGap = gui.io.theme->childGap1,
                .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
                .layoutDirection = CLAY_LEFT_TO_RIGHT
            },
            .backgroundColor = convert_vec4<Clay_Color>(gui.io.theme->backColor1),
            .cornerRadius = CLAY_CORNER_RADIUS(gui.io.theme->windowCorners1)
        }) {
            switch(colorPickerPopupData.screenType) {
                case ColorPickerPopupData::ScreenType::NORMAL:
                    paletteColorPickerGrid();
                    break;
                case ColorPickerPopupData::ScreenType::EXTRA:
                    CLAY_AUTO_ID({
                        .layout = {
                            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                            .childGap = gui.io.theme->childGap1,
                            .layoutDirection = CLAY_TOP_TO_BOTTOM
                        },
                    }) {
                        paletteColorPickerGrid();
                    }
                    CLAY_AUTO_ID({
                        .layout = {
                            .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
                            .childGap = gui.io.theme->childGap1,
                            .layoutDirection = CLAY_TOP_TO_BOTTOM
                        },
                    }) {
                        paletteColorPickerExtras();
                    }
                    break;
                case ColorPickerPopupData::ScreenType::PALETTES:
                    paletteGUI();
                    break;
            }
        }
    });
}

void PhoneDrawingProgramScreen::bottom_extra_toolbar_gui() {
    GUIManager& gui = main.g.gui;

    svg_icon_button(gui, "tool settings", "data/icons/RemixIcon/settings-3-line.svg", {
        .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
        .isSelected = settingsMenuPopup == SettingsMenuPopup::SETTINGS,
        .onClick = [&] {
            settingsMenuPopup = (settingsMenuPopup == SettingsMenuPopup::SETTINGS) ? SettingsMenuPopup::NONE : SettingsMenuPopup::SETTINGS;
        }
    });

    color_button(gui, "foreground color", &main.toolConfig.globalConf.foregroundColor, {
        .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
        .isSelected = settingsMenuPopup == SettingsMenuPopup::FG_COLOR,
        .hasAlpha = true,
        .onClick = [&] {
            settingsMenuPopup = (settingsMenuPopup == SettingsMenuPopup::FG_COLOR) ? SettingsMenuPopup::NONE : SettingsMenuPopup::FG_COLOR;
        }
    });

    color_button(gui, "background color", &main.toolConfig.globalConf.backgroundColor, {
        .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
        .isSelected = settingsMenuPopup == SettingsMenuPopup::BG_COLOR,
        .hasAlpha = true,
        .onClick = [&] {
            settingsMenuPopup = (settingsMenuPopup == SettingsMenuPopup::BG_COLOR) ? SettingsMenuPopup::NONE : SettingsMenuPopup::BG_COLOR;
        }
    });

    // Tree-view panel toggle (M6-a). Mirrors the desktop layout's button.
    svg_icon_button(gui, "Tree View Toggle Button", "data/icons/list.svg", {
        .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
        .isSelected = main.world->treeView.is_visible(),
        .onClick = [&] { main.world->treeView.toggle(); }
    });

    // (Reader-mode toggle moved to the top toolbar so it survives the
    // bottom-toolbar hide that reader mode triggers.)
}

void PhoneDrawingProgramScreen::input_global_back_button_callback() {
    main.world->save_to_file(main.world->filePath);
    main.set_tab_to_close(main.world.get());
    main.g.gui.set_to_layout();
    main.set_screen([&] (std::unique_ptr<Screen>) { return std::make_unique<FileSelectScreen>(main); });
}

void PhoneDrawingProgramScreen::input_app_about_to_go_to_background_callback() {
    main.world->save_to_file(main.world->filePath);
}
