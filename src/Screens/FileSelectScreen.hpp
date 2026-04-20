#pragma once
#include "Screen.hpp"

class FileSelectScreen : public Screen {
    public:
        FileSelectScreen(MainProgram& m);
        virtual void gui_layout_run() override;
        virtual void draw(SkCanvas* canvas) override;
    private:
        struct FileInfo {
            std::string fileName;
            SDL_Time lastModifyTime;
            std::string lastModifyDate;
        };
        const std::vector<FileInfo>& get_file_list();
        std::optional<std::vector<FileInfo>> fileListOptional;
};
