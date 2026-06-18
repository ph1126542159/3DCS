// OpenDVA core POD value types — shared by all modules.
// Header-only, no dependencies beyond the standard library.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace opendva {

// ---- Identifiers (opaque, stable across a session) ----
using PartId    = std::uint64_t;
using PointId   = std::uint64_t;
using FeatureId = std::uint64_t;
using MoveId    = std::uint64_t;
using ToleranceId = std::uint64_t;
using MeasureId = std::uint64_t;
using GdtId     = std::uint64_t;
constexpr std::uint64_t kInvalidId = 0;

// ---- Geometry primitives ----
struct Vec3 {
    double x{0}, y{0}, z{0};
};

// 3x4 rigid transform [R | t]; row-major. Identity by default.
struct Mat34 {
    std::array<std::array<double, 4>, 3> m{{{{1, 0, 0, 0}}, {{0, 1, 0, 0}}, {{0, 0, 1, 0}}}};
};

struct Plane {
    Vec3 point;   // a point on the plane
    Vec3 normal;  // unit normal
};

struct Axis {
    Vec3 point;      // a point on the axis (geometric center)
    Vec3 direction;  // unit direction
};

// ---- Feature classification ----
enum class HoleType { None, Hole, Pin };

enum class FeatureKind { Plane, Cylinder, Cone, Sphere, Edge, SlotTab, PointBased, Combined };

enum class PointKind { Coordinate, Feature, Dynamic };

// ---- Direction system (README §3.3, 6 types) ----
enum class DirectionType { TypeIn, TwoPoints, Normal, AssocDir, PickPtDir, Auto };

struct Direction {
    DirectionType type{DirectionType::TypeIn};
    Vec3 ijk{0, 0, 1};                 // auto-normalised; default (0,0,1) per README §4
    std::vector<PointId> refPoints;    // used by TwoPoints/Normal/Pick
    // refFrame: TypeIn -> LCS, others -> GCS (README §3.3)
};

// ---- Mesh handle (double-layer model, README §3.1.4) ----
struct MeshHandle {
    FeatureId feature{kInvalidId};
    std::size_t meshNodeNum{0};  // analysis-feature node count
    std::size_t cadPtNum{0};     // mesh nodes + feature points
    std::uint64_t version{0};    // bumped on remesh / geometry update
};

struct MeshParams {
    int meshDensity{4};               // README §3 default; CATIA 6 / NX 8
    double chordalHeight{0.0};        // sag; 0 = disabled
    double cylindricalThreshold{95.0};// degrees, README §3.4
};

}  // namespace opendva
