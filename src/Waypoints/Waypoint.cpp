#include "Waypoint.hpp"
#include <cereal/types/string.hpp>
#include "../World.hpp"
#include "../ScaleUpCanvas.hpp"

using namespace NetworkingObjects;

Waypoint::Waypoint() {}

Waypoint::Waypoint(const std::string& initLabel,
                   const CoordSpaceHelper& initCoords,
                   const Vector<int32_t, 2>& initWindowSize)
    : label(initLabel),
      coords(initCoords),
      windowSize(initWindowSize) {}

void Waypoint::scale_up(const WorldScalar& scaleUpAmount) {
    coords.scale_about(WorldVec{0, 0}, scaleUpAmount, true);
}

void Waypoint::save_file(cereal::PortableBinaryOutputArchive& a) const {
    a(label, coords, windowSize);
}

void Waypoint::write_constructor_data(const NetObjTemporaryPtr<Waypoint>& o, cereal::PortableBinaryOutputArchive& a) {
    a(o->label, o->coords, o->windowSize);
}

void Waypoint::register_class(World& w) {
    auto readConstructorData = [&w](const NetObjTemporaryPtr<Waypoint>& o, cereal::PortableBinaryInputArchive& a, const std::shared_ptr<NetServer::ClientData>& c) {
        a(o->label, o->coords, o->windowSize);
        canvas_scale_up_check(*o, w, c);
    };
    w.netObjMan.register_class<Waypoint, Waypoint, Waypoint, Waypoint>({
        .writeConstructorFuncClient = write_constructor_data,
        .readConstructorFuncClient  = readConstructorData,
        .readUpdateFuncClient       = nullptr,
        .writeConstructorFuncServer = write_constructor_data,
        .readConstructorFuncServer  = readConstructorData,
        .readUpdateFuncServer       = nullptr
    });
    register_ordered_list_class<Waypoint>(w.netObjMan);
}
