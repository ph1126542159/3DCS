// A1 geometry-kernel numerics tests (README §3, §14.1). Exercises the Jacobi
// eigen-solver and the least-squares fits / meshing directly.
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "dva_test.h"
#include "opendva/geometry/LinAlg.h"
#include "opendva/geometry/StubGeometryKernel.h"

using namespace opendva;

namespace {

double len(const Vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

// |dot| ~ 1 means the two unit vectors are (anti-)parallel.
double absDot(const Vec3& a, const Vec3& b) {
    return std::fabs(a.x * b.x + a.y * b.y + a.z * b.z);
}

}  // namespace

// ---- Jacobi: diagonal matrix returns its diagonal as eigenvalues ----
TEST("jacobi_diagonal_eigenvalues") {
    linalg::Sym3 a;
    a.at(0, 0) = 3.0;
    a.at(1, 1) = 7.0;
    a.at(2, 2) = -2.0;
    const linalg::Eigen3 e = linalg::jacobiEigen(a);
    // Smallest / largest eigenvalues must be -2 and 7 regardless of order.
    dvatest::checkNear(e.values[linalg::minEigenIndex(e)], -2.0, 1e-9, "min eig");
    dvatest::checkNear(e.values[linalg::maxEigenIndex(e)], 7.0, 1e-9, "max eig");
    // The eigenvector for eigenvalue 7 must align with the y axis.
    const auto col = linalg::eigenColumn(e, linalg::maxEigenIndex(e));
    dvatest::checkNear(std::fabs(col[1]), 1.0, 1e-9, "max eigvec is y");
    dvatest::checkNear(std::fabs(col[0]), 0.0, 1e-9, "max eigvec x=0");
    dvatest::checkNear(std::fabs(col[2]), 0.0, 1e-9, "max eigvec z=0");
}

// ---- Jacobi: known 2x2-in-3x3 symmetric matrix, analytic eigenvalues ----
TEST("jacobi_known_symmetric") {
    // [[2,1,0],[1,2,0],[0,0,5]] -> eigenvalues {1, 3, 5}.
    linalg::Sym3 a;
    a.at(0, 0) = 2.0;
    a.at(1, 1) = 2.0;
    a.at(0, 1) = a.at(1, 0) = 1.0;
    a.at(2, 2) = 5.0;
    const linalg::Eigen3 e = linalg::jacobiEigen(a);
    std::vector<double> vals = {e.values[0], e.values[1], e.values[2]};
    std::sort(vals.begin(), vals.end());
    dvatest::checkNear(vals[0], 1.0, 1e-9, "eig 1");
    dvatest::checkNear(vals[1], 3.0, 1e-9, "eig 3");
    dvatest::checkNear(vals[2], 5.0, 1e-9, "eig 5");
    // Eigenvector for eigenvalue 1 is (1,-1,0)/sqrt2.
    const auto col = linalg::eigenColumn(e, linalg::minEigenIndex(e));
    dvatest::checkNear(std::fabs(col[0]), std::sqrt(0.5), 1e-9, "min vec x");
    dvatest::checkNear(std::fabs(col[1]), std::sqrt(0.5), 1e-9, "min vec y");
    dvatest::checkNear(std::fabs(col[2]), 0.0, 1e-9, "min vec z");
}

// ---- fitLeastSquaresPlane: noisy XY-plane points -> normal ~ (0,0,1) ----
TEST("lsq_plane_xy") {
    StubGeometryKernel k;
    std::vector<Vec3> pts;
    // 5x5 grid on z~0 with tiny deterministic noise.
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            const double noise = 1e-4 * ((i * 7 + j * 3) % 5 - 2);
            pts.push_back({static_cast<double>(i), static_cast<double>(j), noise});
        }
    }
    const Plane p = k.fitLeastSquaresPlane(pts);
    dvatest::checkNear(absDot(p.normal, {0, 0, 1}), 1.0, 1e-3, "normal ~ z");
    dvatest::checkNear(len(p.normal), 1.0, 1e-9, "normal is unit");
    // Plane passes through the centroid (~ (2, 2, ~0)).
    dvatest::checkNear(p.point.x, 2.0, 1e-9, "centroid x");
    dvatest::checkNear(p.point.y, 2.0, 1e-9, "centroid y");
    dvatest::checkNear(p.point.z, 0.0, 1e-3, "centroid z");
}

