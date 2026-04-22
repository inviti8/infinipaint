#include "FileSelectScreen.hpp"
#include "../MainProgram.hpp"
#include "../GUIHolder.hpp"
#include "../GUIStuff/ElementHelpers/TextLabelHelpers.hpp"
#include "../GUIStuff/ElementHelpers/ButtonHelpers.hpp"
#include "Helpers/ConvertVec.hpp"
#include "../GUIStuff/Elements/GridScrollArea.hpp"
#include "../GUIStuff/Elements/ImageDisplay.hpp"
#include "../GUIStuff/Elements/LayoutElement.hpp"
#include "../World.hpp"
#include "Helpers/StringHelpers.hpp"
#include "DrawingProgramScreen.hpp"
#include <SDL3/SDL_timer.h>

#define MAIN_MENU_SIZE 300

using namespace GUIStuff;
using namespace ElementHelpers;

FileSelectScreen::FileSelectScreen(MainProgram& m): Screen(m) {}

void FileSelectScreen::gui_layout_run() {
    auto& gui = main.g.gui;
    CLAY_AUTO_ID({
        .layout = {
            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
            .layoutDirection = CLAY_LEFT_TO_RIGHT
        },
    }) {
        if(mainMenuOpen)
            main_menu();
        if(!(mainMenuOpen && main_menu_fills_screen())) {
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = {.width = CLAY_SIZING_FIXED(gui.io.windowSize.x()), .height = CLAY_SIZING_GROW(0)},
                    .padding = CLAY_PADDING_ALL(gui.io.theme->padding1),
                    .childGap = gui.io.theme->childGap1,
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP },
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                },
            }) {
                switch(selectedMenu) {
                    case SelectedMenu::FILES:
                        title_bar("Files");
                        file_view();
                        break;
                    case SelectedMenu::TRASH:
                        title_bar("Trash");
                        file_view();
                        break;
                    case SelectedMenu::SETTINGS:
                        title_bar("Settings");
                        break;
                }
                menu_black_box();
            }
        }
    }
}

bool FileSelectScreen::main_menu_fills_screen() {
    return main.g.gui.io.windowSize.x() < MAIN_MENU_SIZE + 50;
}

void FileSelectScreen::title_bar(std::string_view title) {
    auto& gui = main.g.gui;
    CLAY_AUTO_ID({
        .layout = {
            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(40)},
            .childGap = gui.io.theme->childGap1,
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        }
    }) {
        svg_icon_button(gui, "main settings button", "data/icons/menu.svg", {
            .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
            .onClick = [&] {
                mainMenuOpen = true;
            }
        });
        text_label(gui, title);
    }
}

void FileSelectScreen::main_menu() {
    auto& gui = main.g.gui;
    gui.element<LayoutElement>("main menu popup", [&] (LayoutElement*, const Clay_ElementId& lId) {
        CLAY(lId, {
            .layout = {
                .sizing = {.width = CLAY_SIZING_GROW(static_cast<float>(main_menu_fills_screen() ? 0 : 300)), .height = CLAY_SIZING_GROW(0)},
                .padding = CLAY_PADDING_ALL(gui.io.theme->padding1),
                .childGap = gui.io.theme->childGap1,
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            },
        }) {
            text_button_with_icon(gui, "Files", "data/icons/folder.svg", "Files", {
                .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
                .isSelected = selectedMenu == SelectedMenu::FILES,
                .wide = true,
                .centered = false,
                .onClick = [&] {
                    selectedMenu = SelectedMenu::FILES;
                    fileListOptional.reset();
                    mainMenuOpen = false;
                }
            });
            text_button_with_icon(gui, "Trash", "data/icons/trash.svg", "Trash", {
                .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
                .isSelected = selectedMenu == SelectedMenu::TRASH,
                .wide = true,
                .centered = false,
                .onClick = [&] {
                    selectedMenu = SelectedMenu::TRASH;
                    fileListOptional.reset();
                    mainMenuOpen = false;
                }
            });
            text_button_with_icon(gui, "Settings", "data/icons/RemixIcon/settings-3-line.svg", "Settings", {
                .drawType = SelectableButton::DrawType::TRANSPARENT_ALL,
                .isSelected = selectedMenu == SelectedMenu::SETTINGS,
                .wide = true,
                .centered = false,
                .onClick = [&] {
                    selectedMenu = SelectedMenu::SETTINGS;
                    mainMenuOpen = false;
                }
            });
        }
    });
}

