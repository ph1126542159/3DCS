#include "opendva/geometry/StubGeometryKernel.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "opendva/geometry/LinAlg.h"

namespace opendva {
namespace {

bool finitePoint(const Vec3& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

double scaledOffset(double value, double origin, double scale) {
    const double direct = value - origin;
    if (std::isfinite(direct)) {
        return direct / scale;
    }
    return value / scale - origin / scale;
}

std::vector<Vec3> finitePoints(const std::vector<Vec3>& pts) {
    std::vector<Vec3> filtered;
    filtered.reserve(pts.size());
    for (const auto& p : pts) {
        if (finitePoint(p)) filtered.push_back(p);
    }
    return filtered;
}

double lerp(double a, double b, double t) { return (1.0 - t) * a + t * b; }

Vec3 centroid(const std::vector<Vec3>& pts) {
    if (pts.empty()) return {};
    const Vec3 origin = pts.front();
    double scale = 0.0;
    for (const auto& p : pts) {
        scale = std::max(scale, std::fabs(origin.x));
        scale = std::max(scale, std::fabs(origin.y));
        scale = std::max(scale, std::fabs(origin.z));
        scale = std::max(scale, std::fabs(p.x));
        scale = std::max(scale, std::fabs(p.y));
        scale = std::max(scale, std::fabs(p.z));
    }
    if (scale <= 0.0 || !std::isfinite(scale)) {
        return origin;
    }
    Vec3 meanOffset{};
    double n = 0.0;
    for (const auto& p : pts) {
        n += 1.0;
        const Vec3 offset{p.x / scale - origin.x / scale, p.y / scale - origin.y / scale,
                          p.z / scale - origin.z / scale};
        meanOffset.x += (offset.x - meanOffset.x) / n;
        meanOffset.y += (offset.y - meanOffset.y) / n;
        meanOffset.z += (offset.z - meanOffset.z) / n;
    }
    return {scale * (origin.x / scale + meanOffset.x),
            scale * (origin.y / scale + meanOffset.y),
            scale * (origin.z / scale + meanOffset.z)};
}

Vec3 normalise(Vec3 v) {
    if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) {
        return {0, 0, 1};
    }
    const double len = std::hypot(v.x, v.y, v.z);
    if (!std::isfinite(len) || len < 1e-12) {
        return {0, 0, 1};  // degenerate -> default per README §4
    }
    return {v.x / len, v.y / len, v.z / len};
}

struct SphereFit {
    Vec3 center{};
    double radius{0.0};
    bool ok{false};
};

bool solve3x3(double a[3][4], Vec3& out) {
    for (int col = 0; col < 3; ++col) {
        int pivot = col;
        for (int row = col + 1; row < 3; ++row) {
            if (std::fabs(a[row][col]) > std::fabs(a[pivot][col])) pivot = row;
        }
        if (std::fabs(a[pivot][col]) < 1e-12) return false;
        if (pivot != col) {
            for (int k = col; k < 4; ++k) std::swap(a[col][k], a[pivot][k]);
        }
        const double invPivot = 1.0 / a[col][col];
        for (int k = col; k < 4; ++k) a[col][k] *= invPivot;
        for (int row = 0; row < 3; ++row) {
            if (row == col) continue;
            const double factor = a[row][col];
            for (int k = col; k < 4; ++k) a[row][k] -= factor * a[col][k];
        }
    }
    out = {a[0][3], a[1][3], a[2][3]};
    return true;
}

SphereFit fitSphere(const std::vector<Vec3>& pts) {
    SphereFit fit;
    if (pts.size() < 4) return fit;

    const Vec3 origin = pts.front();
    double scale = 0.0;
    for (const auto& p : pts) {
        scale = std::max(scale, std::fabs(origin.x));
        scale = std::max(scale, std::fabs(origin.y));
        scale = std::max(scale, std::fabs(origin.z));
        scale = std::max(scale, std::fabs(p.x));
        scale = std::max(scale, std::fabs(p.y));
        scale = std::max(scale, std::fabs(p.z));
    }
    if (scale <= 0.0 || !std::isfinite(scale)) return fit;

    double normal[3][4]{};
    for (std::size_t i = 1; i < pts.size(); ++i) {
        const Vec3 p = pts[i];
        const double qx = p.x / scale - origin.x / scale;
        const double qy = p.y / scale - origin.y / scale;
        const double qz = p.z / scale - origin.z / scale;
        if (!std::isfinite(qx) || !std::isfinite(qy) || !std::isfinite(qz)) continue;
        const double row[3] = {2.0 * qx, 2.0 * qy, 2.0 * qz};
        const double rhs = qx * qx + qy * qy + qz * qz;
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) normal[r][c] += row[r] * row[c];
            normal[r][3] += row[r] * rhs;
        }
    }