// ---- fitLeastSquaresAxis: points along X -> LSA ~ (1,0,0); LSS ~ (0,0,1) ----
TEST("lsq_axis_lsa_lss") {
    StubGeometryKernel k;
    std::vector<Vec3> pts;
    for (int i = 0; i < 10; ++i) {
        const double noise = 1e-4 * ((i % 3) - 1);
        pts.push_back({static_cast<double>(i), 5.0 + noise, -3.0 + noise});
    }
    const Axis lsa = k.fitLeastSquaresAxis(pts, /*shiftOnly=*/false);
    dvatest::checkNear(absDot(lsa.direction, {1, 0, 0}), 1.0, 1e-3, "LSA ~ x");
    dvatest::checkNear(len(lsa.direction), 1.0, 1e-9, "LSA unit");
    // Axis passes through the centroid.
    dvatest::checkNear(lsa.point.x, 4.5, 1e-9, "LSA center x");
    dvatest::checkNear(lsa.point.y, 5.0, 1e-3, "LSA center y");

    const Axis lss = k.fitLeastSquaresAxis(pts, /*shiftOnly=*/true);
    dvatest::checkNear(lss.direction.x, 0.0, 1e-12, "LSS x=0");
    dvatest::checkNear(lss.direction.y, 0.0, 1e-12, "LSS y=0");
    dvatest::checkNear(lss.direction.z, 1.0, 1e-12, "LSS z=1");
    dvatest::checkNear(lss.point.x, 4.5, 1e-9, "LSS center x");
}

TEST("lsq_axis_lsa_handles_huge_finite_points") {
    StubGeometryKernel k;
    const Axis axis = k.fitLeastSquaresAxis({{0.0, 0.0, 0.0}, {1e308, 1e308, 0.0}},
                                            /*shiftOnly=*/false);
    const double invSqrt2 = std::sqrt(0.5);
    dvatest::checkNear(absDot(axis.direction, {invSqrt2, invSqrt2, 0.0}), 1.0, 1e-12,
                       "huge finite LSA direction");
    dvatest::checkNear(len(axis.direction), 1.0, 1e-12, "huge finite LSA unit");
    dvatest::check(std::isfinite(axis.point.x) && std::isfinite(axis.point.y) &&
                       std::isfinite(axis.point.z),
                   "huge finite LSA center is finite");
}

TEST("lsq_axis_uses_overflow_stable_centroid") {
    StubGeometryKernel k;
    const Axis axis = k.fitLeastSquaresAxis({{9e307, 0.0, 0.0}, {9e307, 2.0, 0.0}},
                                            /*shiftOnly=*/false);
    dvatest::checkNear(axis.point.x, 9e307, 1e292, "huge same-sign centroid x");
    dvatest::checkNear(axis.point.y, 1.0, 1e-12, "huge same-sign centroid y");
    dvatest::checkNear(axis.point.z, 0.0, 1e-12, "huge same-sign centroid z");
    dvatest::check(std::isfinite(axis.point.x) && std::isfinite(axis.point.y) &&
                       std::isfinite(axis.point.z),
                   "huge same-sign centroid is finite");
    dvatest::checkNear(absDot(axis.direction, {0.0, 1.0, 0.0}), 1.0, 1e-12,
                       "huge same-sign LSA direction");
}

