#pragma once
#include "Waypoint.hpp"
#include "Edge.hpp"
#include <Helpers/NetworkingObjects/NetObjOwnerPtr.hpp>
#include <Helpers/NetworkingObjects/NetObjOrderedList.hpp>
#include <Helpers/VersionNumber.hpp>
#include <Eigen/Core>
#include <cereal/archives/portable_binary.hpp>
#include <optional>
#include <unordered_map>

class World;

// PHASE1.md §5 — owns the flat list of Waypoint nodes plus the directed
// Edge list between them, and a per-node layout (positions in the tree
// window). The layout is a local UI state field but is serialized so a
// reload preserves the user's tree-window arrangement; it is NOT
// NetObj-synced (each client is free to lay the graph out differently).
//
// Lives on World alongside BookmarkManager. Per §5 the bookmark side
// panel is removed in M5/M6 and BookmarkManager becomes a thin shim
// that round-trips through this graph; for M4 the two coexist
// independently and migration of existing bookmarks lands as M4-b.
class WaypointGraph {
    public:
        explicit WaypointGraph(World& w);

        void server_init_no_file();
        void read_create_message(cereal::PortableBinaryInputArchive& a);
        void write_create_message(cereal::PortableBinaryOutputArchive& a) const;

        void scale_up(const WorldScalar& scaleUpAmount);

        void save_file(cereal::PortableBinaryOutputArchive& a) const;
        void load_file(cereal::PortableBinaryInputArchive& a, VersionNumber version);

        static void register_class(World& w);

        const NetworkingObjects::NetObjOwnerPtr<NetworkingObjects::NetObjOrderedList<Waypoint>>& get_nodes() const { return nodes; }
        const NetworkingObjects::NetObjOwnerPtr<NetworkingObjects::NetObjOrderedList<Edge>>& get_edges() const { return edges; }
        const std::unordered_map<NetworkingObjects::NetObjID, Vector2f>& get_layout() const { return layout; }
        std::unordered_map<NetworkingObjects::NetObjID, Vector2f>& mutable_layout() { return layout; }

        // UI selection — shared between WaypointTool (canvas) and
        // TreeView (graph panel) so a click in either updates the
        // other's chrome. Local-only state (not NetObj-synced or
        // persisted), since selection is per-user-session.
        bool has_selection() const                         { return selectedId.has_value(); }
        NetworkingObjects::NetObjID get_selected() const   { return selectedId.value_or(NetworkingObjects::NetObjID{}); }
        void select(NetworkingObjects::NetObjID id)        { selectedId = id; }
        void clear_selection()                             { selectedId.reset(); }

    private:
        World& world;

        NetworkingObjects::NetObjOwnerPtr<NetworkingObjects::NetObjOrderedList<Waypoint>> nodes;
        NetworkingObjects::NetObjOwnerPtr<NetworkingObjects::NetObjOrderedList<Edge>> edges;
        std::unordered_map<NetworkingObjects::NetObjID, Vector2f> layout;
        std::optional<NetworkingObjects::NetObjID> selectedId;
};