    Vec3 scaledCenter{};
    if (!solve3x3(normal, scaledCenter)) return fit;
    Vec3 center{origin.x + scale * scaledCenter.x, origin.y + scale * scaledCenter.y,
                origin.z + scale * scaledCenter.z};
    if (!finitePoint(center)) return fit;
    double radius = 0.0;
    double n = 0.0;
    for (const auto& p : pts) {
        const double qx = p.x / scale - origin.x / scale - scaledCenter.x;
        const double qy = p.y / scale - origin.y / scale - scaledCenter.y;
        const double qz = p.z / scale - origin.z / scale - scaledCenter.z;
        const double candidate = scale * std::hypot(qx, qy, qz);
        if (!std::isfinite(candidate)) return fit;
        n += 1.0;
        radius += (candidate - radius) / n;
    }
    if (radius <= 0.0 || !std::isfinite(radius)) return fit;
    fit.center = center;
    fit.radius = radius;
    fit.ok = true;
    return fit;
}

// Covariance (scatter) matrix of the point cloud about its centroid. The
// principal axes of this matrix are the least-squares fit directions:
//   - smallest eigenvalue  -> plane normal (least spread)
//   - largest  eigenvalue  -> axis direction (most spread)
linalg::Sym3 covariance(const std::vector<Vec3>& pts, const Vec3& mean) {
    linalg::Sym3 cov;
    double scale = 0.0;
    for (const auto& p : pts) {
        const double dx = p.x - mean.x;
        const double dy = p.y - mean.y;
        const double dz = p.z - mean.z;
        scale = std::max(scale,
                         std::isfinite(dx) ? std::fabs(dx)
                                           : std::max(std::fabs(p.x), std::fabs(mean.x)));
        scale = std::max(scale,
                         std::isfinite(dy) ? std::fabs(dy)
                                           : std::max(std::fabs(p.y), std::fabs(mean.y)));
        scale = std::max(scale,
                         std::isfinite(dz) ? std::fabs(dz)
                                           : std::max(std::fabs(p.z), std::fabs(mean.z)));
    }
    if (scale <= 0.0 || !std::isfinite(scale)) {
        return cov;
    }
    for (const auto& p : pts) {
        const double dx = scaledOffset(p.x, mean.x, scale);
        const double dy = scaledOffset(p.y, mean.y, scale);
        const double dz = scaledOffset(p.z, mean.z, scale);
        if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz)) continue;
        cov.at(0, 0) += dx * dx;
        cov.at(0, 1) += dx * dy;
        cov.at(0, 2) += dx * dz;
        cov.at(1, 1) += dy * dy;
        cov.at(1, 2) += dy * dz;
        cov.at(2, 2) += dz * dz;
    }
    // Mirror into the lower triangle for a full symmetric matrix.
    cov.at(1, 0) = cov.at(0, 1);
    cov.at(2, 0) = cov.at(0, 2);
    cov.at(2, 1) = cov.at(1, 2);
    return cov;
}

}  // namespace

FeatureId StubGeometryKernel::createPlane(const std::vector<Vec3>& pts) {
    const FeatureId id = nextId_++;
    features_[id] = FeatureDef{Kind::Plane, pts, {}, 0.0};
    return id;
}

