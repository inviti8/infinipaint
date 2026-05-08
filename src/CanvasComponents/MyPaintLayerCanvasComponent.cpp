#include "MyPaintLayerCanvasComponent.hpp"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>

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

void MyPaintLayerCanvasComponent::save_file(cereal::PortableBinaryOutputArchive&) const {
}

void MyPaintLayerCanvasComponent::load_file(cereal::PortableBinaryInputArchive&, VersionNumber) {
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

bool MyPaintLayerCanvasComponent::collides_within_coords(const SCollision::ColliderCollection<float>&) const {
    // Erase-by-overlap on raster layers is M3's "eraser interaction defined"
    // deliverable; for now the layer participates in no collisions.
    return false;
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

#endif
