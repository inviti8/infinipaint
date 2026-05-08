#include "MyPaintLayerCanvasComponent.hpp"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>

#ifdef HVYM_HAS_LIBMYPAINT
extern "C" {
#include <mypaint-surface.h>
}
#include <algorithm>
#include <cmath>
#endif

CanvasComponentType MyPaintLayerCanvasComponent::get_type() const {
    return CanvasComponentType::MYPAINTLAYER;
}

#ifdef HVYM_HAS_LIBMYPAINT

MyPaintLayerCanvasComponent::MyPaintLayerCanvasComponent()
    : surface_(std::make_unique<HVYM::Brushes::LibMyPaintSkiaSurface>()) {}

void MyPaintLayerCanvasComponent::mark_dirty() {
    boundsCacheValid_ = false;
}

void MyPaintLayerCanvasComponent::save(cereal::PortableBinaryOutputArchive&) const {
    // M3-minimum stub — see header.
}

void MyPaintLayerCanvasComponent::load(cereal::PortableBinaryInputArchive&) {
}

void MyPaintLayerCanvasComponent::save_file(cereal::PortableBinaryOutputArchive& a) const {
    surface_->save_tiles_to_archive(a);
}

void MyPaintLayerCanvasComponent::load_file(cereal::PortableBinaryInputArchive& a, VersionNumber version) {
    if (version >= VersionNumber(0, 7, 0)) {
        surface_->load_tiles_from_archive(a);
        boundsCacheValid_ = false;
    }
    // Pre-0.7 files had MyPaintLayer save_file as a stub (no bytes
    // written), so there's nothing to read. The layer just stays empty.
}

std::unique_ptr<CanvasComponent> MyPaintLayerCanvasComponent::get_data_copy() const {
    return std::make_unique<MyPaintLayerCanvasComponent>();
}

void MyPaintLayerCanvasComponent::set_data_from(const CanvasComponent&) {
}

void MyPaintLayerCanvasComponent::draw(SkCanvas* canvas, const DrawData&, const std::shared_ptr<void>&) const {
    if (surface_->allocated_tile_count() == 0) return;
    const auto bounds = surface_->allocated_tile_bounds_px();
    if (bounds.empty()) return;

    SkBitmap bmp;
    if (!bmp.tryAllocPixels(SkImageInfo::Make(
            bounds.w, bounds.h, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType))) {
        return;
    }
    bmp.eraseARGB(0, 0, 0, 0);
    surface_->composite_to_bitmap(bmp, bounds.x, bounds.y);

    SkPaint paint;
    canvas->drawImage(bmp.asImage(),
                      static_cast<SkScalar>(bounds.x),
                      static_cast<SkScalar>(bounds.y),
                      SkSamplingOptions{},
                      &paint);
}

void MyPaintLayerCanvasComponent::initialize_draw_data(DrawingProgram&) {
}

bool MyPaintLayerCanvasComponent::collides_within_coords(const SCollision::ColliderCollection<float>& checkAgainst) const {
    // Coarse AABB-vs-AABB. EraserTool needs this to return true so the layer
    // gets through to its dispatch (which then punches destination-out dabs
    // instead of marking the whole component for delete). Per-pixel
    // precision isn't useful here: a dab outside the actually-painted
    // region is a no-op on the raster surface.
    const auto bounds = get_obj_coord_bounds();
    if (bounds.dim().x() <= 0.0f || bounds.dim().y() <= 0.0f) return false;
    return SCollision::collide(checkAgainst.bounds, bounds);
}

void MyPaintLayerCanvasComponent::erase_along_segment(const Vector2f& localStart, const Vector2f& localEnd, float localRadius) {
    if (localRadius <= 0.0f) return;
    MyPaintSurface* surf = surface_->surface();
    const Vector2f delta = localEnd - localStart;
    const float segLen = delta.norm();
    // ~one dab every half-radius along the segment, plus an anchor so a
    // zero-length segment (single click) still punches one dab.
    const float spacing = std::max(localRadius * 0.5f, 1.0f);
    const int steps = std::max(1, static_cast<int>(std::ceil(segLen / spacing)) + 1);
    mypaint_surface_begin_atomic(surf);
    for (int i = 0; i < steps; ++i) {
        const float t = (steps == 1) ? 0.0f : static_cast<float>(i) / static_cast<float>(steps - 1);
        const Vector2f p = localStart + delta * t;
        mypaint_surface_draw_dab(
            surf,
            p.x(), p.y(),
            localRadius,
            0.0f, 0.0f, 0.0f,  // color is irrelevant for an eraser dab (alpha_eraser=0)
            1.0f,              // opaque
            0.6f,              // hardness — somewhat soft eraser edge
            0.0f,              // softness  (NOT 1.0; that's libmypaint's draw-skip kill switch)
            0.0f,              // alpha_eraser=0 → destination-out
            1.0f, 0.0f,        // aspect_ratio, angle
            0.0f, 0.0f,        // lock_alpha, colorize
            0.0f, 0.0f,        // posterize, posterize_num
            1.0f               // paint
        );
    }
    mypaint_surface_end_atomic(surf, nullptr);
    mark_dirty();
}

SCollision::AABB<float> MyPaintLayerCanvasComponent::get_obj_coord_bounds() const {
    if (!boundsCacheValid_) {
        const auto px = surface_->allocated_tile_bounds_px();
        if (px.empty()) {
            boundsCache_ = SCollision::AABB<float>{};
        } else {
            boundsCache_.min = Vector2f(static_cast<float>(px.x), static_cast<float>(px.y));
            boundsCache_.max = Vector2f(static_cast<float>(px.x + px.w),
                                        static_cast<float>(px.y + px.h));
        }
        boundsCacheValid_ = true;
    }
    return boundsCache_;
}

#else  // !HVYM_HAS_LIBMYPAINT

MyPaintLayerCanvasComponent::MyPaintLayerCanvasComponent() {}
void MyPaintLayerCanvasComponent::save(cereal::PortableBinaryOutputArchive&) const {}
void MyPaintLayerCanvasComponent::load(cereal::PortableBinaryInputArchive&) {}
void MyPaintLayerCanvasComponent::save_file(cereal::PortableBinaryOutputArchive&) const {}
void MyPaintLayerCanvasComponent::load_file(cereal::PortableBinaryInputArchive&, VersionNumber) {}
std::unique_ptr<CanvasComponent> MyPaintLayerCanvasComponent::get_data_copy() const {
    return std::make_unique<MyPaintLayerCanvasComponent>();
}
void MyPaintLayerCanvasComponent::set_data_from(const CanvasComponent&) {}
void MyPaintLayerCanvasComponent::draw(SkCanvas*, const DrawData&, const std::shared_ptr<void>&) const {}
void MyPaintLayerCanvasComponent::initialize_draw_data(DrawingProgram&) {}
bool MyPaintLayerCanvasComponent::collides_within_coords(const SCollision::ColliderCollection<float>&) const { return false; }
SCollision::AABB<float> MyPaintLayerCanvasComponent::get_obj_coord_bounds() const { return {}; }
// No libmypaint in this build — erase has nothing to do (the layer can't
// have any pixels in the first place).
// (mark_dirty / surface() are gated by HVYM_HAS_LIBMYPAINT in the header.)

#endif
