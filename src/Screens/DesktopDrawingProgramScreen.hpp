#pragma once
#include "DrawingProgramScreen.hpp"
#include "../Toolbar.hpp"

class DesktopDrawingProgramScreen : public DrawingProgramScreen {
    public:
        DesktopDrawingProgramScreen(MainProgram& m);
        virtual void update() override;
        virtual void gui_layout_run() override;
        virtual bool app_close_requested() override;
        virtual void input_key_callback(const InputManager::KeyCallbackArgs& key) override;
        virtual void input_text_key_callback(const InputManager::KeyCallbackArgs& key) override;
    private:
        Toolbar toolbar;
};