FeatureId StubGeometryKernel::createCylinder(const std::vector<Vec3>& pts, double diameter,
                                             HoleType) {
    const FeatureId id = nextId_++;
    features_[id] = FeatureDef{Kind::Cylinder, pts, {}, diameter};
    return id;
}

FeatureId StubGeometryKernel::createSphere(const std::vector<Vec3>& pts, double diameter) {
    const FeatureId id = nextId_++;
    features_[id] = FeatureDef{Kind::Sphere, pts, {}, diameter};
    return id;
}
FeatureId StubGeometryKernel::createSlotTab(const std::vector<Vec3>& wall1,
                                            const std::vector<Vec3>& wall2) {
    const FeatureId id = nextId_++;
    FeatureDef def{Kind::SlotTab, wall1, wall2, 0.0};
    features_[id] = std::move(def);
    return id;
}

MeshHandle StubGeometryKernel::meshFeature(FeatureId feature, const MeshParams& params) {
    MeshHandle h{};
    h.feature = feature;

    const int density = std::max(1, params.meshDensity);
    std::vector<Vec3> nodes;

    const auto it = features_.find(feature);
    if (it != features_.end() && !it->second.pts.empty()) {
        const FeatureDef& def = it->second;
        if (def.kind == Kind::Plane) {
            // Sample a density x density grid over the in-plane bounding box of
            // the defining points (README §3.1.4 analysis-feature nodes).
            const Plane plane = fitLeastSquaresPlane(def.pts);
            // Build an orthonormal in-plane basis {u, v} around the normal.
            const Vec3 n = plane.normal;
            Vec3 ref = (std::fabs(n.x) < 0.9) ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
            const double dot = ref.x * n.x + ref.y * n.y + ref.z * n.z;
            Vec3 u = normalise({ref.x - dot * n.x, ref.y - dot * n.y, ref.z - dot * n.z});
            Vec3 v{n.y * u.z - n.z * u.y, n.z * u.x - n.x * u.z, n.x * u.y - n.y * u.x};

            // Project defining points onto (u, v) to find the parameter range.
            double uMin = 1e300, uMax = -1e300, vMin = 1e300, vMax = -1e300;
            bool sawFiniteProjection = false;
            double projectionScale = 0.0;
            for (const auto& p : def.pts) {
                if (!finitePoint(p)) continue;
                projectionScale =
                    std::max({projectionScale, std::fabs(p.x), std::fabs(p.y),
                              std::fabs(p.z), std::fabs(plane.point.x),
                              std::fabs(plane.point.y), std::fabs(plane.point.z)});
            }
            if (projectionScale <= 0.0 || !std::isfinite(projectionScale)) {
                projectionScale = 1.0;
            }
            for (const auto& p : def.pts) {
                if (!finitePoint(p)) continue;
                const Vec3 d{scaledOffset(p.x, plane.point.x, projectionScale),
                             scaledOffset(p.y, plane.point.y, projectionScale),
                             scaledOffset(p.z, plane.point.z, projectionScale)};
                const double pu = d.x * u.x + d.y * u.y + d.z * u.z;
                const double pv = d.x * v.x + d.y * v.y + d.z * v.z;
                if (!std::isfinite(pu) || !std::isfinite(pv)) continue;
                uMin = std::min(uMin, pu);
                uMax = std::max(uMax, pu);
                vMin = std::min(vMin, pv);
                vMax = std::max(vMax, pv);
                sawFiniteProjection = true;
            }
            if (sawFiniteProjection) {
                nodes.reserve(static_cast<std::size_t>(density) * density);
                for (int i = 0; i < density; ++i) {
                    const double fu =
                        (density == 1) ? 0.5 : static_cast<double>(i) / (density - 1);
                    const double pu = uMin + fu * (uMax - uMin);
                    for (int j = 0; j < density; ++j) {
                        const double fv =
                            (density == 1) ? 0.5 : static_cast<double>(j) / (density - 1);
                        const double pv = vMin + fv * (vMax - vMin);
                        nodes.push_back(
                            {projectionScale *
                                 (plane.point.x / projectionScale + pu * u.x + pv * v.x),
                             projectionScale *
                                 (plane.point.y / projectionScale + pu * u.y + pv * v.y),
                             projectionScale *
                                 (plane.point.z / projectionScale + pu * u.z + pv * v.z)});
                    }
                }
            }
        } else if (def.kind == Kind::Sphere) {
            const std::vector<Vec3> finite = finitePoints(def.pts);
            Vec3 center = centroid(finite);
            double radius = def.diameter > 0.0 ? 0.5 * def.diameter : 0.0;
            if (radius <= 0.0) {
                const SphereFit fit = fitSphere(finite);
                if (fit.ok) {
                    center = fit.center;
                    radius = fit.radius;
                } else {
                    for (const auto& p : finite) {
                        const Vec3 d{p.x - center.x, p.y - center.y, p.z - center.z};
                        const double candidate = std::hypot(d.x, d.y, d.z);
                        if (std::isfinite(candidate)) {
                            radius = std::max(radius, candidate);
                        }
                    }
                }
            }
            if (!finite.empty() && std::isfinite(radius) && radius >= 0.0) {
                nodes.reserve(static_cast<std::size_t>(density) * density);
                constexpr double kPi = 3.141592653589793;
                constexpr double kTwoPi = 6.283185307179586;
                for (int i = 0; i < density; ++i) {
                    const double v =
                        (density == 1) ? 0.5 : static_cast<double>(i) / (density - 1);
                    const double polar = kPi * v;
                    const double sp = std::sin(polar);
                    const double cp = std::cos(polar);
                    for (int j = 0; j < density; ++j) {
                        const double az = kTwoPi * static_cast<double>(j) / density;
                        nodes.push_back({center.x + radius * sp * std::cos(az),
                                         center.y + radius * sp * std::sin(az),
                                         center.z + radius * cp});
                    }
                }
            }
        } else if (def.kind == Kind::SlotTab && !def.pts2.empty()) {
            const std::size_t usable =
                std::min<std::size_t>(def.pts.size(), def.pts2.size());
            std::vector<Vec3> wall1;
            std::vector<Vec3> wall2;
            wall1.reserve(usable);
            wall2.reserve(usable);
            for (std::size_t i = 0; i < usable; ++i) {
                if (finitePoint(def.pts[i]) && finitePoint(def.pts2[i])) {
                    wall1.push_back(def.pts[i]);
                    wall2.push_back(def.pts2[i]);
                }
            }
            if (wall1.size() >= 2) {
                nodes.reserve(static_cast<std::size_t>(density) * density);
                for (int i = 0; i < density; ++i) {
                    const double along =
                        (density == 1) ? 0.5 : static_cast<double>(i) / (density - 1);
                    const double scaled = along * static_cast<double>(wall1.size() - 1);
                    const std::size_t seg =
                        std::min<std::size_t>(static_cast<std::size_t>(scaled),
                                              wall1.size() - 2);
                    const double local = scaled - static_cast<double>(seg);
                    const Vec3 a{lerp(wall1[seg].x, wall1[seg + 1].x, local),
                                 lerp(wall1[seg].y, wall1[seg + 1].y, local),
                                 lerp(wall1[seg].z, wall1[seg + 1].z, local)};
                    const Vec3 b{lerp(wall2[seg].x, wall2[seg + 1].x, local),
                                 lerp(wall2[seg].y, wall2[seg + 1].y, local),
                                 lerp(wall2[seg].z, wall2[seg + 1].z, local)};
                    for (int j = 0; j < density; ++j) {
                        const double across =
                            (density == 1) ? 0.5 : static_cast<double>(j) / (density - 1);
                        nodes.push_back({lerp(a.x, b.x, across), lerp(a.y, b.y, across),
                                         lerp(a.z, b.z, across)});
                    }
                }
            }
        } else {  // Cylinder: density cross-sections x density circumferential pts.
            const Axis axis = fitLeastSquaresAxis(def.pts, /*shiftOnly=*/false);
            const double radius =
                (def.diameter > 0.0) ? 0.5 * def.diameter
                                     : maxMinCylinder(def.pts, axis).first;  // fall back to fit
            const Vec3 a = axis.direction;
            // In-plane basis perpendicular to the axis.
            Vec3 ref = (std::fabs(a.x) < 0.9) ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
            const double dot = ref.x * a.x + ref.y * a.y + ref.z * a.z;
            Vec3 u = normalise({ref.x - dot * a.x, ref.y - dot * a.y, ref.z - dot * a.z});
            Vec3 w{a.y * u.z - a.z * u.y, a.z * u.x - a.x * u.z, a.x * u.y - a.y * u.x};

            // Axial extent from the projection of the defining points.
            double axialScale = 0.0;
            bool sawFinitePoint = false;
            for (const auto& p : def.pts) {
                if (!finitePoint(p)) continue;
                axialScale = std::max({axialScale, std::fabs(p.x), std::fabs(p.y),
                                       std::fabs(p.z), std::fabs(axis.point.x),
                                       std::fabs(axis.point.y), std::fabs(axis.point.z)});
                sawFinitePoint = true;
            }
            double tMin = 1e300, tMax = -1e300;
            bool sawFiniteProjection = false;
            if (sawFinitePoint && axialScale <= 0.0) {
                tMin = 0.0;
                tMax = 0.0;
                sawFiniteProjection = true;
            } else if (std::isfinite(axialScale)) {
                for (const auto& p : def.pts) {
                    if (!finitePoint(p)) continue;
                    const Vec3 sd{p.x / axialScale - axis.point.x / axialScale,
                                  p.y / axialScale - axis.point.y / axialScale,
                                  p.z / axialScale - axis.point.z / axialScale};
                    const double t = sd.x * a.x + sd.y * a.y + sd.z * a.z;
                    if (!std::isfinite(t)) continue;
                    tMin = std::min(tMin, t);
                    tMax = std::max(tMax, t);
                    sawFiniteProjection = true;
                }
            }
            if (sawFiniteProjection) {
                nodes.reserve(static_cast<std::size_t>(density) * density);
                constexpr double kTwoPi = 6.283185307179586;
                for (int i = 0; i < density; ++i) {
                    const double ft =
                        (density == 1) ? 0.5 : static_cast<double>(i) / (density - 1);
                    const double t = tMin + ft * (tMax - tMin);
                    for (int j = 0; j < density; ++j) {
                        const double ang = kTwoPi * static_cast<double>(j) / density;
                        const double cu = radius * std::cos(ang);
                        const double cw = radius * std::sin(ang);
                        nodes.push_back(
                            {axialScale * (axis.point.x / axialScale + t * a.x) +
                                 cu * u.x + cw * w.x,
                             axialScale * (axis.point.y / axialScale + t * a.y) +
                                 cu * u.y + cw * w.y,
                             axialScale * (axis.point.z / axialScale + t * a.z) +
                                 cu * u.z + cw * w.z});
                    }
                }
            }
        }
    }

    h.meshNodeNum = nodes.size();
    h.cadPtNum = nodes.size();
    const std::uint64_t key = nextMeshKey_++;
    h.version = key;  // doubles as the lookup key for meshNodes().
    meshNodes_[key] = std::move(nodes);
    return h;
}

