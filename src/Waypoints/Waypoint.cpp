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

// PHASE2 M5: CSS-style cubic-bezier easing presets. Control points are
// (x1, y1, x2, y2) — the same convention CSS's cubic-bezier() function
// uses, which BezierEasing accepts directly.
Eigen::Vector4f transition_easing_to_bezier_curve(TransitionEasing e) {
    switch (e) {
        case TransitionEasing::LINEAR:      return {0.0f, 0.0f, 1.0f, 1.0f};
        case TransitionEasing::EASE:        return {0.25f, 0.1f, 0.25f, 1.0f};
        case TransitionEasing::EASE_IN:     return {0.42f, 0.0f, 1.0f, 1.0f};
        case TransitionEasing::EASE_OUT:    return {0.0f, 0.0f, 0.58f, 1.0f};
        case TransitionEasing::EASE_IN_OUT: return {0.42f, 0.0f, 0.58f, 1.0f};
    }
    return {0.25f, 0.1f, 0.25f, 1.0f};  // Fallback to EASE on garbage input.
}

const std::vector<std::string>& transition_easing_display_names() {
    // Order MUST match the enum's numeric values so a dropdown can
    // index this list by static_cast<size_t>(TransitionEasing).
    static const std::vector<std::string> names = {
        "Linear",
        "Ease",
        "Ease in",
        "Ease out",
        "Ease in-out"
    };
    return names;
}

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
    // PHASE2 M4 / M5: per-waypoint reader-mode transition controls. Always
    // written from format v0.9 onward; the load path gates its read
    // on file version >= 0.9 so older files don't try to consume bytes
    // that aren't there.
    a(transitionSpeedMultiplier);
    a(static_cast<uint8_t>(transitionEasing));
    // TRANSITIONS.md — transition-point fields. Written from format
    // v0.10 onward; load path gates on >= 0.10.
    a(isTransition);
    a(stopTime);
}

void Waypoint::load_skin_from_archive(cereal::PortableBinaryInputArchive& a, VersionNumber) {
    std::vector<uint8_t> skinBytes;
    a(skinBytes);
    skin = decode_skin_png(skinBytes);
}

void Waypoint::load_transition_data_from_archive(cereal::PortableBinaryInputArchive& a, VersionNumber) {
    a(transitionSpeedMultiplier);
    transitionSpeedMultiplier = std::clamp(transitionSpeedMultiplier, TRANSITION_SPEED_MIN, TRANSITION_SPEED_MAX);
    uint8_t easingByte;
    a(easingByte);
    // Clamp to valid enum range; unknown values fall back to EASE.
    if (easingByte > static_cast<uint8_t>(TransitionEasing::EASE_IN_OUT))
        easingByte = static_cast<uint8_t>(TransitionEasing::EASE);
    transitionEasing = static_cast<TransitionEasing>(easingByte);
}

void Waypoint::load_transition_point_data_from_archive(cereal::PortableBinaryInputArchive& a, VersionNumber) {
    a(isTransition);
    a(stopTime);
    stopTime = std::clamp(stopTime, TRANSITION_STOP_TIME_MIN, TRANSITION_STOP_TIME_MAX);
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