TEST("lsq_axis_uses_overflow_stable_opposite_sign_centroid") {
    StubGeometryKernel k;
    const Axis axis = k.fitLeastSquaresAxis({{-9e307, 0.0, 0.0}, {9e307, 2.0, 0.0}},
                                            /*shiftOnly=*/false);
    dvatest::checkNear(axis.point.x, 0.0, 1e292, "huge opposite-sign centroid x");
    dvatest::checkNear(axis.point.y, 1.0, 1e-12, "huge opposite-sign centroid y");
    dvatest::checkNear(axis.point.z, 0.0, 1e-12, "huge opposite-sign centroid z");
    dvatest::check(std::isfinite(axis.point.x) && std::isfinite(axis.point.y) &&
                       std::isfinite(axis.point.z),
                   "huge opposite-sign centroid is finite");
    dvatest::checkNear(absDot(axis.direction, {1.0, 0.0, 0.0}), 1.0, 1e-12,
                       "huge opposite-sign LSA direction");
}

TEST("lsq_axis_lsa_handles_opposite_sign_scatter_offsets") {
    StubGeometryKernel k;
    const Axis axis = k.fitLeastSquaresAxis(
        {{-1.5130608402611687e308, -5.460116727277636e307, 1.3230839268770068e308},
         {1.373084463388659e308, -5.331488151103257e307, -1.328840407403186e308},
         {-1.6542988517181947e308, 1.264193389378488e308, -1.4959136644843965e308}},
        /*shiftOnly=*/false);
    dvatest::check(std::isfinite(axis.point.x) && std::isfinite(axis.point.y) &&
                       std::isfinite(axis.point.z),
                   "opposite-sign scatter LSA center is finite");
    dvatest::checkNear(absDot(axis.direction, {-0.7921152024162386, 0.07115789745722534,
                                               0.606209583997604}),
                       1.0, 1e-12,
                       "opposite-sign scatter LSA direction");
}

// ---- maxMinCylinder: points on a known-radius cylinder about Z ----
TEST("maxmin_cylinder") {
    StubGeometryKernel k;
    std::vector<Vec3> pts;
    const double r = 4.0;
    constexpr double kPi = 3.141592653589793;
    for (int s = 0; s < 3; ++s) {       // 3 cross-sections along z
        for (int a = 0; a < 12; ++a) {  // 12 points around
            const double ang = 2.0 * kPi * a / 12.0;
            pts.push_back({r * std::cos(ang), r * std::sin(ang), static_cast<double>(s)});
        }
    }
    Axis axis{{0, 0, 0}, {0, 0, 1}};
    const auto mm = k.maxMinCylinder(pts, axis);
    dvatest::checkNear(mm.first, r, 1e-9, "maxR");
    dvatest::checkNear(mm.second, r, 1e-9, "minR");

    pts.push_back({std::numeric_limits<double>::infinity(), 0.0, 0.0});
    const auto finiteOnly = k.maxMinCylinder(pts, axis);
    dvatest::checkNear(finiteOnly.first, r, 1e-9, "maxR ignores nonfinite point");
    dvatest::checkNear(finiteOnly.second, r, 1e-9, "minR ignores nonfinite point");

    const auto nonfiniteOnly =
        k.maxMinCylinder({{std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0}}, axis);
    dvatest::checkNear(nonfiniteOnly.first, 0.0, 1e-12, "nonfinite-only maxR is zero");
    dvatest::checkNear(nonfiniteOnly.second, 0.0, 1e-12, "nonfinite-only minR is zero");

    const auto hugeFinite = k.maxMinCylinder({{1e308, 0.0, 0.0}}, axis);
    dvatest::checkNear(hugeFinite.first, 1e308, 1e292, "huge finite maxR");
    dvatest::checkNear(hugeFinite.second, 1e308, 1e292, "huge finite minR");

    const double invSqrt3 = 1.0 / std::sqrt(3.0);
    const Axis diagonal{{0, 0, 0}, {invSqrt3, invSqrt3, invSqrt3}};
    const double expectedDiagonalRadius = std::sqrt(1.0 / 150.0) * 1e308;
    const auto hugeDiagonal = k.maxMinCylinder({{1.1e308, 1.1e308, 1.0e308}}, diagonal);
    dvatest::checkNear(hugeDiagonal.first, expectedDiagonalRadius, 1e292,
                       "huge diagonal maxR");
    dvatest::checkNear(hugeDiagonal.second, expectedDiagonalRadius, 1e292,
                       "huge diagonal minR");

    const Axis farAxis{{-9e307, 0, 0}, {1, 0, 0}};
    const auto oppositeSignOffset = k.maxMinCylinder({{9e307, 1.0, 0.0}}, farAxis);
    dvatest::checkNear(oppositeSignOffset.first, 1.0, 1e-12,
                       "opposite-sign axis offset maxR");
    dvatest::checkNear(oppositeSignOffset.second, 1.0, 1e-12,
                       "opposite-sign axis offset minR");

    // Empty set -> both radii 0.
    const auto empty = k.maxMinCylinder({}, axis);
    dvatest::checkNear(empty.first, 0.0, 1e-12, "empty maxR");
    dvatest::checkNear(empty.second, 0.0, 1e-12, "empty minR");
}

