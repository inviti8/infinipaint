#include "LibMyPaintBridgeTest.hpp"

#include <iostream>

#ifdef HVYM_HAS_LIBMYPAINT

#include <cstdint>
#include <cstring>

extern "C" {
#include <mypaint-config.h>
#include <mypaint-fixed-tiled-surface.h>
#include <mypaint-surface.h>
#include <mypaint-tiled-surface.h>
}

#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkStream.h>
#include <include/encode/SkPngEncoder.h>

namespace {

// libmypaint's fixed tiled surface stores 16-bit-per-channel premultiplied RGBA
// (0xFFFF == 1.0). For the spike we un-premultiply and drop to 8 bits — the
// real production conversion (linear → sRGB, dithering, etc.) lives in the M2
// LibMyPaintSkiaSurface adapter.
inline uint8_t channel_to_8bit_unpremul(uint16_t c, uint16_t a) {
    if (a == 0) return 0;
    uint32_t v = (static_cast<uint32_t>(c) * 0xFFFF) / a;
    if (v > 0xFFFF) v = 0xFFFF;
    return static_cast<uint8_t>(v >> 8);
}

void copy_tile_to_bitmap(
    const guint16* tile,
    int tileSizePx,
    int dstX,
    int dstY,
    int dstWidth,
    int dstHeight,
    uint8_t* dstPixels,
    size_t dstRowBytes
) {
    const int rowsToCopy = (dstY + tileSizePx > dstHeight) ? (dstHeight - dstY) : tileSizePx;
    const int colsToCopy = (dstX + tileSizePx > dstWidth)  ? (dstWidth  - dstX) : tileSizePx;
    for (int row = 0; row < rowsToCopy; ++row) {
        const guint16* srcRow = tile + (row * tileSizePx * 4);
        uint8_t* dstRowPtr = dstPixels + ((dstY + row) * dstRowBytes) + (dstX * 4);
        for (int col = 0; col < colsToCopy; ++col) {
            const uint16_t r = srcRow[col * 4 + 0];
            const uint16_t g = srcRow[col * 4 + 1];
            const uint16_t b = srcRow[col * 4 + 2];
            const uint16_t a = srcRow[col * 4 + 3];
            dstRowPtr[col * 4 + 0] = channel_to_8bit_unpremul(r, a);
            dstRowPtr[col * 4 + 1] = channel_to_8bit_unpremul(g, a);
            dstRowPtr[col * 4 + 2] = channel_to_8bit_unpremul(b, a);
            dstRowPtr[col * 4 + 3] = static_cast<uint8_t>(a >> 8);
        }
    }
}

}  // namespace

namespace HVYM::Brushes {

bool run_libmypaint_hello_dab(const std::filesystem::path& outputPng) {
    constexpr int kWidth = 256;
    constexpr int kHeight = 256;

    MyPaintFixedTiledSurface* fixedSurface = mypaint_fixed_tiled_surface_new(kWidth, kHeight);
    if (!fixedSurface) {
        std::cerr << "[mypaint-hello-dab] failed to allocate MyPaintFixedTiledSurface\n";
        return false;
    }
    MyPaintSurface* surface = mypaint_fixed_tiled_surface_interface(fixedSurface);

    // Drive a single dab via the surface API directly. The full brush state
    // machine is exercised in M3; this spike only proves the surface bridge.
    mypaint_surface_begin_atomic(surface);
    mypaint_surface_draw_dab(
        surface,
        kWidth * 0.5f, kHeight * 0.5f,  // x, y
        40.0f,                           // radius
        0.0f, 0.0f, 0.0f,                // color RGB (black)
        1.0f,                            // opaque
        0.8f,                            // hardness
        0.0f,                            // softness  (libmypaint early-outs at softness==1.0)
        1.0f,                            // alpha_eraser
        1.0f, 0.0f,                      // aspect_ratio, angle
        0.0f,                            // lock_alpha
        0.0f,                            // colorize
        0.0f, 0.0f,                      // posterize, posterize_num
        1.0f                             // paint
    );
    mypaint_surface_end_atomic(surface, nullptr);

    constexpr int kTilePx = MYPAINT_TILE_SIZE;
    const int tilesW = (kWidth + kTilePx - 1) / kTilePx;
    const int tilesH = (kHeight + kTilePx - 1) / kTilePx;

    SkBitmap bitmap;
    if (!bitmap.tryAllocPixels(SkImageInfo::Make(
            kWidth, kHeight, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType))) {
        std::cerr << "[mypaint-hello-dab] SkBitmap allocation failed\n";
        mypaint_surface_unref(surface);
        return false;
    }
    uint8_t* dstPixels = static_cast<uint8_t*>(bitmap.getPixels());
    const size_t dstRowBytes = bitmap.rowBytes();

    MyPaintTiledSurface* tiled = reinterpret_cast<MyPaintTiledSurface*>(fixedSurface);

    for (int ty = 0; ty < tilesH; ++ty) {
        for (int tx = 0; tx < tilesW; ++tx) {
            MyPaintTileRequest req{};
            mypaint_tile_request_init(&req, 0, tx, ty, TRUE);
            mypaint_tiled_surface_tile_request_start(tiled, &req);
            if (req.buffer) {
                copy_tile_to_bitmap(
                    req.buffer, kTilePx,
                    tx * kTilePx, ty * kTilePx,
                    kWidth, kHeight,
                    dstPixels, dstRowBytes
                );
            }
            mypaint_tiled_surface_tile_request_end(tiled, &req);
        }
    }

    mypaint_surface_unref(surface);

    SkPixmap pixmap;
    if (!bitmap.peekPixels(&pixmap)) {
        std::cerr << "[mypaint-hello-dab] peekPixels failed\n";
        return false;
    }

    SkFILEWStream out(outputPng.string().c_str());
    if (!out.isValid()) {
        std::cerr << "[mypaint-hello-dab] could not open output file: "
                  << outputPng.string() << "\n";
        return false;
    }
    SkPngEncoder::Options opts;
    if (!SkPngEncoder::Encode(&out, pixmap, opts)) {
        std::cerr << "[mypaint-hello-dab] PNG encoding failed\n";
        return false;
    }

    std::cout << "[mypaint-hello-dab] wrote " << outputPng.string() << "\n";
    return true;
}

}  // namespace HVYM::Brushes

#else  // !HVYM_HAS_LIBMYPAINT

namespace HVYM::Brushes {
bool run_libmypaint_hello_dab(const std::filesystem::path&) {
    std::cerr << "[mypaint-hello-dab] this build was compiled without libmypaint\n";
    return false;
}
}

#endif