void FileSelectScreen::file_view() {
    auto& gui = main.g.gui;
    bool isTrash = selectedMenu == SelectedMenu::TRASH;
    std::string folder = isTrash ? "trash" : "saves";
    auto& fileList = get_file_list(folder);
    gui.element<GridScrollArea>("File selector grid", GridScrollArea::Options{
        .entryWidth = 180.0f,
        .childAlignmentX = CLAY_ALIGN_X_LEFT,
        .entryHeight = 200.0f,
        .entryCount = fileList.size(),
        .elementContent = [&](size_t i) {
            std::filesystem::path filePath = main.conf.configPath / folder / (fileList[i].fileName + ".infpnt");
            gui.element<SelectableButton>("file button", SelectableButton::Data{
                .onClick = [&, filePath] {
                    CustomEvents::emit_event<CustomEvents::OpenInfiniPaintFileEvent>({
                        .isClient = false,
                        .filePathSource = filePath
                    });
                },
                .innerContent = [&](const SelectableButton::InnerContentCallbackParameters& c){
                    CLAY_AUTO_ID({
                        .layout = {
                            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                            .padding = CLAY_PADDING_ALL(gui.io.theme->padding1),
                            .childGap = 6,
                            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                            .layoutDirection = CLAY_TOP_TO_BOTTOM
                        }
                    }) {
                        CLAY_AUTO_ID({
                            .layout = { .sizing = {.width = CLAY_SIZING_FIXED(130), .height = CLAY_SIZING_FIXED(130) }},
                        }) {
                            gui.element<ImageDisplay>("ico", ImageDisplay::Data{
                                .imgPath = main.conf.configPath / folder / (fileList[i].fileName + ".jpg"),
                                .radius = 20
                            });
                        }
                        text_label(gui, fileList[i].fileName);
                        text_label_light(gui, fileList[i].lastModifyDate);
                    }
                }
            });
        }
    });
}

void FileSelectScreen::menu_black_box() {
    auto& gui = main.g.gui;
    if(mainMenuOpen) {
        gui.set_z_index(gui.get_z_index() + 1, [&] {
            gui.element<LayoutElement>("file list disable fill", [&] (LayoutElement* l, const Clay_ElementId& lId) {
                CLAY(lId, {
                    .layout = {
                        .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    },
                    .backgroundColor = {0.0f, 0.0f, 0.0f, 0.6f},
                    .floating = {
                        .zIndex = gui.get_z_index(),
                        .attachPoints = {
                            .element = CLAY_ATTACH_POINT_LEFT_TOP,
                            .parent = CLAY_ATTACH_POINT_LEFT_TOP
                        },
                        .attachTo = CLAY_ATTACH_TO_PARENT
                    }
                }) {
                }
            }, LayoutElement::Callbacks{
                .mouseButton = [&] (LayoutElement* l, const InputManager::MouseButtonCallbackArgs& m) {
                    if(m.down && m.button == InputManager::MouseButton::LEFT && l->mouseHovering) {
                        mainMenuOpen = false;
                        gui.set_to_layout();
                    }
                }
            });
        });
    }
}

const std::vector<FileSelectScreen::FileInfo>& FileSelectScreen::get_file_list(const std::string& folder) {
    if(!fileListOptional.has_value()) {
        std::filesystem::path savePath = main.conf.configPath / folder;
        fileListOptional = std::vector<FileInfo>();
        auto& fL = fileListOptional.value();
        int globCount;
        char** filesInPath = SDL_GlobDirectory(savePath.string().c_str(), "*", 0, &globCount);
        if(filesInPath) {
            for(int i = 0; i < globCount; i++) {
                std::filesystem::path p = filesInPath[i];
                std::string fExt = p.extension().string();
                if(fExt == "." + World::FILE_EXTENSION) {
                    std::filesystem::path fullPath = savePath / filesInPath[i];
                    FileInfo fileInfoToAdd;
                    fileInfoToAdd.fileName = p.stem().string();

                    SDL_PathInfo pathInfo;
                    if(SDL_GetPathInfo(fullPath.string().c_str(), &pathInfo)) {
                        SDL_DateTime pathDt;
                        fileInfoToAdd.lastModifyTime = pathInfo.modify_time;
                        if(SDL_TimeToDateTime(pathInfo.modify_time, &pathDt, true)) {
                            fileInfoToAdd.lastModifyDate = sdl_time_to_nice_access_time(pathDt, main.conf.dateFormat, main.conf.timeFormat);
                        }
                    }

                    fL.emplace_back(fileInfoToAdd);
                }
            }
        }
    }
    return fileListOptional.value();
}

void FileSelectScreen::input_open_infinipaint_file_callback(const CustomEvents::OpenInfiniPaintFileEvent& openFile) {
    main.create_new_tab(openFile);
    main.screen = std::make_unique<DrawingProgramScreen>(main);
}

void FileSelectScreen::draw(SkCanvas* canvas) {
    canvas->clear(main.g.gui.io.theme->backColor1);
}