// ---- meshFeature/meshNodes: plane density=4 -> 16 nodes inside the region ----
TEST("mesh_plane_nodes") {
    StubGeometryKernel k;
    // A unit square patch on z=0.
    const FeatureId f = k.createPlane({{0, 0, 0}, {2, 0, 0}, {2, 2, 0}, {0, 2, 0}});
    MeshParams mp;
    mp.meshDensity = 4;
    const MeshHandle h = k.meshFeature(f, mp);
    dvatest::check(h.meshNodeNum == 16, "16 mesh nodes");
    const std::vector<Vec3> nodes = k.meshNodes(h);
    dvatest::check(nodes.size() == 16, "meshNodes returns 16");
    for (const auto& n : nodes) {
        dvatest::check(n.x >= -1e-9 && n.x <= 2.0 + 1e-9, "node x in [0,2]");
        dvatest::check(n.y >= -1e-9 && n.y <= 2.0 + 1e-9, "node y in [0,2]");
        dvatest::checkNear(n.z, 0.0, 1e-9, "node on z=0 plane");
    }
}

TEST("mesh_plane_ignores_nonfinite_defining_points") {
    StubGeometryKernel k;
    const FeatureId f = k.createPlane({{0, 0, 0},
                                       {2, 0, 0},
                                       {2, 2, 0},
                                       {0, 2, 0},
                                       {std::numeric_limits<double>::infinity(), 0, 0}});
    MeshParams mp;
    mp.meshDensity = 3;
    const std::vector<Vec3> nodes = k.meshNodes(k.meshFeature(f, mp));
    dvatest::check(nodes.size() == 9, "finite plane mesh node count");
    for (const auto& n : nodes) {
        dvatest::check(std::isfinite(n.x) && std::isfinite(n.y) && std::isfinite(n.z),
                       "finite plane mesh node");
        dvatest::check(n.x >= -1e-9 && n.x <= 2.0 + 1e-9,
                       "finite plane mesh x range");
        dvatest::check(n.y >= -1e-9 && n.y <= 2.0 + 1e-9,
                       "finite plane mesh y range");
        dvatest::checkNear(n.z, 0.0, 1e-9, "finite plane mesh z");
    }
}

TEST("mesh_plane_handles_opposite_sign_projection_offsets") {
    StubGeometryKernel k;
    const FeatureId f = k.createPlane({{-1.7e308, 0.0, 0.0},
                                       {-1.7e308, 1.0, 0.0},
                                       {1.7e308, 0.0, 0.0}});
    MeshParams mp;
    mp.meshDensity = 3;
    const std::vector<Vec3> nodes = k.meshNodes(k.meshFeature(f, mp));
    dvatest::check(nodes.size() == 9, "opposite-sign plane mesh node count");
    double maxX = -std::numeric_limits<double>::infinity();
    for (const auto& n : nodes) {
        dvatest::check(std::isfinite(n.x) && std::isfinite(n.y) && std::isfinite(n.z),
                       "opposite-sign plane mesh node finite");
        maxX = std::max(maxX, n.x);
    }
    dvatest::checkNear(maxX, 1.7e308, 2e292, "opposite-sign plane max x");
}

