#pragma once

#include <cereal/archives/portable_binary.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

#ifdef HVYM_HAS_LIBMYPAINT

extern "C" {
#include <mypaint-config.h>
#include <mypaint-surface.h>
#include <mypaint-tiled-surface.h>
}

class SkBitmap;

namespace HVYM::Brushes {

// Adapter from libmypaint's tile-grid model onto a lazily-allocated tile
// store, suitable for an infinite canvas. Implements the MyPaintTiledSurface
// vfunc pair (tile_request_start / tile_request_end). Tiles are allocated on
// first request and zero-initialized (fully transparent), unlike
// MyPaintFixedTiledSurface which pre-allocates every tile and starts opaque
// white.
//
// PHASE1.md §4 (Tile-model bridge risk): drives the M2 deliverable — replaces
// the M1 hello-dab spike's reliance on MyPaintFixedTiledSurface.
class LibMyPaintSkiaSurface {
public:
    LibMyPaintSkiaSurface();
    ~LibMyPaintSkiaSurface();

    LibMyPaintSkiaSurface(const LibMyPaintSkiaSurface&) = delete;
    LibMyPaintSkiaSurface& operator=(const LibMyPaintSkiaSurface&) = delete;

    // Hand to libmypaint for begin_atomic / draw_dab / end_atomic.
    MyPaintSurface* surface() { return &tiled_.parent; }

    static constexpr int kTilePx = MYPAINT_TILE_SIZE;

    struct TileKey {
        int tx;
        int ty;
        bool operator==(const TileKey& o) const noexcept { return tx == o.tx && ty == o.ty; }
    };
    struct TileKeyHash {
        size_t operator()(const TileKey& k) const noexcept {
            return std::hash<uint64_t>{}(
                (static_cast<uint64_t>(static_cast<uint32_t>(k.tx)) << 32) |
                static_cast<uint64_t>(static_cast<uint32_t>(k.ty)));
        }
    };

    size_t allocated_tile_count() const { return tiles_.size(); }

    // Pixel-space bounding box that covers every currently allocated tile.
    // Returns {0,0,0,0} when nothing is allocated.
    struct PxRect {
        int x, y, w, h;
        bool empty() const { return w <= 0 || h <= 0; }
    };
    PxRect allocated_tile_bounds_px() const;

    // Composite every currently allocated tile into dst. dst must already be
    // allocated as kRGBA_8888 / kUnpremul, sized large enough to cover the
    // pixels at (dstOriginPxX, dstOriginPxY) through (dstOriginPxX + dst.width(),
    // dstOriginPxY + dst.height()). Tiles outside that destination rect are
    // skipped. dst pixels outside any allocated tile are left untouched.
    void composite_to_bitmap(SkBitmap& dst, int dstOriginPxX, int dstOriginPxY) const;

    // Persist tile data: count + per-tile (tx, ty, raw 16bpc-premul-RGBA
    // buffer). Format is uncompressed for simplicity — each 64x64 tile
    // is 32 KiB; libmypaint strokes typically touch only a handful of
    // tiles, so total cost is small. zstd / per-tile PNG is a future
    // optimization if file sizes become problematic.
    void save_tiles_to_archive(cereal::PortableBinaryOutputArchive& a) const;
    // Drops any existing tiles, then loads the count + tiles back.
    void load_tiles_from_archive(cereal::PortableBinaryInputArchive& a);

private:
    static void s_tile_request_start(MyPaintTiledSurface*, MyPaintTileRequest*);
    static void s_tile_request_end(MyPaintTiledSurface*, MyPaintTileRequest*);

    void on_tile_request_start(MyPaintTileRequest*);

    // First member: libmypaint hands callbacks a MyPaintTiledSurface*, which
    // we cast back to LibMyPaintSkiaSurface*. Keeping tiled_ at offset 0
    // makes the cast well-defined.
    MyPaintTiledSurface tiled_;

    using TileBuffer = std::unique_ptr<uint16_t[]>;
    std::unordered_map<TileKey, TileBuffer, TileKeyHash> tiles_;
};

}  // namespace HVYM::Brushes

#endif  // HVYM_HAS_LIBMYPAINT
