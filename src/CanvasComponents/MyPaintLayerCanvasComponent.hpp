#pragma once
#include "CanvasComponent.hpp"
#include "Helpers/SCollision.hpp"

#ifdef HVYM_HAS_LIBMYPAINT
#include "../Brushes/LibMyPaintSkiaSurface.hpp"
#endif

#include <memory>

// PHASE1.md §3 — per-stroke raster layer that owns a LibMyPaintSkiaSurface.
// One MyPaintBrushTool stroke produces one of these, mirroring how each
// vector BrushTool stroke produces a BrushStrokeCanvasComponent. Tiles live
// in component-local pixel coordinates; the container's CoordSpaceHelper
// places the layer in world space, exactly like BrushStroke.
//
// Serialization is intentionally a stub for the M3 minimum slice — Phase 1
// §8 spells out the full PNG-per-tile format (M4 file-format work). For now
// strokes are session-only.
class MyPaintLayerCanvasComponent : public CanvasComponent {
    public:
        MyPaintLayerCanvasComponent();

        virtual CanvasComponentType get_type() const override;
        virtual void save(cereal::PortableBinaryOutputArchive& a) const override;
        virtual void load(cereal::PortableBinaryInputArchive& a) override;
        virtual void save_file(cereal::PortableBinaryOutputArchive& a) const override;
        virtual void load_file(cereal::PortableBinaryInputArchive& a, VersionNumber version) override;
        virtual std::unique_ptr<CanvasComponent> get_data_copy() const override;
        virtual void set_data_from(const CanvasComponent& other) override;

#ifdef HVYM_HAS_LIBMYPAINT
        // Public so the tool can hand it to a MyPaintBrush mid-stroke.
        HVYM::Brushes::LibMyPaintSkiaSurface& surface() { return *surface_; }

        // Called by the tool after each end_atomic so cached drawables
        // (currently just the bounds) recompute on the next draw.
        void mark_dirty();

        // Punch destination-out dabs along the segment localStart -> localEnd.
        // localRadius is in component-local pixels (caller is responsible for
        // converting camera-space radius via the same coord transform used
        // for the endpoints). Used by EraserTool — see PHASE1.md §4 risks
        // ("destination-out dab pass on the raster surface for libmypaint
        // layers, unchanged path-erase for vector strokes").
        void erase_along_segment(const Vector2f& localStart, const Vector2f& localEnd, float localRadius);
#endif

    private:
        virtual void draw(SkCanvas* canvas, const DrawData& drawData, const std::shared_ptr<void>& predrawData) const override;
        virtual void initialize_draw_data(DrawingProgram& drawP) override;
        virtual bool collides_within_coords(const SCollision::ColliderCollection<float>& checkAgainst) const override;
        virtual SCollision::AABB<float> get_obj_coord_bounds() const override;

#ifdef HVYM_HAS_LIBMYPAINT
        // unique_ptr so the component is movable/copyable-controlled even
        // though LibMyPaintSkiaSurface is non-copyable. get_data_copy makes a
        // fresh empty surface — copying a populated raster layer is a
        // follow-up (M4 file-format work needs the same machinery).
        std::unique_ptr<HVYM::Brushes::LibMyPaintSkiaSurface> surface_;

        mutable bool boundsCacheValid_ = false;
        mutable SCollision::AABB<float> boundsCache_{};
#endif
};