// ---- meshFeature/meshNodes: cylinder density=3 -> 3 axial rings on radius ----
TEST("mesh_cylinder_nodes") {
    StubGeometryKernel k;
    const FeatureId f = k.createCylinder({{0, 0, 0}, {0, 0, 4}}, 4.0, HoleType::Hole);
    MeshParams mp;
    mp.meshDensity = 3;
    const MeshHandle h = k.meshFeature(f, mp);
    dvatest::check(h.meshNodeNum == 9, "9 cylinder mesh nodes");
    const std::vector<Vec3> nodes = k.meshNodes(h);
    dvatest::check(nodes.size() == 9, "meshNodes returns 9 cylinder nodes");
    bool sawMinZ = false;
    bool sawMaxZ = false;
    for (const auto& n : nodes) {
        dvatest::checkNear(std::sqrt(n.x * n.x + n.y * n.y), 2.0, 1e-9,
                           "cylinder node radius");
        dvatest::check(n.z >= -1e-9 && n.z <= 4.0 + 1e-9, "cylinder z in range");
        if (std::fabs(n.z) <= 1e-9) sawMinZ = true;
        if (std::fabs(n.z - 4.0) <= 1e-9) sawMaxZ = true;
    }
    dvatest::check(sawMinZ && sawMaxZ, "cylinder axial endpoints present");
}

TEST("mesh_cylinder_ignores_nonfinite_defining_points") {
    StubGeometryKernel k;
    const FeatureId f = k.createCylinder({{0, 0, 0},
                                          {0, 0, 4},
                                          {0, 0, std::numeric_limits<double>::infinity()}},
                                         4.0, HoleType::Hole);
    MeshParams mp;
    mp.meshDensity = 3;
    const std::vector<Vec3> nodes = k.meshNodes(k.meshFeature(f, mp));
    dvatest::check(nodes.size() == 9, "finite cylinder mesh node count");
    bool sawMinZ = false;
    bool sawMaxZ = false;
    for (const auto& n : nodes) {
        dvatest::check(std::isfinite(n.x) && std::isfinite(n.y) && std::isfinite(n.z),
                       "finite cylinder mesh node");
        dvatest::check(n.z >= -1e-9 && n.z <= 4.0 + 1e-9,
                       "finite cylinder mesh z range");
        if (std::fabs(n.z) <= 1e-9) sawMinZ = true;
        if (std::fabs(n.z - 4.0) <= 1e-9) sawMaxZ = true;
    }
    dvatest::check(sawMinZ && sawMaxZ, "finite cylinder axial endpoints present");
}

TEST("mesh_cylinder_handles_near_max_diagonal_axis") {
    StubGeometryKernel k;
    const FeatureId f = k.createCylinder({{-1.1e308, -1.1e308, -1.1e308},
                                          {1.1e308, 1.1e308, 1.1e308}},
                                         2.0, HoleType::Hole);
    MeshParams mp;
    mp.meshDensity = 3;
    const std::vector<Vec3> nodes = k.meshNodes(k.meshFeature(f, mp));
    dvatest::check(nodes.size() == 9, "near-max diagonal cylinder mesh node count");
    double minX = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    for (const auto& n : nodes) {
        dvatest::check(std::isfinite(n.x) && std::isfinite(n.y) && std::isfinite(n.z),
                       "near-max diagonal cylinder mesh node finite");
        minX = std::min(minX, n.x);
        maxX = std::max(maxX, n.x);
    }
    dvatest::checkNear(minX, -1.1e308, 2e292, "near-max diagonal cylinder min x");
    dvatest::checkNear(maxX, 1.1e308, 2e292, "near-max diagonal cylinder max x");
}

