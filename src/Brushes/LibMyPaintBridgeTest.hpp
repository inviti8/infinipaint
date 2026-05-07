#pragma once

// M1 spike (PHASE1.md §4): drive a single libmypaint dab onto a Skia raster
// surface and write it as a PNG. Proves the libmypaint → Skia pixel bridge
// before the real adapter (M2: LibMyPaintSkiaSurface) is built.
//
// Triggered from main.cpp via the --mypaint-hello-dab <out.png> CLI flag, then
// exits before the rest of the app starts. Compiled unconditionally; on
// targets without libmypaint (Emscripten, Android) the call returns false.

#include <filesystem>

namespace HVYM::Brushes {

bool run_libmypaint_hello_dab(const std::filesystem::path& outputPng);

}
