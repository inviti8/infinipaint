#include "WaypointGraph.hpp"
#include "../World.hpp"
#include <Helpers/NetworkingObjects/NetObjID.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <unordered_map>
#include <vector>
#include <Eigen/Core>

using namespace NetworkingObjects;

WaypointGraph::WaypointGraph(World& w)
    : world(w) {}

void WaypointGraph::register_class(World& w) {
    Waypoint::register_class(w);
    Edge::register_class(w);
}

void WaypointGraph::server_init_no_file() {
    nodes = world.netObjMan.make_obj<NetObjOrderedList<Waypoint>>();
    edges = world.netObjMan.make_obj<NetObjOrderedList<Edge>>();
    layout.clear();
}

void WaypointGraph::read_create_message(cereal::PortableBinaryInputArchive& a) {
    nodes = world.netObjMan.read_create_message<NetObjOrderedList<Waypoint>>(a, nullptr);
    edges = world.netObjMan.read_create_message<NetObjOrderedList<Edge>>(a, nullptr);
    // Layout is local-only state; not transmitted across the network.
}

void WaypointGraph::write_create_message(cereal::PortableBinaryOutputArchive& a) const {
    nodes.write_create_message(a);
    edges.write_create_message(a);
}

void WaypointGraph::scale_up(const WorldScalar& scaleUpAmount) {
    if (nodes) {
        for (auto& n : *nodes)
            n.obj->scale_up(scaleUpAmount);
    }
}

void WaypointGraph::save_file(cereal::PortableBinaryOutputArchive& a) const {
    // Build NetObjID -> file-index map so edges and layout entries can
    // reference nodes by stable position rather than by runtime
    // NetObjID (which the loader will reassign).
    std::unordered_map<NetObjID, uint32_t> idToIndex;
    const uint32_t nodeCount = nodes ? static_cast<uint32_t>(nodes->size()) : 0;
    a(nodeCount);
    if (nodes) {
        uint32_t i = 0;
        for (auto& info : *nodes) {
            idToIndex[info.obj.get_net_id()] = i++;
            info.obj->save_file(a);
        }
    }

    const uint32_t edgeCount = edges ? static_cast<uint32_t>(edges->size()) : 0;
    a(edgeCount);
    if (edges) {
        for (auto& info : *edges) {
            const auto fromIt = idToIndex.find(info.obj->get_from());
            const auto toIt   = idToIndex.find(info.obj->get_to());
            const uint32_t fromIdx = (fromIt != idToIndex.end()) ? fromIt->second : 0xFFFFFFFFu;
            const uint32_t toIdx   = (toIt   != idToIndex.end()) ? toIt->second   : 0xFFFFFFFFu;
            a(fromIdx, toIdx);
            info.obj->save_label_file(a);
        }
    }

    // Layout: write count + (idx, x, y) tuples. Entries whose key is
    // not a current node are dropped silently.
    uint32_t layoutCount = 0;
    for (const auto& [id, _] : layout) {
        if (idToIndex.find(id) != idToIndex.end()) ++layoutCount;
    }
    a(layoutCount);
    for (const auto& [id, pos] : layout) {
        const auto it = idToIndex.find(id);
        if (it == idToIndex.end()) continue;
        a(it->second);
        float x = pos.x();
        float y = pos.y();
        a(x, y);
    }
}

void WaypointGraph::load_file(cereal::PortableBinaryInputArchive& a, VersionNumber version) {
    nodes = world.netObjMan.make_obj<NetObjOrderedList<Waypoint>>();
    edges = world.netObjMan.make_obj<NetObjOrderedList<Edge>>();
    layout.clear();

    // Read nodes: each gets a fresh NetObjID at insertion time. Track
    // the per-index ID so edges and layout can resolve their refs.
    uint32_t nodeCount = 0;
    a(nodeCount);
    std::vector<NetObjID> idsByIndex;
    idsByIndex.reserve(nodeCount);
    for (uint32_t i = 0; i < nodeCount; ++i) {
        std::string label;
        CoordSpaceHelper coords;
        Vector<int32_t, 2> windowSize{0, 0};
        a(label, coords, windowSize);
        auto it = nodes->emplace_back_direct(nodes, label, coords, windowSize);
        if (version >= VersionNumber(0, 6, 0))
            it->obj->load_skin_from_archive(a, version);
        idsByIndex.push_back(it->obj.get_net_id());
    }

    // Read edges via positional indices.
    uint32_t edgeCount = 0;
    a(edgeCount);
    for (uint32_t i = 0; i < edgeCount; ++i) {
        uint32_t fromIdx = 0;
        uint32_t toIdx = 0;
        std::optional<std::string> label;
        a(fromIdx, toIdx);
        a(label);
        if (fromIdx >= idsByIndex.size() || toIdx >= idsByIndex.size()) {
            // Stale or corrupt index — skip the edge rather than referencing
            // a non-node. (A round-trip through current code can't produce
            // this; older saves with deleted-but-not-renumbered edges might.)
            continue;
        }
        edges->emplace_back_direct(edges, idsByIndex[fromIdx], idsByIndex[toIdx], std::move(label));
    }

    // Read layout map.
    uint32_t layoutCount = 0;
    a(layoutCount);
    for (uint32_t i = 0; i < layoutCount; ++i) {
        uint32_t idx = 0;
        float x = 0.0f, y = 0.0f;
        a(idx, x, y);
        if (idx >= idsByIndex.size()) continue;
        layout.emplace(idsByIndex[idx], Vector2f{x, y});
    }
}
