// A2 domain model (README §0.3, §3.1). The ordered Logic Tree is the heart:
// node order == Move execution order == assembly sequence.
#pragma once
#include <optional>
#include <string>
#include <vector>

#include "opendva/IMeasureEvaluator.h"
#include "opendva/IMoveSolver.h"
#include "opendva/IToleranceSampler.h"
#include "opendva/Types.h"

namespace opendva {

struct Point {
    PointId id{kInvalidId};
    PointKind kind{PointKind::Coordinate};
    Vec3 position{};
    Vec3 ijk{0, 0, 1};
    double diameter{0};
    HoleType holeType{HoleType::None};
    bool active{true};
};

struct Feature {
    FeatureId id{kInvalidId};
    FeatureKind kind{FeatureKind::Plane};
    std::vector<PointId> definingPoints;
    MeshHandle mesh{};
};

struct ToleranceDef {
    ToleranceId id{kInvalidId};
    std::string name;
    bool active{true};
    ToleranceIR ir{};
    std::vector<FeatureId> features;
};

struct MoveDef {
    MoveId id{kInvalidId};
    std::string name;
    bool active{true};
    MoveInputs inputs{};
    std::vector<PartId> moveParts;  // object first, target second
};

struct MeasureRecord {
    MeasureId id{kInvalidId};
    std::string name;
    MeasureDef def{};
};

// ---- GD&T (README §5.3, 19 types) ----
// 3DCS supports 19 GD&T callouts. The geometric effect is realised by A4's
// GeomRule; here we only model the catalogue entry and its DRF / feature wiring.
enum class GdtType {
    Size,                  // 1: uniform scale, no DRF
    Position,              // 2: location, DRF required
    SurfaceProfile,        // 3: location (with DRF) / form (without)
    Flatness,              // 4: form, no DRF
    Perpendicularity,      // 5: orientation, DRF required
    Angularity,            // 6: orientation, DRF required
    Parallelism,           // 7: orientation, DRF required
    Straightness,          // 8: form, no DRF
    TotalRunout,           // 9: location/form, DRF required
    CircularRunout,        // 10: location/form, DRF required
    Circularity,           // 11: form, no DRF
    Cylindricity,          // 12: form, no DRF
    Concentricity,         // 13: location, DRF required
    Symmetry,              // 14: location, DRF required
    LineProfile,           // 15: location (with DRF) / form (without)
    DimensioningLocation,  // 16: location, origin datum
    AngleSize,             // 17: location + orientation
    TorusMinorDiameterSize,// 18: minor diameter size, no DRF
    SetFeatureAverage      // 19: special feature-average function
};

// Datum Reference Frame (README §5.2). Up to three datum features in priority
// order; empty ids mean the slot is unused.
struct DatumReferenceFrame {
    FeatureId primary{kInvalidId};
    FeatureId secondary{kInvalidId};
    FeatureId tertiary{kInvalidId};
};

// A GD&T callout attached to a part (README §3.1 / §5.2).
struct GdtDef {
    GdtId id{kInvalidId};
    std::string name;
    bool active{true};
    GdtType type{GdtType::Position};
    double range{0.0};                 // tolerance zone (Zone Range)
    bool diametrical{false};           // diametrical vs non-diametrical zone
    DatumReferenceFrame drf{};         // referenced datums (unused for Form types)
    std::vector<FeatureId> features;   // controlled features
};

struct Part {
    PartId id{kInvalidId};
    std::string cadName;   // read-only (from assembly)
    std::string dcsName;   // editable
    std::vector<Point> points;
    std::vector<Feature> features;
    std::vector<ToleranceDef> tolerances;
    std::vector<GdtDef> gdts;
    // Moves & measures live at assembly level in execution order; see Model.
};

// A Model Variant (README §6.8): a named subset of MTM components (Moves /
// Tolerances / Measures) that can be activated as a scenario. `active` flags
// which variant is currently selected.
struct ModelVariant {
    std::string name;
    bool active{false};
    std::vector<MoveId> moves;
    std::vector<ToleranceId> tolerances;
    std::vector<MeasureId> measures;
};

// The model owns ordered Move / Measure lists. Order is significant.
struct Model {
    std::string assemblyName;
    std::vector<Part> parts;
    std::vector<MoveDef> moves;        // ORDERED: tree order == solve order
    std::vector<MeasureRecord> measures;
    std::vector<ModelVariant> variants;

    // Convenience lookups (linear; fine for the reference impl).
    const Part* findPart(PartId id) const;
    std::vector<const MoveDef*> activeMovesInOrder() const;
    std::vector<const ToleranceDef*> activeTolerances() const;  // child->parent order

    // README §6.8: return a copy where only the components listed by the named
    // variant stay active; everything else in that variant's component classes
    // is deactivated. An unknown variant name returns an unchanged copy.
    Model applyVariant(const std::string& variantName) const;
    std::optional<std::string> activeVariantName() const;
    Model activeVariantApplied() const;
};

// README §3.9.5: import part matching, in priority order
// Part ID -> CAD Part Name -> 3DCS (DCS) Part Name.
// Returns the first part that matches at the highest-priority available key,
// or nullptr if none match.
const Part* matchPart(const Model& model, PartId id, const std::string& cadName,
                      const std::string& dcsName);

}  // namespace opendva
