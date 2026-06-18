// Header-only linear-algebra helpers for the A1 geometry kernel.
// Dependency-free, pure self-written math (README §14.1): a symmetric 3x3
// Jacobi eigen-decomposition used by the least-squares plane/axis fits.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace opendva {
namespace linalg {

// Symmetric 3x3 matrix, row-major. Only the symmetric part is interpreted.
struct Sym3 {
    std::array<std::array<double, 3>, 3> m{{{{0, 0, 0}}, {{0, 0, 0}}, {{0, 0, 0}}}};

    double& at(int r, int c) { return m[r][c]; }
    double at(int r, int c) const { return m[r][c]; }
};

// Result of a 3x3 symmetric eigen-decomposition.
// eigenvalues[k] pairs with the column vector eigenvectors[*][k].
struct Eigen3 {
    std::array<double, 3> values{{0, 0, 0}};
    std::array<std::array<double, 3>, 3> vectors{
        {{{1, 0, 0}}, {{0, 1, 0}}, {{0, 0, 1}}}};
};

// Classic cyclic Jacobi rotation for a symmetric 3x3 matrix. Diagonalises a
// copy of `a` into eigenvalues, accumulating the orthonormal eigenvectors in
// the columns of `vectors`. Converges quadratically; 50 sweeps is far more
// than enough for the 3x3 symmetric case.
inline Eigen3 jacobiEigen(const Sym3& a) {
    // Working copy of the (symmetrised) matrix.
    std::array<std::array<double, 3>, 3> s{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            s[i][j] = 0.5 * (a.at(i, j) + a.at(j, i));
        }
    }

    // Eigenvector accumulator starts as the identity.
    std::array<std::array<double, 3>, 3> v{
        {{{1, 0, 0}}, {{0, 1, 0}}, {{0, 0, 1}}}};

    constexpr int kMaxSweeps = 50;
    constexpr double kEps = 1e-18;
    for (int sweep = 0; sweep < kMaxSweeps; ++sweep) {
        // Sum of squared off-diagonal magnitudes; stop once negligible.
        const double off = s[0][1] * s[0][1] + s[0][2] * s[0][2] + s[1][2] * s[1][2];
        if (off < kEps) break;

        // Rotate away each upper off-diagonal element (p, q).
        for (int p = 0; p < 2; ++p) {
            for (int q = p + 1; q < 3; ++q) {
                const double apq = s[p][q];
                if (std::fabs(apq) < kEps) continue;

                // Jacobi rotation angle that zeroes s[p][q].
                const double app = s[p][p];
                const double aqq = s[q][q];
                const double phi = 0.5 * (aqq - app) / apq;
                const double t =
                    (phi >= 0 ? 1.0 : -1.0) /
                    (std::fabs(phi) + std::sqrt(phi * phi + 1.0));
                const double c = 1.0 / std::sqrt(t * t + 1.0);
                const double sn = t * c;

                // Apply the symmetric similarity rotation S = J^T S J.
                for (int k = 0; k < 3; ++k) {
                    const double skp = s[k][p];
                    const double skq = s[k][q];
                    s[k][p] = c * skp - sn * skq;
                    s[k][q] = sn * skp + c * skq;
                }
                for (int k = 0; k < 3; ++k) {
                    const double spk = s[p][k];
                    const double sqk = s[q][k];
                    s[p][k] = c * spk - sn * sqk;
                    s[q][k] = sn * spk + c * sqk;
                }
                // Accumulate the rotation into the eigenvector columns.
                for (int k = 0; k < 3; ++k) {
                    const double vkp = v[k][p];
                    const double vkq = v[k][q];
                    v[k][p] = c * vkp - sn * vkq;
                    v[k][q] = sn * vkp + c * vkq;
                }
            }
        }
    }

    Eigen3 out;
    out.values = {s[0][0], s[1][1], s[2][2]};
    out.vectors = v;
    return out;
}

// Index of the smallest / largest eigenvalue.
inline int minEigenIndex(const Eigen3& e) {
    int idx = 0;
    for (int k = 1; k < 3; ++k) {
        if (e.values[k] < e.values[idx]) idx = k;
    }
    return idx;
}

inline int maxEigenIndex(const Eigen3& e) {
    int idx = 0;
    for (int k = 1; k < 3; ++k) {
        if (e.values[k] > e.values[idx]) idx = k;
    }
    return idx;
}

// Extract eigenvector column `k` as a plain length-3 array.
inline std::array<double, 3> eigenColumn(const Eigen3& e, int k) {
    return {e.vectors[0][k], e.vectors[1][k], e.vectors[2][k]};
}

}  // namespace linalg
}  // namespace opendva
