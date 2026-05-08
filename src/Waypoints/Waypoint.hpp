#pragma once
#include <Helpers/NetworkingObjects/NetObjOwnerPtr.hpp>
#include <Helpers/NetworkingObjects/NetObjOrderedList.hpp>
#include <Helpers/VersionNumber.hpp>
#include <cereal/archives/portable_binary.hpp>
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

        const CoordSpaceHelper& get_coords() const { return coords; }
        const Vector<int32_t, 2>& get_window_size() const { return windowSize; }

        void scale_up(const WorldScalar& scaleUpAmount);

        void save_file(cereal::PortableBinaryOutputArchive& a) const;
        // No load_file: WaypointGraph reads the payload from disk and
        // hands the values to the constructor via emplace_back_direct,
        // which keeps NetObjID assignment owned by the manager.

        static void register_class(World& w);

    private:
        static void write_constructor_data(const NetworkingObjects::NetObjTemporaryPtr<Waypoint>& o, cereal::PortableBinaryOutputArchive& a);

        std::string label;
        CoordSpaceHelper coords;
        Vector<int32_t, 2> windowSize{0, 0};
};
