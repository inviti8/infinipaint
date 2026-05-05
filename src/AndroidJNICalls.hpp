#pragma once
#ifdef __ANDROID__
#include <mutex>

struct InputManager;

namespace AndroidJNICalls {
    extern InputManager* globalInputManager;
    extern std::mutex globalCallMutex;

    void startTextInput();
}

#endif