std::vector<Vec3> StubGeometryKernel::meshNodes(const MeshHandle& mesh) const {
    const auto it = meshNodes_.find(mesh.version);
    if (it == meshNodes_.end()) return {};
    return it->second;
}

Plane StubGeometryKernel::fitLeastSquaresPlane(const std::vector<Vec3>& pts) const {
    // Ls LCS (README §14.1): plane through the centroid, normal = eigenvector of
    // the covariance matrix for the *smallest* eigenvalue (direction of least
    // spread). Solved with a self-written symmetric 3x3 Jacobi decomposition.
    const std::vector<Vec3> finite = finitePoints(pts);
    Plane plane{};
    plane.point = centroid(finite);
    if (finite.size() < 3) {
        plane.normal = {0, 0, 1};
        return plane;
    }
    const linalg::Eigen3 e = linalg::jacobiEigen(covariance(finite, plane.point));
    const auto col = linalg::eigenColumn(e, linalg::minEigenIndex(e));
    plane.normal = normalise({col[0], col[1], col[2]});
    return plane;
}

Axis StubGeometryKernel::fitLeastSquaresAxis(const std::vector<Vec3>& pts, bool shiftOnly) const {
    // README §14.1: axis through the geometric center.
    const std::vector<Vec3> finite = finitePoints(pts);
    Axis axis{};
    axis.point = centroid(finite);  // axis point = geometric center
    if (shiftOnly) {
        axis.direction = {0, 0, 1};  // LSS: parallel to nominal, translate only.
        return axis;
    }
    if (finite.size() < 2) {
        axis.direction = {0, 0, 1};
        return axis;
    }
    // LSA: axis direction = eigenvector for the *largest* eigenvalue (the
    // direction of maximum spread = total-least-squares best-fit line).
    const linalg::Eigen3 e = linalg::jacobiEigen(covariance(finite, axis.point));
    const auto col = linalg::eigenColumn(e, linalg::maxEigenIndex(e));
    axis.direction = normalise({col[0], col[1], col[2]});
    return axis;
}