TEST("mesh_cylinder_handles_opposite_sign_axial_offsets") {
    StubGeometryKernel k;
    const FeatureId f = k.createCylinder({{-1.7e308, 0, 0},
                                          {-1.7e308, 1, 0},
                                          {1.7e308, 0, 0}},
                                         2.0, HoleType::Hole);
    MeshParams mp;
    mp.meshDensity = 3;
    const std::vector<Vec3> nodes = k.meshNodes(k.meshFeature(f, mp));
    dvatest::check(nodes.size() == 9, "opposite-sign axial cylinder mesh node count");
    double maxX = -std::numeric_limits<double>::infinity();
    for (const auto& n : nodes) {
        dvatest::check(std::isfinite(n.x) && std::isfinite(n.y) && std::isfinite(n.z),
                       "opposite-sign axial cylinder mesh node finite");
        maxX = std::max(maxX, n.x);
    }
    dvatest::checkNear(maxX, 1.7e308, 2e292, "opposite-sign axial cylinder max x");
}

// ---- meshFeature/meshNodes: sphere density=4 -> finite nodes on the radius ----
TEST("mesh_sphere_nodes") {
    StubGeometryKernel k;
    const FeatureId f = k.createSphere({{1, 2, 3}}, 6.0);
    MeshParams mp;
    mp.meshDensity = 4;
    const MeshHandle h = k.meshFeature(f, mp);
    dvatest::check(h.meshNodeNum == 16, "16 sphere mesh nodes");
    const std::vector<Vec3> nodes = k.meshNodes(h);
    dvatest::check(nodes.size() == 16, "meshNodes returns 16 sphere nodes");
    for (const auto& n : nodes) {
        const Vec3 d{n.x - 1.0, n.y - 2.0, n.z - 3.0};
        dvatest::checkNear(len(d), 3.0, 1e-9, "sphere node on radius");
    }
}

TEST("mesh_sphere_ignores_nonfinite_defining_points") {
    StubGeometryKernel k;
    const FeatureId f = k.createSphere({{1, 2, 3},
                                        {std::numeric_limits<double>::infinity(), 2, 3}},
                                       6.0);
    MeshParams mp;
    mp.meshDensity = 4;
    const std::vector<Vec3> nodes = k.meshNodes(k.meshFeature(f, mp));
    dvatest::check(nodes.size() == 16, "finite sphere mesh node count");
    for (const auto& n : nodes) {
        dvatest::check(std::isfinite(n.x) && std::isfinite(n.y) && std::isfinite(n.z),
                       "finite sphere mesh node");
        const Vec3 d{n.x - 1.0, n.y - 2.0, n.z - 3.0};
        dvatest::checkNear(len(d), 3.0, 1e-9, "finite sphere node radius");
    }
}

// ---- meshFeature/meshNodes: sphere with no diameter infers center/radius ----
TEST("mesh_sphere_infers_from_points") {
    StubGeometryKernel k;
    const FeatureId f =
        k.createSphere({{4, 2, 3}, {1, 5, 3}, {1, 2, 6}, {-2, 2, 3}}, 0.0);
    MeshParams mp;
    mp.meshDensity = 4;
    const std::vector<Vec3> nodes = k.meshNodes(k.meshFeature(f, mp));
    dvatest::check(nodes.size() == 16, "inferred sphere mesh nodes");
    for (const auto& n : nodes) {
        const Vec3 d{n.x - 1.0, n.y - 2.0, n.z - 3.0};
        dvatest::checkNear(len(d), 3.0, 1e-9, "inferred sphere node radius");
    }
}

TEST("mesh_sphere_infers_huge_finite_radius") {
    StubGeometryKernel k;
    const FeatureId f = k.createSphere({{0, 0, 0}, {1e308, 0, 0}}, 0.0);
    MeshParams mp;
    mp.meshDensity = 4;
    const std::vector<Vec3> nodes = k.meshNodes(k.meshFeature(f, mp));
    dvatest::check(nodes.size() == 16, "huge inferred sphere mesh nodes");
    for (const auto& n : nodes) {
        dvatest::check(std::isfinite(n.x) && std::isfinite(n.y) && std::isfinite(n.z),
                       "huge inferred sphere node is finite");
        const Vec3 d{n.x - 5e307, n.y, n.z};
        dvatest::checkNear(std::hypot(d.x, d.y, d.z), 5e307, 2e292,
                           "huge inferred sphere node radius");
    }
}

