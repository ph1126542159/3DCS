// A1 geometry kernel contract (README §3, §14.1).
// Implementations: OcctGeometryKernel (real) and StubGeometryKernel (mock).
#pragma once
#include <utility>
#include <vector>

#include "opendva/Types.h"

namespace opendva {

struct PointContext {
    Vec3 position{};
    Vec3 assocVector{0, 0, 1};  // associated direction carried by the point
    Mat34 lcs{};                // local coordinate system of owning part
};

class IGeometryKernel {
public:
    virtual ~IGeometryKernel() = default;

    // ---- Entity creation (README §3.1) ----
    virtual FeatureId createPlane(const std::vector<Vec3>& pts) = 0;
    virtual FeatureId createCylinder(const std::vector<Vec3>& pts, double diameter,
                                     HoleType holeType) = 0;
    virtual FeatureId createSphere(const std::vector<Vec3>& pts, double diameter) = 0;
    virtual FeatureId createSlotTab(const std::vector<Vec3>& wall1,
                                    const std::vector<Vec3>& wall2) = 0;

    // ---- Meshing (double-layer model, README §3.1.4) ----
    virtual MeshHandle meshFeature(FeatureId feature, const MeshParams& params) = 0;
    virtual std::vector<Vec3> meshNodes(const MeshHandle& mesh) const = 0;

    // ---- Least-squares fitting (README §14.1) ----
    virtual Plane fitLeastSquaresPlane(const std::vector<Vec3>& pts) const = 0;
    // shiftOnly==true -> LSS (axis parallel to nominal, translate only);
    // shiftOnly==false -> LSA (axis direction may deviate).
    virtual Axis fitLeastSquaresAxis(const std::vector<Vec3>& pts, bool shiftOnly) const = 0;
    // Returns {maxCylRadius, minCylRadius} about the given axis.
    virtual std::pair<double, double> maxMinCylinder(const std::vector<Vec3>& pts,
                                                     const Axis& axis) const = 0;

    // ---- Direction system (README §3.3) — resolves to a unit IJK ----
    virtual Vec3 resolveDirection(const Direction& dir, const PointContext& ctx) const = 0;
};

}  // namespace opendva
