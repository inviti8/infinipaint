#pragma once
#include <Helpers/BezierEasing.hpp>

namespace GUIStuff {

class GUIManager;

struct GUIFloatAnimationData {
    float startVal = 0.0f;
    float endVal = 1.0f;
    float duration = 1.0f;
    BezierEasing easing = BezierEasing::linear;
};

class GUIFloatAnimation {
    public:
        GUIFloatAnimation(const GUIFloatAnimationData& animData);
        void update(GUIManager& gui);
        void animation_trigger();
        void animation_trigger_reverse();
        float get_val() const;

        bool is_at_start();
        bool is_at_end();
        bool is_done();

        bool isUsedThisFrame;
    private:
        void calculate_anim_val();
        enum class State {
            RUN,
            RUN_REVERSE,
            DONE
        } state = State::DONE;
        float currentTime = 0.0f;
        GUIFloatAnimationData anim;
        float animVal;
};

}
