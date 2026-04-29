#pragma once
#include "DrawingProgramScreen.hpp"
#include "../Toolbar.hpp"

class PhoneDrawingProgramScreen : public DrawingProgramScreen {
    public:
        PhoneDrawingProgramScreen(MainProgram& m);
        virtual void update() override;
        virtual void gui_layout_run() override;
        virtual void input_global_back_button_callback() override;
        virtual void input_app_about_to_go_to_background_callback() override;
    private:
        void save_files();
        void top_toolbar();
        void bottom_toolbar();
        void main_display();
};