std::pair<double, double> StubGeometryKernel::maxMinCylinder(const std::vector<Vec3>& pts,
                                                             const Axis& axis) const {
    // MaxCyl = smallest enclosing cylinder radius (max radial distance);
    // MinCyl = largest fully-inscribed cylinder radius (min radial distance).
    if (pts.empty()) return {0.0, 0.0};
    const Vec3 dir = normalise(axis.direction);
    double maxR = 0.0;
    double minR = std::numeric_limits<double>::infinity();
    bool sawFiniteRadius = false;
    for (const auto& p : pts) {
        if (!finitePoint(p) || !finitePoint(axis.point)) continue;
        const double scale = std::max({std::fabs(p.x), std::fabs(p.y), std::fabs(p.z),
                                       std::fabs(axis.point.x), std::fabs(axis.point.y),
                                       std::fabs(axis.point.z)});
        double r = 0.0;
        if (scale > 0.0 && std::isfinite(scale)) {
            const Vec3 sd{p.x / scale - axis.point.x / scale,
                          p.y / scale - axis.point.y / scale,
                          p.z / scale - axis.point.z / scale};
            const double proj = sd.x * dir.x + sd.y * dir.y + sd.z * dir.z;
            const Vec3 radial{sd.x - proj * dir.x, sd.y - proj * dir.y,
                              sd.z - proj * dir.z};
            r = scale * std::hypot(radial.x, radial.y, radial.z);
        }
        if (!std::isfinite(r)) continue;
        maxR = std::max(maxR, r);
        minR = std::min(minR, r);
        sawFiniteRadius = true;
    }
    if (!sawFiniteRadius) return {0.0, 0.0};
    return {maxR, minR};
}

Vec3 StubGeometryKernel::resolveDirection(const Direction& dir, const PointContext& ctx) const {
    // Resolves a Direction to a unit IJK (README §3.3, 6 types).
    switch (dir.type) {
        case DirectionType::TypeIn:
            // Authored IJK, relative to the part LCS in a real kernel.
            return normalise(dir.ijk);
        case DirectionType::AssocDir:
        case DirectionType::PickPtDir:
            // Direction carried by the picked/associated point.
            return normalise(ctx.assocVector);
        case DirectionType::TwoPoints:
        case DirectionType::Normal:
            // TODO: TwoPoints needs the two refPoints' coordinates and Normal
            // needs the surface normal at refPoints; this kernel has no point
            // store, so the upstream domain layer resolves these and passes the
            // result down via dir.ijk. Fall through to the authored IJK.
            return normalise(dir.ijk);
        case DirectionType::Auto:
        default:
            // Auto picks the first feature's direction upstream; use IJK here.
            return normalise(dir.ijk);
    }
}

}  // namespace opendva
