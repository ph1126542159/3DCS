#include "opendva/domain/ModelEditing.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "opendva/dcs_plugin_api.h"
#include "opendva/plugin/PluginHost.h"

namespace opendva {
namespace {

Part* findMutablePart(Model& model, PartId id) {
    if (id == kInvalidId) return model.parts.empty() ? nullptr : &model.parts.front();
    for (Part& part : model.parts) {
        if (part.id == id) return &part;
    }
    return nullptr;
}

Point* findMutablePoint(Model& model, PointId id) {
    for (Part& part : model.parts) {
        for (Point& point : part.points) {
            if (point.id == id) return &point;
        }
    }
    return nullptr;
}

const Point* findPoint(const Model& model, PointId id) {
    for (const Part& part : model.parts) {
        for (const Point& point : part.points) {
            if (point.id == id) return &point;
        }
    }
    return nullptr;
}

Feature* findMutableFeature(Model& model, FeatureId id) {
    for (Part& part : model.parts) {
        for (Feature& feature : part.features) {
            if (feature.id == id) return &feature;
        }
    }
    return nullptr;
}

const Feature* findFeature(const Model& model, FeatureId id) {
    for (const Part& part : model.parts) {
        for (const Feature& feature : part.features) {
            if (feature.id == id) return &feature;
        }
    }
    return nullptr;
}

ToleranceDef* findMutableTolerance(Model& model, ToleranceId id) {
    for (Part& part : model.parts) {
        for (ToleranceDef& tolerance : part.tolerances) {
            if (tolerance.id == id) return &tolerance;
        }
    }
    return nullptr;
}

GdtDef* findMutableGdt(Model& model, GdtId id) {
    for (Part& part : model.parts) {
        for (GdtDef& gdt : part.gdts) {
            if (gdt.id == id) return &gdt;
        }
    }
    return nullptr;
}

MeasureRecord* findMutableMeasure(Model& model, MeasureId id) {
    for (MeasureRecord& measure : model.measures) {
        if (measure.id == id) return &measure;
    }
    return nullptr;
}

ModelVariant* findMutableVariant(Model& model, const std::string& name) {
    for (ModelVariant& variant : model.variants) {
        if (variant.name == name) return &variant;
    }
    return nullptr;
}

MoveDef* findMutableMove(Model& model, MoveId id) {
    for (MoveDef& move : model.moves) {
        if (move.id == id) return &move;
    }
    return nullptr;
}

std::vector<PointId> allPointIds(const Model& model) {
    std::vector<PointId> ids;
    for (const Part& part : model.parts) {
        for (const Point& point : part.points) ids.push_back(point.id);
    }
    return ids;
}

template <typename T>
void eraseValue(std::vector<T>& values, T value) {
    values.erase(std::remove(values.begin(), values.end(), value), values.end());
}

template <typename T, typename Pred>
bool eraseIf(std::vector<T>& values, Pred pred) {
    const auto oldSize = values.size();
    values.erase(std::remove_if(values.begin(), values.end(), pred), values.end());
    return values.size() != oldSize;
}

bool containsPoint(const MeasureRecord& measure, PointId id) {
    return std::find(measure.def.inputPoints.begin(), measure.def.inputPoints.end(),
                     id) != measure.def.inputPoints.end();
}

bool directionReferencesPoint(const Direction& direction, PointId id) {
    return std::find(direction.refPoints.begin(), direction.refPoints.end(), id) !=
           direction.refPoints.end();
}

bool activeMeasureReferencesPoint(const Model& model, PointId id) {
    for (const MeasureRecord& measure : model.measures) {
        if (measure.def.active && containsPoint(measure, id)) return true;
    }
    return false;
}

bool featureRefsReferenceDefiningPoint(const Model& model,
                                       const std::vector<FeatureId>& featureIds,
                                       PointId id) {
    for (const FeatureId featureId : featureIds) {
        const Feature* feature = findFeature(model, featureId);
        if (feature &&
            std::find(feature->definingPoints.begin(),
                      feature->definingPoints.end(),
                      id) != feature->definingPoints.end()) {
            return true;
        }
    }
    return false;
}

bool featureRefsHaveActiveDefiningPoints(
    const Model& model, const std::vector<FeatureId>& featureIds) {
    for (const FeatureId featureId : featureIds) {
        if (featureId == kInvalidId) continue;
        const Feature* feature = findFeature(model, featureId);
        if (!feature || feature->definingPoints.empty()) return false;
        for (const PointId pointId : feature->definingPoints) {
            const Point* point = findPoint(model, pointId);
            if (!point || !point->active) return false;
        }
    }
    return true;
}

bool pointRefsAreActive(const Model& model,
                        const std::vector<PointId>& pointIds) {
    if (pointIds.empty()) return false;
    for (const PointId pointId : pointIds) {
        const Point* point = findPoint(model, pointId);
        if (!point || !point->active) return false;
    }
    return true;
}

bool containsFeatureRef(const std::vector<FeatureId>& featureIds,
                        FeatureId id) {
    return std::find(featureIds.begin(), featureIds.end(), id) !=
           featureIds.end();
}

bool activeMeasureReferencesFeature(const Model& model, FeatureId id) {
    for (const MeasureRecord& measure : model.measures) {
        if (!measure.def.active) continue;
        if (measure.def.type == MeasureType::Combination ||
            measure.def.type == MeasureType::Equation) {
            continue;
        }
        if (containsFeatureRef(measure.def.inputFeatures, id)) return true;
    }
    return false;
}

bool activeMeasureReferencesDefiningPoint(const Model& model, PointId id) {
    for (const MeasureRecord& measure : model.measures) {
        if (!measure.def.active) continue;
        if (measure.def.type == MeasureType::Combination ||
            measure.def.type == MeasureType::Equation) {
            continue;
        }
        if (featureRefsReferenceDefiningPoint(model, measure.def.inputFeatures,
                                              id)) {
            return true;
        }
    }
    return false;
}

bool activeToleranceReferencesDefiningPoint(const Model& model, PointId id) {
    for (const Part& part : model.parts) {
        for (const ToleranceDef& tolerance : part.tolerances) {
            if (!tolerance.active) continue;
            if (featureRefsReferenceDefiningPoint(model, tolerance.features, id)) {
                return true;
            }
        }
    }
    return false;
}

bool activeToleranceReferencesFeature(const Model& model, FeatureId id) {
    for (const Part& part : model.parts) {
        for (const ToleranceDef& tolerance : part.tolerances) {
            if (tolerance.active && containsFeatureRef(tolerance.features, id)) {
                return true;
            }
        }
    }
    return false;
}

bool activeGdtReferencesDefiningPoint(const Model& model, PointId id) {
    for (const Part& part : model.parts) {
        for (const GdtDef& gdt : part.gdts) {
            if (!gdt.active) continue;
            if (featureRefsReferenceDefiningPoint(model, gdt.features, id)) {
                return true;
            }
            const std::vector<FeatureId> datumFeatures = {
                gdt.drf.primary, gdt.drf.secondary, gdt.drf.tertiary};
            if (featureRefsReferenceDefiningPoint(model, datumFeatures, id)) {
                return true;
            }
        }
    }
    return false;
}

bool activeGdtReferencesFeature(const Model& model, FeatureId id) {
    for (const Part& part : model.parts) {
        for (const GdtDef& gdt : part.gdts) {
            if (!gdt.active) continue;
            if (containsFeatureRef(gdt.features, id)) return true;
            const std::vector<FeatureId> datumFeatures = {
                gdt.drf.primary, gdt.drf.secondary, gdt.drf.tertiary};
            if (containsFeatureRef(datumFeatures, id)) return true;
        }
    }
    return false;
}

bool activeObjectReferencesFeature(const Model& model, FeatureId id) {
    return activeMeasureReferencesFeature(model, id) ||
           activeToleranceReferencesFeature(model, id) ||
           activeGdtReferencesFeature(model, id);
}

bool activeDirectionReferencesPoint(const Model& model, PointId id) {
    for (const Part& part : model.parts) {
        for (const ToleranceDef& tolerance : part.tolerances) {
            if (tolerance.active &&
                directionReferencesPoint(tolerance.ir.direction, id)) {
                return true;
            }
        }
    }
    for (const MeasureRecord& measure : model.measures) {
        if (measure.def.active &&
            directionReferencesPoint(measure.def.direction, id)) {
            return true;
        }
    }
    for (const MoveDef& move : model.moves) {
        if (!move.active) continue;
        for (const MovePair& pair : move.inputs.pairs) {
            if (directionReferencesPoint(pair.direction, id)) return true;
        }
    }
    return false;
}

bool activeObjectReferencesPoint(const Model& model, PointId id) {
    return activeMeasureReferencesPoint(model, id) ||
           activeMeasureReferencesDefiningPoint(model, id) ||
           activeToleranceReferencesDefiningPoint(model, id) ||
           activeGdtReferencesDefiningPoint(model, id) ||
           activeDirectionReferencesPoint(model, id);
}

bool containsPartId(const Model& model, PartId id) {
    return std::find_if(model.parts.begin(), model.parts.end(),
                        [&](const Part& part) { return part.id == id; }) !=
           model.parts.end();
}

bool containsPointId(const Model& model, PointId id) {
    for (const Part& part : model.parts) {
        const auto found =
            std::find_if(part.points.begin(), part.points.end(),
                         [&](const Point& point) { return point.id == id; });
        if (found != part.points.end()) return true;
    }
    return false;
}

bool containsFeatureId(const Model& model, FeatureId id) {
    for (const Part& part : model.parts) {
        const auto found =
            std::find_if(part.features.begin(), part.features.end(),
                         [&](const Feature& feature) {
                             return feature.id == id;
                         });
        if (found != part.features.end()) return true;
    }
    return false;
}

bool containsMoveId(const Model& model, MoveId id) {
    return std::find_if(model.moves.begin(), model.moves.end(),
                        [&](const MoveDef& move) { return move.id == id; }) !=
           model.moves.end();
}

bool containsToleranceId(const Model& model, ToleranceId id) {
    for (const Part& part : model.parts) {
        const auto found =
            std::find_if(part.tolerances.begin(), part.tolerances.end(),
                         [&](const ToleranceDef& tolerance) {
                             return tolerance.id == id;
                         });
        if (found != part.tolerances.end()) return true;
    }
    return false;
}

bool containsMeasureId(const Model& model, MeasureId id) {
    return std::find_if(model.measures.begin(), model.measures.end(),
                        [&](const MeasureRecord& measure) {
                            return measure.id == id;
                        }) != model.measures.end();
}

std::size_t measureIndex(const Model& model, MeasureId id) {
    for (std::size_t i = 0; i < model.measures.size(); ++i) {
        if (model.measures[i].id == id) return i;
    }
    return model.measures.size();
}

bool measureIsActive(const Model& model, MeasureId id) {
    const auto found =
        std::find_if(model.measures.begin(), model.measures.end(),
                     [&](const MeasureRecord& measure) {
                         return measure.id == id;
                     });
    return found != model.measures.end() && found->def.active;
}

template <typename IdT, typename Exists>
bool allReferencesExist(const std::vector<IdT>& ids, Exists exists) {
    for (const IdT id : ids) {
        if (!exists(id)) return false;
    }
    return true;
}

template <typename IdT>
bool allIdsDistinct(const std::vector<IdT>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        for (std::size_t j = i + 1; j < ids.size(); ++j) {
            if (ids[i] == ids[j]) return false;
        }
    }
    return true;
}

bool variantReferencesExist(const Model& model, const ModelVariant& variant) {
    return allReferencesExist(variant.moves, [&](MoveId id) {
               return containsMoveId(model, id);
           }) &&
           allReferencesExist(variant.tolerances, [&](ToleranceId id) {
               return containsToleranceId(model, id);
           }) &&
           allReferencesExist(variant.measures, [&](MeasureId id) {
               return containsMeasureId(model, id);
           });
}

bool variantReferencesAreDistinct(const ModelVariant& variant) {
    return allIdsDistinct(variant.moves) &&
           allIdsDistinct(variant.tolerances) &&
           allIdsDistinct(variant.measures);
}

bool directionRefPointsExist(const Model& model, const Direction& direction) {
    return allReferencesExist(direction.refPoints, [&](PointId pointId) {
        return containsPointId(model, pointId);
    });
}

bool directionRefPointsActive(const Model& model, const Direction& direction) {
    return allReferencesExist(direction.refPoints, [&](PointId pointId) {
        const Point* point = findPoint(model, pointId);
        return point && point->active;
    });
}

bool directionRefPointCountMatches(const Direction& direction) {
    switch (direction.type) {
        case DirectionType::TwoPoints:
            return direction.refPoints.size() == 2;
        case DirectionType::Normal:
        case DirectionType::PickPtDir:
            return direction.refPoints.size() == 1;
        case DirectionType::TypeIn:
        case DirectionType::AssocDir:
        case DirectionType::Auto:
            return true;
    }
    return true;
}

bool directionRefPointsDistinct(const Direction& direction) {
    return direction.type != DirectionType::TwoPoints ||
           allIdsDistinct(direction.refPoints);
}

bool movePairsReferencesExist(const Model& model,
                              const std::vector<MovePair>& pairs) {
    for (const MovePair& pair : pairs) {
        if (!directionRefPointsExist(model, pair.direction)) return false;
    }
    return true;
}

bool datumReferenceExists(const Model& model, FeatureId id) {
    return id == kInvalidId || containsFeatureId(model, id);
}

bool drfReferencesExist(const Model& model, const DatumReferenceFrame& drf) {
    return datumReferenceExists(model, drf.primary) &&
           datumReferenceExists(model, drf.secondary) &&
           datumReferenceExists(model, drf.tertiary);
}

bool drfReferencesAreDistinct(const DatumReferenceFrame& drf) {
    std::vector<FeatureId> datums;
    const FeatureId ids[] = {drf.primary, drf.secondary, drf.tertiary};
    for (const FeatureId id : ids) {
        if (id != kInvalidId) datums.push_back(id);
    }
    return allIdsDistinct(datums);
}

bool drfReferencesAreContiguous(const DatumReferenceFrame& drf) {
    return !(drf.primary == kInvalidId &&
             (drf.secondary != kInvalidId || drf.tertiary != kInvalidId)) &&
           !(drf.secondary == kInvalidId && drf.tertiary != kInvalidId);
}

bool referencesAnyFeature(const std::vector<FeatureId>& refs,
                          const std::vector<FeatureId>& deletedIds) {
    for (const FeatureId ref : refs) {
        if (std::find(deletedIds.begin(), deletedIds.end(), ref) != deletedIds.end()) {
            return true;
        }
    }
    return false;
}

bool referencesAnyFeature(const DatumReferenceFrame& drf,
                          const std::vector<FeatureId>& deletedIds) {
    return std::find(deletedIds.begin(), deletedIds.end(), drf.primary) !=
               deletedIds.end() ||
           std::find(deletedIds.begin(), deletedIds.end(), drf.secondary) !=
               deletedIds.end() ||
           std::find(deletedIds.begin(), deletedIds.end(), drf.tertiary) !=
               deletedIds.end();
}

bool referencesAnyPoint(const std::vector<PointId>& refs,
                        const std::vector<PointId>& deletedIds) {
    for (const PointId ref : refs) {
        if (std::find(deletedIds.begin(), deletedIds.end(), ref) != deletedIds.end()) {
            return true;
        }
    }
    return false;
}

bool referencesAnyPoint(const Direction& direction,
                        const std::vector<PointId>& deletedIds) {
    return referencesAnyPoint(direction.refPoints, deletedIds);
}

bool referencesAnyPoint(const MoveDef& move,
                        const std::vector<PointId>& deletedIds) {
    for (const MovePair& pair : move.inputs.pairs) {
        if (referencesAnyPoint(pair.direction, deletedIds)) return true;
    }
    return false;
}

void removeVariantMoveRefs(Model& model, MoveId id) {
    for (ModelVariant& variant : model.variants) eraseValue(variant.moves, id);
}

void removeVariantToleranceRefs(Model& model, ToleranceId id) {
    for (ModelVariant& variant : model.variants) eraseValue(variant.tolerances, id);
}

void removeVariantMeasureRefs(Model& model, MeasureId id) {
    for (ModelVariant& variant : model.variants) eraseValue(variant.measures, id);
}

bool isFinite(double value) {
    return std::isfinite(value);
}

bool isNonNegativeFinite(double value) {
    return std::isfinite(value) && value >= 0.0;
}

bool isPositiveFinite(double value) {
    return std::isfinite(value) && value > 0.0;
}

bool isValidDistributionType(DistributionType distribution) {
    switch (distribution) {
        case DistributionType::Normal:
        case DistributionType::Uniform:
        case DistributionType::Triangular:
        case DistributionType::BiMode:
        case DistributionType::RightSkew:
        case DistributionType::LeftSkew:
        case DistributionType::OpenUp:
        case DistributionType::OpenDown:
        case DistributionType::UserDefined:
        case DistributionType::Step:
        case DistributionType::Constant:
        case DistributionType::Weibull4:
        case DistributionType::Pearson4:
        case DistributionType::Modal:
        case DistributionType::Trapezoid:
        case DistributionType::PowerFunction:
        case DistributionType::Normal2D:
        case DistributionType::Uniform2D:
        case DistributionType::Triangular2D:
        case DistributionType::Trapezoid2D:
            return true;
    }
    return false;
}

bool isValidGeomRule(GeomRule geomRule) {
    switch (geomRule) {
        case GeomRule::TranslateAlongVector:
        case GeomRule::RotateAboutLocatorPoint:
        case GeomRule::NodeNormalOffset:
        case GeomRule::SectionRadialOffset:
        case GeomRule::DiameterScale:
            return true;
    }
    return false;
}

bool isValidPointKind(PointKind kind) {
    switch (kind) {
        case PointKind::Coordinate:
        case PointKind::Feature:
        case PointKind::Dynamic:
            return true;
    }
    return false;
}

bool isValidHoleType(HoleType holeType) {
    switch (holeType) {
        case HoleType::None:
        case HoleType::Hole:
        case HoleType::Pin:
            return true;
    }
    return false;
}

bool isValidFeatureKind(FeatureKind kind) {
    switch (kind) {
        case FeatureKind::Plane:
        case FeatureKind::Cylinder:
        case FeatureKind::Cone:
        case FeatureKind::Sphere:
        case FeatureKind::Edge:
        case FeatureKind::SlotTab:
        case FeatureKind::PointBased:
        case FeatureKind::Combined:
            return true;
    }
    return false;
}

bool isValidGdtType(GdtType type) {
    switch (type) {
        case GdtType::Size:
        case GdtType::Position:
        case GdtType::SurfaceProfile:
        case GdtType::Flatness:
        case GdtType::Perpendicularity:
        case GdtType::Angularity:
        case GdtType::Parallelism:
        case GdtType::Straightness:
        case GdtType::TotalRunout:
        case GdtType::CircularRunout:
        case GdtType::Circularity:
        case GdtType::Cylindricity:
        case GdtType::Concentricity:
        case GdtType::Symmetry:
        case GdtType::LineProfile:
        case GdtType::DimensioningLocation:
        case GdtType::AngleSize:
        case GdtType::TorusMinorDiameterSize:
        case GdtType::SetFeatureAverage:
            return true;
    }
    return false;
}

bool isValidMoveType(MoveType type) {
    switch (type) {
        case MoveType::StepPlane:
        case MoveType::SixPlane:
        case MoveType::ThreePoint:
        case MoveType::TwoPoint:
        case MoveType::BestFit:
        case MoveType::FeatureMove:
        case MoveType::PatternRigid:
        case MoveType::PatternFit:
        case MoveType::Match:
        case MoveType::Iteration:
        case MoveType::Transform:
        case MoveType::ThermalScaling:
        case MoveType::UserDll:
        case MoveType::AutoBend:
        case MoveType::RTouch:
        case MoveType::RotateLine:
        case MoveType::Gravity:
        case MoveType::LeastSquaresAxis:
        case MoveType::CrossProduct:
        case MoveType::LinePlane:
            return true;
    }
    return false;
}

bool isValidMeasureType(MeasureType type) {
    switch (type) {
        case MeasureType::NominalPoint:
        case MeasureType::PointPoint:
        case MeasureType::PointLine:
        case MeasureType::PointPlane:
        case MeasureType::DimensionalDistance:
        case MeasureType::CircleInterference:
        case MeasureType::VirtualClearance:
        case MeasureType::CircleDiameter:
        case MeasureType::Circularity:
        case MeasureType::FeatureMeasure:
        case MeasureType::FeatureAngle:
        case MeasureType::LineNominal:
        case MeasureType::LineLine:
        case MeasureType::LinePlane:
        case MeasureType::PlaneNominal:
        case MeasureType::PlanePlane:
        case MeasureType::TwoPointList:
        case MeasureType::Combination:
        case MeasureType::Equation:
        case MeasureType::GdtPosition:
        case MeasureType::GdtSurfaceProfile:
        case MeasureType::GdtPerpendicularity:
        case MeasureType::GdtAngularity:
        case MeasureType::GdtParallelism:
        case MeasureType::GdtConcentricity:
        case MeasureType::UserDll:
            return true;
    }
    return false;
}

bool isValidSpecMode(SpecMode mode) {
    switch (mode) {
        case SpecMode::Absolute:
        case SpecMode::RelativeToNominal:
            return true;
    }
    return false;
}

bool isValidDirectionMode(DirectionMode mode) {
    switch (mode) {
        case DirectionMode::TrueDistance:
        case DirectionMode::ProjectedOnVector:
        case DirectionMode::ProjectedOnPlane:
            return true;
    }
    return false;
}

bool isFiniteVec3(const Vec3& value) {
    return isFinite(value.x) && isFinite(value.y) && isFinite(value.z);
}

Vec3 unitOrZ(const Vec3& v) {
    const double length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (!isFiniteVec3(v) || !std::isfinite(length) || length < 1e-12) {
        return {0.0, 0.0, 1.0};
    }
    return {v.x / length, v.y / length, v.z / length};
}

bool isFiniteNonzeroVec3(const Vec3& value) {
    const double length =
        std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    return isFiniteVec3(value) && std::isfinite(length) && length >= 1e-12;
}

bool equationHasOverlongLine(const std::string& equation) {
    constexpr std::size_t kMaxEquationLineLength = 400;
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        if (end - start > kMaxEquationLineLength) return true;
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

std::string trimCopy(const std::string& s) {
    std::size_t first = 0;
    while (first < s.size() && std::isspace(static_cast<unsigned char>(s[first]))) {
        ++first;
    }
    std::size_t last = s.size();
    while (last > first && std::isspace(static_cast<unsigned char>(s[last - 1]))) {
        --last;
    }
    return s.substr(first, last - first);
}

bool equationHasInvalidSetCfg(const std::string& equation) {
    constexpr const char* kPrefix = "SETCFG=";
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        bool hasSetCfgPrefix = line.size() >= 7;
        for (std::size_t i = 0; hasSetCfgPrefix && i < 7; ++i) {
            hasSetCfgPrefix =
                std::tolower(static_cast<unsigned char>(line[i])) ==
                std::tolower(static_cast<unsigned char>(kPrefix[i]));
        }
        if (hasSetCfgPrefix) {
            const std::string value = line.substr(7);
            if (line.rfind(kPrefix, 0) != 0 ||
                (value != "NUMBERDATA" && value != "LENGTHDATA" &&
                value != "ANGLEDATA" && value != "AREADATA" &&
                value != "VOLUMEDATA" && value != "FORCEDATA")) {
                return true;
            }
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool isEquationRemLine(const std::string& line) {
    return line.size() >= 3 && (line[0] == 'r' || line[0] == 'R') &&
           (line[1] == 'e' || line[1] == 'E') &&
           (line[2] == 'm' || line[2] == 'M');
}

bool equationEndsWithRemLine(const std::string& equation) {
    std::string lastContentLine;
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        if (!line.empty()) {
            lastContentLine = line;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return isEquationRemLine(lastContentLine);
}

bool equationHasMalformedVariable(const std::string& equation) {
    std::size_t openCount = 0;
    for (const char c : equation) {
        if (c == '[') {
            ++openCount;
        } else if (c == ']') {
            if (openCount == 0) return true;
            --openCount;
        }
    }
    return openCount != 0;
}

bool startsWithEquationConditional(const std::string& line) {
    std::size_t pos = 0;
    while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos]))) {
        ++pos;
    }
    std::string word;
    while (pos < line.size() &&
           (std::isalnum(static_cast<unsigned char>(line[pos])) ||
            line[pos] == '_')) {
        word.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(line[pos]))));
        ++pos;
    }
    return word == "if" || word == "if_then_else";
}

