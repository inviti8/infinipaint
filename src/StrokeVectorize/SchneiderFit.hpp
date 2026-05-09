#pragma once
#include <Eigen/Core>
#include <vector>

namespace StrokeVectorize {

// A single cubic Bezier segment in 2D. p0 + p3 are the curve's
// endpoints; p1, p2 are the control points. Evaluation:
//   B(t) = (1-t)^3 * p0 + 3(1-t)^2 t * p1 + 3(1-t) t^2 * p2 + t^3 * p3
struct CubicBezier2D {
    Eigen::Vector2f p0, p1, p2, p3;
};

// Fit a sequence of cubic Bezier segments through the input polyline
// such that no input point is more than `errorTolerance` away from
// the fitted curve. Adjacent output beziers join endpoint-to-endpoint
// (segment N's p3 == segment N+1's p0).
//
// Implementation: Schneider 1990 ("An Algorithm for Automatically
// Fitting Digitized Curves," Graphics Gems I) — chord-length
// parameterization, least-squares solve for inner control points
// given fixed tangent directions at endpoints, Newton-Raphson
// reparameterization refinement, recursive splitting at the
// worst-error point when tolerance can't be met.
//
// `errorTolerance` is in the same units as the input points (pixels
// for canvas-local stroke samples). Smaller tolerance → more segments.
// For ink-style libmypaint strokes, ~0.5 to ~1.0 px gives a good
// fidelity / segment-count tradeoff.
//
// Returns empty vector on degenerate input (fewer than 2 points).
// For exactly 2 points, returns a single bezier laid out as a straight
// line (control points placed at 1/3 + 2/3 along the segment).
std::vector<CubicBezier2D> fit_cubic_beziers(
    const std::vector<Eigen::Vector2f>& points,
    float errorTolerance);

}  // namespace StrokeVectorize
