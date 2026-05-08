#pragma once
#include <Helpers/NetworkingObjects/NetObjOwnerPtr.hpp>
#include <Helpers/NetworkingObjects/NetObjOrderedList.hpp>
#include <Helpers/NetworkingObjects/NetObjID.hpp>
#include <Helpers/VersionNumber.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <optional>
#include <string>

class World;

// PHASE1.md §5 — directed edge between two waypoints. The label, when set,
// becomes the choice text in branching reader mode. Endpoints are
// NetObjIDs into the same netObjMan that owns the Waypoint nodes.
//
// On disk, WaypointGraph translates from/to into positional indices
// because NetObjIDs aren't stable across save/load (the loader assigns
// fresh IDs as nodes are constructed). The runtime fields stay as
// NetObjIDs so live multi-user edits during a session work normally.
class Edge {
    public:
        Edge();
        Edge(NetworkingObjects::NetObjID initFrom,
             NetworkingObjects::NetObjID initTo,
             std::optional<std::string> initLabel = std::nullopt);

        NetworkingObjects::NetObjID get_from() const { return from; }
        NetworkingObjects::NetObjID get_to() const { return to; }
        const std::optional<std::string>& get_label() const { return label; }
        void set_label(std::optional<std::string> newLabel) { label = std::move(newLabel); }

        // Saves only the label — endpoints are written by WaypointGraph
        // as indices into its node list, then handed back to the Edge
        // constructor on load.
        void save_label_file(cereal::PortableBinaryOutputArchive& a) const;

        static void register_class(World& w);

    private:
        static void write_constructor_data(const NetworkingObjects::NetObjTemporaryPtr<Edge>& o, cereal::PortableBinaryOutputArchive& a);

        NetworkingObjects::NetObjID from{};
        NetworkingObjects::NetObjID to{};
        std::optional<std::string> label;
};
