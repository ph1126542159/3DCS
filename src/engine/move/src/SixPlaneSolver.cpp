// A3 Six-Plane general solver (README §4.2). Solves a rigid transform (R,t)
// so that each object point lands on its corresponding target plane
// (point + normal). Other move types are degenerate/ordered cases.
//
// Reference implementation: small-rotation linearisation, iterated to
// convergence. 6 point-to-plane constraints -> 6x6 linear system per step.
#include <array>
#include <cmath>

#include "opendva/IMoveSolver.h"

namespace opendva {
namespace {

Vec3 normalise(Vec3 v) {
    const double len = std::hypot(v.x, v.y, v.z);
    if (!std::isfinite(len) || len < 1e-12) return {0, 0, 1};
    return {v.x / len, v.y / len, v.z / len};
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

bool finiteVec(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// Apply small twist (rx,ry,rz translation + wx,wy,wz rotation about origin).
Vec3 applyTwist(const Vec3& p, const std::array<double, 6>& t) {
    // delta = translation + omega x p
    return {p.x + t[0] + (t[4] * p.z - t[5] * p.y),
            p.y + t[1] + (t[5] * p.x - t[3] * p.z),
            p.z + t[2] + (t[3] * p.y - t[4] * p.x)};
}

// Solve 6x6 A x = b via Gaussian elimination with partial pivoting.
bool solve6(std::array<std::array<double, 6>, 6> A, std::array<double, 6> b,
            std::array<double, 6>& x) {
    for (int col = 0; col < 6; ++col) {
        int piv = col;
        double best = std::fabs(A[col][col]);
        for (int r = col + 1; r < 6; ++r) {
            if (std::fabs(A[r][col]) > best) { best = std::fabs(A[r][col]); piv = r; }
        }
        if (best < 1e-12) return false;  // singular -> ill-defined move
        std::swap(A[col], A[piv]);
        std::swap(b[col], b[piv]);
        for (int r = col + 1; r < 6; ++r) {
            const double f = A[r][col] / A[col][col];
            for (int c = col; c < 6; ++c) A[r][c] -= f * A[col][c];
            b[r] -= f * b[col];
        }
    }
    for (int row = 5; row >= 0; --row) {
        double s = b[row];
        for (int c = row + 1; c < 6; ++c) s -= A[row][c] * x[c];
        x[row] = s / A[row][row];
    }
    return true;
}

Mat34 composeTwist(const Mat34& cur, const std::array<double, 6>& t) {
    // Build incremental rotation (small-angle) and translation, premultiply.
    const double wx = t[3], wy = t[4], wz = t[5];
    const std::array<std::array<double, 3>, 3> dR = {{
        {{1, -wz, wy}}, {{wz, 1, -wx}}, {{-wy, wx, 1}}}};
    Mat34 out{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            double s = 0;
            for (int k = 0; k < 3; ++k) s += dR[i][k] * cur.m[k][j];
            out.m[i][j] = s;
        }
        out.m[i][3] = dR[i][0] * cur.m[0][3] + dR[i][1] * cur.m[1][3] + dR[i][2] * cur.m[2][3] +
                      t[i];
    }
    return out;
}

bool finiteMat34(const Mat34& T) {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (!std::isfinite(T.m[i][j])) return false;
        }
    }
    return true;
}

Vec3 transform(const Mat34& T, const Vec3& p) {
    return {T.m[0][0] * p.x + T.m[0][1] * p.y + T.m[0][2] * p.z + T.m[0][3],
            T.m[1][0] * p.x + T.m[1][1] * p.y + T.m[1][2] * p.z + T.m[1][3],
            T.m[2][0] * p.x + T.m[2][1] * p.y + T.m[2][2] * p.z + T.m[2][3]};
}

}  // namespace

// Shared by the factory for SixPlane and all degenerate point-to-plane moves.
MoveResult solveSixPlane(const MoveInputs& in) {
    MoveResult res{};
    if (in.pairs.size() < 3) {
        res.log = "No Solution: insufficient point pairs";
        return res;
    }
    for (const auto& pr : in.pairs) {
        if (!finiteVec(pr.objectPoint) || !finiteVec(pr.targetPoint)) {
            res.log = "No Solution: Six-Plane constraint point is not finite";
            return res;
        }
        const double normalLen =
            std::hypot(pr.direction.ijk.x, pr.direction.ijk.y, pr.direction.ijk.z);
        if (!std::isfinite(normalLen)) {
            res.log = "No Solution: Six-Plane constraint normal is not finite";
            return res;
        }
        if (normalLen < 1e-12) {
            res.log = "No Solution: Six-Plane constraint normal is zero";
            return res;
        }
    }
    Mat34 T{};  // identity
    const int maxIter = in.maxIterations > 0 ? in.maxIterations : 50;
    for (int iter = 0; iter < maxIter; ++iter) {
        std::array<std::array<double, 6>, 6> AtA{};
        std::array<double, 6> Atb{};
        double maxResidual = 0.0;
        for (const auto& pr : in.pairs) {
            const Vec3 n = normalise(pr.direction.ijk);
            const Vec3 op = transform(T, pr.objectPoint);
            const double residual = dot(n, {pr.targetPoint.x - op.x, pr.targetPoint.y - op.y,
                                            pr.targetPoint.z - op.z});
            maxResidual = std::max(maxResidual, std::fabs(residual));
            // Jacobian row: d(residual)/d(twist) = [n , (op x n)]
            const Vec3 rot = cross(op, n);
            const std::array<double, 6> J = {n.x, n.y, n.z, rot.x, rot.y, rot.z};
            for (int i = 0; i < 6; ++i) {
                Atb[i] += J[i] * residual;
                for (int j = 0; j < 6; ++j) AtA[i][j] += J[i] * J[j];
            }
        }
        if (maxResidual < in.searchAccuracy) break;
        // Tikhonov (damped least squares): add a small lambda to the diagonal so
        // under-determined DOFs (e.g. rotation in a translation-only system)
        // resolve to zero motion instead of producing a singular system.
        for (int d = 0; d < 6; ++d) AtA[d][d] += 1e-9;
        std::array<double, 6> dt{};
        if (!solve6(AtA, Atb, dt)) {
            res.log = "No Solution: singular constraint system (under/over-constrained)";
            return res;  // failure -> caller reverts to pre-move pose
        }
        T = composeTwist(T, dt);
        if (!finiteMat34(T)) {
            res.transform = Mat34{};
            res.log = "No Solution: Six-Plane transform is not finite";
            return res;
        }
    }
    if (!finiteMat34(T)) {
        res.transform = Mat34{};
        res.log = "No Solution: Six-Plane transform is not finite";
        return res;
    }
    res.transform = T;
    res.ok = true;
    return res;
}

}  // namespace opendva
