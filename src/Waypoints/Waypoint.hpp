#pragma once
#include <Helpers/NetworkingObjects/NetObjOwnerPtr.hpp>
#include <Helpers/NetworkingObjects/NetObjOrderedList.hpp>
#include <Helpers/VersionNumber.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <Eigen/Core>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include "../CoordSpaceHelper.hpp"

// PHASE2 M5 — fixed set of CSS-style easing presets for the per-waypoint
// reader-mode transition curve. Stored as a uint8 in the file; resolved
// to (x1, y1, x2, y2) cubic-bezier control points at render time so the
// curve definitions can be tweaked without breaking saved files.
//
// Custom-curve editing (a CSS cubic-bezier visualizer that lets the
// artist drag handles) is intentionally Phase 3 territory. Phase 2
// ships these five and stops there.
enum class TransitionEasing : uint8_t {
    LINEAR      = 0,
    EASE        = 1,  // CSS default — the "ease" timing function
    EASE_IN     = 2,
    EASE_OUT    = 3,
    EASE_IN_OUT = 4
};
Eigen::Vector4f transition_easing_to_bezier_curve(TransitionEasing e);
const std::vector<std::string>& transition_easing_display_names();

class World;
class WaypointGraph;

// PHASE1.md §5 — a single waypoint node. Stores a camera state
// (CoordSpaceHelper + windowSize, mirroring BookmarkData) plus a label.
// One Waypoint == one droppable canvas marker; edges between them live
// in WaypointGraph alongside.
//
// NetObj-class-registered so it participates in multi-user sync (the
// design doc commits to this in §12 to avoid silently breaking the
// existing bookmark sync we're replacing).
//
// Thumbnail is intentionally omitted at M4: it's a lazily regenerated
// SkImage of the framing rect and lives in M5 with the canvas component
// that shows it.
class Waypoint {
    public:
        Waypoint();
        Waypoint(const std::string& initLabel,
                 const CoordSpaceHelper& initCoords,
                 const Vector<int32_t, 2>& initWindowSize);

        const std::string& get_label() const { return label; }
        void set_label(const std::string& newLabel) { label = newLabel; }
        // Direct mutable handle for the label-edit text input. NetObj sync
        // for multi-user concurrent edits is deferred to a polish pass —
        // single-user edits work because the field lives in a NetObj that
        // already participates in the file-format save/load round-trip.
        std::string& mutable_label() { return label; }

        const CoordSpaceHelper& get_coords() const { return coords; }
        const Vector<int32_t, 2>& get_window_size() const { return windowSize; }

        // PHASE1.md §5a — artist-drawn navigation-button image. Captured
        // by ButtonSelectTool. Persisted as PNG bytes in the file format
        // (no NetObj sync for the moment — large payloads through NetObj
        // messages are their own scoping problem and this fork is
        // single-user anyway).
        bool has_skin() const             { return skin != nullptr; }
        sk_sp<SkImage> get_skin() const   { return skin; }
        void set_skin(sk_sp<SkImage> img) { skin = std::move(img); }
        void clear_skin()                 { skin.reset(); }

        // PHASE2 M4: per-waypoint speed multiplier for the reader-mode
        // camera transition INTO this waypoint. Range 0.1 .. 2.0. The
        // applied transition duration is globalDuration / multiplier,
        // so 2.0 plays the transition twice as fast and 0.5 plays it
        // half-speed. Default 1.0 leaves the global behavior unchanged.
        static constexpr float TRANSITION_SPEED_MIN = 0.1f;
        static constexpr float TRANSITION_SPEED_MAX = 2.0f;
        static constexpr float TRANSITION_SPEED_DEFAULT = 1.0f;
        float get_transition_speed_multiplier() const { return transitionSpeedMultiplier; }
        void set_transition_speed_multiplier(float v) { transitionSpeedMultiplier = std::clamp(v, TRANSITION_SPEED_MIN, TRANSITION_SPEED_MAX); }
        // Direct mutable reference for the WaypointTool slider; the slider
        // widget enforces the min/max range so external clamping isn't
        // strictly required, but set_transition_speed_multiplier still
        // clamps when callers come through that path.
        float& mutable_transition_speed_multiplier() { return transitionSpeedMultiplier; }

        // PHASE2 M5: easing preset for the reader-mode transition INTO
        // this waypoint. Stored as enum, resolved to a bezier curve via
        // transition_easing_to_bezier_curve() at transition time.
        TransitionEasing get_transition_easing() const { return transitionEasing; }
        void set_transition_easing(TransitionEasing e) { transitionEasing = e; }
        // Mutable raw access for the WaypointTool dropdown widget.
        TransitionEasing& mutable_transition_easing() { return transitionEasing; }

        void scale_up(const WorldScalar& scaleUpAmount);

        void save_file(cereal::PortableBinaryOutputArchive& a) const;
        // Reads the skin payload (PNG bytes) from `a` and assigns it to
        // this waypoint. Called by WaypointGraph::load_file after the
        // waypoint has been constructed via emplace_back_direct (which
        // takes label/coords/windowSize). Only meaningful for files at
        // version >= 0.6.0; pre-0.6 files don't have the payload, so
        // the caller skips this call.
        void load_skin_from_archive(cereal::PortableBinaryInputArchive& a, VersionNumber version);
        // PHASE2 M4: reads the per-waypoint transition controls
        // (currently just the speed multiplier; M5 will append easing).
        // Caller gates on file version >= 0.9.0.
        void load_transition_data_from_archive(cereal::PortableBinaryInputArchive& a, VersionNumber version);

        static void register_class(World& w);

    private:
        static void write_constructor_data(const NetworkingObjects::NetObjTemporaryPtr<Waypoint>& o, cereal::PortableBinaryOutputArchive& a);

        std::string label;
        CoordSpaceHelper coords;
        Vector<int32_t, 2> windowSize{0, 0};
        sk_sp<SkImage> skin;
        float transitionSpeedMultiplier = TRANSITION_SPEED_DEFAULT;
        TransitionEasing transitionEasing = TransitionEasing::EASE;
};
