#include "Waypoint.hpp"
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include "../World.hpp"
#include "../ScaleUpCanvas.hpp"

#include <include/codec/SkCodec.h>
#include <include/codec/SkPngDecoder.h>
#include <include/core/SkData.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkStream.h>
#include <include/encode/SkPngEncoder.h>

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

namespace {
// Encode an SkImage as PNG bytes for serialization. Returns empty
// vector on failure (or no skin) — callers treat that as "no skin".
std::vector<uint8_t> encode_skin_png(const sk_sp<SkImage>& img) {
    if (!img) return {};
    SkPixmap pix;
    sk_sp<SkImage> raster = img->makeRasterImage(nullptr);
    if (!raster || !raster->peekPixels(&pix)) return {};
    SkDynamicMemoryWStream stream;
    if (!SkPngEncoder::Encode(&stream, pix, {})) return {};
    auto skData = stream.detachAsData();
    if (!skData) return {};
    std::vector<uint8_t> out(skData->size());
    std::memcpy(out.data(), skData->bytes(), skData->size());
    return out;
}

sk_sp<SkImage> decode_skin_png(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) return nullptr;
    auto data = SkData::MakeWithCopy(bytes.data(), bytes.size());
    if (!data) return nullptr;
    auto codec = SkCodec::MakeFromData(data, {SkPngDecoder::Decoder()});
    if (!codec) return nullptr;
    auto [img, _] = codec->getImage();
    return img;
}
}  // namespace

void Waypoint::save_file(cereal::PortableBinaryOutputArchive& a) const {
    a(label, coords, windowSize);
    std::vector<uint8_t> skinBytes = encode_skin_png(skin);
    a(skinBytes);
}

void Waypoint::load_skin_from_archive(cereal::PortableBinaryInputArchive& a, VersionNumber) {
    std::vector<uint8_t> skinBytes;
    a(skinBytes);
    skin = decode_skin_png(skinBytes);
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
