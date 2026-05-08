#pragma once

#include <filesystem>

namespace HVYM::Brushes {

// M2 test (PHASE1.md §10): drives a synthetic multi-dab stroke through the
// LibMyPaintSkiaSurface adapter, composites the touched tiles into an
// SkBitmap, and writes outputPng. Returns false on any programmatic
// assertion failure (no tiles allocated, sampled stroke pixel transparent,
// etc.) so the caller can surface a non-zero process exit.
bool run_libmypaint_stroke_test(const std::filesystem::path& outputPng);

}  // namespace HVYM::Brushes
