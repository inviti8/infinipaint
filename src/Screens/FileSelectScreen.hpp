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
        const std::vector<FileInfo>& get_file_list(const std::string& folder);
        std::optional<std::vector<FileInfo>> fileListOptional;

        void main_menu();
        void file_view();
        bool main_menu_fills_screen();
        void menu_black_box();
        void title_bar(std::string_view title);

        enum class SelectedMenu {
            FILES,
            TRASH,
            SETTINGS
        } selectedMenu = SelectedMenu::FILES;
        bool mainMenuOpen = false;
};
