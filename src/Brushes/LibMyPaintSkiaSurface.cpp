#include "LibMyPaintSkiaSurface.hpp"

#ifdef HVYM_HAS_LIBMYPAINT

#include <algorithm>
#include <cstring>
#include <limits>

#include <cereal/types/vector.hpp>

#include <include/core/SkBitmap.h>
#include <include/core/SkImageInfo.h>

namespace HVYM::Brushes {

namespace {

// libmypaint stores tiles as 16-bit-per-channel premultiplied RGBA. Mirrors
// the conversion used in the M1 hello-dab spike (LibMyPaintBridgeTest.cpp).
inline uint8_t channel_to_8bit_unpremul(uint16_t c, uint16_t a) {
    if (a == 0) return 0;
    uint32_t v = (static_cast<uint32_t>(c) * 0xFFFF) / a;
    if (v > 0xFFFF) v = 0xFFFF;
    return static_cast<uint8_t>(v >> 8);
}

constexpr size_t kChannelsPerPx = 4;
constexpr size_t kPixelsPerTile = static_cast<size_t>(LibMyPaintSkiaSurface::kTilePx) * LibMyPaintSkiaSurface::kTilePx;
constexpr size_t kU16PerTile = kPixelsPerTile * kChannelsPerPx;

}  // namespace

LibMyPaintSkiaSurface::LibMyPaintSkiaSurface() {
    mypaint_tiled_surface_init(&tiled_, &s_tile_request_start, &s_tile_request_end);
}

LibMyPaintSkiaSurface::~LibMyPaintSkiaSurface() {
    mypaint_tiled_surface_destroy(&tiled_);
}

void LibMyPaintSkiaSurface::s_tile_request_start(MyPaintTiledSurface* tiled, MyPaintTileRequest* req) {
    auto* self = reinterpret_cast<LibMyPaintSkiaSurface*>(tiled);
    self->on_tile_request_start(req);
}

void LibMyPaintSkiaSurface::s_tile_request_end(MyPaintTiledSurface*, MyPaintTileRequest*) {
    // Tiles are stored directly; libmypaint wrote in place. Nothing to do.
}

void LibMyPaintSkiaSurface::on_tile_request_start(MyPaintTileRequest* req) {
    const TileKey key{req->tx, req->ty};
    auto it = tiles_.find(key);
    if (it == tiles_.end()) {
        // Read-only requests against an unallocated tile must still hand back
        // a real buffer (libmypaint reads the destination color). Allocate
        // and store it so subsequent writes find the same memory.
        auto buf = std::make_unique<uint16_t[]>(kU16PerTile);
        std::memset(buf.get(), 0, kU16PerTile * sizeof(uint16_t));
        it = tiles_.emplace(key, std::move(buf)).first;
    }
    req->buffer = it->second.get();
}

LibMyPaintSkiaSurface::PxRect LibMyPaintSkiaSurface::allocated_tile_bounds_px() const {
    if (tiles_.empty()) return {0, 0, 0, 0};
    int tx_min = std::numeric_limits<int>::max();
    int ty_min = std::numeric_limits<int>::max();
    int tx_max = std::numeric_limits<int>::min();
    int ty_max = std::numeric_limits<int>::min();
    for (const auto& [key, _] : tiles_) {
        tx_min = std::min(tx_min, key.tx);
        ty_min = std::min(ty_min, key.ty);
        tx_max = std::max(tx_max, key.tx);
        ty_max = std::max(ty_max, key.ty);
    }
    return {
        tx_min * kTilePx,
        ty_min * kTilePx,
        (tx_max - tx_min + 1) * kTilePx,
        (ty_max - ty_min + 1) * kTilePx,
    };
}

void LibMyPaintSkiaSurface::composite_to_bitmap(SkBitmap& dst, int dstOriginPxX, int dstOriginPxY) const {
    if (dst.colorType() != kRGBA_8888_SkColorType) return;
    uint8_t* dstPixels = static_cast<uint8_t*>(dst.getPixels());
    if (!dstPixels) return;
    const int dstW = dst.width();
    const int dstH = dst.height();
    const size_t dstRowBytes = dst.rowBytes();

    for (const auto& [key, buf] : tiles_) {
        const int tilePxX = key.tx * kTilePx;
        const int tilePxY = key.ty * kTilePx;
        const int dstX = tilePxX - dstOriginPxX;
        const int dstY = tilePxY - dstOriginPxY;

        const int copyX0 = std::max(0, dstX);
        const int copyY0 = std::max(0, dstY);
        const int copyX1 = std::min(dstW, dstX + kTilePx);
        const int copyY1 = std::min(dstH, dstY + kTilePx);
        if (copyX0 >= copyX1 || copyY0 >= copyY1) continue;

        const uint16_t* tile = buf.get();
        for (int row = copyY0; row < copyY1; ++row) {
            const int srcRowOffset = (row - dstY) * kTilePx * 4;
            uint8_t* dstRow = dstPixels + row * dstRowBytes + copyX0 * 4;
            const uint16_t* srcRow = tile + srcRowOffset + (copyX0 - dstX) * 4;
            for (int col = copyX0; col < copyX1; ++col) {
                const uint16_t r = srcRow[0];
                const uint16_t g = srcRow[1];
                const uint16_t b = srcRow[2];
                const uint16_t a = srcRow[3];
                dstRow[0] = channel_to_8bit_unpremul(r, a);
                dstRow[1] = channel_to_8bit_unpremul(g, a);
                dstRow[2] = channel_to_8bit_unpremul(b, a);
                dstRow[3] = static_cast<uint8_t>(a >> 8);
                srcRow += 4;
                dstRow += 4;
            }
        }
    }
}

void LibMyPaintSkiaSurface::save_tiles_to_archive(cereal::PortableBinaryOutputArchive& a) const {
    const uint32_t count = static_cast<uint32_t>(tiles_.size());
    a(count);
    constexpr size_t bytesPerTile = kU16PerTile * sizeof(uint16_t);
    for (const auto& [key, buf] : tiles_) {
        const int32_t tx = key.tx;
        const int32_t ty = key.ty;
        a(tx, ty);
        // Write the raw tile bytes via cereal's binary_data helper —
        // matches how cereal serializes PODs without per-element overhead.
        a(cereal::binary_data(buf.get(), bytesPerTile));
    }
}

void LibMyPaintSkiaSurface::load_tiles_from_archive(cereal::PortableBinaryInputArchive& a) {
    tiles_.clear();
    uint32_t count = 0;
    a(count);
    constexpr size_t bytesPerTile = kU16PerTile * sizeof(uint16_t);
    for (uint32_t i = 0; i < count; ++i) {
        int32_t tx = 0, ty = 0;
        a(tx, ty);
        auto buf = std::make_unique<uint16_t[]>(kU16PerTile);
        a(cereal::binary_data(buf.get(), bytesPerTile));
        tiles_.emplace(TileKey{tx, ty}, std::move(buf));
    }
}

void LibMyPaintSkiaSurface::copy_tiles_from(const LibMyPaintSkiaSurface& other) {
    tiles_.clear();
    for (const auto& [key, srcBuf] : other.tiles_) {
        auto buf = std::make_unique<uint16_t[]>(kU16PerTile);
        std::memcpy(buf.get(), srcBuf.get(), kU16PerTile * sizeof(uint16_t));
        tiles_.emplace(key, std::move(buf));
    }
}

}  // namespace HVYM::Brushes

#endif  // HVYM_HAS_LIBMYPAINT