TEST("mesh_sphere_fits_huge_finite_points") {
    StubGeometryKernel k;
    const FeatureId f = k.createSphere(
        {{1e155, 0, 0}, {0, 1e155, 0}, {0, 0, 1e155}, {-1e155, 0, 0}}, 0.0);
    MeshParams mp;
    mp.meshDensity = 4;
    const std::vector<Vec3> nodes = k.meshNodes(k.meshFeature(f, mp));
    dvatest::check(nodes.size() == 16, "huge fitted sphere mesh nodes");
    for (const auto& n : nodes) {
        dvatest::check(std::isfinite(n.x) && std::isfinite(n.y) && std::isfinite(n.z),
                       "huge fitted sphere node is finite");
        dvatest::checkNear(std::hypot(n.x, n.y, n.z), 1e155, 1e140,
                           "huge fitted sphere node radius");
    }
}

TEST("mesh_sphere_fits_near_max_finite_points") {
    StubGeometryKernel k;
    const FeatureId f = k.createSphere(
        {{1e308, 0, 0}, {0, 1e308, 0}, {0, 0, 1e308}, {-1e308, 0, 0}}, 0.0);
    MeshParams mp;
    mp.meshDensity = 4;
    const std::vector<Vec3> nodes = k.meshNodes(k.meshFeature(f, mp));
    dvatest::check(nodes.size() == 16, "near-max fitted sphere mesh nodes");
    for (const auto& n : nodes) {
        dvatest::check(std::isfinite(n.x) && std::isfinite(n.y) && std::isfinite(n.z),
                       "near-max fitted sphere node is finite");
        dvatest::checkNear(std::hypot(n.x, n.y, n.z), 1e308, 2e292,
                           "near-max fitted sphere node radius");
    }
}

// ---- meshFeature/meshNodes: slot/tab walls produce a ruled surface grid ----
TEST("mesh_slot_tab_nodes") {
    StubGeometryKernel k;
    const FeatureId f = k.createSlotTab({{0, 0, 0}, {4, 0, 0}}, {{0, 2, 0}, {4, 2, 0}});
    MeshParams mp;
    mp.meshDensity = 3;
    const MeshHandle h = k.meshFeature(f, mp);
    dvatest::check(h.meshNodeNum == 9, "9 slot tab mesh nodes");
    const std::vector<Vec3> nodes = k.meshNodes(h);
    dvatest::check(nodes.size() == 9, "meshNodes returns 9 slot tab nodes");
    for (const auto& n : nodes) {
        dvatest::check(n.x >= -1e-9 && n.x <= 4.0 + 1e-9, "slot node x in [0,4]");
        dvatest::check(n.y >= -1e-9 && n.y <= 2.0 + 1e-9, "slot node y in [0,2]");
        dvatest::checkNear(n.z, 0.0, 1e-9, "slot node on z=0");
    }
}

TEST("mesh_slot_tab_ignores_nonfinite_wall_pairs") {
    StubGeometryKernel k;
    const FeatureId f =
        k.createSlotTab({{0, 0, 0}, {4, 0, 0}, {std::numeric_limits<double>::infinity(), 0, 0}},
                        {{0, 2, 0}, {4, 2, 0}, {std::numeric_limits<double>::infinity(), 2, 0}});
    MeshParams mp;
    mp.meshDensity = 3;
    const std::vector<Vec3> nodes = k.meshNodes(k.meshFeature(f, mp));
    dvatest::check(nodes.size() == 9, "finite slot tab mesh node count");
    for (const auto& n : nodes) {
        dvatest::check(std::isfinite(n.x) && std::isfinite(n.y) && std::isfinite(n.z),
                       "finite slot tab mesh node");
        dvatest::check(n.x >= -1e-9 && n.x <= 4.0 + 1e-9,
                       "finite slot tab x range");
        dvatest::check(n.y >= -1e-9 && n.y <= 2.0 + 1e-9,
                       "finite slot tab y range");
        dvatest::checkNear(n.z, 0.0, 1e-9, "finite slot tab z");
    }
}

