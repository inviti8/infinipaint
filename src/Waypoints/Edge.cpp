#include "Edge.hpp"
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include "../World.hpp"

using namespace NetworkingObjects;

Edge::Edge() {}

Edge::Edge(NetObjID initFrom,
           NetObjID initTo,
           std::optional<std::string> initLabel)
    : from(initFrom),
      to(initTo),
      label(std::move(initLabel)) {}

void Edge::save_label_file(cereal::PortableBinaryOutputArchive& a) const {
    a(label);
}

void Edge::write_constructor_data(const NetObjTemporaryPtr<Edge>& o, cereal::PortableBinaryOutputArchive& a) {
    a(o->from, o->to, o->label);
}

void Edge::register_class(World& w) {
    auto readConstructorData = [](const NetObjTemporaryPtr<Edge>& o, cereal::PortableBinaryInputArchive& a, const std::shared_ptr<NetServer::ClientData>&) {
        a(o->from, o->to, o->label);
    };
    w.netObjMan.register_class<Edge, Edge, Edge, Edge>({
        .writeConstructorFuncClient = write_constructor_data,
        .readConstructorFuncClient  = readConstructorData,
        .readUpdateFuncClient       = nullptr,
        .writeConstructorFuncServer = write_constructor_data,
        .readConstructorFuncServer  = readConstructorData,
        .readUpdateFuncServer       = nullptr
    });
    register_ordered_list_class<Edge>(w.netObjMan);
}
