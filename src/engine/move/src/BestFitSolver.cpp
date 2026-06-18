// A3 Best-Fit Move (README §4.5, "True Distance" sub-type). Finds the rigid
// transform (R, t) minimising sum_i |R*o_i + t - t_i|^2 over N >= 3 object/target
// point pairs (an over-determined point-to-point least-squares registration).
//
// method = True Distance: 6-DOF point-to-point fit with no per-pair direction
// projection and unit weights. Solved in closed form via the Kabsch algorithm:
//   1. centroids ocen, tcen; centre both clouds.
//   2. cross-covariance H = sum_i (o_i - ocen)(t_i - tcen)^T.
//   3. R = V * U^T from the SVD H = U S V^T, with a sign fix det(R) = +1 to
//      forbid reflections.
//   4. t = tcen - R*ocen.
//
// The 3x3 SVD is computed without any external library: H^T H is a symmetric 3x3
// matrix whose eigen-decomposition (cyclic Jacobi rotations) yields V and the
// singular values; U is then recovered as U = H V S^-1 (with a small-singular-
// value guard). This is original clean-room linear algebra.
#include <algorithm>
#include <cmath>

#include "MoveMath.h"
#include "opendva/IMoveSolver.h"

namespace opendva {
namespace {

using namespace movemath;

using Mat3 = double[3][3];

bool finiteVec(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

double stableDiffAbs(double a, double b) {
    const double s = std::max(std::fabs(a), std::fabs(b));
    if (s <= 0.0) return 0.0;
    return s * std::fabs(a / s - b / s);
}

double spreadScale(const MoveInputs& in, bool target, const Vec3& center) {
    double spread = 0.0;
    for (const auto& pr : in.pairs) {
        const Vec3 p = target ? pr.targetPoint : pr.objectPoint;
        spread = std::max(spread, stableDiffAbs(p.x, center.x));
        spread = std::max(spread, stableDiffAbs(p.y, center.y));
        spread = std::max(spread, stableDiffAbs(p.z, center.z));
    }
    return spread;
}

double centeredScaled(double value, double center, double spread) {
    const double s = std::max(std::fabs(value), std::fabs(center));
    if (s <= 0.0) return 0.0;
    return (value / s - center / s) * (s / spread);
}

Vec3 centeredScaledVec(const Vec3& value, const Vec3& center, double spread) {
    return {centeredScaled(value.x, center.x, spread),
            centeredScaled(value.y, center.y, spread),
            centeredScaled(value.z, center.z, spread)};
}

void matMul(const Mat3 a, const Mat3 b, Mat3 out) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k) s += a[i][k] * b[k][j];
            out[i][j] = s;
        }
}

void matT(const Mat3 a, Mat3 out) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) out[i][j] = a[j][i];
}

double det3(const Mat3 a) {
    return a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) -
           a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
           a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
}

