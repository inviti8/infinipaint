#pragma once
#include "Screen.hpp"
#include "nlohmann/json.hpp"


class FileSelectScreen : public Screen {
    public:
        FileSelectScreen(MainProgram& m);
        virtual void gui_layout_run() override;
        virtual void draw(SkCanvas* canvas) override;
        virtual void input_open_infinipaint_file_callback(const CustomEvents::OpenInfiniPaintFileEvent& openFile) override;

        struct TrashInfo {
            struct TrashFile {
                SDL_Time trashTime;
                NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TrashFile, trashTime)
            };
            std::unordered_map<std::string, TrashFile> files;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TrashInfo, files)
        };
        ~FileSelectScreen();

    private:
        std::filesystem::path savePath;
        std::filesystem::path trashPath;
        std::filesystem::path trashInfoPath;

        struct FileInfo {
            std::string fileName;
            SDL_Time lastModifyTime;
            std::string lastModifyDate;
            bool selected = false;
        };

        void update_file_list(std::vector<FileInfo>& fL, const std::filesystem::path& savePath, bool trashUpdate);

        TrashInfo trashInfo;
        std::vector<FileInfo> fileList;

        void main_display();
        void main_menu();
        bool main_menu_fills_screen();
        void file_view();
        void file_view_edit();
        void menu_black_box();
        void edit_action_bar();
        void edit_title_bar();
        void title_bar();
        void text_transparent_option_button(const char* id, const char* text, const std::function<void()>& onClick);
        void icon_text_transparent_option_selected_button(const char* id, const std::string& svgPath, const char* text, bool isSelected, const std::function<void()>& onClick);
        void edit_action_bar_button(const char* id, const std::string& svgPath, const char* text, const std::function<void()>& onClick);

        enum class TrashMoveType {
            NONE,
            MOVE_TO_TRASH,
            MOVE_OUT_OF_TRASH
        };
        void move_selected_files(const std::filesystem::path& fromPath, const std::filesystem::path& toPath, TrashMoveType trashMoveType);
        void duplicate_selected_files(const std::filesystem::path& inPath);
        void delete_selected_files_in_trash();

        enum class FileViewType {
            LARGE_GRID,
            MEDIUM_GRID,
            SMALL_GRID,
            LIST
        } fileViewType = FileViewType::LARGE_GRID;

        enum class MoreOptionsMenu {
            CLOSED,
            MAIN,
            VIEW
        } moreOptionsMenu = MoreOptionsMenu::CLOSED;

        enum class SelectedMenu {
            FILES,
            TRASH,
            SETTINGS
        } selectedMenu = SelectedMenu::FILES;

        void start_edit_mode();
        size_t numberOfSelectedEntries;
        bool editMode = false;

        bool mainMenuOpen = false;
};
