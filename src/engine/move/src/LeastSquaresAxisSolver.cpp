// A3 Least Squares Axis Move (README §4.19). This is a POINT-level routine, not a
// part move: it moves the two object end points onto a least-squares axis fitted
// to a set of target centre points, but it does NOT move the part. The
// IMoveSolver contract returns a rigid part transform, so this solver returns the
// IDENTITY (no part motion) and reports the fitted axis (a point on it + unit
// direction) in the result log. The A4 geometry layer applies the actual point
// repositioning (axis-to-end-face intersection per README §4.19).
//
// Axis fit: the direction is the dominant principal component (largest-eigenvalue
// eigenvector of the target points' covariance); the axis point is their
// centroid. Implemented with a self-contained symmetric 3x3 Jacobi eigensolver
// (no external dependency).
//
// Inputs: target centre points = pairs[i].targetPoint.
//
// Original clean-room implementation.
#include <cmath>
#include <string>

#include "MoveMath.h"
#include "opendva/IMoveSolver.h"

namespace opendva {
namespace {

using namespace movemath;

using Mat3 = double[3][3];

bool finiteVec(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

Vec3 centroid(const MoveInputs& in) {
    const Vec3 origin = in.pairs.front().targetPoint;
    double scaleValue = 0.0;
    for (const auto& pr : in.pairs) {
        const Vec3 p = pr.targetPoint;
        scaleValue = std::max(scaleValue, std::fabs(origin.x));
        scaleValue = std::max(scaleValue, std::fabs(origin.y));
        scaleValue = std::max(scaleValue, std::fabs(origin.z));
        scaleValue = std::max(scaleValue, std::fabs(p.x));
        scaleValue = std::max(scaleValue, std::fabs(p.y));
        scaleValue = std::max(scaleValue, std::fabs(p.z));
    }
    if (scaleValue <= 0.0 || !std::isfinite(scaleValue)) return origin;
    Vec3 meanOffset{};
    double n = 0.0;
    for (const auto& pr : in.pairs) {
        const Vec3 p = pr.targetPoint;
        n += 1.0;
        const Vec3 offset{p.x / scaleValue - origin.x / scaleValue,
                          p.y / scaleValue - origin.y / scaleValue,
                          p.z / scaleValue - origin.z / scaleValue};
        meanOffset.x += (offset.x - meanOffset.x) / n;
        meanOffset.y += (offset.y - meanOffset.y) / n;
        meanOffset.z += (offset.z - meanOffset.z) / n;
    }
    return {scaleValue * (origin.x / scaleValue + meanOffset.x),
            scaleValue * (origin.y / scaleValue + meanOffset.y),
            scaleValue * (origin.z / scaleValue + meanOffset.z)};
}

// Jacobi eigen-decomposition of a symmetric 3x3 matrix; eigenvectors as columns
// of V, eigenvalues in w.
void jacobiEigenSym3(Mat3 A, double w[3], Mat3 V) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) V[i][j] = (i == j) ? 1.0 : 0.0;
    for (int sweep = 0; sweep < 50; ++sweep) {
        const double off = std::fabs(A[0][1]) + std::fabs(A[0][2]) + std::fabs(A[1][2]);
        if (off < 1e-18) break;
        for (int p = 0; p < 2; ++p) {
            for (int q = p + 1; q < 3; ++q) {
                if (std::fabs(A[p][q]) < 1e-300) continue;
                const double phi = 0.5 * std::atan2(2.0 * A[p][q], A[q][q] - A[p][p]);
                const double c = std::cos(phi), s = std::sin(phi);
                for (int k = 0; k < 3; ++k) {
                    const double akp = A[k][p], akq = A[k][q];
                    A[k][p] = c * akp - s * akq;
                    A[k][q] = s * akp + c * akq;
                }
                for (int k = 0; k < 3; ++k) {
                    const double apk = A[p][k], aqk = A[q][k];
                    A[p][k] = c * apk - s * aqk;
                    A[q][k] = s * apk + c * aqk;
                }
                for (int k = 0; k < 3; ++k) {
                    const double vkp = V[k][p], vkq = V[k][q];
                    V[k][p] = c * vkp - s * vkq;
                    V[k][q] = s * vkp + c * vkq;
                }
            }
        }
    }
    for (int i = 0; i < 3; ++i) w[i] = A[i][i];
}

class LeastSquaresAxisSolver final : public IMoveSolver {
public:
    MoveType type() const override { return MoveType::LeastSquaresAxis; }

    MoveResult solve(const MoveInputs& in, IRng& /*rng*/) override {
        MoveResult res{};
        if (in.pairs.size() < 2) {
            res.log = "No Solution: LSQ-Axis requires >= 2 target centre points";
            return res;
        }
        for (const auto& pr : in.pairs) {
            if (!finiteVec(pr.targetPoint)) {
                res.log = "No Solution: LSQ-Axis target centre point is not finite";
                return res;
            }
        }
        const Vec3 cen = centroid(in);

        // Covariance matrix of the centred points.
        double spreadScale = 0.0;
        for (const auto& pr : in.pairs) {
            const Vec3 d = sub(pr.targetPoint, cen);
            spreadScale = std::max(spreadScale, std::fabs(d.x));
            spreadScale = std::max(spreadScale, std::fabs(d.y));
            spreadScale = std::max(spreadScale, std::fabs(d.z));
        }
        if (spreadScale <= 0.0 || !std::isfinite(spreadScale)) {
            res.log = "No Solution: LSQ-Axis axis is degenerate";
            return res;
        }

        Mat3 C = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
        double maxSpread2 = 0.0;
        for (const auto& pr : in.pairs) {
            const Vec3 d = scale(sub(pr.targetPoint, cen), 1.0 / spreadScale);
            maxSpread2 = std::max(maxSpread2, dot(d, d));
            const double v[3] = {d.x, d.y, d.z};
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) C[i][j] += v[i] * v[j];
        }
        if (maxSpread2 < 1e-24) {
            res.log = "No Solution: LSQ-Axis axis is degenerate";
            return res;
        }

        double w[3];
        Mat3 V;
        jacobiEigenSym3(C, w, V);
        // Dominant principal component = eigenvector with the largest eigenvalue.
        int best = 0;
        for (int i = 1; i < 3; ++i)
            if (w[i] > w[best]) best = i;
        const Vec3 dir = normalise(Vec3{V[0][best], V[1][best], V[2][best]});

        // Part is NOT moved: return identity, report the fitted axis in the log.
        res.transform = Mat34{};
        res.ok = true;
        res.log = "Fitted axis: point=(" + std::to_string(cen.x) + ", " +
                  std::to_string(cen.y) + ", " + std::to_string(cen.z) + ") dir=(" +
                  std::to_string(dir.x) + ", " + std::to_string(dir.y) + ", " +
                  std::to_string(dir.z) + ")";
        return res;
    }
};

}  // namespace

std::unique_ptr<IMoveSolver> makeLeastSquaresAxisSolver() {
    return std::make_unique<LeastSquaresAxisSolver>();
}

}  // namespace opendva