bool lineHasTopLevelComparison(const std::string& line) {
    int parenDepth = 0;
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
        } else if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
        } else if (bracketDepth == 0 && c == '(') {
            ++parenDepth;
        } else if (bracketDepth == 0 && c == ')' && parenDepth > 0) {
            --parenDepth;
        } else if (bracketDepth == 0 && parenDepth == 0) {
            if (i + 1 < line.size() &&
                ((c == '=' && line[i + 1] == '=') ||
                 (c == '<' && line[i + 1] == '=') ||
                 (c == '>' && line[i + 1] == '='))) {
                return true;
            }
            if (c == '=') return true;
            if (c == '<' || c == '>') return true;
        }
    }
    return false;
}

bool equationHasTopLevelComparison(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            !startsWithEquationConditional(line) &&
            lineHasTopLevelComparison(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool lineHasTrailingEquationOperator(const std::string& line) {
    int bracketDepth = 0;
    for (std::size_t i = line.size(); i > 0; --i) {
        const char c = line[i - 1];
        if (c == ']') {
            return false;
        }
        if (c == '[' && bracketDepth > 0) {
            --bracketDepth;
            continue;
        }
        if (bracketDepth > 0 ||
            std::isspace(static_cast<unsigned char>(c))) {
            continue;
        }
        return c == '+' || c == '-' || c == '*' || c == '/' ||
               c == '^' || c == ',';
    }
    return false;
}

bool equationHasTrailingOperator(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasTrailingEquationOperator(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool lineHasBareNegativeConstant(const std::string& line) {
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
        } else if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
        }
        if (c != '-' || bracketDepth > 0 ||
            i + 1 >= line.size() ||
            (!std::isdigit(static_cast<unsigned char>(line[i + 1])) &&
             line[i + 1] != '.')) {
            continue;
        }
        std::size_t prev = i;
        while (prev > 0 &&
               std::isspace(static_cast<unsigned char>(line[prev - 1]))) {
            --prev;
        }
        if (prev == 0) return true;
        const char before = line[prev - 1];
        if (before == '+' || before == '-' || before == '*' ||
            before == '/' || before == '^' || before == ',') {
            return true;
        }
    }
    return false;
}

bool equationHasBareNegativeConstant(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasBareNegativeConstant(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool lineHasNonFiniteNumericLiteral(const std::string& line) {
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
            continue;
        }
        if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
            continue;
        }
        if (bracketDepth > 0) continue;
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            while (i + 1 < line.size() &&
                   (std::isalnum(static_cast<unsigned char>(line[i + 1])) ||
                    line[i + 1] == '_')) {
                ++i;
            }
            continue;
        }
        if (!std::isdigit(static_cast<unsigned char>(c)) &&
            !(c == '.' && i + 1 < line.size() &&
              std::isdigit(static_cast<unsigned char>(line[i + 1])))) {
            continue;
        }

        const std::size_t start = i;
        while (i < line.size() &&
               std::isdigit(static_cast<unsigned char>(line[i]))) {
            ++i;
        }
        if (i < line.size() && line[i] == '.') {
            ++i;
            while (i < line.size() &&
                   std::isdigit(static_cast<unsigned char>(line[i]))) {
                ++i;
            }
        }
        if (i < line.size() && (line[i] == 'e' || line[i] == 'E')) {
            const std::size_t exponentStart = i;
            ++i;
            if (i < line.size() && (line[i] == '+' || line[i] == '-')) ++i;
            const std::size_t digitStart = i;
            while (i < line.size() &&
                   std::isdigit(static_cast<unsigned char>(line[i]))) {
                ++i;
            }
            if (digitStart == i) i = exponentStart;
        }
        const std::string token = line.substr(start, i - start);
        try {
            std::size_t parsed = 0;
            const double value = std::stod(token, &parsed);
            if (parsed == token.size() && !std::isfinite(value)) return true;
        } catch (...) {
            return true;
        }
        if (i == 0) break;
        --i;
    }
    return false;
}

bool equationHasNonFiniteNumericLiteral(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasNonFiniteNumericLiteral(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool parseEquationNumberAt(const std::string& line, std::size_t pos,
                           double& value, std::size_t& end) {
    if (pos >= line.size() ||
        (!std::isdigit(static_cast<unsigned char>(line[pos])) &&
         !(line[pos] == '.' && pos + 1 < line.size() &&
           std::isdigit(static_cast<unsigned char>(line[pos + 1]))))) {
        return false;
    }
    std::size_t i = pos;
    while (i < line.size() &&
           std::isdigit(static_cast<unsigned char>(line[i]))) {
        ++i;
    }
    if (i < line.size() && line[i] == '.') {
        ++i;
        while (i < line.size() &&
               std::isdigit(static_cast<unsigned char>(line[i]))) {
            ++i;
        }
    }
    if (i < line.size() && (line[i] == 'e' || line[i] == 'E')) {
        const std::size_t exponentStart = i;
        ++i;
        if (i < line.size() && (line[i] == '+' || line[i] == '-')) ++i;
        const std::size_t digitStart = i;
        while (i < line.size() &&
               std::isdigit(static_cast<unsigned char>(line[i]))) {
            ++i;
        }
        if (digitStart == i) i = exponentStart;
    }
    const std::string token = line.substr(pos, i - pos);
    try {
        std::size_t parsed = 0;
        value = std::stod(token, &parsed);
        if (parsed != token.size()) return false;
    } catch (...) {
        value = std::numeric_limits<double>::infinity();
    }
    end = i;
    return true;
}

bool parseEquationLiteralAt(const std::string& line, std::size_t pos,
                            double& value, std::size_t& end) {
    if (parseEquationNumberAt(line, pos, value, end)) return true;
    if (pos >= line.size() || line[pos] != '(') return false;
    std::size_t inner = pos + 1;
    while (inner < line.size() &&
           std::isspace(static_cast<unsigned char>(line[inner]))) {
        ++inner;
    }
    double sign = 1.0;
    if (inner < line.size() && (line[inner] == '+' || line[inner] == '-')) {
        sign = line[inner] == '-' ? -1.0 : 1.0;
        ++inner;
        while (inner < line.size() &&
               std::isspace(static_cast<unsigned char>(line[inner]))) {
            ++inner;
        }
    }
    std::size_t valueEnd = 0;
    if (!parseEquationNumberAt(line, inner, value, valueEnd)) return false;
    std::size_t close = valueEnd;
    while (close < line.size() &&
           std::isspace(static_cast<unsigned char>(line[close]))) {
        ++close;
    }
    if (close >= line.size() || line[close] != ')') return false;
    value *= sign;
    end = close + 1;
    return true;
}

bool lineHasNonFiniteNumericArithmetic(const std::string& line) {
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
            continue;
        }
        if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
            continue;
        }
        if (bracketDepth > 0) continue;
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            while (i + 1 < line.size() &&
                   (std::isalnum(static_cast<unsigned char>(line[i + 1])) ||
                    line[i + 1] == '_')) {
                ++i;
            }
            continue;
        }
        double left = 0.0;
        std::size_t leftEnd = 0;
        if (!parseEquationLiteralAt(line, i, left, leftEnd)) continue;
        std::size_t opPos = leftEnd;
        while (opPos < line.size() &&
               std::isspace(static_cast<unsigned char>(line[opPos]))) {
            ++opPos;
        }
        if (opPos >= line.size() ||
            (line[opPos] != '*' && line[opPos] != '/' &&
             line[opPos] != '+' && line[opPos] != '-')) {
            i = leftEnd == 0 ? i : leftEnd - 1;
            continue;
        }
        std::size_t rightStart = opPos + 1;
        while (rightStart < line.size() &&
               std::isspace(static_cast<unsigned char>(line[rightStart]))) {
            ++rightStart;
        }
        double right = 0.0;
        std::size_t rightEnd = 0;
        if (!parseEquationLiteralAt(line, rightStart, right, rightEnd)) {
            i = leftEnd == 0 ? i : leftEnd - 1;
            continue;
        }
        double result = 0.0;
        if (line[opPos] == '*') {
            result = left * right;
        } else if (line[opPos] == '/') {
            result = left / right;
        } else if (line[opPos] == '+') {
            result = left + right;
        } else {
            result = left - right;
        }
        if (!std::isfinite(result)) return true;
        i = rightEnd == 0 ? i : rightEnd - 1;
    }
    return false;
}

