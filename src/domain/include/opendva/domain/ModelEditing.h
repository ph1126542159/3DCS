// Pure C++ model editing helpers shared by UI, CLI tooling, and tests.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "opendva/domain/Model.h"

namespace opendva {

PartId nextPartId(const Model& model);
PointId nextPointId(const Model& model);
FeatureId nextFeatureId(const Model& model);
ToleranceId nextToleranceId(const Model& model);
GdtId nextGdtId(const Model& model);
MeasureId nextMeasureId(const Model& model);
MoveId nextMoveId(const Model& model);

Model createStarterModel();
Part& ensureEditablePart(Model& model, const std::string& defaultName = "Part");
PartId owningPartOfPoint(const Model& model, PointId id);
PartId owningPartOfFeature(const Model& model, FeatureId id);
PartId owningPartOfTolerance(const Model& model, ToleranceId id);
PartId owningPartOfGdt(const Model& model, GdtId id);

PartId addPart(Model& model, const std::string& name);
PointId addCoordinatePoint(Model& model, const Vec3& position,
                           PartId partId = kInvalidId);
FeatureId addFeature(Model& model, FeatureKind kind = FeatureKind::Plane,
                     PartId partId = kInvalidId);
ToleranceId addLinearTolerance(Model& model, double range,
                               PartId partId = kInvalidId);
GdtId addGdt(Model& model, GdtType type, double range,
             bool diametrical = false, PartId partId = kInvalidId);
MeasureId addPointPointMeasure(Model& model, double lsl = 0.0,
                               double usl = 100.0);
MoveId addTransformMove(Model& model, const Vec3& translation,
                        PartId objectPartId = kInvalidId,
                        PartId targetPartId = kInvalidId);

bool renameAssembly(Model& model, const std::string& name);
bool renamePart(Model& model, PartId id, const std::string& dcsName);
bool setPointPosition(Model& model, PointId id, const Vec3& position);
bool setPointActive(Model& model, PointId id, bool active);
bool setPointKind(Model& model, PointId id, PointKind kind);
bool setPointHoleType(Model& model, PointId id, HoleType holeType);
bool setPointDiameter(Model& model, PointId id, double diameter);
bool setPointDirection(Model& model, PointId id, const Vec3& direction);
bool setFeatureKind(Model& model, FeatureId id, FeatureKind kind);
bool setFeatureDefiningPoints(Model& model, FeatureId id,
                              const std::vector<PointId>& points);
bool renameTolerance(Model& model, ToleranceId id, const std::string& name);
bool setToleranceActive(Model& model, ToleranceId id, bool active);
bool setToleranceDistribution(Model& model, ToleranceId id,
                              DistributionType distribution);
bool setToleranceRange(Model& model, ToleranceId id, double range);
bool setToleranceOffset(Model& model, ToleranceId id, double offset);
bool setToleranceSigmaNumber(Model& model, ToleranceId id, double sigmaNumber);
bool setToleranceUserDefinedSamplePath(Model& model, ToleranceId id,
                                       const std::string& path);
bool setToleranceRandomVariables(Model& model, ToleranceId id,
                                 const std::vector<RandSpec>& rands);
bool setToleranceGeomRule(Model& model, ToleranceId id, GeomRule geomRule);
bool setToleranceRangeScale(Model& model, ToleranceId id, double rangeScale);
bool setToleranceDirection(Model& model, ToleranceId id, const Vec3& direction);
bool setToleranceTruncation(Model& model, ToleranceId id, double minTrunc,
                            double maxTrunc, bool active);
bool setToleranceFeatures(Model& model, ToleranceId id,
                          const std::vector<FeatureId>& features);
bool renameGdt(Model& model, GdtId id, const std::string& name);
bool setGdtType(Model& model, GdtId id, GdtType type);
bool setGdtActive(Model& model, GdtId id, bool active);
bool setGdtRange(Model& model, GdtId id, double range);
bool setGdtDiametrical(Model& model, GdtId id, bool diametrical);
bool setGdtDrf(Model& model, GdtId id, const DatumReferenceFrame& drf);
bool setGdtFeatures(Model& model, GdtId id,
                    const std::vector<FeatureId>& features);
bool renameMove(Model& model, MoveId id, const std::string& name);
bool setMoveType(Model& model, MoveId id, MoveType type);
bool setMoveActive(Model& model, MoveId id, bool active);
bool setMoveNominalBuild(Model& model, MoveId id, bool nominalBuild);
bool setTransformMoveTranslation(Model& model, MoveId id, const Vec3& translation);
bool setMovePairs(Model& model, MoveId id, const std::vector<MovePair>& pairs);
bool setMovePairDirection(Model& model, MoveId id, std::size_t pairIndex,
                          const Vec3& direction);
bool setMovePairObjectPoint(Model& model, MoveId id, std::size_t pairIndex,
                            const Vec3& point);
bool setMovePairTargetPoint(Model& model, MoveId id, std::size_t pairIndex,
                            const Vec3& point);
bool setMoveParts(Model& model, MoveId id, const std::vector<PartId>& parts);
bool setMoveUserDllRoutine(Model& model, MoveId id, const std::string& routine);
bool setMoveSearchAccuracy(Model& model, MoveId id, double searchAccuracy);
bool setMoveMaxIterations(Model& model, MoveId id, int maxIterations);
bool setMoveFloatActive(Model& model, MoveId id, bool active);
bool setMoveFloatSigmaNumber(Model& model, MoveId id, int sigmaNumber);
bool setMoveFloatRangeScale(Model& model, MoveId id, double rangeScale);
bool setMoveFloatAngleRange(Model& model, MoveId id, double angleRangeDeg);
bool setMoveFloatAngleOffset(Model& model, MoveId id, double angleOffsetDeg);
bool renameMeasure(Model& model, MeasureId id, const std::string& name);
bool setMeasureType(Model& model, MeasureId id, MeasureType type);
bool setMeasureActive(Model& model, MeasureId id, bool active);
bool setMeasureAsOutput(Model& model, MeasureId id, bool asOutput);
bool setMeasureLslActive(Model& model, MeasureId id, bool active);
bool setMeasureUslActive(Model& model, MeasureId id, bool active);
bool setMeasureInputPoints(Model& model, MeasureId id,
                           const std::vector<PointId>& points);
bool setMeasureInputFeatures(Model& model, MeasureId id,
                             const std::vector<FeatureId>& features);
bool setMeasureSpecMode(Model& model, MeasureId id, SpecMode mode);
bool setMeasureDirectionMode(Model& model, MeasureId id, DirectionMode mode);
bool setMeasureDirection(Model& model, MeasureId id, const Vec3& direction);
bool setMeasureScale(Model& model, MeasureId id, double scale);
bool setMeasureEquation(Model& model, MeasureId id, const std::string& equation);
bool setMeasureValues(Model& model, MeasureId id,
                      const std::vector<double>& values);
bool setMeasureSpec(Model& model, MeasureId id, double lsl, double usl,
                    bool lslActive, bool uslActive);
bool reorderMove(Model& model, MoveId id, std::size_t newIndex);
bool deletePart(Model& model, PartId id);
bool deletePoint(Model& model, PointId id);
bool deleteFeature(Model& model, FeatureId id);
bool deleteTolerance(Model& model, ToleranceId id);
bool deleteGdt(Model& model, GdtId id);
bool deleteMove(Model& model, MoveId id);
bool deleteMeasure(Model& model, MeasureId id);

ModelVariant captureActiveVariant(const Model& model, const std::string& name);
bool addOrReplaceModelVariant(Model& model, const ModelVariant& variant);
bool renameModelVariant(Model& model, const std::string& oldName,
                        const std::string& newName);
bool setModelVariantActive(Model& model, const std::string& name, bool active);
bool setModelVariantMoves(Model& model, const std::string& name,
                          const std::vector<MoveId>& moves);
bool setModelVariantTolerances(Model& model, const std::string& name,
                               const std::vector<ToleranceId>& tolerances);
bool setModelVariantMeasures(Model& model, const std::string& name,
                             const std::vector<MeasureId>& measures);
bool deleteModelVariant(Model& model, const std::string& name);
bool applyModelVariant(Model& model, const std::string& name);

}  // namespace opendva
