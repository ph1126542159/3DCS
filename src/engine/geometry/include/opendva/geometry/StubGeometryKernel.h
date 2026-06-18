// A1 stub kernel: analytic geometry, no OCCT. Lets the pipeline run headless
// in CI. Replaced by OcctGeometryKernel when OPENDVA_USE_OCCT is ON.
#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "opendva/IGeometryKernel.h"

namespace opendva {

class StubGeometryKernel final : public IGeometryKernel {
public:
    FeatureId createPlane(const std::vector<Vec3>& pts) override;
    FeatureId createCylinder(const std::vector<Vec3>& pts, double diameter,
                             HoleType holeType) override;
    FeatureId createSphere(const std::vector<Vec3>& pts, double diameter) override;
    FeatureId createSlotTab(const std::vector<Vec3>& wall1,
                            const std::vector<Vec3>& wall2) override;

    MeshHandle meshFeature(FeatureId feature, const MeshParams& params) override;
    std::vector<Vec3> meshNodes(const MeshHandle& mesh) const override;

    Plane fitLeastSquaresPlane(const std::vector<Vec3>& pts) const override;
    Axis fitLeastSquaresAxis(const std::vector<Vec3>& pts, bool shiftOnly) const override;
    std::pair<double, double> maxMinCylinder(const std::vector<Vec3>& pts,
                                             const Axis& axis) const override;

    Vec3 resolveDirection(const Direction& dir, const PointContext& ctx) const override;

private:
    // Stored geometry of a created feature, used to mesh it later.
    enum class Kind { Plane, Cylinder, Sphere, SlotTab };
    struct FeatureDef {
        Kind kind{Kind::Plane};
        std::vector<Vec3> pts;  // defining points
        std::vector<Vec3> pts2; // secondary wall points for slot/tab
        double diameter{0.0};   // cylinders/spheres
    };

    FeatureId nextId_{1};
    std::uint64_t nextMeshKey_{1};
    std::unordered_map<FeatureId, FeatureDef> features_;
    // Mesh nodes keyed by MeshHandle::version (a unique key per mesh).
    std::unordered_map<std::uint64_t, std::vector<Vec3>> meshNodes_;
};

}  // namespace opendva
