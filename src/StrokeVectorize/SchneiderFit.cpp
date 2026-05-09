#include "SchneiderFit.hpp"

#include <algorithm>
#include <cmath>

namespace StrokeVectorize {

using Vec2 = Eigen::Vector2f;

namespace {

// Bernstein polynomial blending functions for cubic Bezier.
inline float B0(float u) { float mu = 1.0f - u; return mu * mu * mu; }
inline float B1(float u) { float mu = 1.0f - u; return 3.0f * mu * mu * u; }
inline float B2(float u) { float mu = 1.0f - u; return 3.0f * mu * u * u; }
inline float B3(float u) { return u * u * u; }

// Evaluate the cubic Bezier at parameter u.
Vec2 bezier_eval(const CubicBezier2D& bz, float u) {
    return B0(u) * bz.p0 + B1(u) * bz.p1 + B2(u) * bz.p2 + B3(u) * bz.p3;
}

// Compute first derivative at u: B'(t) = 3 * sum_{i=0..2} B_{i,2}(t) * (P_{i+1} - P_i).
Vec2 bezier_first_derivative(const CubicBezier2D& bz, float u) {
    const Vec2 q0 = bz.p1 - bz.p0;
    const Vec2 q1 = bz.p2 - bz.p1;
    const Vec2 q2 = bz.p3 - bz.p2;
    const float mu = 1.0f - u;
    return 3.0f * (mu * mu * q0 + 2.0f * mu * u * q1 + u * u * q2);
}

// Second derivative: B''(t) = 6 * ((1-t)*(P2 - 2P1 + P0) + t*(P3 - 2P2 + P1)).
Vec2 bezier_second_derivative(const CubicBezier2D& bz, float u) {
    const Vec2 r0 = bz.p2 - 2.0f * bz.p1 + bz.p0;
    const Vec2 r1 = bz.p3 - 2.0f * bz.p2 + bz.p1;
    return 6.0f * ((1.0f - u) * r0 + u * r1);
}

// Initial parameter assignment via accumulated chord length, normalized to [0, 1].
std::vector<float> chord_length_parametrize(const std::vector<Vec2>& pts, int first, int last) {
    std::vector<float> u(static_cast<size_t>(last - first + 1));
    u[0] = 0.0f;
    for (int i = first + 1; i <= last; ++i) {
        const float d = (pts[i] - pts[i - 1]).norm();
        u[i - first] = u[i - first - 1] + d;
    }
    const float total = u.back();
    if (total <= 0.0f) {
        // Degenerate (all points coincide). Spread evenly so callers don't divide by zero.
        for (size_t i = 0; i < u.size(); ++i)
            u[i] = static_cast<float>(i) / static_cast<float>(u.size() - 1);
        return u;
    }
    for (auto& v : u) v /= total;
    return u;
}

// Generate a cubic Bezier fitting pts[first..last] with given parameters
// and constrained endpoint tangents. Solves a 2x2 least-squares system
// for the magnitudes of P1-P0 and P3-P2 along the tangent directions
// (Schneider 1990, eq 2-3). Falls back to a Wu-Barsky heuristic when the
// system is near-singular (1/3 chord length placement).
CubicBezier2D generate_bezier(
    const std::vector<Vec2>& pts,
    int first,
    int last,
    const std::vector<float>& uParam,
    const Vec2& leftTangent,
    const Vec2& rightTangent)
{
    const int n = last - first + 1;
    CubicBezier2D bz{};
    bz.p0 = pts[first];
    bz.p3 = pts[last];

    // Compute the A matrix: A[i] is a 2-vector array (one for each control point's basis).
    std::vector<std::array<Vec2, 2>> A(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const float u = uParam[i];
        A[i][0] = leftTangent * B1(u);
        A[i][1] = rightTangent * B2(u);
    }

    // Compute C matrix (2x2) and X vector (2x1) for the normal equations.
    float C00 = 0.0f, C01 = 0.0f, C11 = 0.0f;
    float X0 = 0.0f, X1 = 0.0f;
    for (int i = 0; i < n; ++i) {
        C00 += A[i][0].dot(A[i][0]);
        C01 += A[i][0].dot(A[i][1]);
        C11 += A[i][1].dot(A[i][1]);

        const float u = uParam[i];
        const Vec2 tmp = pts[first + i] - (B0(u) * bz.p0 + B1(u) * bz.p0 + B2(u) * bz.p3 + B3(u) * bz.p3);
        X0 += A[i][0].dot(tmp);
        X1 += A[i][1].dot(tmp);
    }

    // Solve via Cramer's rule.
    const float detC = C00 * C11 - C01 * C01;
    const float detC0X = C00 * X1 - C01 * X0;
    const float detXC1 = X0 * C11 - X1 * C01;

    const float chordLen = (bz.p3 - bz.p0).norm();
    constexpr float kEpsilon = 1e-12f;

    float alphaL, alphaR;
    if (std::abs(detC) < kEpsilon) {
        // Near-singular: fall back to placing control points at 1/3 of the chord length.
        alphaL = chordLen / 3.0f;
        alphaR = chordLen / 3.0f;
    } else {
        alphaL = detXC1 / detC;
        alphaR = detC0X / detC;
    }

    // Negative or wildly-too-small magnitudes also indicate the least-squares solver
    // produced a degenerate fit (sharp corner near an endpoint, etc.). Use the same
    // 1/3 chord fallback — Schneider's "Wu-Barsky" heuristic.
    constexpr float kSmall = 1e-6f;
    if (alphaL < kSmall || alphaR < kSmall) {
        alphaL = chordLen / 3.0f;
        alphaR = chordLen / 3.0f;
    }

    bz.p1 = bz.p0 + alphaL * leftTangent;
    bz.p2 = bz.p3 + alphaR * rightTangent;
    return bz;
}

// Walk the points and find the maximum-error point along the fitted curve.
// Returns (maxError^2, indexOfMaxErrorPoint). Endpoint distances are zero
// by construction, so we only check the interior.
std::pair<float, int> compute_max_error(
    const std::vector<Vec2>& pts,
    int first,
    int last,
    const CubicBezier2D& bz,
    const std::vector<float>& uParam)
{
    float maxErrSq = 0.0f;
    int splitIndex = (first + last) / 2;
    const int n = last - first + 1;
    for (int i = 1; i < n - 1; ++i) {
        const Vec2 onCurve = bezier_eval(bz, uParam[i]);
        const Vec2 diff = onCurve - pts[first + i];
        const float distSq = diff.squaredNorm();
        if (distSq >= maxErrSq) {
            maxErrSq = distSq;
            splitIndex = first + i;
        }
    }
    return {maxErrSq, splitIndex};
}

// Single Newton-Raphson step: improve a single u-parameter by moving it
// along the curve toward the input point. Returns the refined u.
float newton_raphson_root_find(const CubicBezier2D& bz, const Vec2& point, float u) {
    const Vec2 onCurve = bezier_eval(bz, u);
    const Vec2 d1 = bezier_first_derivative(bz, u);
    const Vec2 d2 = bezier_second_derivative(bz, u);
    const Vec2 diff = onCurve - point;

    const float numerator = diff.dot(d1);
    const float denominator = d1.dot(d1) + diff.dot(d2);
    if (std::abs(denominator) < 1e-12f) return u;
    return u - numerator / denominator;
}

// Apply one Newton-Raphson refinement pass to all parameters.
std::vector<float> reparameterize(
    const std::vector<Vec2>& pts,
    int first,
    int last,
    const std::vector<float>& uParam,
    const CubicBezier2D& bz)
{
    std::vector<float> uPrime(uParam.size());
    const int n = last - first + 1;
    for (int i = 0; i < n; ++i)
        uPrime[i] = newton_raphson_root_find(bz, pts[first + i], uParam[i]);
    return uPrime;
}

// Estimate the tangent direction at the start of pts[first..last]
// using the unit vector toward the next point.
Vec2 tangent_at_start(const std::vector<Vec2>& pts, int first, int last) {
    Vec2 t = pts[first + 1] - pts[first];
    // Walk forward through any duplicate points so we get a usable direction.
    int j = first + 1;
    while (t.squaredNorm() < 1e-20f && j < last) {
        ++j;
        t = pts[j] - pts[first];
    }
    if (t.squaredNorm() < 1e-20f) return Vec2{1.0f, 0.0f};
    return t.normalized();
}

Vec2 tangent_at_end(const std::vector<Vec2>& pts, int first, int last) {
    Vec2 t = pts[last - 1] - pts[last];
    int j = last - 1;
    while (t.squaredNorm() < 1e-20f && j > first) {
        --j;
        t = pts[j] - pts[last];
    }
    if (t.squaredNorm() < 1e-20f) return Vec2{-1.0f, 0.0f};
    return t.normalized();
}

// Tangent at an interior split point — average of the chords on each side.
Vec2 tangent_at_center(const std::vector<Vec2>& pts, int center) {
    const Vec2 v1 = pts[center - 1] - pts[center];
    const Vec2 v2 = pts[center] - pts[center + 1];
    Vec2 t = (v1 + v2) * 0.5f;
    if (t.squaredNorm() < 1e-20f) {
        // If the two chords cancel, fall back to one of them.
        if (v1.squaredNorm() > 1e-20f) return v1.normalized();
        if (v2.squaredNorm() > 1e-20f) return v2.normalized();
        return Vec2{1.0f, 0.0f};
    }
    return t.normalized();
}

void fit_cubic_recursive(
    const std::vector<Vec2>& pts,
    int first,
    int last,
    const Vec2& leftTangent,
    const Vec2& rightTangent,
    float errorToleranceSq,
    std::vector<CubicBezier2D>& out)
{
    // Two-point case: degenerate to a straight line laid out as a Bezier.
    if (last - first == 1) {
        const Vec2 p0 = pts[first];
        const Vec2 p3 = pts[last];
        const float dist = (p3 - p0).norm() / 3.0f;
        CubicBezier2D bz{};
        bz.p0 = p0;
        bz.p3 = p3;
        bz.p1 = p0 + leftTangent * dist;
        bz.p2 = p3 + rightTangent * dist;
        out.push_back(bz);
        return;
    }

    auto u = chord_length_parametrize(pts, first, last);
    CubicBezier2D bz = generate_bezier(pts, first, last, u, leftTangent, rightTangent);
    auto [maxErrSq, splitIndex] = compute_max_error(pts, first, last, bz, u);

    if (maxErrSq < errorToleranceSq) {
        out.push_back(bz);
        return;
    }

    // Try Newton-Raphson reparameterization once (Schneider's refinement).
    // The original paper uses a heuristic of 4*tolerance^2 as the cutoff
    // beyond which reparameterization isn't worth attempting.
    if (maxErrSq < errorToleranceSq * 4.0f) {
        constexpr int kReparamIters = 4;
        for (int iter = 0; iter < kReparamIters; ++iter) {
            u = reparameterize(pts, first, last, u, bz);
            bz = generate_bezier(pts, first, last, u, leftTangent, rightTangent);
            auto [me2, si2] = compute_max_error(pts, first, last, bz, u);
            maxErrSq = me2;
            splitIndex = si2;
            if (maxErrSq < errorToleranceSq) {
                out.push_back(bz);
                return;
            }
        }
    }

    // Tolerance still not met — split at the worst-error point and recurse.
    // Make sure we don't split at an endpoint (would infinite-loop).
    if (splitIndex <= first) splitIndex = first + 1;
    if (splitIndex >= last) splitIndex = last - 1;

    const Vec2 centerTangent = tangent_at_center(pts, splitIndex);
    fit_cubic_recursive(pts, first, splitIndex, leftTangent, -centerTangent, errorToleranceSq, out);
    fit_cubic_recursive(pts, splitIndex, last, centerTangent, rightTangent, errorToleranceSq, out);
}

}  // namespace

std::vector<CubicBezier2D> fit_cubic_beziers(
    const std::vector<Vec2>& points,
    float errorTolerance)
{
    std::vector<CubicBezier2D> out;
    if (points.size() < 2) return out;

    const Vec2 leftTangent  = tangent_at_start(points, 0, static_cast<int>(points.size()) - 1);
    const Vec2 rightTangent = tangent_at_end(points, 0, static_cast<int>(points.size()) - 1);

    const float toleranceSq = errorTolerance * errorTolerance;
    fit_cubic_recursive(points, 0, static_cast<int>(points.size()) - 1,
                        leftTangent, rightTangent, toleranceSq, out);
    return out;
}

}  // namespace StrokeVectorize