Vec3 centroid(const MoveInputs& in, bool target) {
    const Vec3 origin = target ? in.pairs.front().targetPoint : in.pairs.front().objectPoint;
    double scaleValue = 0.0;
    for (const auto& pr : in.pairs) {
        const Vec3 p = target ? pr.targetPoint : pr.objectPoint;
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
        const Vec3 p = target ? pr.targetPoint : pr.objectPoint;
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

// Jacobi eigen-decomposition of a symmetric 3x3 matrix A.
// On return: eigenvalues in w[i], eigenvectors as the columns of V.
void jacobiEigenSym3(Mat3 A, double w[3], Mat3 V) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) V[i][j] = (i == j) ? 1.0 : 0.0;
    for (int sweep = 0; sweep < 50; ++sweep) {
        double off = std::fabs(A[0][1]) + std::fabs(A[0][2]) + std::fabs(A[1][2]);
        if (off < 1e-18) break;
        for (int p = 0; p < 2; ++p) {
            for (int q = p + 1; q < 3; ++q) {
                if (std::fabs(A[p][q]) < 1e-300) continue;
                const double app = A[p][p], aqq = A[q][q], apq = A[p][q];
                const double phi = 0.5 * std::atan2(2.0 * apq, aqq - app);
                const double c = std::cos(phi), s = std::sin(phi);
                // Apply rotation J^T A J.
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
                // Accumulate eigenvectors V = V * J.
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

class BestFitSolver final : public IMoveSolver {
public:
    MoveType type() const override { return MoveType::BestFit; }

    MoveResult solve(const MoveInputs& in, IRng& /*rng*/) override {
        MoveResult res{};
        const std::size_t n = in.pairs.size();
        if (n < 3) {
            res.log = "No Solution: Best-Fit (True Distance) requires >= 3 point pairs";
            return res;
        }
        for (const auto& pr : in.pairs) {
            if (!finiteVec(pr.objectPoint) || !finiteVec(pr.targetPoint)) {
                res.log = "No Solution: Best-Fit point pair is not finite";
                return res;
            }
        }
        const Vec3 ocen = centroid(in, /*target=*/false);
        const Vec3 tcen = centroid(in, /*target=*/true);

        const double objectSpread = spreadScale(in, /*target=*/false, ocen);
        const double targetSpread = spreadScale(in, /*target=*/true, tcen);
        if (objectSpread <= 0.0 || targetSpread <= 0.0 ||
            !std::isfinite(objectSpread) || !std::isfinite(targetSpread)) {
            res.log = "No Solution: Best-Fit point cloud is degenerate";
            return res;
        }

        // Cross-covariance H = sum (o-ocen)(t-tcen)^T. The centered vectors are
        // scaled by each cloud's spread so near-max finite coordinates do not
        // overflow while preserving the rotation encoded by H.
        Mat3 H = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
        double maxObjectSpread2 = 0.0;
        double maxTargetSpread2 = 0.0;
        for (const auto& pr : in.pairs) {
            const Vec3 o = centeredScaledVec(pr.objectPoint, ocen, objectSpread);
            const Vec3 t = centeredScaledVec(pr.targetPoint, tcen, targetSpread);
            maxObjectSpread2 = std::max(maxObjectSpread2, dot(o, o));
            maxTargetSpread2 = std::max(maxTargetSpread2, dot(t, t));
            const double ov[3] = {o.x, o.y, o.z};
            const double tv[3] = {t.x, t.y, t.z};
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) H[i][j] += ov[i] * tv[j];
        }
        if (maxObjectSpread2 < 1e-24 || maxTargetSpread2 < 1e-24) {
            res.log = "No Solution: Best-Fit point cloud is degenerate";
            return res;
        }

        // SVD via eigen-decomposition of H^T H = V S^2 V^T.
        Mat3 Ht, HtH;
        matT(H, Ht);
        matMul(Ht, H, HtH);
        double w[3];
        Mat3 V;
        jacobiEigenSym3(HtH, w, V);
        double sv[3];
        for (int i = 0; i < 3; ++i) sv[i] = std::sqrt(std::max(0.0, w[i]));

        // U = H V S^-1 (columns); guard tiny singular values to keep U orthonormal.
        Mat3 U = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
        for (int col = 0; col < 3; ++col) {
            const Vec3 vcol = {V[0][col], V[1][col], V[2][col]};
            // Hv.
            Vec3 hv = {H[0][0] * vcol.x + H[0][1] * vcol.y + H[0][2] * vcol.z,
                       H[1][0] * vcol.x + H[1][1] * vcol.y + H[1][2] * vcol.z,
                       H[2][0] * vcol.x + H[2][1] * vcol.y + H[2][2] * vcol.z};
            if (sv[col] > 1e-9) {
                hv = scale(hv, 1.0 / sv[col]);
            }
            U[0][col] = hv.x;
            U[1][col] = hv.y;
            U[2][col] = hv.z;
        }
        // Re-orthonormalise U columns (Gram-Schmidt) so degenerate/near-zero
        // singular directions still yield a proper rotation.
        orthonormalise(U);

        // R = V U^T.
        Mat3 Ut, R;
        matT(U, Ut);
        matMul(V, Ut, R);
        // Reflection fix: if det(R) < 0, flip the column of V tied to the smallest
        // singular value, then recompute R.
        if (det3(R) < 0.0) {
            int smallest = 0;
            for (int i = 1; i < 3; ++i)
                if (sv[i] < sv[smallest]) smallest = i;
            for (int i = 0; i < 3; ++i) V[i][smallest] = -V[i][smallest];
            matMul(V, Ut, R);
        }

        // t = tcen - R*ocen.
        const Vec3 Rocen = {R[0][0] * ocen.x + R[0][1] * ocen.y + R[0][2] * ocen.z,
                            R[1][0] * ocen.x + R[1][1] * ocen.y + R[1][2] * ocen.z,
                            R[2][0] * ocen.x + R[2][1] * ocen.y + R[2][2] * ocen.z};
        const Vec3 t = sub(tcen, Rocen);

        Mat34 out{};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) out.m[i][j] = R[i][j];
        out.m[0][3] = t.x;
        out.m[1][3] = t.y;
        out.m[2][3] = t.z;

        res.transform = out;
        if (!finiteMat34(res.transform)) {
            res.transform = Mat34{};
            res.log = "No Solution: Best-Fit transform is not finite";
            return res;
        }
        res.ok = true;
        return res;
    }

private:
    static void orthonormalise(Mat3 M) {
        // Modified Gram-Schmidt on the columns of M.
        Vec3 c0 = {M[0][0], M[1][0], M[2][0]};
        Vec3 c1 = {M[0][1], M[1][1], M[2][1]};
        Vec3 c2 = {M[0][2], M[1][2], M[2][2]};
        c0 = normalise(c0);
        c1 = sub(c1, scale(c0, dot(c0, c1)));
        c1 = normalise(c1);
        c2 = sub(c2, scale(c0, dot(c0, c2)));
        c2 = sub(c2, scale(c1, dot(c1, c2)));
        c2 = normalise(c2);
        M[0][0] = c0.x; M[1][0] = c0.y; M[2][0] = c0.z;
        M[0][1] = c1.x; M[1][1] = c1.y; M[2][1] = c1.z;
        M[0][2] = c2.x; M[1][2] = c2.y; M[2][2] = c2.z;
    }
};

}  // namespace

std::unique_ptr<IMoveSolver> makeBestFitSolver() {
    return std::make_unique<BestFitSolver>();
}

}  // namespace opendva