bool equationHasNonFiniteNumericArithmetic(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasNonFiniteNumericArithmetic(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool lineHasNonFiniteNumericFunctionResult(const std::string& line) {
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
            continue;
        }
        if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
            continue;
        }
        if (bracketDepth > 0 ||
            !(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
            continue;
        }
        std::string rawName;
        while (i < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[i])) ||
                line[i] == '_')) {
            rawName.push_back(line[i]);
            ++i;
        }
        std::string lowerName = rawName;
        for (char& ch : lowerName) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (lowerName != "pow" && lowerName != "exp" &&
            lowerName != "inch2mm" && lowerName != "rad2deg") {
            if (i == 0) break;
            --i;
            continue;
        }
        std::size_t argStart = i;
        while (argStart < line.size() &&
               std::isspace(static_cast<unsigned char>(line[argStart]))) {
            ++argStart;
        }
        if (argStart >= line.size() || line[argStart] != '(') {
            if (i == 0) break;
            --i;
            continue;
        }
        ++argStart;
        while (argStart < line.size() &&
               std::isspace(static_cast<unsigned char>(line[argStart]))) {
            ++argStart;
        }
        double base = 0.0;
        std::size_t baseEnd = 0;
        if (!parseEquationLiteralAt(line, argStart, base, baseEnd)) {
            if (i == 0) break;
            --i;
            continue;
        }
        while (baseEnd < line.size() &&
               std::isspace(static_cast<unsigned char>(line[baseEnd]))) {
            ++baseEnd;
        }
        if (lowerName == "exp") {
            if (baseEnd >= line.size() || line[baseEnd] != ')') {
                if (i == 0) break;
                --i;
                continue;
            }
            if (!std::isfinite(std::exp(base))) return true;
            i = baseEnd;
            continue;
        }
        if (lowerName == "inch2mm" || lowerName == "rad2deg") {
            if (baseEnd >= line.size() || line[baseEnd] != ')') {
                if (i == 0) break;
                --i;
                continue;
            }
            const double result = lowerName == "inch2mm"
                                      ? base * 25.4
                                      : base / 3.14159265358979323846 * 180.0;
            if (!std::isfinite(result)) return true;
            i = baseEnd;
            continue;
        }
        if (baseEnd >= line.size() || line[baseEnd] != ',') {
            if (i == 0) break;
            --i;
            continue;
        }
        std::size_t exponentStart = baseEnd + 1;
        while (exponentStart < line.size() &&
               std::isspace(static_cast<unsigned char>(line[exponentStart]))) {
            ++exponentStart;
        }
        double exponent = 0.0;
        std::size_t exponentEnd = 0;
        if (!parseEquationLiteralAt(line, exponentStart, exponent, exponentEnd)) {
            if (i == 0) break;
            --i;
            continue;
        }
        while (exponentEnd < line.size() &&
               std::isspace(static_cast<unsigned char>(line[exponentEnd]))) {
            ++exponentEnd;
        }
        if (exponentEnd >= line.size() || line[exponentEnd] != ')') {
            if (i == 0) break;
            --i;
            continue;
        }
        if (!std::isfinite(std::pow(base, exponent))) return true;
        i = exponentEnd;
    }
    return false;
}

bool equationHasNonFiniteNumericFunctionResult(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasNonFiniteNumericFunctionResult(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool lineHasOutOfRangeTrigLiteral(const std::string& line) {
    constexpr double kHalfPi = 1.5707963267948966;
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
            continue;
        }
        if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
            continue;
        }
        if (bracketDepth > 0 ||
            !(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
            continue;
        }
        std::string rawName;
        while (i < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[i])) ||
                line[i] == '_')) {
            rawName.push_back(line[i]);
            ++i;
        }
        std::string lowerName = rawName;
        for (char& ch : lowerName) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        const bool isInverseTrig = lowerName == "asin" ||
                                   lowerName == "arcsin" ||
                                   lowerName == "arcsine" ||
                                   lowerName == "acos" ||
                                   lowerName == "arccos" ||
                                   lowerName == "arccosine";
        if (lowerName != "sin" && lowerName != "tan" &&
            lowerName != "cos" && !isInverseTrig) {
            if (i == 0) break;
            --i;
            continue;
        }
        std::size_t argStart = i;
        while (argStart < line.size() &&
               std::isspace(static_cast<unsigned char>(line[argStart]))) {
            ++argStart;
        }
        if (argStart >= line.size() || line[argStart] != '(') {
            if (i == 0) break;
            --i;
            continue;
        }
        ++argStart;
        while (argStart < line.size() &&
               std::isspace(static_cast<unsigned char>(line[argStart]))) {
            ++argStart;
        }
        double value = 0.0;
        std::size_t argEnd = 0;
        if (!parseEquationLiteralAt(line, argStart, value, argEnd)) {
            if (i == 0) break;
            --i;
            continue;
        }
        while (argEnd < line.size() &&
               std::isspace(static_cast<unsigned char>(line[argEnd]))) {
            ++argEnd;
        }
        if (argEnd >= line.size() || line[argEnd] != ')') {
            if (i == 0) break;
            --i;
            continue;
        }
        const double magnitude = std::fabs(value);
        if (isInverseTrig) {
            if (magnitude > 1.0) return true;
        } else if (lowerName == "cos" || lowerName == "tan") {
            if (magnitude >= kHalfPi) return true;
        } else if (magnitude > kHalfPi) {
            return true;
        }
        i = argEnd;
    }
    return false;
}

bool equationHasOutOfRangeTrigLiteral(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasOutOfRangeTrigLiteral(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool lineHasInvalidDomainFunctionLiteral(const std::string& line) {
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
            continue;
        }
        if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
            continue;
        }
        if (bracketDepth > 0 ||
            !(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
            continue;
        }
        std::string rawName;
        while (i < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[i])) ||
                line[i] == '_')) {
            rawName.push_back(line[i]);
            ++i;
        }
        std::string lowerName = rawName;
        for (char& ch : lowerName) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (lowerName != "sqrt" && lowerName != "log" &&
            lowerName != "mod") {
            if (i == 0) break;
            --i;
            continue;
        }
        std::size_t argStart = i;
        while (argStart < line.size() &&
               std::isspace(static_cast<unsigned char>(line[argStart]))) {
            ++argStart;
        }
        if (argStart >= line.size() || line[argStart] != '(') {
            if (i == 0) break;
            --i;
            continue;
        }
        ++argStart;
        while (argStart < line.size() &&
               std::isspace(static_cast<unsigned char>(line[argStart]))) {
            ++argStart;
        }
        double value = 0.0;
        std::size_t argEnd = 0;
        if (!parseEquationLiteralAt(line, argStart, value, argEnd)) {
            if (i == 0) break;
            --i;
            continue;
        }
        while (argEnd < line.size() &&
               std::isspace(static_cast<unsigned char>(line[argEnd]))) {
            ++argEnd;
        }
        if (lowerName == "mod") {
            if (argEnd >= line.size() || line[argEnd] != ',') {
                if (i == 0) break;
                --i;
                continue;
            }
            std::size_t divisorStart = argEnd + 1;
            while (divisorStart < line.size() &&
                   std::isspace(static_cast<unsigned char>(line[divisorStart]))) {
                ++divisorStart;
            }
            double divisor = 0.0;
            std::size_t divisorEnd = 0;
            if (!parseEquationLiteralAt(line, divisorStart, divisor, divisorEnd)) {
                if (i == 0) break;
                --i;
                continue;
            }
            while (divisorEnd < line.size() &&
                   std::isspace(static_cast<unsigned char>(line[divisorEnd]))) {
                ++divisorEnd;
            }
            if (divisorEnd >= line.size() || line[divisorEnd] != ')') {
                if (i == 0) break;
                --i;
                continue;
            }
            if (std::fabs(divisor) < 1e-15) return true;
            i = divisorEnd;
            continue;
        }
        if (argEnd >= line.size() || line[argEnd] != ')') {
            if (i == 0) break;
            --i;
            continue;
        }
        if (lowerName == "sqrt") {
            if (value < 0.0) return true;
        } else if (value <= 0.0) {
            return true;
        }
        i = argEnd;
    }
    return false;
}

bool equationHasInvalidDomainFunctionLiteral(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasInvalidDomainFunctionLiteral(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

std::size_t requiredEquationFunctionArity(const std::string& lowerName) {
    if (lowerName == "pow" || lowerName == "mod") return 2;
    if (lowerName == "if_then_else") return 3;
    if (lowerName == "min" || lowerName == "max" ||
        lowerName == "if") {
        return 0;
    }
    if (lowerName == "sqrt" || lowerName == "sin" ||
        lowerName == "cos" || lowerName == "tan" ||
        lowerName == "arcsin" || lowerName == "arcsine" ||
        lowerName == "asin" || lowerName == "arccos" ||
        lowerName == "arccosine" || lowerName == "acos" ||
        lowerName == "arctan" || lowerName == "arctangent" ||
        lowerName == "atan" || lowerName == "abs" ||
        lowerName == "log" || lowerName == "exp" ||
        lowerName == "roundup" || lowerName == "rounddown" ||
        lowerName == "round" || lowerName == "deg2rad" ||
        lowerName == "rad2deg" || lowerName == "ang2pos" ||
        lowerName == "ang2neg" || lowerName == "mm2inch" ||
        lowerName == "inch2mm") {
        return 1;
    }
    return 0;
}

bool lineHasInvalidFunctionArity(const std::string& line) {
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
            continue;
        }
        if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
            continue;
        }
        if (bracketDepth > 0 ||
            !(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
            continue;
        }
        const std::size_t nameStart = i;
        std::string rawName;
        while (i < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[i])) ||
                line[i] == '_')) {
            rawName.push_back(line[i]);
            ++i;
        }
        std::size_t open = i;
        while (open < line.size() &&
               std::isspace(static_cast<unsigned char>(line[open]))) {
            ++open;
        }
        if (open >= line.size() || line[open] != '(') {
            i = nameStart;
            continue;
        }
        std::string lowerName = rawName;
        for (char& ch : lowerName) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        const bool isVariadicMinMax =
            lowerName == "min" || lowerName == "max";
        const std::size_t requiredArity =
            requiredEquationFunctionArity(lowerName);
        if (requiredArity == 0 && !isVariadicMinMax) {
            i = open;
            continue;
        }
        int parenDepth = 1;
        int variableDepth = 0;
        std::size_t commaCount = 0;
        bool hasArgumentToken = false;
        bool currentArgumentHasToken = false;
        bool hasEmptyArgumentSlot = false;
        std::size_t j = open + 1;
        for (; j < line.size(); ++j) {
            const char ch = line[j];
            if (ch == '[') {
                ++variableDepth;
                hasArgumentToken = true;
                currentArgumentHasToken = true;
                continue;
            }
            if (ch == ']' && variableDepth > 0) {
                --variableDepth;
                continue;
            }
            if (variableDepth > 0) continue;
            if (ch == '(') {
                ++parenDepth;
                hasArgumentToken = true;
                currentArgumentHasToken = true;
            } else if (ch == ')') {
                --parenDepth;
                if (parenDepth == 0) break;
            } else if (ch == ',' && parenDepth == 1) {
                if (!currentArgumentHasToken) hasEmptyArgumentSlot = true;
                ++commaCount;
                currentArgumentHasToken = false;
            } else if (!std::isspace(static_cast<unsigned char>(ch))) {
                hasArgumentToken = true;
                currentArgumentHasToken = true;
            }
        }
        if (j >= line.size() || parenDepth != 0) {
            i = open;
            continue;
        }
        if (hasArgumentToken && !currentArgumentHasToken) {
            hasEmptyArgumentSlot = true;
        }
        if (hasEmptyArgumentSlot) return true;
        const std::size_t actualArity = hasArgumentToken ? commaCount + 1 : 0;
        if (isVariadicMinMax) {
            if (actualArity == 0) return true;
            i = j;
            continue;
        }
        if (actualArity != requiredArity) return true;
        i = j;
    }
    return false;
}

bool equationHasInvalidFunctionArity(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasInvalidFunctionArity(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool lineHasInvalidMathOperatorCase(const std::string& line) {
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
        } else if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
        }
        if (bracketDepth > 0 ||
            !(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
            continue;
        }
        const std::size_t start = i;
        std::string rawName;
        while (i < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[i])) ||
                line[i] == '_')) {
            rawName.push_back(line[i]);
            ++i;
        }
        std::size_t afterName = i;
        while (afterName < line.size() &&
               std::isspace(static_cast<unsigned char>(line[afterName]))) {
            ++afterName;
        }
        if (afterName >= line.size() || line[afterName] != '(') {
            i = start;
            continue;
        }
        std::string lowerName = rawName;
        for (char& ch : lowerName) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        const bool conditionalName =
            lowerName == "if" || lowerName == "if_then_else";
        if (rawName != lowerName && !conditionalName &&
            rawName != "MIN" && rawName != "MAX") {
            return true;
        }
        if (i == 0) break;
        --i;
    }
    return false;
}

bool isSupportedEquationFunction(const std::string& lowerName) {
    return lowerName == "if" || lowerName == "if_then_else" ||
           lowerName == "min" || lowerName == "max" ||
           lowerName == "pow" || lowerName == "mod" ||
           lowerName == "sqrt" || lowerName == "sin" ||
           lowerName == "cos" || lowerName == "tan" ||
           lowerName == "arcsin" || lowerName == "arcsine" ||
           lowerName == "asin" || lowerName == "arccos" ||
           lowerName == "arccosine" || lowerName == "acos" ||
           lowerName == "arctan" || lowerName == "arctangent" ||
           lowerName == "atan" || lowerName == "abs" ||
           lowerName == "log" || lowerName == "exp" ||
           lowerName == "roundup" || lowerName == "rounddown" ||
           lowerName == "round" || lowerName == "deg2rad" ||
           lowerName == "rad2deg" || lowerName == "ang2pos" ||
           lowerName == "ang2neg" || lowerName == "mm2inch" ||
           lowerName == "inch2mm";
}

bool lineHasUnsupportedMathOperator(const std::string& line) {
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
        } else if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
        }
        if (bracketDepth > 0 ||
            !(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
            continue;
        }
        const std::size_t start = i;
        std::string rawName;
        while (i < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[i])) ||
                line[i] == '_')) {
            rawName.push_back(line[i]);
            ++i;
        }
        std::size_t afterName = i;
        while (afterName < line.size() &&
               std::isspace(static_cast<unsigned char>(line[afterName]))) {
            ++afterName;
        }
        if (afterName >= line.size() || line[afterName] != '(') {
            i = start;
            continue;
        }
        std::string lowerName = rawName;
        for (char& ch : lowerName) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (!isSupportedEquationFunction(lowerName)) return true;
        if (i == 0) break;
        --i;
    }
    return false;
}

bool equationHasInvalidMathOperatorCase(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasInvalidMathOperatorCase(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool equationHasUnsupportedMathOperator(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasUnsupportedMathOperator(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool lineHasNestedMultiEntryOperator(const std::string& line) {
    int bracketDepth = 0;
    int multiEntryDepth = 0;
    std::vector<bool> parenMultiEntry;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
            continue;
        }
        if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
            continue;
        }
        if (bracketDepth > 0) continue;
        if (c == ')') {
            if (!parenMultiEntry.empty()) {
                if (parenMultiEntry.back() && multiEntryDepth > 0) {
                    --multiEntryDepth;
                }
                parenMultiEntry.pop_back();
            }
            continue;
        }
        if (!(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
            if (c == '(') parenMultiEntry.push_back(false);
            continue;
        }

        std::string rawName;
        while (i < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[i])) ||
                line[i] == '_')) {
            rawName.push_back(line[i]);
            ++i;
        }
        std::size_t afterName = i;
        while (afterName < line.size() &&
               std::isspace(static_cast<unsigned char>(line[afterName]))) {
            ++afterName;
        }
        if (afterName >= line.size() || line[afterName] != '(') {
            if (i == 0) break;
            --i;
            continue;
        }
        std::string lowerName = rawName;
        for (char& ch : lowerName) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        const bool isMultiEntry = lowerName == "min" || lowerName == "max";
        if (isMultiEntry && multiEntryDepth > 0) return true;
        parenMultiEntry.push_back(isMultiEntry);
        if (isMultiEntry) ++multiEntryDepth;
        i = afterName;
    }
    return false;
}

bool equationHasNestedMultiEntryOperator(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasNestedMultiEntryOperator(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool lineHasNestedConditionalOperator(const std::string& line) {
    int bracketDepth = 0;
    int conditionalDepth = 0;
    std::vector<bool> parenConditional;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
            continue;
        }
        if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
            continue;
        }
        if (bracketDepth > 0) continue;
        if (c == ')') {
            if (!parenConditional.empty()) {
                if (parenConditional.back() && conditionalDepth > 0) {
                    --conditionalDepth;
                }
                parenConditional.pop_back();
            }
            continue;
        }
        if (!(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
            if (c == '(') parenConditional.push_back(false);
            continue;
        }

        std::string rawName;
        while (i < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[i])) ||
                line[i] == '_')) {
            rawName.push_back(line[i]);
            ++i;
        }
        std::size_t afterName = i;
        while (afterName < line.size() &&
               std::isspace(static_cast<unsigned char>(line[afterName]))) {
            ++afterName;
        }
        if (afterName >= line.size() || line[afterName] != '(') {
            if (i == 0) break;
            --i;
            continue;
        }
        std::string lowerName = rawName;
        for (char& ch : lowerName) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        const bool isConditional =
            lowerName == "if" || lowerName == "if_then_else";
        if (isConditional && conditionalDepth > 0) return true;
        parenConditional.push_back(isConditional);
        if (isConditional) ++conditionalDepth;
        i = afterName;
    }
    return false;
}