TEST("mesh_slot_tab_handles_near_max_wall_interpolation") {
    StubGeometryKernel k;
    const FeatureId f = k.createSlotTab({{-1.1e308, 0, 0}, {1.1e308, 0, 0}},
                                        {{-1.1e308, 2, 0}, {1.1e308, 2, 0}});
    MeshParams mp;
    mp.meshDensity = 3;
    const std::vector<Vec3> nodes = k.meshNodes(k.meshFeature(f, mp));
    dvatest::check(nodes.size() == 9, "near-max slot tab mesh node count");
    bool sawCenter = false;
    for (const auto& n : nodes) {
        dvatest::check(std::isfinite(n.x) && std::isfinite(n.y) && std::isfinite(n.z),
                       "near-max slot tab mesh node finite");
        if (std::fabs(n.x) <= 1e292) sawCenter = true;
    }
    dvatest::check(sawCenter, "near-max slot tab center column");
}

// ---- meshNodes on an unknown handle returns empty ----
TEST("mesh_unknown_handle_empty") {
    StubGeometryKernel k;
    MeshHandle bogus{};
    bogus.version = 999999;
    dvatest::check(k.meshNodes(bogus).empty(), "unknown handle -> empty");
}

TEST("resolve_direction_handles_huge_and_nonfinite_vectors") {
    StubGeometryKernel k;
    PointContext ctx{};

    Direction huge;
    huge.ijk = {1e308, 1e308, 0.0};
    const Vec3 unit = k.resolveDirection(huge, ctx);
    dvatest::checkNear(len(unit), 1.0, 1e-12, "huge finite direction remains unit");
    dvatest::check(unit.x > 0.7 && unit.y > 0.7,
                   "huge finite direction preserves orientation");

    Direction bad;
    bad.ijk = {std::numeric_limits<double>::infinity(), 0.0, 0.0};
    const Vec3 fallback = k.resolveDirection(bad, ctx);
    dvatest::checkNear(fallback.x, 0.0, 1e-12, "nonfinite direction fallback x");
    dvatest::checkNear(fallback.y, 0.0, 1e-12, "nonfinite direction fallback y");
    dvatest::checkNear(fallback.z, 1.0, 1e-12, "nonfinite direction fallback z");
}

TEST("least_squares_fits_ignore_nonfinite_points") {
    StubGeometryKernel k;
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    const Plane plane = k.fitLeastSquaresPlane(
        {{0.0, 0.0, 0.0}, {2.0, 0.0, 0.0}, {0.0, 2.0, 0.0}, {inf, 1.0, 0.0}});
    dvatest::checkNear(plane.point.x, 2.0 / 3.0, 1e-12,
                       "plane fit ignores nonfinite point x");
    dvatest::checkNear(plane.point.y, 2.0 / 3.0, 1e-12,
                       "plane fit ignores nonfinite point y");
    dvatest::checkNear(plane.point.z, 0.0, 1e-12,
                       "plane fit ignores nonfinite point z");
    dvatest::checkNear(absDot(plane.normal, {0.0, 0.0, 1.0}), 1.0, 1e-12,
                       "plane fit normal uses finite points");

    const Axis axis = k.fitLeastSquaresAxis(
        {{0.0, 1.0, 0.0}, {5.0, 1.0, 0.0}, {10.0, 1.0, 0.0}, {nan, 0.0, 0.0}},
        /*shiftOnly=*/false);
    dvatest::checkNear(axis.point.x, 5.0, 1e-12, "axis fit ignores nonfinite point x");
    dvatest::checkNear(axis.point.y, 1.0, 1e-12, "axis fit ignores nonfinite point y");
    dvatest::checkNear(absDot(axis.direction, {1.0, 0.0, 0.0}), 1.0, 1e-12,
                       "axis fit direction uses finite points");
}
