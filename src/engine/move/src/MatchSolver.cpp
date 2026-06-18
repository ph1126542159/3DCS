// A3 Match Move (README §4.10). Constrains a single rotational DOF: the object
// point O3 is rotated about the O1-O2 axis until its distance to a target
// geometry equals a specified target distance. Solved as a 1-D root-find on the
// rotation angle theta via bisection.
//
// Inputs (per README):
//   - O1/O2 : pairs[0].objectPoint / pairs[1].objectPoint  (rotation axis)
//   - O3    : pairs[2].objectPoint                         (moving point)
//   - T1/T2/T3 : pairs[0..2].targetPoint                   (target geometry)
//   - T4/T5    : pairs[3].targetPoint / pairs[4].targetPoint (target distance)
//
// Target geometry type is inferred from T1/T2/T3 coincidence (README §4.10):
//   point (T1=T2=T3) / line (T2=T3, else along T1-T2-T3 collinear) /
//   plane (all distinct). Target distance = |T4 - T5|.
//
// The distance function d(theta) = dist(rotate(O3, theta), target_geometry) is
// continuous; we sample it over [0, 2pi) to bracket a sign change of
// f(theta) = d(theta) - target_dist, then bisect. If no bracket is found we
// return the angle that minimises |f| (closest achievable), flagged ok.
//
// Original clean-room implementation.
#include <algorithm>
#include <cmath>

#include "MoveMath.h"
#include "opendva/IMoveSolver.h"

namespace opendva {
namespace {

using namespace movemath;

enum class TargetKind { Point, Line, Plane };

bool finiteVec(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// Distance from p to the target geometry defined by t1/t2/t3.
double distanceToTarget(const Vec3& p, TargetKind kind, const Vec3& t1, const Vec3& t2,
                        const Vec3& t3) {
    switch (kind) {
        case TargetKind::Point:
            return norm(sub(p, t1));
        case TargetKind::Line: {
            // Distance from p to the line through the first non-coincident target
            // pair. T1==T2 with T3 distinct is still a valid line definition.
            Vec3 lineVec = sub(t2, t1);
            if (norm(lineVec) < 1e-12) lineVec = sub(t3, t1);
            if (norm(lineVec) < 1e-12) lineVec = sub(t3, t2);
            const Vec3 dir = normalise(lineVec);
            const Vec3 w = sub(p, t1);
            const Vec3 perp = sub(w, scale(dir, dot(w, dir)));
            return norm(perp);
        }
        case TargetKind::Plane: {
            // Signed distance to the plane through t1 with normal (t2-t1)x(t3-t1).
            const Vec3 n = normalise(cross(sub(t2, t1), sub(t3, t1)));
            return std::fabs(dot(sub(p, t1), n));
        }
    }
    return 0.0;
}

class MatchSolver final : public IMoveSolver {
public:
    MoveType type() const override { return MoveType::Match; }

    MoveResult solve(const MoveInputs& in, IRng& /*rng*/) override {
        MoveResult res{};
        if (in.pairs.size() < 5) {
            res.log = "No Solution: Match requires O1/O2/O3 + T1..T5 (5 pairs)";
            return res;
        }
        const Vec3 o1 = in.pairs[0].objectPoint;
        const Vec3 o2 = in.pairs[1].objectPoint;
        const Vec3 o3 = in.pairs[2].objectPoint;
        const Vec3 t1 = in.pairs[0].targetPoint;
        const Vec3 t2 = in.pairs[1].targetPoint;
        const Vec3 t3 = in.pairs[2].targetPoint;
        const Vec3 t4 = in.pairs[3].targetPoint;
        const Vec3 t5 = in.pairs[4].targetPoint;

        if (!finiteVec(o1) || !finiteVec(o2) || !finiteVec(o3)) {
            res.log = "No Solution: Match object points are not finite";
            return res;
        }

        if (!finiteVec(t1) || !finiteVec(t2) || !finiteVec(t3)) {
            res.log = "No Solution: Match target geometry is not finite";
            return res;
        }

        const Vec3 axisVec = sub(o2, o1);
        const double axisLen = norm(axisVec);
        if (!std::isfinite(axisLen)) {
            res.log = "No Solution: Match rotation axis (O1-O2) is not finite";
            return res;
        }
        if (axisLen < 1e-12) {
            res.log = "No Solution: Match rotation axis (O1-O2) is degenerate";
            return res;
        }
        const Vec3 axis = normalise(axisVec);
        const double targetDist = norm(sub(t4, t5));
        if (!std::isfinite(targetDist)) {
            res.log = "No Solution: Match target distance is not finite";
            return res;
        }

        // Infer target geometry kind from T1/T2/T3 coincidence.
        const bool t12 = norm(sub(t1, t2)) < 1e-9;
        const bool t23 = norm(sub(t2, t3)) < 1e-9;
        TargetKind kind;
        if (t12 && t23) {
            kind = TargetKind::Point;
        } else if (t23 || norm(cross(sub(t2, t1), sub(t3, t1))) < 1e-9) {
            kind = TargetKind::Line;  // collinear (incl. T2=T3): a line.
        } else {
            kind = TargetKind::Plane;
        }

        auto f = [&](double theta) {
            const Vec3 p = rotateAboutPoint(o3, o1, axis, theta);
            return distanceToTarget(p, kind, t1, t2, t3) - targetDist;
        };

        // Sample to bracket a sign change of f over [0, 2pi).
        const int kSamples = 360;
        double prevTheta = 0.0;
        double prevF = f(prevTheta);
        double bestTheta = prevTheta;
        double bestAbs = std::fabs(prevF);
        bool bracketed = false;
        double loT = 0.0, hiT = 0.0;
        for (int i = 1; i <= kSamples; ++i) {
            const double theta = 2.0 * kPi * static_cast<double>(i) / kSamples;
            const double fv = f(theta);
            if (std::fabs(fv) < bestAbs) {
                bestAbs = std::fabs(fv);
                bestTheta = theta;
            }
            if (prevF == 0.0) {  // exact hit at a sample
                loT = hiT = prevTheta;
                bracketed = true;
                break;
            }
            if ((prevF < 0.0) != (fv < 0.0)) {
                loT = prevTheta;
                hiT = theta;
                bracketed = true;
                break;
            }
            prevTheta = theta;
            prevF = fv;
        }

        double solTheta = bestTheta;
        if (bracketed) {
            // Bisection on [loT, hiT].
            double a = loT, b = hiT;
            double fa = f(a);
            const double tol = in.searchAccuracy > 0 ? in.searchAccuracy : 1e-6;
            const int maxIt = in.maxIterations > 0 ? in.maxIterations : 100;
            for (int it = 0; it < maxIt; ++it) {
                const double mid = 0.5 * (a + b);
                const double fm = f(mid);
                if (std::fabs(fm) < tol || (b - a) < 1e-12) {
                    solTheta = mid;
                    break;
                }
                if ((fa < 0.0) != (fm < 0.0)) {
                    b = mid;
                } else {
                    a = mid;
                    fa = fm;
                }
                solTheta = mid;
            }
        }
        // If not bracketed, solTheta is the angle minimising |d(theta) - target|.

        res.transform = rotationMat34(o1, axis, solTheta);
        if (!finiteMat34(res.transform)) {
            res.transform = Mat34{};
            res.log = "No Solution: Match transform is not finite";
            return res;
        }
        res.ok = true;
        return res;
    }
};

}  // namespace

std::unique_ptr<IMoveSolver> makeMatchSolver() {
    return std::make_unique<MatchSolver>();
}

}  // namespace opendva
