#include "FileSelectScreen.hpp"
#include "../MainProgram.hpp"
#include "../GUIHolder.hpp"
#include "../GUIStuff/ElementHelpers/TextLabelHelpers.hpp"
#include "../GUIStuff/ElementHelpers/ButtonHelpers.hpp"
#include "Helpers/ConvertVec.hpp"
#include "../GUIStuff/Elements/GridScrollArea.hpp"
#include "../GUIStuff/Elements/ImageDisplay.hpp"
#include "../World.hpp"
#include "Helpers/StringHelpers.hpp"
#include <SDL3/SDL_timer.h>

FileSelectScreen::FileSelectScreen(MainProgram& m): Screen(m) {}

void FileSelectScreen::gui_layout_run() {
    using namespace GUIStuff;
    using namespace ElementHelpers;
    auto& gui = main.g.gui;

    CLAY_AUTO_ID({
        .layout = {
            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
            .padding = CLAY_PADDING_ALL(gui.io.theme->padding1),
            .childGap = gui.io.theme->childGap1,
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
    }) {
        CLAY_AUTO_ID({
            .layout = {
                .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(40)},
                .padding = CLAY_PADDING_ALL(gui.io.theme->padding1),
                .childGap = gui.io.theme->childGap1,
                .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            }
        }) {
            svg_icon_button(gui, "main settings button", "data/icons/menu.svg", {
                .drawType = SelectableButton::DrawType::TRANSPARENT_ALL
            });
            text_label(gui, "File selector");
        }
        auto& fileList = get_file_list();
        gui.element<GridScrollArea>("File selector grid", GridScrollArea::Options{
            .entryWidth = 200.0f,
            .childAlignmentX = CLAY_ALIGN_X_LEFT,
            .entryHeight = 220.0f,
            .entryCount = fileList.size(),
            .elementContent = [&](size_t i) {
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
                        .layout = { .sizing = {.width = CLAY_SIZING_FIXED(150), .height = CLAY_SIZING_FIXED(150) }},
                    }) {
                        gui.element<ImageDisplay>("ico", ImageDisplay::Data{
                            .imgPath = main.conf.configPath / "saveThumbnails" / (fileList[i].fileName + ".jpg"),
                            .radius = 20
                        });
                    }
                    text_label(gui, fileList[i].fileName);
                    text_label_light(gui, fileList[i].lastModifyDate);
                }
            }
        });
    }
}

const std::vector<FileSelectScreen::FileInfo>& FileSelectScreen::get_file_list() {
    if(!fileListOptional.has_value()) {
        std::filesystem::path savePath = main.conf.configPath / "saves";
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

void FileSelectScreen::draw(SkCanvas* canvas) {
    canvas->clear(main.g.gui.io.theme->backColor1);
}
