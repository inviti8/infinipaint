#pragma once
#include <Helpers/NetworkingObjects/NetObjOwnerPtr.hpp>
#include <Helpers/NetworkingObjects/NetObjOrderedList.hpp>
#include <Helpers/VersionNumber.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <string>
#include "../CoordSpaceHelper.hpp"

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

        void scale_up(const WorldScalar& scaleUpAmount);

        void save_file(cereal::PortableBinaryOutputArchive& a) const;
        // Reads the skin payload (PNG bytes) from `a` and assigns it to
        // this waypoint. Called by WaypointGraph::load_file after the
        // waypoint has been constructed via emplace_back_direct (which
        // takes label/coords/windowSize). Only meaningful for files at
        // version >= 0.6.0; pre-0.6 files don't have the payload, so
        // the caller skips this call.
        void load_skin_from_archive(cereal::PortableBinaryInputArchive& a, VersionNumber version);

        static void register_class(World& w);

    private:
        static void write_constructor_data(const NetworkingObjects::NetObjTemporaryPtr<Waypoint>& o, cereal::PortableBinaryOutputArchive& a);

        std::string label;
        CoordSpaceHelper coords;
        Vector<int32_t, 2> windowSize{0, 0};
        sk_sp<SkImage> skin;
};
