#pragma once
#include <Eigen/Dense>
#include <Helpers/ConvertVec.hpp>
#include "SharedTypes.hpp"
#include <Helpers/SCollision.hpp>
#include "CoordSpaceHelper.hpp"
#include <Helpers/VersionNumber.hpp>
#include "InputManager.hpp"

using namespace Eigen;

class World;

class DrawCamera {
    public:
        DrawCamera();

        CoordSpaceHelper c;
        Vector2f viewingArea{-1, -1};
        SCollision::AABB<WorldScalar> viewingAreaGenerousCollider;

        void set_viewing_area(Vector2f viewingAreaNew);
        // PHASE2 M4: speedMultiplier scales the transition duration:
        // applied duration = w.main.conf.jumpTransitionTime / speedMultiplier.
        // Multiplier 1.0 (default) = global behavior. >1 = faster, <1 = slower.
        // Ignored when instantJump is true. Range is callers' responsibility
        // to validate; out-of-range values may produce odd timings but won't
        // crash. The 0.1..2.0 range used by Waypoint is a UX choice, not
        // a structural constraint here.
        void smooth_move_to(World& w, const CoordSpaceHelper& c, Vector2f windowSize,
                            bool instantJump = false,
                            float speedMultiplier = 1.0f);
        void set_based_on_properties(World& w, const WorldVec& newPos, const WorldScalar& newZoom, double newRotate);
        void set_based_on_center(World& w, const WorldVec& newPos, const WorldScalar& newZoom, double newRotate);
        void update_main(World& main);

        void scale_up(World& w, const WorldScalar& scaleUpAmount);

        void save_file(cereal::PortableBinaryOutputArchive& a, const World& w) const;
        void load_file(cereal::PortableBinaryInputArchive& a, VersionNumber version, World& w);

        void input_key_callback(const InputManager::KeyCallbackArgs& key);
        void input_mouse_button_on_canvas_callback(World& w, const InputManager::MouseButtonCallbackArgs& button);
        void input_mouse_motion_callback(World& w, const InputManager::MouseMotionCallbackArgs& motion);
        void input_mouse_wheel_callback(World& w, const InputManager::MouseWheelCallbackArgs& wheel);
        void input_multi_finger_touch_callback(World& w, const InputManager::MultiFingerTouchCallbackArgs& touch);
        void input_multi_finger_motion_callback(World& w, const InputManager::MultiFingerMotionCallbackArgs& motion);
    private:
        struct SmoothMove {
            CoordSpaceHelper start;
            WorldVec startCenter;
            WorldVec endCenter;
            WorldScalar endUniformZoom;
            CoordSpaceHelper end;
            Vector2f endWindowSize;
            bool occurring = false;
            float moveTime;
            // PHASE2 M4: total duration for this transition. Snapshot at
            // smooth_move_to time as (jumpTransitionTime / speedMultiplier).
            // update_main reads this rather than the live config so a
            // mid-transition config change doesn't slow/speed an active move.
            float targetDuration = 0.0f;
        } smoothMove;

        WorldScalar startZoomVal;
        WorldVec startZoomMousePos;
        WorldVec startZoomCameraPos;
        bool isAccurateZooming = false;

        CoordSpaceHelper touchInitialC;
        std::vector<Vector2f> touchInitialPositions;
        bool isTouchTransforming = false;

        void check_if_scale_up_required(World& w);
        void checks_after_input(World& w);
};