bool equationHasNestedConditionalOperator(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasNestedConditionalOperator(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool parsePositiveIndex(const std::string& text, std::size_t& out) {
    if (text.empty()) return false;
    std::size_t value = 0;
    for (const unsigned char c : text) {
        if (!std::isdigit(c)) return false;
        const std::size_t digit = static_cast<std::size_t>(c - '0');
        if (value > (std::numeric_limits<std::size_t>::max() - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
    }
    if (value == 0) return false;
    out = value;
    return true;
}

std::size_t equationMaxIndexedVariable(const std::string& equation,
                                       const std::string& variableKey) {
    std::size_t maxIndex = 0;
    std::size_t pos = 0;
    while ((pos = equation.find('[', pos)) != std::string::npos) {
        const std::size_t close = equation.find(']', pos + 1);
        if (close == std::string::npos) return maxIndex;
        const std::string token = equation.substr(pos + 1, close - pos - 1);
        const std::size_t colon = token.find(':');
        const std::string key =
            colon == std::string::npos ? token : token.substr(0, colon);
        if (key == variableKey) {
            std::size_t index = 0;
            if (colon == std::string::npos ||
                !parsePositiveIndex(token.substr(colon + 1), index)) {
                return std::numeric_limits<std::size_t>::max();
            }
            maxIndex = std::max(maxIndex, index);
        }
        pos = close + 1;
    }
    return maxIndex;
}

std::size_t equationRequiredPointVariableCount(const std::string& equation) {
    std::size_t requiredCount = 0;
    std::size_t pos = 0;
    while ((pos = equation.find('[', pos)) != std::string::npos) {
        const std::size_t close = equation.find(']', pos + 1);
        if (close == std::string::npos) return requiredCount;
        const std::string token = equation.substr(pos + 1, close - pos - 1);
        const std::size_t colon = token.find(':');
        const std::string key =
            colon == std::string::npos ? token : token.substr(0, colon);
        if (key.size() >= 3 && key[0] == 'P') {
            const char axis = key.back();
            if (axis == 'X' || axis == 'Y' || axis == 'Z' ||
                axis == 'C' || axis == 'I' || axis == 'J' || axis == 'K') {
                const std::string group = key.substr(1, key.size() - 2);
                if (group == "1" || group == "2") {
                    std::size_t requiredIndex = group == "1" ? 1u : 2u;
                    if (colon != std::string::npos) {
                        std::size_t index = 0;
                        if (!parsePositiveIndex(token.substr(colon + 1), index)) {
                            return std::numeric_limits<std::size_t>::max();
                        }
                        requiredIndex = group == "1" ? index : index + 1;
                    }
                    requiredCount = std::max(requiredCount, requiredIndex);
                }
            }
        }
        pos = close + 1;
    }
    return requiredCount;
}

bool equationHasUnknownVariable(const std::string& equation) {
    std::size_t pos = 0;
    while ((pos = equation.find('[', pos)) != std::string::npos) {
        const std::size_t close = equation.find(']', pos + 1);
        if (close == std::string::npos) return false;
        const std::string token = equation.substr(pos + 1, close - pos - 1);
        const std::size_t colon = token.find(':');
        const std::string key =
            colon == std::string::npos ? token : token.substr(0, colon);
        const bool knownScalarList =
            key == "VAL" || key == "MS" || key == "STR";
        const bool pointVariable = !key.empty() && key[0] == 'P';
        const bool directionVariable =
            key.size() == 3 && key[0] == 'D' && key[1] == 'R' &&
            (key[2] == 'I' || key[2] == 'J' || key[2] == 'K');
        if (!knownScalarList && !pointVariable && !directionVariable) return true;
        pos = close + 1;
    }
    return false;
}

bool equationHasInvalidScalarVariableIndex(const std::string& equation) {
    std::size_t pos = 0;
    while ((pos = equation.find('[', pos)) != std::string::npos) {
        const std::size_t close = equation.find(']', pos + 1);
        if (close == std::string::npos) return false;
        const std::string token = equation.substr(pos + 1, close - pos - 1);
        const std::size_t colon = token.find(':');
        const std::string key =
            colon == std::string::npos ? token : token.substr(0, colon);
        if (key == "VAL" || key == "MS" || key == "STR") {
            std::size_t index = 0;
            if (colon == std::string::npos ||
                !parsePositiveIndex(token.substr(colon + 1), index)) {
                return true;
            }
        }
        pos = close + 1;
    }
    return false;
}

bool equationHasInvalidPointVariable(const std::string& equation) {
    std::size_t pos = 0;
    while ((pos = equation.find('[', pos)) != std::string::npos) {
        const std::size_t close = equation.find(']', pos + 1);
        if (close == std::string::npos) return false;
        const std::string token = equation.substr(pos + 1, close - pos - 1);
        const std::size_t colon = token.find(':');
        const std::string key =
            colon == std::string::npos ? token : token.substr(0, colon);
        if (!key.empty() && key[0] == 'P') {
            if (key.size() < 3) return true;
            const char axis = key.back();
            const bool knownAxis = axis == 'X' || axis == 'Y' || axis == 'Z' ||
                                   axis == 'C' || axis == 'I' || axis == 'J' ||
                                   axis == 'K';
            const std::string group = key.substr(1, key.size() - 2);
            if ((group != "1" && group != "2") || !knownAxis) return true;
            if (colon != std::string::npos) {
                std::size_t index = 0;
                if (!parsePositiveIndex(token.substr(colon + 1), index)) return true;
            }
        }
        pos = close + 1;
    }
    return false;
}

bool equationHasInvalidDirectionVariable(const std::string& equation) {
    std::size_t pos = 0;
    while ((pos = equation.find('[', pos)) != std::string::npos) {
        const std::size_t close = equation.find(']', pos + 1);
        if (close == std::string::npos) return false;
        const std::string token = equation.substr(pos + 1, close - pos - 1);
        const std::size_t colon = token.find(':');
        const std::string key =
            colon == std::string::npos ? token : token.substr(0, colon);
        if (key.size() == 3 && key[0] == 'D' && key[1] == 'R' &&
            (key[2] == 'I' || key[2] == 'J' || key[2] == 'K')) {
            std::size_t index = 0;
            if (colon == std::string::npos ||
                !parsePositiveIndex(token.substr(colon + 1), index) ||
                index != 1) {
                return true;
            }
        }
        pos = close + 1;
    }
    return false;
}

bool equationHasMissingStringReference(const std::string& equation) {
    std::size_t stringValueCount = 0;
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine) {
            std::size_t pos = 0;
            while ((pos = line.find('[', pos)) != std::string::npos) {
                const std::size_t close = line.find(']', pos + 1);
                if (close == std::string::npos) break;
                const std::string token = line.substr(pos + 1, close - pos - 1);
                const std::size_t colon = token.find(':');
                const std::string key =
                    colon == std::string::npos ? token : token.substr(0, colon);
                if (key == "STR") {
                    std::size_t index = 0;
                    if (colon == std::string::npos ||
                        !parsePositiveIndex(token.substr(colon + 1), index) ||
                        index > stringValueCount) {
                        return true;
                    }
                }
                pos = close + 1;
            }
            ++stringValueCount;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool measureEquationIsEditable(const std::string& equation) {
    return !equationHasOverlongLine(equation) &&
           !equationHasInvalidSetCfg(equation) &&
           !equationEndsWithRemLine(equation) &&
           !equationHasMalformedVariable(equation) &&
           !equationHasTopLevelComparison(equation) &&
           !equationHasTrailingOperator(equation) &&
           !equationHasBareNegativeConstant(equation) &&
           !equationHasNonFiniteNumericLiteral(equation) &&
           !equationHasNonFiniteNumericArithmetic(equation) &&
           !equationHasNonFiniteNumericFunctionResult(equation) &&
           !equationHasOutOfRangeTrigLiteral(equation) &&
           !equationHasInvalidDomainFunctionLiteral(equation) &&
           !equationHasInvalidFunctionArity(equation) &&
           !equationHasInvalidMathOperatorCase(equation) &&
           !equationHasUnsupportedMathOperator(equation) &&
           !equationHasNestedMultiEntryOperator(equation) &&
           !equationHasNestedConditionalOperator(equation) &&
           !equationHasUnknownVariable(equation) &&
           !equationHasInvalidScalarVariableIndex(equation) &&
           !equationHasInvalidPointVariable(equation) &&
           !equationHasInvalidDirectionVariable(equation);
}

bool measureSpecLimitsAreOrdered(double lsl, double usl, bool lslActive,
                                 bool uslActive) {
    return !lslActive || !uslActive || lsl <= usl;
}

bool measureInputFeaturesAreMeasureRefs(MeasureType type) {
    return type == MeasureType::Combination || type == MeasureType::Equation;
}

std::size_t requiredMeasurePointCount(MeasureType type) {
    switch (type) {
        case MeasureType::NominalPoint:
        case MeasureType::CircleDiameter:
        case MeasureType::GdtPosition:
        case MeasureType::GdtSurfaceProfile:
        case MeasureType::GdtConcentricity:
            return 1;
        case MeasureType::PointPoint:
        case MeasureType::LineNominal:
        case MeasureType::TwoPointList:
        case MeasureType::DimensionalDistance:
        case MeasureType::CircleInterference:
        case MeasureType::VirtualClearance:
        case MeasureType::GdtPerpendicularity:
        case MeasureType::GdtAngularity:
        case MeasureType::GdtParallelism:
            return 2;
        case MeasureType::PointLine:
        case MeasureType::PlaneNominal:
            return 3;
        case MeasureType::PointPlane:
        case MeasureType::LineLine:
        case MeasureType::Circularity:
        case MeasureType::FeatureAngle:
            return 4;
        case MeasureType::LinePlane:
            return 5;
        case MeasureType::PlanePlane:
            return 6;
        case MeasureType::FeatureMeasure:
        case MeasureType::Combination:
        case MeasureType::Equation:
        case MeasureType::UserDll:
            return 0;
    }
    return 0;
}

bool featureAngleHasUsableFeatureInputs(const Model& model,
                                        const MeasureDef& def,
                                        MeasureType type) {
    if (type != MeasureType::FeatureAngle || def.inputFeatures.size() < 2) {
        return false;
    }
    const Feature* first = findFeature(model, def.inputFeatures[0]);
    const Feature* second = findFeature(model, def.inputFeatures[1]);
    return first != nullptr && second != nullptr &&
           first->definingPoints.size() >= 2 &&
           second->definingPoints.size() >= 2;
}

bool measureHasEnoughInputPoints(const Model& model, const MeasureDef& def,
                                 MeasureType type) {
    return def.inputPoints.size() >= requiredMeasurePointCount(type) ||
           featureAngleHasUsableFeatureInputs(model, def, type);
}

bool measureHasRequiredInputFeatures(const MeasureDef& def,
                                     MeasureType type) {
    return type != MeasureType::FeatureMeasure || !def.inputFeatures.empty();
}

bool measureHasActiveInputFeaturePoints(const Model& model,
                                        const MeasureDef& def) {
    return measureInputFeaturesAreMeasureRefs(def.type) ||
           def.inputFeatures.empty() ||
           featureRefsHaveActiveDefiningPoints(model, def.inputFeatures);
}

bool gdtHasActiveFeaturePoints(const Model& model, const GdtDef& gdt) {
    if (!featureRefsHaveActiveDefiningPoints(model, gdt.features)) return false;
    const std::vector<FeatureId> datumFeatures = {
        gdt.drf.primary, gdt.drf.secondary, gdt.drf.tertiary};
    return featureRefsHaveActiveDefiningPoints(model, datumFeatures);
}

bool measureReferencesAnyMeasure(const MeasureDef& def,
                                 const std::vector<MeasureId>& deletedIds) {
    if (!measureInputFeaturesAreMeasureRefs(def.type)) return false;
    for (const FeatureId ref : def.inputFeatures) {
        const MeasureId measureId = static_cast<MeasureId>(ref);
        if (std::find(deletedIds.begin(), deletedIds.end(), measureId) !=
            deletedIds.end()) {
            return true;
        }
    }
    return false;
}

std::vector<MeasureId> collectDependentMeasureDeletes(const Model& model,
                                                      MeasureId id) {
    std::vector<MeasureId> deletedIds{id};
    bool changed = true;
    while (changed) {
        changed = false;
        for (const MeasureRecord& measure : model.measures) {
            if (std::find(deletedIds.begin(), deletedIds.end(), measure.id) !=
                deletedIds.end()) {
                continue;
            }
            if (measureReferencesAnyMeasure(measure.def, deletedIds)) {
                deletedIds.push_back(measure.id);
                changed = true;
            }
        }
    }
    return deletedIds;
}

bool measureRefsAreRunnable(const Model& model, MeasureId currentId,
                            const std::vector<FeatureId>& refs) {
    const std::size_t currentIndex = measureIndex(model, currentId);
    if (currentIndex >= model.measures.size()) return false;
    for (const FeatureId ref : refs) {
        const MeasureId measureId = static_cast<MeasureId>(ref);
        if (measureIndex(model, measureId) >= currentIndex) return false;
        if (!measureIsActive(model, measureId)) return false;
    }
    return true;
}

bool gdtTypeRequiresDatum(GdtType type) {
    switch (type) {
        case GdtType::Position:
        case GdtType::Perpendicularity:
        case GdtType::Angularity:
        case GdtType::Parallelism:
        case GdtType::TotalRunout:
        case GdtType::CircularRunout:
        case GdtType::Concentricity:
        case GdtType::Symmetry:
            return true;
        case GdtType::Size:
        case GdtType::SurfaceProfile:
        case GdtType::Flatness:
        case GdtType::Straightness:
        case GdtType::Circularity:
        case GdtType::Cylindricity:
        case GdtType::LineProfile:
        case GdtType::DimensioningLocation:
        case GdtType::AngleSize:
        case GdtType::TorusMinorDiameterSize:
        case GdtType::SetFeatureAverage:
            return false;
    }
    return false;
}

bool gdtHasRequiredDatum(GdtType type, const DatumReferenceFrame& drf) {
    return !gdtTypeRequiresDatum(type) || drf.primary != kInvalidId;
}

std::size_t requiredMovePairCount(MoveType type) {
    switch (type) {
        case MoveType::Transform:
        case MoveType::RTouch:
        case MoveType::Gravity:
            return 1;
        case MoveType::TwoPoint:
        case MoveType::CrossProduct:
        case MoveType::LeastSquaresAxis:
            return 2;
        case MoveType::StepPlane:
        case MoveType::SixPlane:
        case MoveType::ThreePoint:
        case MoveType::BestFit:
        case MoveType::FeatureMove:
        case MoveType::PatternRigid:
        case MoveType::PatternFit:
        case MoveType::RotateLine:
        case MoveType::LinePlane:
            return 3;
        case MoveType::Match:
            return 5;
        case MoveType::ThermalScaling:
        case MoveType::Iteration:
        case MoveType::UserDll:
        case MoveType::AutoBend:
            return 0;
    }
    return 0;
}

bool moveHasEnoughPairs(MoveType type, std::size_t pairCount) {
    return pairCount >= requiredMovePairCount(type);
}

bool moveHasEnoughParts(std::size_t partCount) {
    return partCount >= 2;
}

bool moveHasValidControls(const MoveInputs& inputs) {
    if (!isPositiveFinite(inputs.searchAccuracy)) return false;
    if (inputs.maxIterations <= 0) return false;
    if (!inputs.hole_pin_float.active) return true;
    return inputs.hole_pin_float.sigmaNumber >= 1 &&
           inputs.hole_pin_float.sigmaNumber <= 8 &&
           isPositiveFinite(inputs.hole_pin_float.rangeScale) &&
           isNonNegativeFinite(inputs.hole_pin_float.angleRangeDeg) &&
           isFinite(inputs.hole_pin_float.angleOffsetDeg);
}

bool movePartsAreRunnable(const Model& model,
                          const std::vector<PartId>& parts) {
    return moveHasEnoughParts(parts.size()) && allIdsDistinct(parts) &&
           parts[0] != parts[1] &&
           allReferencesExist(parts, [&](PartId partId) {
               return containsPartId(model, partId);
           });
}

bool hasBoundUserDllMoveRoutine(const MoveInputs& inputs) {
    if (inputs.userDllRoutine.empty()) return false;
    const auto* host = plugin::PluginHost::active();
    if (host == nullptr) return false;
    for (const auto& routine : host->routines()) {
        if (routine.name == inputs.userDllRoutine &&
            routine.type == dcsCalTypeMove) {
            return true;
        }
    }
    return false;
}

bool moveRequiresUnsupportedInfrastructure(const MoveInputs& inputs) {
    return (inputs.type == MoveType::UserDll &&
            !hasBoundUserDllMoveRoutine(inputs)) ||
           inputs.type == MoveType::AutoBend ||
           (inputs.type == MoveType::Iteration && !inputs.pairs.empty());
}

bool movePairsAreRunnable(const Model& model,
                          const std::vector<MovePair>& pairs) {
    for (const MovePair& pair : pairs) {
        if (!isFiniteVec3(pair.objectPoint) ||
            !isFiniteVec3(pair.targetPoint) ||
            !isFiniteNonzeroVec3(pair.direction.ijk)) {
            return false;
        }
        if (!directionRefPointCountMatches(pair.direction) ||
            !directionRefPointsDistinct(pair.direction) ||
            !directionRefPointsExist(model, pair.direction) ||
            !directionRefPointsActive(model, pair.direction)) {
            return false;
        }
    }
    return true;
}

bool moveIsActivatable(const Model& model, const MoveDef& move) {
    return !moveRequiresUnsupportedInfrastructure(move.inputs) &&
           moveHasValidControls(move.inputs) &&
           moveHasEnoughPairs(move.inputs.type, move.inputs.pairs.size()) &&
           movePartsAreRunnable(model, move.moveParts) &&
           movePairsAreRunnable(model, move.inputs.pairs);
}

bool toleranceRandomVariablesAreRunnable(const std::vector<RandSpec>& rands) {
    if (rands.empty()) return false;
    for (const RandSpec& rand : rands) {
        if (!isNonNegativeFinite(rand.range) || !isFinite(rand.offset) ||
            !isPositiveFinite(rand.sigmaNum)) {
            return false;
        }
    }
    return true;
}

bool toleranceTruncationIsRunnable(const Truncation& truncation) {
    if (!truncation.active) return true;
    return isFinite(truncation.minTrunc) && isFinite(truncation.maxTrunc) &&
           truncation.minTrunc <= truncation.maxTrunc;
}

bool toleranceDirectionIsRunnable(const Model& model,
                                  const Direction& direction) {
    return isFiniteNonzeroVec3(direction.ijk) &&
           directionRefPointCountMatches(direction) &&
           directionRefPointsDistinct(direction) &&
           directionRefPointsExist(model, direction) &&
           directionRefPointsActive(model, direction);
}

bool toleranceFeaturesAreRunnable(const Model& model,
                                  const std::vector<FeatureId>& features) {
    return !features.empty() && allIdsDistinct(features) &&
           allReferencesExist(features, [&](FeatureId featureId) {
               return containsFeatureId(model, featureId);
           }) &&
           featureRefsHaveActiveDefiningPoints(model, features);
}

bool toleranceIsActivatable(const Model& model,
                            const ToleranceDef& tolerance) {
    return toleranceFeaturesAreRunnable(model, tolerance.features) &&
           toleranceRandomVariablesAreRunnable(tolerance.ir.rands) &&
           isPositiveFinite(tolerance.ir.rangeScale) &&
           toleranceTruncationIsRunnable(tolerance.ir.truncation) &&
           toleranceDirectionIsRunnable(model, tolerance.ir.direction);
}

bool gdtFeaturesAreRunnable(const Model& model,
                            const std::vector<FeatureId>& features) {
    return !features.empty() && allIdsDistinct(features) &&
           allReferencesExist(features, [&](FeatureId featureId) {
               return containsFeatureId(model, featureId);
           }) &&
           featureRefsHaveActiveDefiningPoints(model, features);
}

bool gdtDrfIsRunnable(const Model& model, GdtType type,
                      const DatumReferenceFrame& drf) {
    return drfReferencesAreContiguous(drf) &&
           gdtHasRequiredDatum(type, drf) &&
           drfReferencesAreDistinct(drf) &&
           drfReferencesExist(model, drf) &&
           featureRefsHaveActiveDefiningPoints(
               model, {drf.primary, drf.secondary, drf.tertiary});
}

bool gdtIsActivatable(const Model& model, const GdtDef& gdt) {
    return gdtFeaturesAreRunnable(model, gdt.features) &&
           isNonNegativeFinite(gdt.range) &&
           gdtDrfIsRunnable(model, gdt.type, gdt.drf);
}

bool measureInputPointsAreRunnable(const Model& model,
                                   const MeasureDef& def,
                                   MeasureType type) {
    if (!allIdsDistinct(def.inputPoints)) return false;
    for (const PointId pointId : def.inputPoints) {
        const Point* point = findPoint(model, pointId);
        if (!point || !point->active) return false;
    }
    return measureHasEnoughInputPoints(model, def, type);
}

bool measureInputFeaturesAreRunnable(const Model& model, MeasureId id,
                                     const MeasureDef& def,
                                     MeasureType type) {
    if (measureInputFeaturesAreMeasureRefs(type)) {
        return measureRefsAreRunnable(model, id, def.inputFeatures);
    }
    if (!allIdsDistinct(def.inputFeatures)) return false;
    if (!allReferencesExist(def.inputFeatures, [&](FeatureId featureId) {
            return containsFeatureId(model, featureId);
        })) {
        return false;
    }
    return measureHasRequiredInputFeatures(def, type) &&
           measureHasActiveInputFeaturePoints(model, def);
}

bool measureDirectionIsRunnable(const Model& model, const Direction& direction) {
    return isFiniteNonzeroVec3(direction.ijk) &&
           directionRefPointCountMatches(direction) &&
           directionRefPointsDistinct(direction) &&
           directionRefPointsExist(model, direction) &&
           directionRefPointsActive(model, direction);
}

bool measureValuesAreRunnable(const std::vector<double>& values) {
    for (const double value : values) {
        if (!isFinite(value)) return false;
    }
    return true;
}

bool measureSpecIsRunnable(const SpecLimits& spec) {
    return isFinite(spec.lsl) && isFinite(spec.usl) &&
           measureSpecLimitsAreOrdered(spec.lsl, spec.usl, spec.lslActive,
                                       spec.uslActive);
}

bool equationMeasureIsRunnable(const MeasureDef& def) {
    return measureEquationIsEditable(def.equation) &&
           equationMaxIndexedVariable(def.equation, "VAL") <= def.values.size() &&
           equationMaxIndexedVariable(def.equation, "MS") <=
               def.inputFeatures.size() &&
           equationRequiredPointVariableCount(def.equation) <=
               def.inputPoints.size() &&
           !equationHasMissingStringReference(def.equation);
}

bool hasBoundUserDllMeasureRoutine(const MeasureDef& def) {
    if (def.equation.empty()) return false;
    const auto* host = plugin::PluginHost::active();
    if (host == nullptr) return false;
    for (const auto& routine : host->routines()) {
        if (routine.name == def.equation && routine.type == dcsCalTypeMeas) {
            return true;
        }
    }
    return false;
}

bool measureIsActivatable(const Model& model, MeasureId id,
                          const MeasureDef& def) {
    return (def.type != MeasureType::UserDll ||
            hasBoundUserDllMeasureRoutine(def)) &&
           measureInputPointsAreRunnable(model, def, def.type) &&
           measureInputFeaturesAreRunnable(model, id, def, def.type) &&
           measureDirectionIsRunnable(model, def.direction) &&
           isFinite(def.scale) && def.scale != 0.0 &&
           measureValuesAreRunnable(def.values) &&
           measureSpecIsRunnable(def.spec) &&
           (def.type != MeasureType::Equation ||
            equationMeasureIsRunnable(def));
}

bool activeDependentsAreRunnable(const Model& model) {
    for (const Part& part : model.parts) {
        for (const ToleranceDef& tolerance : part.tolerances) {
            if (tolerance.active &&
                !toleranceIsActivatable(model, tolerance)) {
                return false;
            }
        }
        for (const GdtDef& gdt : part.gdts) {
            if (gdt.active && !gdtIsActivatable(model, gdt)) {
                return false;
            }
        }
    }
    for (const MeasureRecord& measure : model.measures) {
        if (measure.def.active &&
            !measureIsActivatable(model, measure.id, measure.def)) {
            return false;
        }
    }
    for (const MoveDef& move : model.moves) {
        if (move.active && !moveIsActivatable(model, move)) {
            return false;
        }
    }
    return true;
}

bool variantActiveStateIsRunnable(const Model& model) {
    for (const Part& part : model.parts) {
        for (const ToleranceDef& tolerance : part.tolerances) {
            if (tolerance.active &&
                !toleranceIsActivatable(model, tolerance)) {
                return false;
            }
        }
        for (const GdtDef& gdt : part.gdts) {
            if (gdt.active && !gdtIsActivatable(model, gdt)) {
                return false;
            }
        }
    }
    for (const MeasureRecord& measure : model.measures) {
        if (measure.def.active &&
            !measureIsActivatable(model, measure.id, measure.def)) {
            return false;
        }
    }
    for (const MoveDef& move : model.moves) {
        if (move.active && !moveIsActivatable(model, move)) {
            return false;
        }
    }
    return true;
}

bool activeVariantApplyStateIsRunnable(const Model& model,
                                       const std::string& name) {
    return variantActiveStateIsRunnable(model.applyVariant(name));
}

bool pointPositionEditKeepsActiveDependentsRunnable(const Model& model,
                                                    PointId id,
                                                    const Vec3& position) {
    Model candidate = model;
    Point* point = findMutablePoint(candidate, id);
    if (!point) return false;
    point->position = position;
    return activeDependentsAreRunnable(candidate);
}

bool pointDirectionEditKeepsActiveDependentsRunnable(const Model& model,
                                                     PointId id,
                                                     const Vec3& direction) {
    Model candidate = model;
    Point* point = findMutablePoint(candidate, id);
    if (!point) return false;
    point->ijk = direction;
    return activeDependentsAreRunnable(candidate);
}

bool pointDiameterEditKeepsActiveDependentsRunnable(const Model& model,
                                                    PointId id,
                                                    double diameter) {
    Model candidate = model;
    Point* point = findMutablePoint(candidate, id);
    if (!point) return false;
    point->diameter = diameter;
    return activeDependentsAreRunnable(candidate);
}

bool featureDefiningPointEditKeepsActiveDependentsRunnable(
    const Model& model, FeatureId id, const std::vector<PointId>& points) {
    Model candidate = model;
    Feature* feature = findMutableFeature(candidate, id);
    if (!feature) return false;
    feature->definingPoints = points;
    return activeDependentsAreRunnable(candidate);
}

bool featureKindEditKeepsActiveDependentsRunnable(const Model& model,
                                                  FeatureId id,
                                                  FeatureKind kind) {
    Model candidate = model;
    Feature* feature = findMutableFeature(candidate, id);
    if (!feature) return false;
    feature->kind = kind;
    return activeDependentsAreRunnable(candidate);
}

}  // namespace

PartId nextPartId(const Model& model) {
    PartId id = 1;
    for (const Part& part : model.parts) id = std::max(id, part.id + 1);
    return id;
}

PointId nextPointId(const Model& model) {
    PointId id = 101;
    for (const Part& part : model.parts) {
        for (const Point& point : part.points) id = std::max(id, point.id + 1);
    }
    return id;
}

FeatureId nextFeatureId(const Model& model) {
    FeatureId id = 201;
    for (const Part& part : model.parts) {
        for (const Feature& feature : part.features) id = std::max(id, feature.id + 1);
    }
    return id;
}

ToleranceId nextToleranceId(const Model& model) {
    ToleranceId id = 301;
    for (const Part& part : model.parts) {
        for (const ToleranceDef& tolerance : part.tolerances) {
            id = std::max(id, tolerance.id + 1);
        }
    }
    return id;
}

GdtId nextGdtId(const Model& model) {
    GdtId id = 501;
    for (const Part& part : model.parts) {
        for (const GdtDef& gdt : part.gdts) {
            id = std::max(id, gdt.id + 1);
        }
    }
    return id;
}

MeasureId nextMeasureId(const Model& model) {
    MeasureId id = 401;
    for (const MeasureRecord& measure : model.measures) {
        id = std::max(id, measure.id + 1);
    }
    return id;
}

MoveId nextMoveId(const Model& model) {
    MoveId id = 601;
    for (const MoveDef& move : model.moves) {
        id = std::max(id, move.id + 1);
    }
    return id;
}

Model createStarterModel() {
    Model model;
    model.assemblyName = "UntitledModel";
    const PartId partId = addPart(model, "Block");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    addLinearTolerance(model, 1.0, partId);
    addPointPointMeasure(model, 8.5, 11.5);
    return model;
}

Part& ensureEditablePart(Model& model, const std::string& defaultName) {
    if (model.parts.empty()) {
        Part part;
        part.id = nextPartId(model);
        part.cadName = defaultName;
        part.dcsName = defaultName;
        model.parts.push_back(std::move(part));
    }
    return model.parts.front();
}

PartId owningPartOfPoint(const Model& model, PointId id) {
    for (const Part& part : model.parts) {
        for (const Point& point : part.points) {
            if (point.id == id) return part.id;
        }
    }
    return kInvalidId;
}

PartId owningPartOfFeature(const Model& model, FeatureId id) {
    for (const Part& part : model.parts) {
        for (const Feature& feature : part.features) {
            if (feature.id == id) return part.id;
        }
    }
    return kInvalidId;
}

PartId owningPartOfTolerance(const Model& model, ToleranceId id) {
    for (const Part& part : model.parts) {
        for (const ToleranceDef& tolerance : part.tolerances) {
            if (tolerance.id == id) return part.id;
        }
    }
    return kInvalidId;
}

PartId owningPartOfGdt(const Model& model, GdtId id) {
    for (const Part& part : model.parts) {
        for (const GdtDef& gdt : part.gdts) {
            if (gdt.id == id) return part.id;
        }
    }
    return kInvalidId;
}

PartId addPart(Model& model, const std::string& name) {
    Part part;
    part.id = nextPartId(model);
    part.cadName = name;
    part.dcsName = name;
    const PartId id = part.id;
    model.parts.push_back(std::move(part));
    return id;
}

PointId addCoordinatePoint(Model& model, const Vec3& position, PartId partId) {
    if (!isFiniteVec3(position)) return kInvalidId;

    Part* part = findMutablePart(model, partId);
    if (!part) part = &ensureEditablePart(model);

    Point point;
    point.id = nextPointId(model);
    point.kind = PointKind::Coordinate;
    point.position = position;
    point.ijk = {0, 0, 1};
    const PointId pointId = point.id;
    part->points.push_back(point);

    Feature feature;
    feature.id = nextFeatureId(model);
    feature.kind = FeatureKind::PointBased;
    feature.definingPoints = {pointId};
    part->features.push_back(std::move(feature));
    return pointId;
}

FeatureId addFeature(Model& model, FeatureKind kind, PartId partId) {
    Part* part = findMutablePart(model, partId);
    if (!part || part->points.empty()) return kInvalidId;

    Feature feature;
    feature.id = nextFeatureId(model);
    feature.kind = kind;
    feature.definingPoints = {part->points.back().id};

    const FeatureId id = feature.id;
    part->features.push_back(std::move(feature));
    return id;
}

ToleranceId addLinearTolerance(Model& model, double range, PartId partId) {
    if (!isNonNegativeFinite(range)) return kInvalidId;

    Part* part = findMutablePart(model, partId);
    if (!part || part->points.empty()) return kInvalidId;

    if (part->features.empty()) {
        Feature feature;
        feature.id = nextFeatureId(model);
        feature.kind = FeatureKind::PointBased;
        feature.definingPoints = {part->points.back().id};
        part->features.push_back(std::move(feature));
    }

    ToleranceDef tolerance;
    tolerance.id = nextToleranceId(model);
    tolerance.name = "LinearTol_" + std::to_string(tolerance.id);
    tolerance.active = true;
    tolerance.features = {part->features.back().id};
    tolerance.ir.geomRule = GeomRule::TranslateAlongVector;
    tolerance.ir.direction.type = DirectionType::TypeIn;
    tolerance.ir.direction.ijk = {0, 0, 1};

    RandSpec rand;
    rand.distribution = DistributionType::Normal;
    rand.range = range;
    rand.offset = 0.0;
    rand.sigmaNum = 3.0;
    tolerance.ir.rands = {rand};

    const ToleranceId id = tolerance.id;
    part->tolerances.push_back(std::move(tolerance));
    return id;
}

GdtId addGdt(Model& model, GdtType type, double range, bool diametrical,
             PartId partId) {
    if (!isNonNegativeFinite(range)) return kInvalidId;

    Part* part = findMutablePart(model, partId);
    if (!part || part->features.empty()) return kInvalidId;

    GdtDef gdt;
    gdt.id = nextGdtId(model);
    gdt.name = "GDT_" + std::to_string(gdt.id);
    gdt.active = true;
    gdt.type = type;
    gdt.range = range;
    gdt.diametrical = diametrical;
    gdt.features = {part->features.back().id};

    const GdtId id = gdt.id;
    part->gdts.push_back(std::move(gdt));
    return id;
}

MeasureId addPointPointMeasure(Model& model, double lsl, double usl) {
    if (!isFinite(lsl) || !isFinite(usl)) return kInvalidId;

    const std::vector<PointId> points = allPointIds(model);
    if (points.size() < 2) return kInvalidId;

    MeasureRecord measure;
    measure.id = nextMeasureId(model);
    measure.name = "PointPoint_" + std::to_string(measure.id);
    measure.def.type = MeasureType::PointPoint;
    measure.def.inputPoints = {points[0], points[1]};
    measure.def.direction.type = DirectionType::TypeIn;
    measure.def.direction.ijk = {0, 0, 1};
    measure.def.dirMode = DirectionMode::ProjectedOnVector;
    measure.def.spec.lsl = lsl;
    measure.def.spec.usl = usl;
    measure.def.spec.lslActive = true;
    measure.def.spec.uslActive = true;

    const MeasureId id = measure.id;
    model.measures.push_back(std::move(measure));
    return id;
}

MoveId addTransformMove(Model& model, const Vec3& translation,
                        PartId objectPartId, PartId targetPartId) {
    if (!isFiniteVec3(translation)) return kInvalidId;

    if (model.parts.size() < 2) return kInvalidId;

    PartId objectId = objectPartId;
    PartId targetId = targetPartId;
    if (objectId == kInvalidId) objectId = model.parts[0].id;
    if (targetId == kInvalidId) targetId = model.parts[1].id;
    if (objectId == targetId || findMutablePart(model, objectId) == nullptr ||
        findMutablePart(model, targetId) == nullptr) {
        return kInvalidId;
    }

    MoveDef move;
    move.id = nextMoveId(model);
    move.name = "Transform_" + std::to_string(move.id);
    move.active = true;
    move.moveParts = {objectId, targetId};
    move.inputs.type = MoveType::Transform;

    MovePair pair;
    pair.objectPoint = {0.0, 0.0, 0.0};
    pair.targetPoint = translation;
    pair.direction.type = DirectionType::TypeIn;
    pair.direction.ijk = unitOrZ(translation);
    move.inputs.pairs = {pair};

    const MoveId id = move.id;
    model.moves.push_back(std::move(move));
    return id;
}

bool renameAssembly(Model& model, const std::string& name) {
    model.assemblyName = name;
    return true;
}

bool renamePart(Model& model, PartId id, const std::string& dcsName) {
    Part* part = findMutablePart(model, id);
    if (!part) return false;
    part->dcsName = dcsName;
    return true;
}

bool setPointPosition(Model& model, PointId id, const Vec3& position) {
    Point* point = findMutablePoint(model, id);
    if (!point || !isFiniteVec3(position)) return false;
    if (activeObjectReferencesPoint(model, id) &&
        !pointPositionEditKeepsActiveDependentsRunnable(model, id, position)) {
        return false;
    }
    point->position = position;
    return true;
}

bool setPointActive(Model& model, PointId id, bool active) {
    Point* point = findMutablePoint(model, id);
    if (!point) return false;
    if (!active && point->active &&
        (activeMeasureReferencesPoint(model, id) ||
         activeMeasureReferencesDefiningPoint(model, id) ||
         activeToleranceReferencesDefiningPoint(model, id) ||
         activeGdtReferencesDefiningPoint(model, id) ||
         activeDirectionReferencesPoint(model, id))) {
        return false;
    }
    point->active = active;
    return true;
}

bool setPointKind(Model& model, PointId id, PointKind kind) {
    Point* point = findMutablePoint(model, id);
    if (!point || !isValidPointKind(kind)) return false;
    point->kind = kind;
    return true;
}

bool setPointHoleType(Model& model, PointId id, HoleType holeType) {
    Point* point = findMutablePoint(model, id);
    if (!point || !isValidHoleType(holeType)) return false;
    point->holeType = holeType;
    return true;
}

bool setPointDiameter(Model& model, PointId id, double diameter) {
    Point* point = findMutablePoint(model, id);
    if (!point || !isNonNegativeFinite(diameter)) return false;
    if (activeObjectReferencesPoint(model, id) &&
        !pointDiameterEditKeepsActiveDependentsRunnable(model, id, diameter)) {
        return false;
    }
    point->diameter = diameter;
    return true;
}

bool setPointDirection(Model& model, PointId id, const Vec3& direction) {
    Point* point = findMutablePoint(model, id);
    const double length = std::sqrt(direction.x * direction.x +
                                    direction.y * direction.y +
                                    direction.z * direction.z);
    if (!point || !isFiniteVec3(direction) || !std::isfinite(length) ||
        length < 1e-12) {
        return false;
    }
    const Vec3 unitDirection = {direction.x / length, direction.y / length,
                                direction.z / length};
    if (activeObjectReferencesPoint(model, id) &&
        !pointDirectionEditKeepsActiveDependentsRunnable(model, id,
                                                         unitDirection)) {
        return false;
    }
    point->ijk = unitDirection;
    return true;
}

bool setFeatureKind(Model& model, FeatureId id, FeatureKind kind) {
    Feature* feature = findMutableFeature(model, id);
    if (!feature || !isValidFeatureKind(kind)) return false;
    if (activeObjectReferencesFeature(model, id) &&
        !featureKindEditKeepsActiveDependentsRunnable(model, id, kind)) {
        return false;
    }
    feature->kind = kind;
    return true;
}

bool setFeatureDefiningPoints(Model& model, FeatureId id,
                              const std::vector<PointId>& points) {
    Feature* feature = findMutableFeature(model, id);
    if (!feature) return false;
    if (!allIdsDistinct(points)) return false;
    if (!allReferencesExist(points, [&](PointId pointId) {
            return containsPointId(model, pointId);
        })) {
        return false;
    }
    if (activeObjectReferencesFeature(model, id) &&
        !pointRefsAreActive(model, points)) {
        return false;
    }
    if (activeObjectReferencesFeature(model, id) &&
        !featureDefiningPointEditKeepsActiveDependentsRunnable(model, id,
                                                               points)) {
        return false;
    }
    feature->definingPoints = points;
    return true;
}

bool renameTolerance(Model& model, ToleranceId id, const std::string& name) {
    ToleranceDef* tolerance = findMutableTolerance(model, id);
    if (!tolerance) return false;
    if (tolerance->active && !toleranceIsActivatable(model, *tolerance)) {
        return false;
    }
    tolerance->name = name;
    return true;
}

bool setToleranceActive(Model& model, ToleranceId id, bool active) {
    ToleranceDef* tolerance = findMutableTolerance(model, id);
    if (!tolerance) return false;
    if (active && !toleranceIsActivatable(model, *tolerance)) return false;
    tolerance->active = active;
    return true;
}

bool setToleranceDistribution(Model& model, ToleranceId id,
                              DistributionType distribution) {
    ToleranceDef* tolerance = findMutableTolerance(model, id);
    if (!tolerance || tolerance->ir.rands.empty() ||
        !isValidDistributionType(distribution)) {
        return false;
    }
    if (tolerance->active) {
        ToleranceDef candidate = *tolerance;
        candidate.ir.rands.front().distribution = distribution;
        if (!toleranceIsActivatable(model, candidate)) return false;
    }
    tolerance->ir.rands.front().distribution = distribution;
    return true;
}

bool setToleranceRange(Model& model, ToleranceId id, double range) {
    ToleranceDef* tolerance = findMutableTolerance(model, id);
    if (!tolerance || tolerance->ir.rands.empty() ||
        !isNonNegativeFinite(range)) {
        return false;
    }
    if (tolerance->active) {
        ToleranceDef candidate = *tolerance;
        candidate.ir.rands.front().range = range;
        if (!toleranceIsActivatable(model, candidate)) return false;
    }
    tolerance->ir.rands.front().range = range;
    return true;
}

bool setToleranceOffset(Model& model, ToleranceId id, double offset) {
    ToleranceDef* tolerance = findMutableTolerance(model, id);
    if (!tolerance || tolerance->ir.rands.empty() || !isFinite(offset)) return false;
    if (tolerance->active) {
        ToleranceDef candidate = *tolerance;
        candidate.ir.rands.front().offset = offset;
        if (!toleranceIsActivatable(model, candidate)) return false;
    }
    tolerance->ir.rands.front().offset = offset;
    return true;
}

bool setToleranceSigmaNumber(Model& model, ToleranceId id, double sigmaNumber) {
    ToleranceDef* tolerance = findMutableTolerance(model, id);
    if (!tolerance || tolerance->ir.rands.empty() ||
        !isPositiveFinite(sigmaNumber)) {
        return false;
    }
    if (tolerance->active) {
        ToleranceDef candidate = *tolerance;
        candidate.ir.rands.front().sigmaNum = sigmaNumber;
        if (!toleranceIsActivatable(model, candidate)) return false;
    }
    tolerance->ir.rands.front().sigmaNum = sigmaNumber;
    return true;
}

bool setToleranceUserDefinedSamplePath(Model& model, ToleranceId id,
                                       const std::string& path) {
    ToleranceDef* tolerance = findMutableTolerance(model, id);
    if (!tolerance || tolerance->ir.rands.empty()) return false;
    if (tolerance->active) {
        ToleranceDef candidate = *tolerance;
        candidate.ir.rands.front().userDefinedSamplePath = path;
        if (!toleranceIsActivatable(model, candidate)) return false;
    }
    tolerance->ir.rands.front().userDefinedSamplePath = path;
    return true;
}

bool setToleranceRandomVariables(Model& model, ToleranceId id,
                                 const std::vector<RandSpec>& rands) {
    ToleranceDef* tolerance = findMutableTolerance(model, id);
    if (!tolerance) return false;
    for (const RandSpec& rand : rands) {
        if (!isNonNegativeFinite(rand.range) || !isFinite(rand.offset) ||
            !isPositiveFinite(rand.sigmaNum)) {
            return false;
        }
    }
    if (tolerance->active) {
        ToleranceDef candidate = *tolerance;
        candidate.ir.rands = rands;
        if (!toleranceIsActivatable(model, candidate)) return false;
    }
    tolerance->ir.rands = rands;
    return true;
}

bool setToleranceGeomRule(Model& model, ToleranceId id, GeomRule geomRule) {
    ToleranceDef* tolerance = findMutableTolerance(model, id);
    if (!tolerance || !isValidGeomRule(geomRule)) return false;
    if (tolerance->active) {
        ToleranceDef candidate = *tolerance;
        candidate.ir.geomRule = geomRule;
        if (!toleranceIsActivatable(model, candidate)) return false;
    }
    tolerance->ir.geomRule = geomRule;
    return true;
}

bool setToleranceRangeScale(Model& model, ToleranceId id, double rangeScale) {
    ToleranceDef* tolerance = findMutableTolerance(model, id);
    if (!tolerance || !isPositiveFinite(rangeScale)) return false;
    if (tolerance->active) {
        ToleranceDef candidate = *tolerance;
        candidate.ir.rangeScale = rangeScale;
        if (!toleranceIsActivatable(model, candidate)) return false;
    }
    tolerance->ir.rangeScale = rangeScale;
    return true;
}

bool setToleranceDirection(Model& model, ToleranceId id, const Vec3& direction) {
    ToleranceDef* tolerance = findMutableTolerance(model, id);
    if (!tolerance || !isFiniteNonzeroVec3(direction)) return false;
    const Vec3 unitDirection = unitOrZ(direction);
    if (tolerance->active) {
        ToleranceDef candidate = *tolerance;
        candidate.ir.direction.type = DirectionType::TypeIn;
        candidate.ir.direction.ijk = unitDirection;
        candidate.ir.direction.refPoints.clear();
        if (!toleranceIsActivatable(model, candidate)) return false;
    }
    tolerance->ir.direction.type = DirectionType::TypeIn;
    tolerance->ir.direction.ijk = unitDirection;
    tolerance->ir.direction.refPoints.clear();
    return true;
}

bool setToleranceTruncation(Model& model, ToleranceId id, double minTrunc,
                            double maxTrunc, bool active) {
    ToleranceDef* tolerance = findMutableTolerance(model, id);
    if (!tolerance || !isFinite(minTrunc) || !isFinite(maxTrunc)) return false;
    if (active && minTrunc > maxTrunc) return false;
    if (tolerance->active) {
        ToleranceDef candidate = *tolerance;
        candidate.ir.truncation.minTrunc = minTrunc;
        candidate.ir.truncation.maxTrunc = maxTrunc;
        candidate.ir.truncation.active = active;
        if (!toleranceIsActivatable(model, candidate)) return false;
    }
    tolerance->ir.truncation.minTrunc = minTrunc;
    tolerance->ir.truncation.maxTrunc = maxTrunc;
    tolerance->ir.truncation.active = active;
    return true;
}

bool setToleranceFeatures(Model& model, ToleranceId id,
                          const std::vector<FeatureId>& features) {
    ToleranceDef* tolerance = findMutableTolerance(model, id);
    if (!tolerance) return false;
    if (!allIdsDistinct(features)) return false;
    if (!allReferencesExist(features, [&](FeatureId featureId) {
            return containsFeatureId(model, featureId);
        })) {
        return false;
    }
    if (tolerance->active) {
        ToleranceDef candidate = *tolerance;
        candidate.features = features;
        if (!toleranceIsActivatable(model, candidate)) return false;
    }
    tolerance->features = features;
    return true;
}

bool renameGdt(Model& model, GdtId id, const std::string& name) {
    GdtDef* gdt = findMutableGdt(model, id);
    if (!gdt) return false;
    if (gdt->active && !gdtIsActivatable(model, *gdt)) {
        return false;
    }
    gdt->name = name;
    return true;
}

bool setGdtType(Model& model, GdtId id, GdtType type) {
    GdtDef* gdt = findMutableGdt(model, id);
    if (!gdt || !isValidGdtType(type)) return false;
    if (gdt->active) {
        GdtDef candidate = *gdt;
        candidate.type = type;
        if (!gdtIsActivatable(model, candidate)) return false;
    }
    gdt->type = type;
    return true;
}

bool setGdtActive(Model& model, GdtId id, bool active) {
    GdtDef* gdt = findMutableGdt(model, id);
    if (!gdt) return false;
    if (active && !gdtIsActivatable(model, *gdt)) return false;
    gdt->active = active;
    return true;
}

bool setGdtRange(Model& model, GdtId id, double range) {
    GdtDef* gdt = findMutableGdt(model, id);
    if (!gdt || !isNonNegativeFinite(range)) return false;
    if (gdt->active) {
        GdtDef candidate = *gdt;
        candidate.range = range;
        if (!gdtIsActivatable(model, candidate)) return false;
    }
    gdt->range = range;
    return true;
}

bool setGdtDiametrical(Model& model, GdtId id, bool diametrical) {
    GdtDef* gdt = findMutableGdt(model, id);
    if (!gdt) return false;
    if (gdt->active) {
        GdtDef candidate = *gdt;
        candidate.diametrical = diametrical;
        if (!gdtIsActivatable(model, candidate)) return false;
    }
    gdt->diametrical = diametrical;
    return true;
}

bool setGdtDrf(Model& model, GdtId id, const DatumReferenceFrame& drf) {
    GdtDef* gdt = findMutableGdt(model, id);
    if (!gdt) return false;
    if (!drfReferencesExist(model, drf)) return false;
    if (!drfReferencesAreContiguous(drf)) return false;
    if (!drfReferencesAreDistinct(drf)) return false;
    if (gdt->active) {
        GdtDef candidate = *gdt;
        candidate.drf = drf;
        if (!gdtIsActivatable(model, candidate)) return false;
    }
    gdt->drf = drf;
    return true;
}

bool setGdtFeatures(Model& model, GdtId id,
                    const std::vector<FeatureId>& features) {
    GdtDef* gdt = findMutableGdt(model, id);
    if (!gdt) return false;
    if (!allIdsDistinct(features)) return false;
    if (!allReferencesExist(features, [&](FeatureId featureId) {
            return containsFeatureId(model, featureId);
        })) {
        return false;
    }
    if (gdt->active) {
        GdtDef candidate = *gdt;
        candidate.features = features;
        if (!gdtIsActivatable(model, candidate)) return false;
    }
    gdt->features = features;
    return true;
}

bool renameMove(Model& model, MoveId id, const std::string& name) {
    MoveDef* move = findMutableMove(model, id);
    if (!move) return false;
    if (move->active && !moveIsActivatable(model, *move)) {
        return false;
    }
    move->name = name;
    return true;
}

bool setMoveType(Model& model, MoveId id, MoveType type) {
    MoveDef* move = findMutableMove(model, id);
    if (!move || !isValidMoveType(type)) return false;
    if (move->active) {
        MoveDef candidate = *move;
        candidate.inputs.type = type;
        if (!moveIsActivatable(model, candidate)) return false;
    }
    move->inputs.type = type;
    return true;
}

bool setMoveActive(Model& model, MoveId id, bool active) {
    MoveDef* move = findMutableMove(model, id);
    if (!move) return false;
    if (active && !moveIsActivatable(model, *move)) return false;
    move->active = active;
    return true;
}

bool setMoveNominalBuild(Model& model, MoveId id, bool nominalBuild) {
    MoveDef* move = findMutableMove(model, id);
    if (!move) return false;
    if (move->active) {
        MoveDef candidate = *move;
        candidate.inputs.isNominalBuild = nominalBuild;
        if (!moveIsActivatable(model, candidate)) return false;
    }
    move->inputs.isNominalBuild = nominalBuild;
    return true;
}

bool setTransformMoveTranslation(Model& model, MoveId id, const Vec3& translation) {
    MoveDef* move = findMutableMove(model, id);
    if (!move || move->inputs.type != MoveType::Transform ||
        move->inputs.pairs.empty() || !isFiniteVec3(translation)) {
        return false;
    }
    if (move->active) {
        MoveDef candidate = *move;
        MovePair& candidatePair = candidate.inputs.pairs.front();
        candidatePair.objectPoint = {0.0, 0.0, 0.0};
        candidatePair.targetPoint = translation;
        candidatePair.direction.type = DirectionType::TypeIn;
        candidatePair.direction.ijk = unitOrZ(translation);
        candidatePair.direction.refPoints.clear();
        if (!moveIsActivatable(model, candidate)) return false;
    }

    MovePair& pair = move->inputs.pairs.front();
    pair.objectPoint = {0.0, 0.0, 0.0};
    pair.targetPoint = translation;
    pair.direction.type = DirectionType::TypeIn;
    pair.direction.ijk = unitOrZ(translation);
    pair.direction.refPoints.clear();
    return true;
}

bool setMovePairs(Model& model, MoveId id, const std::vector<MovePair>& pairs) {
    MoveDef* move = findMutableMove(model, id);
    if (!move) return false;
    if (move->active && !moveHasEnoughPairs(move->inputs.type, pairs.size())) {
        return false;
    }
    if (!movePairsReferencesExist(model, pairs)) return false;
    for (const MovePair& pair : pairs) {
        if (!isFiniteVec3(pair.objectPoint) || !isFiniteVec3(pair.targetPoint) ||
            !isFiniteNonzeroVec3(pair.direction.ijk)) {
            return false;
        }
    }
    if (move->active) {
        MoveDef candidate = *move;
        candidate.inputs.pairs = pairs;
        if (!moveIsActivatable(model, candidate)) return false;
    }

    move->inputs.pairs = pairs;
    for (MovePair& pair : move->inputs.pairs) {
        pair.direction.type = DirectionType::TypeIn;
        pair.direction.ijk = unitOrZ(pair.direction.ijk);
        pair.direction.refPoints.clear();
    }
    return true;
}

bool setMovePairDirection(Model& model, MoveId id, std::size_t pairIndex,
                          const Vec3& direction) {
    MoveDef* move = findMutableMove(model, id);
    if (!move || pairIndex >= move->inputs.pairs.size() ||
        !isFiniteNonzeroVec3(direction)) {
        return false;
    }
    if (move->active) {
        MoveDef candidate = *move;
        MovePair& candidatePair = candidate.inputs.pairs[pairIndex];
        candidatePair.direction.type = DirectionType::TypeIn;
        candidatePair.direction.ijk = unitOrZ(direction);
        candidatePair.direction.refPoints.clear();
        if (!moveIsActivatable(model, candidate)) return false;
    }

    MovePair& pair = move->inputs.pairs[pairIndex];
    pair.direction.type = DirectionType::TypeIn;
    pair.direction.ijk = unitOrZ(direction);
    pair.direction.refPoints.clear();
    return true;
}

bool setMovePairObjectPoint(Model& model, MoveId id, std::size_t pairIndex,
                            const Vec3& point) {
    MoveDef* move = findMutableMove(model, id);
    if (!move || pairIndex >= move->inputs.pairs.size() || !isFiniteVec3(point)) {
        return false;
    }
    if (move->active) {
        MoveDef candidate = *move;
        candidate.inputs.pairs[pairIndex].objectPoint = point;
        if (!moveIsActivatable(model, candidate)) return false;
    }

    move->inputs.pairs[pairIndex].objectPoint = point;
    return true;
}

bool setMovePairTargetPoint(Model& model, MoveId id, std::size_t pairIndex,
                            const Vec3& point) {
    MoveDef* move = findMutableMove(model, id);
    if (!move || pairIndex >= move->inputs.pairs.size() || !isFiniteVec3(point)) {
        return false;
    }
    if (move->active) {
        MoveDef candidate = *move;
        candidate.inputs.pairs[pairIndex].targetPoint = point;
        if (!moveIsActivatable(model, candidate)) return false;
    }

    move->inputs.pairs[pairIndex].targetPoint = point;
    return true;
}

bool setMoveParts(Model& model, MoveId id, const std::vector<PartId>& parts) {
    MoveDef* move = findMutableMove(model, id);
    if (!move) return false;
    if (move->active && !moveHasEnoughParts(parts.size())) return false;
    if (!allIdsDistinct(parts)) return false;
    if (!allReferencesExist(parts, [&](PartId partId) {
            return containsPartId(model, partId);
        })) {
        return false;
    }
    if (move->active) {
        MoveDef candidate = *move;
        candidate.moveParts = parts;
        if (!moveIsActivatable(model, candidate)) return false;
    }
    move->moveParts = parts;
    return true;
}

bool setMoveUserDllRoutine(Model& model, MoveId id, const std::string& routine) {
    MoveDef* move = findMutableMove(model, id);
    if (!move) return false;
    if (move->active) {
        MoveDef candidate = *move;
        candidate.inputs.userDllRoutine = routine;
        if (!moveIsActivatable(model, candidate)) return false;
    }
    move->inputs.userDllRoutine = routine;
    return true;
}

bool setMoveSearchAccuracy(Model& model, MoveId id, double searchAccuracy) {
    MoveDef* move = findMutableMove(model, id);
    if (!move || !isPositiveFinite(searchAccuracy)) return false;
    if (move->active) {
        MoveDef candidate = *move;
        candidate.inputs.searchAccuracy = searchAccuracy;
        if (!moveIsActivatable(model, candidate)) return false;
    }
    move->inputs.searchAccuracy = searchAccuracy;
    return true;
}

bool setMoveMaxIterations(Model& model, MoveId id, int maxIterations) {
    MoveDef* move = findMutableMove(model, id);
    if (!move || maxIterations <= 0) return false;
    if (move->active) {
        MoveDef candidate = *move;
        candidate.inputs.maxIterations = maxIterations;
        if (!moveIsActivatable(model, candidate)) return false;
    }
    move->inputs.maxIterations = maxIterations;
    return true;
}

bool setMoveFloatActive(Model& model, MoveId id, bool active) {
    MoveDef* move = findMutableMove(model, id);
    if (!move) return false;
    if (move->active) {
        MoveDef candidate = *move;
        candidate.inputs.hole_pin_float.active = active;
        if (!moveIsActivatable(model, candidate)) return false;
    }
    move->inputs.hole_pin_float.active = active;
    return true;
}

bool setMoveFloatSigmaNumber(Model& model, MoveId id, int sigmaNumber) {
    MoveDef* move = findMutableMove(model, id);
    if (!move || sigmaNumber < 1 || sigmaNumber > 8) return false;
    if (move->active) {
        MoveDef candidate = *move;
        candidate.inputs.hole_pin_float.sigmaNumber = sigmaNumber;
        if (!moveIsActivatable(model, candidate)) return false;
    }
    move->inputs.hole_pin_float.sigmaNumber = sigmaNumber;
    return true;
}

bool setMoveFloatRangeScale(Model& model, MoveId id, double rangeScale) {
    MoveDef* move = findMutableMove(model, id);
    if (!move || !isPositiveFinite(rangeScale)) return false;
    if (move->active) {
        MoveDef candidate = *move;
        candidate.inputs.hole_pin_float.rangeScale = rangeScale;
        if (!moveIsActivatable(model, candidate)) return false;
    }
    move->inputs.hole_pin_float.rangeScale = rangeScale;
    return true;
}

bool setMoveFloatAngleRange(Model& model, MoveId id, double angleRangeDeg) {
    MoveDef* move = findMutableMove(model, id);
    if (!move || !isNonNegativeFinite(angleRangeDeg)) return false;
    if (move->active) {
        MoveDef candidate = *move;
        candidate.inputs.hole_pin_float.angleRangeDeg = angleRangeDeg;
        if (!moveIsActivatable(model, candidate)) return false;
    }
    move->inputs.hole_pin_float.angleRangeDeg = angleRangeDeg;
    return true;
}

bool setMoveFloatAngleOffset(Model& model, MoveId id, double angleOffsetDeg) {
    MoveDef* move = findMutableMove(model, id);
    if (!move || !isFinite(angleOffsetDeg)) return false;
    if (move->active) {
        MoveDef candidate = *move;
        candidate.inputs.hole_pin_float.angleOffsetDeg = angleOffsetDeg;
        if (!moveIsActivatable(model, candidate)) return false;
    }
    move->inputs.hole_pin_float.angleOffsetDeg = angleOffsetDeg;
    return true;
}

bool renameMeasure(Model& model, MeasureId id, const std::string& name) {
    MeasureRecord* measure = findMutableMeasure(model, id);
    if (!measure) return false;
    if (measure->def.active && !measureIsActivatable(model, id, measure->def)) {
        return false;
    }
    measure->name = name;
    return true;
}

bool setMeasureType(Model& model, MeasureId id, MeasureType type) {
    MeasureRecord* measure = findMutableMeasure(model, id);
    if (!measure || !isValidMeasureType(type)) return false;
    if (measure->def.active) {
        MeasureDef candidate = measure->def;
        candidate.type = type;
        if (!measureIsActivatable(model, id, candidate)) return false;
    }
    measure->def.type = type;
    return true;
}

bool setMeasureActive(Model& model, MeasureId id, bool active) {
    MeasureRecord* measure = findMutableMeasure(model, id);
    if (!measure) return false;
    if (active && !measureIsActivatable(model, id, measure->def)) return false;
    measure->def.active = active;
    return true;
}

bool setMeasureAsOutput(Model& model, MeasureId id, bool asOutput) {
    MeasureRecord* measure = findMutableMeasure(model, id);
    if (!measure) return false;
    if (measure->def.active) {
        MeasureDef candidate = measure->def;
        candidate.asOutput = asOutput;
        if (!measureIsActivatable(model, id, candidate)) return false;
    }
    measure->def.asOutput = asOutput;
    return true;
}

bool setMeasureLslActive(Model& model, MeasureId id, bool active) {
    MeasureRecord* measure = findMutableMeasure(model, id);
    if (!measure) return false;
    const auto& spec = measure->def.spec;
    if (!measureSpecLimitsAreOrdered(spec.lsl, spec.usl, active,
                                     spec.uslActive)) {
        return false;
    }
    if (measure->def.active) {
        MeasureDef candidate = measure->def;
        candidate.spec.lslActive = active;
        if (!measureIsActivatable(model, id, candidate)) return false;
    }
    measure->def.spec.lslActive = active;
    return true;
}

bool setMeasureUslActive(Model& model, MeasureId id, bool active) {
    MeasureRecord* measure = findMutableMeasure(model, id);
    if (!measure) return false;
    const auto& spec = measure->def.spec;
    if (!measureSpecLimitsAreOrdered(spec.lsl, spec.usl, spec.lslActive,
                                     active)) {
        return false;
    }
    if (measure->def.active) {
        MeasureDef candidate = measure->def;
        candidate.spec.uslActive = active;
        if (!measureIsActivatable(model, id, candidate)) return false;
    }
    measure->def.spec.uslActive = active;
    return true;
}

bool setMeasureInputPoints(Model& model, MeasureId id,
                           const std::vector<PointId>& points) {
    MeasureRecord* measure = findMutableMeasure(model, id);
    if (!measure) return false;
    if (!allIdsDistinct(points)) return false;
    if (!allReferencesExist(points, [&](PointId pointId) {
            return containsPointId(model, pointId);
        })) {
        return false;
    }
    MeasureDef candidate = measure->def;
    candidate.inputPoints = points;
    if (measure->def.active && !measureIsActivatable(model, id, candidate)) {
        return false;
    }
    measure->def.inputPoints = points;
    return true;
}

bool setMeasureInputFeatures(Model& model, MeasureId id,
                             const std::vector<FeatureId>& features) {
    MeasureRecord* measure = findMutableMeasure(model, id);
    if (!measure) return false;
    if (!allIdsDistinct(features)) return false;
    if (measureInputFeaturesAreMeasureRefs(measure->def.type)) {
        if (!allReferencesExist(features, [&](FeatureId measureId) {
                return containsMeasureId(model, static_cast<MeasureId>(measureId));
            })) {
            return false;
        }
        MeasureDef candidate = measure->def;
        candidate.inputFeatures = features;
        if (measure->def.active && !measureIsActivatable(model, id, candidate)) {
            return false;
        }
        measure->def.inputFeatures = features;
        return true;
    }
    if (!allReferencesExist(features, [&](FeatureId featureId) {
            return containsFeatureId(model, featureId);
        })) {
        return false;
    }
    MeasureDef candidate = measure->def;
    candidate.inputFeatures = features;
    if (measure->def.active && !measureIsActivatable(model, id, candidate)) {
        return false;
    }
    measure->def.inputFeatures = features;
    return true;
}

bool setMeasureSpecMode(Model& model, MeasureId id, SpecMode mode) {
    MeasureRecord* measure = findMutableMeasure(model, id);
    if (!measure || !isValidSpecMode(mode)) return false;
    if (measure->def.active) {
        MeasureDef candidate = measure->def;
        candidate.spec.mode = mode;
        if (!measureIsActivatable(model, id, candidate)) return false;
    }
    measure->def.spec.mode = mode;
    return true;
}

bool setMeasureDirectionMode(Model& model, MeasureId id, DirectionMode mode) {
    MeasureRecord* measure = findMutableMeasure(model, id);
    if (!measure || !isValidDirectionMode(mode)) return false;
    if (measure->def.active) {
        MeasureDef candidate = measure->def;
        candidate.dirMode = mode;
        if (!measureIsActivatable(model, id, candidate)) return false;
    }
    measure->def.dirMode = mode;
    return true;
}

bool setMeasureDirection(Model& model, MeasureId id, const Vec3& direction) {
    MeasureRecord* measure = findMutableMeasure(model, id);
    if (!measure || !isFiniteNonzeroVec3(direction)) return false;
    if (measure->def.active) {
        MeasureDef candidate = measure->def;
        candidate.direction.type = DirectionType::TypeIn;
        candidate.direction.ijk = unitOrZ(direction);
        candidate.direction.refPoints.clear();
        if (!measureIsActivatable(model, id, candidate)) return false;
    }
    measure->def.direction.type = DirectionType::TypeIn;
    measure->def.direction.ijk = unitOrZ(direction);
    measure->def.direction.refPoints.clear();
    return true;
}

bool setMeasureScale(Model& model, MeasureId id, double scale) {
    MeasureRecord* measure = findMutableMeasure(model, id);
    if (!measure || !isFinite(scale) || scale == 0.0) return false;
    if (measure->def.active) {
        MeasureDef candidate = measure->def;
        candidate.scale = scale;
        if (!measureIsActivatable(model, id, candidate)) return false;
    }
    measure->def.scale = scale;
    return true;
}

bool setMeasureEquation(Model& model, MeasureId id, const std::string& equation) {
    MeasureRecord* measure = findMutableMeasure(model, id);
    if (!measure || !measureEquationIsEditable(equation)) return false;
    if (measure->def.active) {
        MeasureDef candidate = measure->def;
        candidate.equation = equation;
        if (!measureIsActivatable(model, id, candidate)) return false;
    }
    measure->def.equation = equation;
    return true;
}

bool setMeasureValues(Model& model, MeasureId id,
                      const std::vector<double>& values) {
    MeasureRecord* measure = findMutableMeasure(model, id);
    if (!measure) return false;
    for (const double value : values) {
        if (!isFinite(value)) return false;
    }
    if (measure->def.active) {
        MeasureDef candidate = measure->def;
        candidate.values = values;
        if (!measureIsActivatable(model, id, candidate)) return false;
    }
    measure->def.values = values;
    return true;
}

bool setMeasureSpec(Model& model, MeasureId id, double lsl, double usl,
                    bool lslActive, bool uslActive) {
    MeasureRecord* measure = findMutableMeasure(model, id);
    if (!measure || !isFinite(lsl) || !isFinite(usl)) return false;
    if (!measureSpecLimitsAreOrdered(lsl, usl, lslActive, uslActive)) {
        return false;
    }
    if (measure->def.active) {
        MeasureDef candidate = measure->def;
        candidate.spec.lsl = lsl;
        candidate.spec.usl = usl;
        candidate.spec.lslActive = lslActive;
        candidate.spec.uslActive = uslActive;
        if (!measureIsActivatable(model, id, candidate)) return false;
    }
    measure->def.spec.lsl = lsl;
    measure->def.spec.usl = usl;
    measure->def.spec.lslActive = lslActive;
    measure->def.spec.uslActive = uslActive;
    return true;
}

bool reorderMove(Model& model, MoveId id, std::size_t newIndex) {
    const auto it = std::find_if(model.moves.begin(), model.moves.end(),
                                 [&](const MoveDef& move) {
                                     return move.id == id;
                                 });
    if (it == model.moves.end()) return false;

    MoveDef move = std::move(*it);
    model.moves.erase(it);
    if (newIndex > model.moves.size()) {
        newIndex = model.moves.size();
    }
    model.moves.insert(model.moves.begin() + static_cast<std::ptrdiff_t>(newIndex),
                       std::move(move));
    return true;
}

bool deleteTolerance(Model& model, ToleranceId id) {
    for (Part& part : model.parts) {
        const bool removed = eraseIf(part.tolerances, [&](const ToleranceDef& tolerance) {
            return tolerance.id == id;
        });
        if (removed) {
            removeVariantToleranceRefs(model, id);
            return true;
        }
    }
    return false;
}

bool deleteGdt(Model& model, GdtId id) {
    for (Part& part : model.parts) {
        if (eraseIf(part.gdts, [&](const GdtDef& gdt) { return gdt.id == id; })) {
            return true;
        }
    }
    return false;
}

bool deleteMove(Model& model, MoveId id) {
    const bool removed =
        eraseIf(model.moves, [&](const MoveDef& move) { return move.id == id; });
    if (removed) removeVariantMoveRefs(model, id);
    return removed;
}

bool deleteMeasure(Model& model, MeasureId id) {
    if (!containsMeasureId(model, id)) return false;
    const std::vector<MeasureId> deletedIds =
        collectDependentMeasureDeletes(model, id);
    eraseIf(model.measures, [&](const MeasureRecord& measure) {
        return std::find(deletedIds.begin(), deletedIds.end(), measure.id) !=
               deletedIds.end();
    });
    for (const MeasureId measureId : deletedIds) {
        removeVariantMeasureRefs(model, measureId);
    }
    return true;
}

bool deleteFeature(Model& model, FeatureId id) {
    for (Part& part : model.parts) {
        const bool removed =
            eraseIf(part.features, [&](const Feature& feature) { return feature.id == id; });
        if (!removed) continue;

        const std::vector<FeatureId> deletedFeatureIds{id};
        std::vector<ToleranceId> deletedToleranceIds;
        for (const ToleranceDef& tolerance : part.tolerances) {
            if (referencesAnyFeature(tolerance.features, deletedFeatureIds)) {
                deletedToleranceIds.push_back(tolerance.id);
            }
        }
        std::vector<GdtId> deletedGdtIds;
        for (const GdtDef& gdt : part.gdts) {
            if (referencesAnyFeature(gdt.features, deletedFeatureIds) ||
                referencesAnyFeature(gdt.drf, deletedFeatureIds)) {
                deletedGdtIds.push_back(gdt.id);
            }
        }
        std::vector<MeasureId> deletedMeasureIds;
        for (const MeasureRecord& measure : model.measures) {
            if (referencesAnyFeature(measure.def.inputFeatures, deletedFeatureIds)) {
                deletedMeasureIds.push_back(measure.id);
            }
        }

        eraseIf(part.tolerances, [&](const ToleranceDef& tolerance) {
            return referencesAnyFeature(tolerance.features, deletedFeatureIds);
        });
        eraseIf(part.gdts, [&](const GdtDef& gdt) {
            return referencesAnyFeature(gdt.features, deletedFeatureIds) ||
                   referencesAnyFeature(gdt.drf, deletedFeatureIds);
        });
        eraseIf(model.measures, [&](const MeasureRecord& measure) {
            return referencesAnyFeature(measure.def.inputFeatures, deletedFeatureIds);
        });
        for (const ToleranceId toleranceId : deletedToleranceIds) {
            removeVariantToleranceRefs(model, toleranceId);
        }
        for (const MeasureId measureId : deletedMeasureIds) {
            removeVariantMeasureRefs(model, measureId);
        }
        (void)deletedGdtIds;
        return true;
    }
    return false;
}

bool deletePoint(Model& model, PointId id) {
    for (Part& part : model.parts) {
        const bool removed =
            eraseIf(part.points, [&](const Point& point) { return point.id == id; });
        if (!removed) continue;

        const std::vector<PointId> deletedPointIds{id};
        std::vector<FeatureId> deletedFeatureIds;
        for (const Feature& feature : part.features) {
            if (std::find(feature.definingPoints.begin(), feature.definingPoints.end(),
                          id) != feature.definingPoints.end()) {
                deletedFeatureIds.push_back(feature.id);
            }
        }

        std::vector<ToleranceId> deletedToleranceIds;
        for (const ToleranceDef& tolerance : part.tolerances) {
            if (referencesAnyFeature(tolerance.features, deletedFeatureIds) ||
                referencesAnyPoint(tolerance.ir.direction, deletedPointIds)) {
                deletedToleranceIds.push_back(tolerance.id);
            }
        }
        std::vector<GdtId> deletedGdtIds;
        for (const GdtDef& gdt : part.gdts) {
            if (referencesAnyFeature(gdt.features, deletedFeatureIds) ||
                referencesAnyFeature(gdt.drf, deletedFeatureIds)) {
                deletedGdtIds.push_back(gdt.id);
            }
        }
        std::vector<MeasureId> deletedMeasureIds;
        for (const MeasureRecord& measure : model.measures) {
            if (containsPoint(measure, id) ||
                referencesAnyFeature(measure.def.inputFeatures, deletedFeatureIds) ||
                referencesAnyPoint(measure.def.direction, deletedPointIds)) {
                deletedMeasureIds.push_back(measure.id);
            }
        }
        std::vector<MoveId> deletedMoveIds;
        for (const MoveDef& move : model.moves) {
            if (referencesAnyPoint(move, deletedPointIds)) {
                deletedMoveIds.push_back(move.id);
            }
        }

        eraseIf(part.features, [&](const Feature& feature) {
            return std::find(deletedFeatureIds.begin(), deletedFeatureIds.end(),
                             feature.id) != deletedFeatureIds.end();
        });
        eraseIf(part.tolerances, [&](const ToleranceDef& tolerance) {
            return referencesAnyFeature(tolerance.features, deletedFeatureIds) ||
                   referencesAnyPoint(tolerance.ir.direction, deletedPointIds);
        });
        eraseIf(part.gdts, [&](const GdtDef& gdt) {
            return referencesAnyFeature(gdt.features, deletedFeatureIds) ||
                   referencesAnyFeature(gdt.drf, deletedFeatureIds);
        });
        eraseIf(model.measures, [&](const MeasureRecord& measure) {
            return containsPoint(measure, id) ||
                   referencesAnyFeature(measure.def.inputFeatures, deletedFeatureIds) ||
                   referencesAnyPoint(measure.def.direction, deletedPointIds);
        });
        eraseIf(model.moves, [&](const MoveDef& move) {
            return referencesAnyPoint(move, deletedPointIds);
        });

        for (const ToleranceId toleranceId : deletedToleranceIds) {
            removeVariantToleranceRefs(model, toleranceId);
        }
        (void)deletedGdtIds;
        for (const MeasureId measureId : deletedMeasureIds) {
            removeVariantMeasureRefs(model, measureId);
        }
        for (const MoveId moveId : deletedMoveIds) removeVariantMoveRefs(model, moveId);
        return true;
    }
    return false;
}

bool deletePart(Model& model, PartId id) {
    const auto partIt = std::find_if(model.parts.begin(), model.parts.end(),
                                     [&](const Part& part) { return part.id == id; });
    if (partIt == model.parts.end()) return false;

    std::vector<PointId> deletedPointIds;
    for (const Point& point : partIt->points) deletedPointIds.push_back(point.id);

    std::vector<FeatureId> deletedFeatureIds;
    for (const Feature& feature : partIt->features) deletedFeatureIds.push_back(feature.id);

    std::vector<ToleranceId> deletedToleranceIds;
    for (const ToleranceDef& tolerance : partIt->tolerances) {
        deletedToleranceIds.push_back(tolerance.id);
    }
    for (const Part& part : model.parts) {
        if (part.id == id) continue;
        for (const ToleranceDef& tolerance : part.tolerances) {
            if (referencesAnyFeature(tolerance.features, deletedFeatureIds) ||
                referencesAnyPoint(tolerance.ir.direction, deletedPointIds)) {
                deletedToleranceIds.push_back(tolerance.id);
            }
        }
    }

    std::vector<GdtId> deletedGdtIds;
    for (const Part& part : model.parts) {
        if (part.id == id) {
            for (const GdtDef& gdt : part.gdts) deletedGdtIds.push_back(gdt.id);
            continue;
        }
        for (const GdtDef& gdt : part.gdts) {
            if (referencesAnyFeature(gdt.features, deletedFeatureIds) ||
                referencesAnyFeature(gdt.drf, deletedFeatureIds)) {
                deletedGdtIds.push_back(gdt.id);
            }
        }
    }

    std::vector<MoveId> deletedMoveIds;
    for (const MoveDef& move : model.moves) {
        if (std::find(move.moveParts.begin(), move.moveParts.end(), id) !=
                move.moveParts.end() ||
            referencesAnyPoint(move, deletedPointIds)) {
            deletedMoveIds.push_back(move.id);
        }
    }

    std::vector<MeasureId> deletedMeasureIds;
    for (const MeasureRecord& measure : model.measures) {
        if (referencesAnyFeature(measure.def.inputFeatures, deletedFeatureIds)) {
            deletedMeasureIds.push_back(measure.id);
            continue;
        }
        if (referencesAnyPoint(measure.def.direction, deletedPointIds)) {
            deletedMeasureIds.push_back(measure.id);
            continue;
        }
        for (const PointId pointId : deletedPointIds) {
            if (containsPoint(measure, pointId)) {
                deletedMeasureIds.push_back(measure.id);
                break;
            }
        }
    }

    model.parts.erase(partIt);
    eraseIf(model.moves, [&](const MoveDef& move) {
        return std::find(move.moveParts.begin(), move.moveParts.end(), id) !=
                   move.moveParts.end() ||
               referencesAnyPoint(move, deletedPointIds);
    });
    for (Part& part : model.parts) {
        eraseIf(part.tolerances, [&](const ToleranceDef& tolerance) {
            return referencesAnyFeature(tolerance.features, deletedFeatureIds) ||
                   referencesAnyPoint(tolerance.ir.direction, deletedPointIds);
        });
        eraseIf(part.gdts, [&](const GdtDef& gdt) {
            return referencesAnyFeature(gdt.features, deletedFeatureIds) ||
                   referencesAnyFeature(gdt.drf, deletedFeatureIds);
        });
    }
    eraseIf(model.measures, [&](const MeasureRecord& measure) {
        if (referencesAnyFeature(measure.def.inputFeatures, deletedFeatureIds)) {
            return true;
        }
        if (referencesAnyPoint(measure.def.direction, deletedPointIds)) {
            return true;
        }
        for (const PointId pointId : deletedPointIds) {
            if (containsPoint(measure, pointId)) return true;
        }
        return false;
    });

    for (const ToleranceId toleranceId : deletedToleranceIds) {
        removeVariantToleranceRefs(model, toleranceId);
    }
    (void)deletedGdtIds;
    for (const MoveId moveId : deletedMoveIds) removeVariantMoveRefs(model, moveId);
    for (const MeasureId measureId : deletedMeasureIds) {
        removeVariantMeasureRefs(model, measureId);
    }
    return true;
}

ModelVariant captureActiveVariant(const Model& model, const std::string& name) {
    ModelVariant variant;
    variant.name = name;
    variant.active = false;

    for (const MoveDef& move : model.moves) {
        if (move.active) variant.moves.push_back(move.id);
    }
    for (const Part& part : model.parts) {
        for (const ToleranceDef& tolerance : part.tolerances) {
            if (tolerance.active) variant.tolerances.push_back(tolerance.id);
        }
    }
    for (const MeasureRecord& measure : model.measures) {
        if (measure.def.active) variant.measures.push_back(measure.id);
    }

    return variant;
}

bool addOrReplaceModelVariant(Model& model, const ModelVariant& variant) {
    if (variant.name.empty()) return false;
    if (!variantReferencesExist(model, variant)) return false;
    if (!variantReferencesAreDistinct(variant)) return false;

    ModelVariant stored = variant;
    if (stored.active) {
        Model candidate = model;
        for (ModelVariant& existing : candidate.variants) existing.active = false;
        bool replaced = false;
        for (ModelVariant& existing : candidate.variants) {
            if (existing.name == stored.name) {
                existing = stored;
                replaced = true;
                break;
            }
        }
        if (!replaced) candidate.variants.push_back(stored);
        if (!activeVariantApplyStateIsRunnable(candidate, stored.name)) {
            return false;
        }
        for (ModelVariant& existing : model.variants) existing.active = false;
    }

    for (ModelVariant& existing : model.variants) {
        if (existing.name == stored.name) {
            existing = stored;
            return true;
        }
    }

    model.variants.push_back(stored);
    return true;
}

bool renameModelVariant(Model& model, const std::string& oldName,
                        const std::string& newName) {
    if (newName.empty()) return false;

    ModelVariant* variant = findMutableVariant(model, oldName);
    if (!variant) return false;
    for (const ModelVariant& existing : model.variants) {
        if (existing.name == newName && existing.name != oldName) return false;
    }

    variant->name = newName;
    return true;
}

bool setModelVariantActive(Model& model, const std::string& name, bool active) {
    ModelVariant* variant = findMutableVariant(model, name);
    if (!variant) return false;

    if (active) {
        Model candidate = model;
        for (ModelVariant& existing : candidate.variants) existing.active = false;
        ModelVariant* candidateVariant = findMutableVariant(candidate, name);
        if (!candidateVariant) return false;
        candidateVariant->active = true;
        if (!activeVariantApplyStateIsRunnable(candidate, name)) return false;
        for (ModelVariant& existing : model.variants) existing.active = false;
    }
    variant->active = active;
    return true;
}

bool setModelVariantMoves(Model& model, const std::string& name,
                          const std::vector<MoveId>& moves) {
    ModelVariant* variant = findMutableVariant(model, name);
    if (!variant) return false;
    if (!allIdsDistinct(moves)) return false;
    if (!allReferencesExist(moves, [&](MoveId id) {
            return containsMoveId(model, id);
        })) {
        return false;
    }
    if (variant->active) {
        Model candidate = model;
        ModelVariant* candidateVariant = findMutableVariant(candidate, name);
        if (!candidateVariant) return false;
        candidateVariant->moves = moves;
        if (!activeVariantApplyStateIsRunnable(candidate, name)) return false;
    }
    variant->moves = moves;
    return true;
}

bool setModelVariantTolerances(Model& model, const std::string& name,
                               const std::vector<ToleranceId>& tolerances) {
    ModelVariant* variant = findMutableVariant(model, name);
    if (!variant) return false;
    if (!allIdsDistinct(tolerances)) return false;
    if (!allReferencesExist(tolerances, [&](ToleranceId id) {
            return containsToleranceId(model, id);
        })) {
        return false;
    }
    if (variant->active) {
        Model candidate = model;
        ModelVariant* candidateVariant = findMutableVariant(candidate, name);
        if (!candidateVariant) return false;
        candidateVariant->tolerances = tolerances;
        if (!activeVariantApplyStateIsRunnable(candidate, name)) return false;
    }
    variant->tolerances = tolerances;
    return true;
}

bool setModelVariantMeasures(Model& model, const std::string& name,
                             const std::vector<MeasureId>& measures) {
    ModelVariant* variant = findMutableVariant(model, name);
    if (!variant) return false;
    if (!allIdsDistinct(measures)) return false;
    if (!allReferencesExist(measures, [&](MeasureId id) {
            return containsMeasureId(model, id);
        })) {
        return false;
    }
    if (variant->active) {
        Model candidate = model;
        ModelVariant* candidateVariant = findMutableVariant(candidate, name);
        if (!candidateVariant) return false;
        candidateVariant->measures = measures;
        if (!activeVariantApplyStateIsRunnable(candidate, name)) return false;
    }
    variant->measures = measures;
    return true;
}

bool deleteModelVariant(Model& model, const std::string& name) {
    const auto oldSize = model.variants.size();
    model.variants.erase(
        std::remove_if(model.variants.begin(), model.variants.end(),
                       [&](const ModelVariant& variant) {
                           return variant.name == name;
                       }),
        model.variants.end());
    return model.variants.size() != oldSize;
}

bool applyModelVariant(Model& model, const std::string& name) {
    for (const ModelVariant& variant : model.variants) {
        if (variant.name == name) {
            Model candidate = model.applyVariant(name);
            if (!variantActiveStateIsRunnable(candidate)) {
                return false;
            }
            model = std::move(candidate);
            return true;
        }
    }
    return false;
}

}  // namespace opendva
