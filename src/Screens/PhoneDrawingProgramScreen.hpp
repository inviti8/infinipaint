#pragma once
#include "DrawingProgramScreen.hpp"
#include "../Toolbar.hpp"

class PhoneDrawingProgramScreen : public DrawingProgramScreen {
    public:
        PhoneDrawingProgramScreen(MainProgram& m);
        virtual void update() override;
        virtual void gui_layout_run() override;
    private:
        void top_toolbar();
        void main_display();
};
