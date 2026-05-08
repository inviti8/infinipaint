#include "LibMyPaintStrokeTest.hpp"

#include <cstdio>
#include <iostream>

#ifdef HVYM_HAS_LIBMYPAINT

#include <cmath>
#include <cstdint>

extern "C" {
#include <mypaint-config.h>
#include <mypaint-surface.h>
}

#include <include/core/SkBitmap.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkStream.h>
#include <include/encode/SkPngEncoder.h>

#include "LibMyPaintSkiaSurface.hpp"

namespace HVYM::Brushes {

namespace {

// Drive one dab through the surface's vfunc dispatch, wrapped in
// begin/end_atomic so the operation queue actually flushes to tiles.
// Mirrors the call shape established by the M1 hello-dab spike.
void draw_one_dab(MyPaintSurface* surface, float x, float y, float radius, float opaque) {
    mypaint_surface_begin_atomic(surface);
    mypaint_surface_draw_dab(
        surface,
        x, y,
        radius,
        0.05f, 0.05f, 0.10f,  // near-black ink
        opaque,
        0.85f,  // hardness
        0.0f,   // softness — must NOT be 1.0; libmypaint early-outs there
        1.0f,   // alpha_eraser
        1.0f, 0.0f,  // aspect_ratio, angle
        0.0f,   // lock_alpha
        0.0f,   // colorize
        0.0f, 0.0f,  // posterize, posterize_num
        1.0f    // paint
    );
    mypaint_surface_end_atomic(surface, nullptr);
}

}  // namespace

bool run_libmypaint_stroke_test(const std::filesystem::path& outputPng) {
    // Windows builds use SUBSYSTEM:WINDOWS so stdout/stderr are detached from
    // the launching console. Redirect both to a log next to the output PNG so
    // assertion diagnostics survive — and use raw fprintf+fflush since SDL's
    // SDL_APP_SUCCESS exit path bypasses normal C++ static destruction, so
    // std::cout's buffer would otherwise never flush.
    const std::filesystem::path logPath = std::filesystem::path(outputPng).replace_extension(".log");
    std::freopen(logPath.string().c_str(), "w", stderr);
    std::freopen(logPath.string().c_str(), "a", stdout);

    LibMyPaintSkiaSurface surface;

    // 10 dabs along a diagonal across roughly two tiles. Coordinates are
    // chosen to land in negative tile space too, exercising the lazy
    // allocation across the (tx,ty) origin where MyPaintFixedTiledSurface
    // would have refused to draw at all.
    constexpr int kNumDabs = 10;
    constexpr float kStartX = -40.0f;
    constexpr float kStartY = -20.0f;
    constexpr float kEndX = 90.0f;
    constexpr float kEndY = 70.0f;
    for (int i = 0; i < kNumDabs; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kNumDabs - 1);
        const float x = kStartX + (kEndX - kStartX) * t;
        const float y = kStartY + (kEndY - kStartY) * t;
        // Pressure-like falloff at stroke ends so the bbox isn't perfectly rectangular.
        const float pressure = 0.4f + 0.6f * std::sin(t * 3.14159f);
        draw_one_dab(surface.surface(), x, y, 12.0f, pressure);
    }

    // Assertion 1: at least one tile must be allocated.
    if (surface.allocated_tile_count() == 0) {
        std::fprintf(stderr, "[mypaint-stroke-test] no tiles allocated - surface bridge dropped every dab\n");
        std::fflush(stderr);
        return false;
    }

    // Assertion 2: bounds must be non-empty.
    const auto bounds = surface.allocated_tile_bounds_px();
    if (bounds.empty()) {
        std::fprintf(stderr, "[mypaint-stroke-test] tile bounds empty despite tiles allocated\n");
        std::fflush(stderr);
        return false;
    }

    SkBitmap bmp;
    if (!bmp.tryAllocPixels(SkImageInfo::Make(
            bounds.w, bounds.h, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType))) {
        std::fprintf(stderr, "[mypaint-stroke-test] SkBitmap allocation failed (%dx%d)\n", bounds.w, bounds.h);
        std::fflush(stderr);
        return false;
    }
    // Fill destination with transparent black before compositing — confirms
    // we only see pixels the surface actually wrote.
    bmp.eraseARGB(0, 0, 0, 0);

    surface.composite_to_bitmap(bmp, bounds.x, bounds.y);

    // Assertion 3: a pixel near the middle of the stroke path should have
    // non-zero alpha — proves dabs reached pixels (not just queued).
    const float midX = (kStartX + kEndX) * 0.5f;
    const float midY = (kStartY + kEndY) * 0.5f;
    const int sampleX = static_cast<int>(midX) - bounds.x;
    const int sampleY = static_cast<int>(midY) - bounds.y;
    if (sampleX < 0 || sampleY < 0 || sampleX >= bounds.w || sampleY >= bounds.h) {
        std::fprintf(stderr, "[mypaint-stroke-test] stroke midpoint outside composited bounds\n");
        std::fflush(stderr);
        return false;
    }
    SkPixmap pixmap;
    if (!bmp.peekPixels(&pixmap)) {
        std::fprintf(stderr, "[mypaint-stroke-test] peekPixels failed\n");
        std::fflush(stderr);
        return false;
    }
    const uint8_t* p = static_cast<const uint8_t*>(pixmap.addr(sampleX, sampleY));
    const uint8_t alphaAtMid = p[3];
    if (alphaAtMid == 0) {
        std::fprintf(stderr, "[mypaint-stroke-test] stroke midpoint pixel is fully transparent (a=0); dabs did not reach pixels\n");
        std::fflush(stderr);
        return false;
    }

    SkFILEWStream out(outputPng.string().c_str());
    if (!out.isValid()) {
        std::fprintf(stderr, "[mypaint-stroke-test] could not open output file: %s\n", outputPng.string().c_str());
        std::fflush(stderr);
        return false;
    }
    SkPngEncoder::Options opts;
    if (!SkPngEncoder::Encode(&out, pixmap, opts)) {
        std::fprintf(stderr, "[mypaint-stroke-test] PNG encoding failed\n");
        std::fflush(stderr);
        return false;
    }

    std::fprintf(stdout,
                 "[mypaint-stroke-test] tiles=%zu bounds=%dx%d mid_alpha=%d wrote %s\n",
                 surface.allocated_tile_count(),
                 bounds.w, bounds.h,
                 static_cast<int>(alphaAtMid),
                 outputPng.string().c_str());
    std::fflush(stdout);
    return true;
}

}  // namespace HVYM::Brushes

#else  // !HVYM_HAS_LIBMYPAINT

namespace HVYM::Brushes {
bool run_libmypaint_stroke_test(const std::filesystem::path&) {
    std::cerr << "[mypaint-stroke-test] this build was compiled without libmypaint\n";
    return false;
}
}

#endif
