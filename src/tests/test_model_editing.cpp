// Domain-level editing helpers for the desktop modeling workflow.
#include <algorithm>
#include <limits>

#include "dva_test.h"
#include "opendva/dcs_plugin_api.h"
#include "opendva/domain/ModelEditing.h"
#include "opendva/domain/ModelSerializer.h"
#include "opendva/domain/ModelValidation.h"
#include "opendva/plugin/PluginHost.h"

using namespace opendva;

namespace {

void boundUserDllRoutine(dcsDataPtr) {}

bool hasBlockingValidationIssue(const Model& model) {
    return hasBlockingIssues(validateModelForSimulation(model));
}

}  // namespace

TEST("model editing: starter model is runnable and serializable") {
    const Model model = createStarterModel();

    dvatest::check(model.assemblyName == "UntitledModel",
                   "starter assembly name");
    dvatest::check(model.parts.size() == 1, "starter has one part");
    dvatest::check(model.parts[0].points.size() == 2,
                   "starter has two points");
    dvatest::check(model.parts[0].features.size() >= 1,
                   "starter has feature geometry");
    dvatest::check(model.parts[0].tolerances.size() == 1,
                   "starter has one tolerance");
    dvatest::check(model.measures.size() == 1,
                   "starter has one output measure");
    dvatest::check(!hasBlockingValidationIssue(model),
                   "starter model can pass run validation");

    Model loaded;
    dvatest::check(loadModelFromString(loaded, saveModelToString(model)),
                   "starter model XML round-trips");
    dvatest::check(!hasBlockingValidationIssue(loaded),
                   "round-tripped starter model remains runnable");
}

TEST("model editing: add part assigns stable next id and names") {
    Model model;
    model.assemblyName = "Editable";

    const PartId baseId = addPart(model, "Base");
    const PartId lidId = addPart(model, "Lid");

    dvatest::check(baseId == 1, "first part id starts at 1");
    dvatest::check(lidId == 2, "second part id increments");
    dvatest::check(model.parts.size() == 2, "two parts created");
    dvatest::check(model.parts[0].cadName == "Base", "cad name set");
    dvatest::check(model.parts[0].dcsName == "Base", "dcs name set");
    dvatest::check(nextPartId(model) == 3, "next part id after additions");
}

TEST("model editing: add coordinate point creates a point-backed feature") {
    Model model;
    addPart(model, "Base");

    const PointId pointId = addCoordinatePoint(model, {1.0, 2.0, 3.0});

    dvatest::check(pointId == 101, "first point id starts at 101");
    dvatest::check(model.parts[0].points.size() == 1, "point created");
    dvatest::check(model.parts[0].features.size() == 1, "feature created");
    dvatest::check(model.parts[0].points[0].kind == PointKind::Coordinate,
                   "point is coordinate point");
    dvatest::checkNear(model.parts[0].points[0].position.z, 3.0, 1e-12,
                       "point z position");
    dvatest::check(model.parts[0].features[0].kind == FeatureKind::PointBased,
                   "feature is point based");
    dvatest::check(model.parts[0].features[0].definingPoints.size() == 1,
                   "feature has one defining point");
    dvatest::check(model.parts[0].features[0].definingPoints[0] == pointId,
                   "feature references new point");
}

TEST("model editing: add feature uses the part's last point") {
    Model model;
    const PartId partId = addPart(model, "Base");
    const PointId firstPoint = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const PointId lastPoint = addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    const std::size_t previousFeatureCount = model.parts[0].features.size();

    const FeatureId featureId = addFeature(model, FeatureKind::Plane, partId);

    dvatest::check(featureId == 203, "new feature id follows existing features");
    dvatest::check(model.parts[0].features.size() == previousFeatureCount + 1,
                   "feature created");
    const Feature& feature = model.parts[0].features.back();
    dvatest::check(feature.kind == FeatureKind::Plane, "feature kind preserved");
    dvatest::check(feature.definingPoints.size() == 1,
                   "feature has one defining point");
    dvatest::check(feature.definingPoints[0] == lastPoint,
                   "feature references the last point");
    dvatest::check(feature.definingPoints[0] != firstPoint,
                   "feature does not reuse the first point");
    dvatest::check(nextFeatureId(model) == 204, "next feature id after addition");
}

TEST("model editing: add linear tolerance targets last feature") {
    Model model;
    addPart(model, "Base");
    addCoordinatePoint(model, {0.0, 0.0, 5.0});
    const FeatureId featureId = model.parts[0].features.back().id;

    const ToleranceId tolId = addLinearTolerance(model, 0.8);

    dvatest::check(tolId == 301, "first tolerance id starts at 301");
    dvatest::check(model.parts[0].tolerances.size() == 1, "tolerance created");
    const ToleranceDef& tolerance = model.parts[0].tolerances[0];
    dvatest::check(tolerance.features.size() == 1, "tolerance targets one feature");
    dvatest::check(tolerance.features[0] == featureId, "tolerance targets last feature");
    dvatest::check(tolerance.ir.geomRule == GeomRule::TranslateAlongVector,
                   "linear tolerance translates along vector");
    dvatest::check(tolerance.ir.direction.ijk.z == 1.0, "linear tolerance uses +Z");
    dvatest::check(tolerance.ir.rands.size() == 1, "one random variable");
    dvatest::check(tolerance.ir.rands[0].distribution == DistributionType::Normal,
                   "normal distribution");
    dvatest::checkNear(tolerance.ir.rands[0].range, 0.8, 1e-12, "range preserved");
}

TEST("model editing: add GD&T callout targets last feature") {
    Model model;
    const PartId partId = addPart(model, "Base");
    addCoordinatePoint(model, {0.0, 0.0, 5.0}, partId);
    const FeatureId featureId = model.parts[0].features.back().id;

    const GdtId gdtId = addGdt(model, GdtType::Position, 1.2, true, partId);

    dvatest::check(gdtId == 501, "first gdt id starts at 501");
    dvatest::check(model.parts[0].gdts.size() == 1, "gdt created");
    const GdtDef& gdt = model.parts[0].gdts[0];
    dvatest::check(gdt.type == GdtType::Position, "gdt type preserved");
    dvatest::check(gdt.features.size() == 1, "gdt targets one feature");
    dvatest::check(gdt.features[0] == featureId, "gdt targets last feature");
    dvatest::checkNear(gdt.range, 1.2, 1e-12, "gdt range preserved");
    dvatest::check(gdt.diametrical, "gdt diametrical zone");
    dvatest::check(gdt.active, "gdt active by default");
    dvatest::check(nextGdtId(model) == 502, "next gdt id after addition");
}

TEST("model editing: add point-point measure uses first two model points") {
    Model model;
    addPart(model, "Base");
    const PointId p1 = addCoordinatePoint(model, {0.0, 0.0, 0.0});
    const PointId p2 = addCoordinatePoint(model, {0.0, 0.0, 10.0});

    const MeasureId measureId = addPointPointMeasure(model, 8.5, 11.5);

    dvatest::check(measureId == 401, "first measure id starts at 401");
    dvatest::check(model.measures.size() == 1, "measure created");
    const MeasureRecord& measure = model.measures[0];
    dvatest::check(measure.def.type == MeasureType::PointPoint, "point-point measure");
    dvatest::check(measure.def.inputPoints.size() == 2, "two input points");
    dvatest::check(measure.def.inputPoints[0] == p1, "first input point");
    dvatest::check(measure.def.inputPoints[1] == p2, "second input point");
    dvatest::check(measure.def.dirMode == DirectionMode::ProjectedOnVector,
                   "projected measure direction");
    dvatest::check(measure.def.spec.lslActive && measure.def.spec.uslActive,
                   "spec limits active");
    dvatest::checkNear(measure.def.spec.lsl, 8.5, 1e-12, "lsl preserved");
    dvatest::checkNear(measure.def.spec.usl, 11.5, 1e-12, "usl preserved");
}

TEST("model editing: add transform move uses first two parts") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId bracketId = addPart(model, "Bracket");

    const MoveId moveId = addTransformMove(model, {10.0, 0.0, 0.0});

    dvatest::check(moveId == 601, "first move id starts at 601");
    dvatest::check(model.moves.size() == 1, "move created");
    const MoveDef& move = model.moves[0];
    dvatest::check(move.name == "Transform_601", "default transform name");
    dvatest::check(move.active, "move active by default");
    dvatest::check(move.inputs.type == MoveType::Transform, "transform move type");
    dvatest::check(move.moveParts.size() == 2, "object and target parts");
    dvatest::check(move.moveParts[0] == baseId, "first part is object");
    dvatest::check(move.moveParts[1] == bracketId, "second part is target");
    dvatest::check(move.inputs.pairs.size() == 1, "one transform pair");
    dvatest::checkNear(move.inputs.pairs[0].objectPoint.x, 0.0, 1e-12,
                       "object point origin");
    dvatest::checkNear(move.inputs.pairs[0].targetPoint.x, 10.0, 1e-12,
                       "target point translation x");
    dvatest::checkNear(move.inputs.pairs[0].direction.ijk.x, 1.0, 1e-12,
                       "direction normalized x");
    dvatest::check(nextMoveId(model) == 602, "next move id after addition");
}

TEST("model editing: owner lookup finds part for nested objects") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PointId basePoint = addCoordinatePoint(model, {0.0, 0.0, 0.0}, baseId);
    const FeatureId baseFeature = addFeature(model, FeatureKind::Plane, baseId);
    const ToleranceId baseTolerance = addLinearTolerance(model, 0.5, baseId);
    const GdtId baseGdt = addGdt(model, GdtType::Flatness, 0.2, false, baseId);

    const PartId lidId = addPart(model, "Lid");
    const PointId lidPoint = addCoordinatePoint(model, {0.0, 0.0, 5.0}, lidId);
    const FeatureId lidFeature = addFeature(model, FeatureKind::Cylinder, lidId);
    const ToleranceId lidTolerance = addLinearTolerance(model, 0.8, lidId);
    const GdtId lidGdt = addGdt(model, GdtType::Position, 0.3, true, lidId);

    dvatest::check(owningPartOfPoint(model, basePoint) == baseId,
                   "base point owner");
    dvatest::check(owningPartOfFeature(model, baseFeature) == baseId,
                   "base feature owner");
    dvatest::check(owningPartOfTolerance(model, baseTolerance) == baseId,
                   "base tolerance owner");
    dvatest::check(owningPartOfGdt(model, baseGdt) == baseId,
                   "base gdt owner");
    dvatest::check(owningPartOfPoint(model, lidPoint) == lidId,
                   "lid point owner");
    dvatest::check(owningPartOfFeature(model, lidFeature) == lidId,
                   "lid feature owner");
    dvatest::check(owningPartOfTolerance(model, lidTolerance) == lidId,
                   "lid tolerance owner");
    dvatest::check(owningPartOfGdt(model, lidGdt) == lidId,
                   "lid gdt owner");
    dvatest::check(owningPartOfPoint(model, 999) == kInvalidId,
                   "missing point has no owner");
    dvatest::check(owningPartOfFeature(model, 999) == kInvalidId,
                   "missing feature has no owner");
    dvatest::check(owningPartOfTolerance(model, 999) == kInvalidId,
                   "missing tolerance has no owner");
    dvatest::check(owningPartOfGdt(model, 999) == kInvalidId,
                   "missing gdt has no owner");
}

TEST("model editing: insufficient data returns invalid ids") {
    Model model;

    dvatest::check(addLinearTolerance(model, 1.0) == kInvalidId,
                   "linear tolerance requires a point");
    dvatest::check(addFeature(model) == kInvalidId,
                   "feature requires a point");
    dvatest::check(addPointPointMeasure(model) == kInvalidId,
                   "point-point measure requires two points");
    dvatest::check(addTransformMove(model, {1.0, 0.0, 0.0}) == kInvalidId,
                   "transform move requires two parts");
}

TEST("model editing: add helpers reject invalid numeric inputs") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");

    dvatest::check(addCoordinatePoint(model, {nan, 0.0, 0.0}, baseId) ==
                       kInvalidId,
                   "coordinate point rejects non-finite position");
    dvatest::check(model.parts[0].points.empty(),
                   "invalid coordinate point is not stored");
    dvatest::check(model.parts[0].features.empty(),
                   "invalid coordinate point does not create feature");

    const PointId p1 = addCoordinatePoint(model, {0.0, 0.0, 0.0}, baseId);
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, baseId);
    dvatest::check(p1 != kInvalidId, "valid coordinate point still works");
    const std::size_t toleranceCount = model.parts[0].tolerances.size();
    const std::size_t gdtCount = model.parts[0].gdts.size();
    const std::size_t measureCount = model.measures.size();
    const std::size_t moveCount = model.moves.size();

    dvatest::check(addLinearTolerance(model, -0.1, baseId) == kInvalidId,
                   "linear tolerance rejects negative range");
    dvatest::check(addLinearTolerance(model, inf, baseId) == kInvalidId,
                   "linear tolerance rejects non-finite range");
    dvatest::check(model.parts[0].tolerances.size() == toleranceCount,
                   "invalid linear tolerance is not stored");

    dvatest::check(addGdt(model, GdtType::Position, -0.1, false, baseId) ==
                       kInvalidId,
                   "gdt rejects negative range");
    dvatest::check(addGdt(model, GdtType::Position, nan, false, baseId) ==
                       kInvalidId,
                   "gdt rejects non-finite range");
    dvatest::check(model.parts[0].gdts.size() == gdtCount,
                   "invalid gdt is not stored");

    dvatest::check(addPointPointMeasure(model, nan, 1.0) == kInvalidId,
                   "measure rejects non-finite lsl");
    dvatest::check(addPointPointMeasure(model, 0.0, inf) == kInvalidId,
                   "measure rejects non-finite usl");
    dvatest::check(model.measures.size() == measureCount,
                   "invalid measure is not stored");

    dvatest::check(addTransformMove(model, {0.0, inf, 0.0}, baseId,
                                    targetId) == kInvalidId,
                   "transform move rejects non-finite translation");
    dvatest::check(model.moves.size() == moveCount,
                   "invalid transform move is not stored");
}

TEST("model editing: update editable properties by id") {
    Model model;
    model.assemblyName = "Original";
    const PartId partId = addPart(model, "Base");
    const PointId p1 = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    const FeatureId featureId = model.parts[0].features[0].id;
    const FeatureId datumFeatureId = model.parts[0].features[1].id;
    const FeatureId secondaryFeatureId = addFeature(model, FeatureKind::Plane, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, partId);
    const GdtId gdtId = addGdt(model, GdtType::Flatness, 0.3, false, partId);
    const MeasureId measureId = addPointPointMeasure(model, 8.5, 11.5);
    const PartId targetPartId = addPart(model, "Target");
    const MoveId moveId = addTransformMove(model, {10.0, 0.0, 0.0},
                                           partId, targetPartId);

    dvatest::check(renameAssembly(model, "Door Assembly"), "rename assembly");
    dvatest::check(renamePart(model, partId, "Outer Panel"), "rename part");
    dvatest::check(setPointPosition(model, p1, {1.0, 2.0, 3.0}), "set point position");
    dvatest::check(setMeasureActive(model, measureId, false),
                   "deactivate measure before deactivating input point");
    dvatest::check(setPointActive(model, p1, false), "deactivate point");
    dvatest::check(setPointKind(model, p1, PointKind::Feature), "set point kind");
    dvatest::check(setPointHoleType(model, p1, HoleType::Hole),
                   "set point hole type");
    dvatest::check(setPointDiameter(model, p1, 4.5), "set point diameter");
    dvatest::check(setPointDirection(model, p1, {0.0, 3.0, 4.0}),
                   "set point direction");
    dvatest::check(setFeatureKind(model, featureId, FeatureKind::Sphere),
                   "set feature kind");
    dvatest::check(setFeatureDefiningPoints(model, featureId, {102, 101}),
                   "set feature defining points");
    dvatest::check(renameTolerance(model, toleranceId, "Panel Tol"), "rename tolerance");
    dvatest::check(setToleranceActive(model, toleranceId, false), "deactivate tolerance");
    dvatest::check(setToleranceDistribution(model, toleranceId,
                                            DistributionType::Uniform),
                   "set tolerance distribution");
    dvatest::check(setToleranceRange(model, toleranceId, 0.25), "set tolerance range");
    dvatest::check(setToleranceOffset(model, toleranceId, 0.05),
                   "set tolerance offset");
    dvatest::check(setToleranceSigmaNumber(model, toleranceId, 4.0),
                   "set tolerance sigma number");
    dvatest::check(setToleranceGeomRule(model, toleranceId,
                                        GeomRule::RotateAboutLocatorPoint),
                   "set tolerance geometry rule");
    dvatest::check(setToleranceRangeScale(model, toleranceId, 1.5),
                   "set tolerance range scale");
    dvatest::check(setToleranceDirection(model, toleranceId, {0.0, 3.0, 4.0}),
                   "set tolerance direction");
    dvatest::check(setToleranceTruncation(model, toleranceId, -0.5, 0.5, true),
                   "set tolerance truncation");
    dvatest::check(setToleranceFeatures(model, toleranceId,
                                        {featureId, secondaryFeatureId}),
                   "set tolerance features");
    RandSpec rand1;
    rand1.distribution = DistributionType::Uniform;
    rand1.range = 0.4;
    rand1.offset = 0.1;
    rand1.sigmaNum = 2.0;
    RandSpec rand2;
    rand2.distribution = DistributionType::Triangular;
    rand2.range = 0.6;
    rand2.offset = -0.2;
    rand2.sigmaNum = 5.0;
    dvatest::check(setToleranceRandomVariables(model, toleranceId, {rand1, rand2}),
                   "set tolerance random variables");
    dvatest::check(renameGdt(model, gdtId, "Panel Flatness"), "rename gdt");
    dvatest::check(setGdtActive(model, gdtId, false), "deactivate gdt");
    dvatest::check(setGdtType(model, gdtId, GdtType::Position), "set gdt type");
    dvatest::check(setGdtRange(model, gdtId, 0.15), "set gdt range");
    dvatest::check(setGdtDiametrical(model, gdtId, true), "set gdt diametrical");
    dvatest::check(setGdtDrf(model, gdtId,
                             {featureId, datumFeatureId, secondaryFeatureId}),
                   "set gdt drf");
    dvatest::check(setGdtFeatures(model, gdtId,
                                  {featureId, secondaryFeatureId}),
                   "set gdt controlled features");
    dvatest::check(renameMove(model, moveId, "Panel Transform"), "rename move");
    dvatest::check(setMoveActive(model, moveId, false), "deactivate move");
    dvatest::check(setMoveNominalBuild(model, moveId, true),
                   "set move nominal build");
    MovePair pair1;
    pair1.objectPoint = {10.0, 0.0, 0.0};
    pair1.targetPoint = {20.0, 0.0, 0.0};
    pair1.direction.ijk = {0.0, 0.0, 5.0};
    MovePair pair2;
    pair2.objectPoint = {1.0, 2.0, 3.0};
    pair2.targetPoint = {4.0, 5.0, 6.0};
    pair2.direction.ijk = {0.0, 3.0, 4.0};
    dvatest::check(setMovePairs(model, moveId, {pair1, pair2}), "set move pairs");
    dvatest::check(setTransformMoveTranslation(model, moveId, {2.0, 3.0, 4.0}),
                   "set transform move translation");
    dvatest::check(setMovePairDirection(model, moveId, 0, {0.0, 6.0, 8.0}),
                   "set move pair direction");
    dvatest::check(setMovePairObjectPoint(model, moveId, 0, {7.0, 8.0, 9.0}),
                   "set move pair object point");
    dvatest::check(setMovePairTargetPoint(model, moveId, 0, {11.0, 12.0, 13.0}),
                   "set move pair target point");
    dvatest::check(setMoveParts(model, moveId, {targetPartId, partId}),
                   "set move parts");
    dvatest::check(setMoveUserDllRoutine(model, moveId, "externalMove"),
                   "set move user-dll routine");
    dvatest::check(setMoveSearchAccuracy(model, moveId, 1e-4),
                   "set move search accuracy");
    dvatest::check(setMoveMaxIterations(model, moveId, 25),
                   "set move max iterations");
    dvatest::check(setMoveFloatActive(model, moveId, true),
                   "set move float active");
    dvatest::check(setMoveFloatSigmaNumber(model, moveId, 5),
                   "set move float sigma number");
    dvatest::check(setMoveFloatRangeScale(model, moveId, 1.25),
                   "set move float range scale");
    dvatest::check(setMoveFloatAngleRange(model, moveId, 120.0),
                   "set move float angle range");
    dvatest::check(setMoveFloatAngleOffset(model, moveId, -15.0),
                   "set move float angle offset");
    dvatest::check(setMoveType(model, moveId, MoveType::ThermalScaling),
                   "set move type");
    dvatest::check(renameMeasure(model, measureId, "Gap Flush"), "rename measure");
    dvatest::check(setMeasureSpec(model, measureId, 9.0, 10.5, true, true),
                   "set measure spec");
    dvatest::check(setMeasureType(model, measureId, MeasureType::NominalPoint),
                   "set measure type");
    dvatest::check(setMeasureInputPoints(model, measureId, {102, 101}),
                   "set measure input points");
    dvatest::check(setMeasureInputFeatures(model, measureId,
                                           {featureId, secondaryFeatureId}),
                   "set measure input features");
    dvatest::check(setMeasureSpecMode(model, measureId,
                                      SpecMode::RelativeToNominal),
                   "set measure spec mode");
    dvatest::check(setMeasureDirectionMode(model, measureId,
                                           DirectionMode::ProjectedOnPlane),
                   "set measure direction mode");
    dvatest::check(setMeasureDirection(model, measureId, {3.0, 4.0, 0.0}),
                   "set measure direction");
    dvatest::check(setMeasureScale(model, measureId, 2.5),
                   "set measure scale");
    dvatest::check(setMeasureEquation(model, measureId, "P1X + [VAL:1]"),
                   "set measure equation");
    dvatest::check(!setMeasureEquation(model, measureId, std::string(401, '1')),
                   "reject overlong measure equation line");
    dvatest::check(!setMeasureEquation(model, measureId, "SETCFG=BOGUSDATA\n2+3"),
                   "reject unsupported measure equation SETCFG");
    dvatest::check(!setMeasureEquation(model, measureId, "setcfg=lengthdata\n2+3"),
                   "reject lowercase measure equation SETCFG");
    dvatest::check(!setMeasureEquation(model, measureId, "2+3\nREM trailing"),
                   "reject trailing measure equation REM");
    dvatest::check(!setMeasureEquation(model, measureId, "2+3\n  REM trailing"),
                   "reject whitespace-prefixed trailing measure equation REM");
    dvatest::check(model.measures[0].def.equation == "P1X + [VAL:1]",
                   "invalid measure equation leaves value");
    dvatest::check(
        setMeasureEquation(model, measureId,
                           "2+3\nREM ignored string\nSETCFG=LENGTHDATA"),
        "allow SETCFG after measure equation REM");
    dvatest::check(model.measures[0].def.equation ==
                       "2+3\nREM ignored string\nSETCFG=LENGTHDATA",
                   "SETCFG after REM measure equation is stored");
    dvatest::check(setMeasureEquation(model, measureId, "P1X + [VAL:1]"),
                   "restore measure equation after SETCFG-after-REM check");
    dvatest::check(setMeasureValues(model, measureId, {2.0, 3.5}),
                   "set measure values");
    dvatest::check(setMeasureLslActive(model, measureId, false),
                   "disable measure lsl");
    dvatest::check(setMeasureUslActive(model, measureId, false),
                   "disable measure usl");
    dvatest::check(setMeasureActive(model, measureId, false),
                   "deactivate measure");
    dvatest::check(setMeasureAsOutput(model, measureId, false),
                   "disable measure output");

    dvatest::check(model.assemblyName == "Door Assembly", "assembly name changed");
    dvatest::check(model.parts[0].dcsName == "Outer Panel", "part dcsName changed");
    dvatest::checkNear(model.parts[0].points[0].position.x, 1.0, 1e-12, "point x");
    dvatest::checkNear(model.parts[0].points[0].position.y, 2.0, 1e-12, "point y");
    dvatest::checkNear(model.parts[0].points[0].position.z, 3.0, 1e-12, "point z");
    dvatest::check(!model.parts[0].points[0].active, "point inactive");
    dvatest::check(model.parts[0].points[0].kind == PointKind::Feature,
                   "point kind changed");
    dvatest::check(model.parts[0].points[0].holeType == HoleType::Hole,
                   "point hole type changed");
    dvatest::checkNear(model.parts[0].points[0].diameter, 4.5, 1e-12,
                       "point diameter");
    dvatest::checkNear(model.parts[0].points[0].ijk.x, 0.0, 1e-12,
                       "point direction i");
    dvatest::checkNear(model.parts[0].points[0].ijk.y, 0.6, 1e-12,
                       "point direction j");
    dvatest::checkNear(model.parts[0].points[0].ijk.z, 0.8, 1e-12,
                       "point direction k");
    dvatest::check(model.parts[0].features[0].kind == FeatureKind::Sphere,
                   "feature kind changed");
    dvatest::check(model.parts[0].features[0].definingPoints.size() == 2,
                   "feature defining point count");
    dvatest::check(model.parts[0].features[0].definingPoints[0] == 102,
                   "feature defining point 1");
    dvatest::check(model.parts[0].features[0].definingPoints[1] == 101,
                   "feature defining point 2");
    dvatest::check(model.parts[0].tolerances[0].name == "Panel Tol", "tol name");
    dvatest::check(!model.parts[0].tolerances[0].active, "tol inactive");
    dvatest::check(model.parts[0].tolerances[0].ir.rands.size() == 2,
                   "tol random variable count");
    dvatest::check(model.parts[0].tolerances[0].ir.rands[0].distribution ==
                       DistributionType::Uniform,
                   "tol distribution");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.rands[0].range, 0.4,
                       1e-12, "tol range");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.rands[0].offset, 0.1,
                       1e-12, "tol offset");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.rands[0].sigmaNum, 2.0,
                       1e-12, "tol sigma number");
    dvatest::check(model.parts[0].tolerances[0].ir.rands[1].distribution ==
                       DistributionType::Triangular,
                   "tol second distribution");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.rands[1].range, 0.6,
                       1e-12, "tol second range");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.rands[1].offset, -0.2,
                       1e-12, "tol second offset");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.rands[1].sigmaNum, 5.0,
                       1e-12, "tol second sigma number");
    dvatest::check(model.parts[0].tolerances[0].ir.geomRule ==
                       GeomRule::RotateAboutLocatorPoint,
                   "tol geometry rule");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.rangeScale, 1.5,
                       1e-12, "tol range scale");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.direction.ijk.x, 0.0,
                       1e-12, "tol direction i");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.direction.ijk.y, 0.6,
                       1e-12, "tol direction j");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.direction.ijk.z, 0.8,
                       1e-12, "tol direction k");
    dvatest::check(model.parts[0].tolerances[0].ir.truncation.active,
                   "tol truncation active");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.truncation.minTrunc,
                       -0.5, 1e-12, "tol min truncation");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.truncation.maxTrunc,
                       0.5, 1e-12, "tol max truncation");
    dvatest::check(model.parts[0].tolerances[0].features.size() == 2,
                   "tol feature count");
    dvatest::check(model.parts[0].tolerances[0].features[0] == featureId,
                   "tol feature 1");
    dvatest::check(model.parts[0].tolerances[0].features[1] == secondaryFeatureId,
                   "tol feature 2");
    dvatest::check(model.parts[0].gdts[0].name == "Panel Flatness", "gdt name");
    dvatest::check(model.parts[0].gdts[0].type == GdtType::Position, "gdt type");
    dvatest::check(!model.parts[0].gdts[0].active, "gdt inactive");
    dvatest::checkNear(model.parts[0].gdts[0].range, 0.15, 1e-12, "gdt range");
    dvatest::check(model.parts[0].gdts[0].diametrical, "gdt diametrical");
    dvatest::check(model.parts[0].gdts[0].drf.primary == featureId,
                   "gdt drf primary");
    dvatest::check(model.parts[0].gdts[0].drf.secondary == datumFeatureId,
                   "gdt drf secondary");
    dvatest::check(model.parts[0].gdts[0].drf.tertiary == secondaryFeatureId,
                   "gdt drf tertiary");
    dvatest::check(model.parts[0].gdts[0].features.size() == 2,
                   "gdt controlled feature count");
    dvatest::check(model.parts[0].gdts[0].features[0] == featureId,
                   "gdt controlled feature 1");
    dvatest::check(model.parts[0].gdts[0].features[1] == secondaryFeatureId,
                   "gdt controlled feature 2");
    dvatest::check(model.moves[0].name == "Panel Transform", "move name");
    dvatest::check(!model.moves[0].active, "move inactive");
    dvatest::check(model.moves[0].inputs.isNominalBuild,
                   "move nominal build");
    dvatest::checkNear(model.moves[0].inputs.pairs[0].objectPoint.x, 7.0,
                       1e-12, "move object x");
    dvatest::checkNear(model.moves[0].inputs.pairs[0].objectPoint.y, 8.0,
                       1e-12, "move object y");
    dvatest::checkNear(model.moves[0].inputs.pairs[0].objectPoint.z, 9.0,
                       1e-12, "move object z");
    dvatest::checkNear(model.moves[0].inputs.pairs[0].targetPoint.x, 11.0,
                       1e-12, "move translation x");
    dvatest::checkNear(model.moves[0].inputs.pairs[0].targetPoint.y, 12.0,
                       1e-12, "move translation y");
    dvatest::checkNear(model.moves[0].inputs.pairs[0].targetPoint.z, 13.0,
                       1e-12, "move translation z");
    dvatest::checkNear(model.moves[0].inputs.pairs[0].direction.ijk.x,
                       0.0, 1e-12, "move direction x");
    dvatest::checkNear(model.moves[0].inputs.pairs[0].direction.ijk.y,
                       0.6, 1e-12, "move direction y");
    dvatest::checkNear(model.moves[0].inputs.pairs[0].direction.ijk.z,
                       0.8, 1e-12, "move direction z");
    dvatest::check(model.moves[0].inputs.pairs.size() == 2,
                   "move pair count");
    dvatest::checkNear(model.moves[0].inputs.pairs[1].objectPoint.x, 1.0,
                       1e-12, "move second object x");
    dvatest::checkNear(model.moves[0].inputs.pairs[1].targetPoint.z, 6.0,
                       1e-12, "move second target z");
    dvatest::checkNear(model.moves[0].inputs.pairs[1].direction.ijk.y, 0.6,
                       1e-12, "move second direction y");
    dvatest::checkNear(model.moves[0].inputs.pairs[1].direction.ijk.z, 0.8,
                       1e-12, "move second direction z");
    dvatest::check(model.moves[0].moveParts.size() == 2, "move part count");
    dvatest::check(model.moves[0].moveParts[0] == targetPartId, "move part 1");
    dvatest::check(model.moves[0].moveParts[1] == partId, "move part 2");
    dvatest::check(model.moves[0].inputs.userDllRoutine == "externalMove",
                   "move user-dll routine changed");
    dvatest::check(model.moves[0].inputs.type == MoveType::ThermalScaling,
                   "move type");
    dvatest::checkNear(model.moves[0].inputs.searchAccuracy, 1e-4, 1e-12,
                       "move search accuracy");
    dvatest::check(model.moves[0].inputs.maxIterations == 25,
                   "move max iterations");
    dvatest::check(model.moves[0].inputs.hole_pin_float.active,
                   "move float active");
    dvatest::check(model.moves[0].inputs.hole_pin_float.sigmaNumber == 5,
                   "move float sigma number");
    dvatest::checkNear(model.moves[0].inputs.hole_pin_float.rangeScale, 1.25,
                       1e-12, "move float range scale");
    dvatest::checkNear(model.moves[0].inputs.hole_pin_float.angleRangeDeg,
                       120.0, 1e-12, "move float angle range");
    dvatest::checkNear(model.moves[0].inputs.hole_pin_float.angleOffsetDeg,
                       -15.0, 1e-12, "move float angle offset");
    dvatest::check(model.measures[0].name == "Gap Flush", "measure name");
    dvatest::check(model.measures[0].def.type == MeasureType::NominalPoint,
                   "measure type");
    dvatest::check(model.measures[0].def.inputPoints.size() == 2,
                   "measure input point count");
    dvatest::check(model.measures[0].def.inputPoints[0] == 102,
                   "measure input point 1");
    dvatest::check(model.measures[0].def.inputPoints[1] == 101,
                   "measure input point 2");
    dvatest::check(model.measures[0].def.inputFeatures.size() == 2,
                   "measure input feature count");
    dvatest::check(model.measures[0].def.inputFeatures[0] == featureId,
                   "measure input feature 1");
    dvatest::check(model.measures[0].def.inputFeatures[1] == secondaryFeatureId,
                   "measure input feature 2");
    dvatest::check(model.measures[0].def.spec.mode ==
                       SpecMode::RelativeToNominal,
                   "measure spec mode");
    dvatest::check(model.measures[0].def.dirMode ==
                       DirectionMode::ProjectedOnPlane,
                   "measure direction mode");
    dvatest::checkNear(model.measures[0].def.direction.ijk.x, 0.6, 1e-12,
                       "measure direction i");
    dvatest::checkNear(model.measures[0].def.direction.ijk.y, 0.8, 1e-12,
                       "measure direction j");
    dvatest::checkNear(model.measures[0].def.direction.ijk.z, 0.0, 1e-12,
                       "measure direction k");
    dvatest::checkNear(model.measures[0].def.scale, 2.5, 1e-12,
                       "measure scale");
    dvatest::check(model.measures[0].def.equation == "P1X + [VAL:1]",
                   "measure equation");
    dvatest::check(model.measures[0].def.values.size() == 2,
                   "measure value count");
    dvatest::checkNear(model.measures[0].def.values[0], 2.0, 1e-12,
                       "measure value 1");
    dvatest::checkNear(model.measures[0].def.values[1], 3.5, 1e-12,
                       "measure value 2");
    dvatest::checkNear(model.measures[0].def.spec.lsl, 9.0, 1e-12, "measure lsl");
    dvatest::checkNear(model.measures[0].def.spec.usl, 10.5, 1e-12, "measure usl");
    dvatest::check(!model.measures[0].def.spec.lslActive, "measure lsl inactive");
    dvatest::check(!model.measures[0].def.spec.uslActive, "measure usl inactive");
    dvatest::check(!model.measures[0].def.active, "measure inactive");
    dvatest::check(!model.measures[0].def.asOutput, "measure not output");
}

TEST("model editing: relationship setters reject duplicate references") {
    Model model;
    const PartId partId = addPart(model, "Base");
    const PointId pointA = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const PointId pointB = addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    const FeatureId featureA = model.parts[0].features[0].id;
    const FeatureId featureB = model.parts[0].features[1].id;
    const FeatureId featureC = addFeature(model, FeatureKind::Plane, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.4, partId);
    const GdtId gdtId = addGdt(model, GdtType::Flatness, 0.3, false, partId);
    const MeasureId measureId = addPointPointMeasure(model, 8.0, 12.0);
    const PartId targetPartId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, partId, targetPartId);

    dvatest::check(setFeatureDefiningPoints(model, featureC, {pointA, pointB}),
                   "feature baseline refs set");
    dvatest::check(setToleranceFeatures(model, toleranceId, {featureA, featureB}),
                   "tolerance baseline refs set");
    dvatest::check(setGdtFeatures(model, gdtId, {featureA, featureB}),
                   "gdt baseline refs set");
    dvatest::check(setMeasureInputFeatures(model, measureId, {featureA, featureB}),
                   "measure feature baseline refs set");

    dvatest::check(!setFeatureDefiningPoints(model, featureC, {pointA, pointA}),
                   "duplicate feature defining points rejected");
    dvatest::check(!setToleranceFeatures(model, toleranceId, {featureA, featureA}),
                   "duplicate tolerance features rejected");
    dvatest::check(!setGdtFeatures(model, gdtId, {featureA, featureA}),
                   "duplicate gdt features rejected");
    dvatest::check(!setMoveParts(model, moveId, {partId, partId}),
                   "duplicate move parts rejected");
    dvatest::check(!setMeasureInputPoints(model, measureId, {pointA, pointA}),
                   "duplicate measure input points rejected");
    dvatest::check(!setMeasureInputFeatures(model, measureId,
                                            {featureA, featureA}),
                   "duplicate measure input features rejected");

    dvatest::check(model.parts[0].features[2].definingPoints.size() == 2 &&
                       model.parts[0].features[2].definingPoints[0] == pointA &&
                       model.parts[0].features[2].definingPoints[1] == pointB,
                   "failed feature edit leaves value unchanged");
    dvatest::check(model.parts[0].tolerances[0].features.size() == 2 &&
                       model.parts[0].tolerances[0].features[0] == featureA &&
                       model.parts[0].tolerances[0].features[1] == featureB,
                   "failed tolerance edit leaves value unchanged");
    dvatest::check(model.parts[0].gdts[0].features.size() == 2 &&
                       model.parts[0].gdts[0].features[0] == featureA &&
                       model.parts[0].gdts[0].features[1] == featureB,
                   "failed gdt edit leaves value unchanged");
    dvatest::check(model.moves[0].moveParts.size() == 2 &&
                       model.moves[0].moveParts[0] == partId &&
                       model.moves[0].moveParts[1] == targetPartId,
                   "failed move part edit leaves value unchanged");
    dvatest::check(model.measures[0].def.inputPoints.size() == 2 &&
                       model.measures[0].def.inputPoints[0] == pointA &&
                       model.measures[0].def.inputPoints[1] == pointB,
                   "failed measure point edit leaves value unchanged");
    dvatest::check(model.measures[0].def.inputFeatures.size() == 2 &&
                       model.measures[0].def.inputFeatures[0] == featureA &&
                       model.measures[0].def.inputFeatures[1] == featureB,
                   "failed measure feature edit leaves value unchanged");
}

TEST("model editing: combination and equation measure refs can be edited") {
    Model model;
    const PartId partId = addPart(model, "Base");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    const MeasureId baseA = addPointPointMeasure(model, 8.0, 12.0);
    const MeasureId baseB = addPointPointMeasure(model, 9.0, 11.0);
    const MeasureId combo = addPointPointMeasure(model, 0.0, 20.0);
    const MeasureId equation = addPointPointMeasure(model, 0.0, 20.0);
    const MeasureId later = addPointPointMeasure(model, 1.0, 2.0);

    dvatest::check(setMeasureType(model, combo, MeasureType::Combination),
                   "set combination measure type");
    dvatest::check(setMeasureInputFeatures(model, combo, {baseA, baseB}),
                   "combination measure refs edited");
    dvatest::check(model.measures[2].def.inputFeatures.size() == 2 &&
                       model.measures[2].def.inputFeatures[0] == baseA &&
                       model.measures[2].def.inputFeatures[1] == baseB,
                   "combination measure refs stored");
    dvatest::check(!setMeasureInputFeatures(model, combo, {later}),
                   "active combination rejects later measure ref");
    dvatest::check(setMeasureActive(model, baseB, false),
                   "deactivate referenced measure");
    dvatest::check(!setMeasureInputFeatures(model, combo, {baseB}),
                   "active combination rejects inactive measure ref");
    dvatest::check(model.measures[2].def.inputFeatures.size() == 2 &&
                       model.measures[2].def.inputFeatures[0] == baseA &&
                       model.measures[2].def.inputFeatures[1] == baseB,
                   "invalid combination refs leave value unchanged");
    dvatest::check(setMeasureActive(model, baseB, true),
                   "reactivate referenced measure");
    dvatest::check(setMeasureActive(model, combo, false),
                   "deactivate combination before staging later ref");
    dvatest::check(setMeasureInputFeatures(model, combo, {later}),
                   "inactive combination can stage later measure ref");
    dvatest::check(!setMeasureActive(model, combo, true),
                   "reject activating combination with later measure ref");
    dvatest::check(!model.measures[2].def.active,
                   "invalid combination activation leaves value unchanged");
    dvatest::check(setMeasureInputFeatures(model, combo, {baseA, baseB}),
                   "restore valid combination refs");
    dvatest::check(setMeasureActive(model, combo, true),
                   "reactivate valid combination");

    const FeatureId featureId = model.parts[0].features[0].id;
    dvatest::check(setMeasureActive(model, combo, false),
                   "deactivate combination before staging feature input");
    dvatest::check(setMeasureType(model, combo, MeasureType::FeatureMeasure),
                   "stage feature measure before invalid combination type switch");
    dvatest::check(setMeasureInputFeatures(model, combo, {featureId}),
                   "stage feature input before invalid combination type switch");
    dvatest::check(setMeasureActive(model, combo, true),
                   "activate feature measure before invalid combination type switch");
    dvatest::check(!setMeasureType(model, combo, MeasureType::Combination),
                   "active combination type switch rejects feature input refs");
    dvatest::check(model.measures[2].def.type == MeasureType::FeatureMeasure,
                   "invalid combination type switch leaves value unchanged");
    dvatest::check(setMeasureActive(model, combo, false),
                   "deactivate feature measure before restoring combination");
    dvatest::check(setMeasureType(model, combo, MeasureType::Combination),
                   "restore inactive combination type");
    dvatest::check(setMeasureInputFeatures(model, combo, {baseA, baseB}),
                   "restore valid combination refs after type switch rejection");
    dvatest::check(setMeasureActive(model, combo, true),
                   "reactivate valid combination after type switch rejection");

    dvatest::check(setMeasureType(model, equation, MeasureType::Equation),
                   "set equation measure type");
    dvatest::check(setMeasureInputFeatures(model, equation, {baseA, combo}),
                   "equation measure refs edited");
    dvatest::check(model.measures[3].def.inputFeatures.size() == 2 &&
                       model.measures[3].def.inputFeatures[0] == baseA &&
                       model.measures[3].def.inputFeatures[1] == combo,
                   "equation measure refs stored");

    addOrReplaceModelVariant(model, captureActiveVariant(model, "MeasureRefs"));
    dvatest::check(deleteMeasure(model, combo),
                   "delete referenced combination measure");
    dvatest::check(model.measures.size() == 3,
                   "equation referencing deleted measure removed");
    dvatest::check(model.variants[0].measures.size() == 3,
                   "variant measure references cleaned after measure delete");
    dvatest::check(std::none_of(model.variants[0].measures.begin(),
                                model.variants[0].measures.end(),
                                [&](MeasureId id) {
                                    return id == combo || id == equation;
                                }),
                   "variant removes deleted and dependent measure refs");
}

TEST("model editing: active feature angle requires usable feature inputs") {
    Model model;
    const PartId partId = addPart(model, "Base");
    const PointId x0 = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const PointId x1 = addCoordinatePoint(model, {1.0, 0.0, 0.0}, partId);
    const PointId y0 = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const PointId y1 = addCoordinatePoint(model, {0.0, 1.0, 0.0}, partId);
    const FeatureId featureX = addFeature(model, FeatureKind::Edge, partId);
    const FeatureId featureY = addFeature(model, FeatureKind::Edge, partId);
    dvatest::check(setFeatureDefiningPoints(model, featureX, {x0, x1}),
                   "feature angle first feature vector");
    dvatest::check(setFeatureDefiningPoints(model, featureY, {y0, y1}),
                   "feature angle second feature vector");

    const MeasureId measureId = addPointPointMeasure(model, 0.0, 180.0);
    dvatest::check(setMeasureActive(model, measureId, false),
                   "deactivate feature angle while staging");
    dvatest::check(setMeasureType(model, measureId, MeasureType::FeatureAngle),
                   "stage feature angle type");
    dvatest::check(setMeasureInputPoints(model, measureId, {}),
                   "feature angle uses feature inputs instead of point inputs");
    dvatest::check(setMeasureInputFeatures(model, measureId,
                                           {featureX, featureY}),
                   "stage usable feature angle features");
    dvatest::check(setMeasureActive(model, measureId, true),
                   "activate feature angle with two usable features");

    dvatest::check(!setMeasureInputFeatures(model, measureId, {featureX}),
                   "reject active feature angle with one feature input");
    dvatest::check(model.measures[0].def.inputFeatures.size() == 2 &&
                       model.measures[0].def.inputFeatures[0] == featureX &&
                       model.measures[0].def.inputFeatures[1] == featureY,
                   "invalid feature angle inputs leave value");
    dvatest::check(setMeasureActive(model, measureId, false),
                   "deactivate feature angle before staging invalid inputs");
    dvatest::check(setMeasureInputFeatures(model, measureId, {featureX}),
                   "allow inactive feature angle with one feature input");
    dvatest::check(!setMeasureActive(model, measureId, true),
                   "reject activating feature angle with one feature input");
    dvatest::check(!model.measures[0].def.active,
                   "invalid feature angle activation leaves value");
}

TEST("model editing: active feature angle feature edits preserve usable inputs") {
    Model model;
    const PartId partId = addPart(model, "Base");
    const PointId x0 = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const PointId x1 = addCoordinatePoint(model, {1.0, 0.0, 0.0}, partId);
    const PointId y0 = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const PointId y1 = addCoordinatePoint(model, {0.0, 1.0, 0.0}, partId);
    const FeatureId featureX = addFeature(model, FeatureKind::Edge, partId);
    const FeatureId featureY = addFeature(model, FeatureKind::Edge, partId);
    dvatest::check(setFeatureDefiningPoints(model, featureX, {x0, x1}),
                   "feature angle first feature vector");
    dvatest::check(setFeatureDefiningPoints(model, featureY, {y0, y1}),
                   "feature angle second feature vector");

    const MeasureId measureId = addPointPointMeasure(model, 0.0, 180.0);
    dvatest::check(setMeasureActive(model, measureId, false),
                   "deactivate feature angle while staging");
    dvatest::check(setMeasureType(model, measureId, MeasureType::FeatureAngle),
                   "stage feature angle type");
    dvatest::check(setMeasureInputPoints(model, measureId, {}),
                   "feature angle uses feature inputs instead of point inputs");
    dvatest::check(setMeasureInputFeatures(model, measureId,
                                           {featureX, featureY}),
                   "stage usable feature angle features");
    dvatest::check(setMeasureActive(model, measureId, true),
                   "activate feature angle with two usable features");

    dvatest::check(!setFeatureDefiningPoints(model, featureX, {x0}),
                   "reject active feature angle edit leaving one-point feature");
    dvatest::check(model.parts[0].features[4].definingPoints.size() == 2 &&
                       model.parts[0].features[4].definingPoints[0] == x0 &&
                       model.parts[0].features[4].definingPoints[1] == x1,
                   "invalid feature angle feature edit leaves value");
    dvatest::check(setMeasureActive(model, measureId, false),
                   "deactivate feature angle before one-point feature edit");
    dvatest::check(setFeatureDefiningPoints(model, featureX, {x0}),
                   "allow inactive feature angle one-point feature edit");
}

TEST("model editing: active feature measure keeps defining point active") {
    Model model;
    const PartId partId = addPart(model, "Part");
    const PointId pointId = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    const FeatureId featureId = model.parts[0].features[0].id;
    const MeasureId measureId = addPointPointMeasure(model, 0.0, 1.0);
    dvatest::check(setMeasureActive(model, measureId, false),
                   "deactivate measure before staging feature measure");
    dvatest::check(setMeasureType(model, measureId, MeasureType::FeatureMeasure),
                   "stage feature measure type");
    dvatest::check(setMeasureInputPoints(model, measureId, {}),
                   "feature measure uses feature inputs instead of point inputs");
    dvatest::check(setMeasureInputFeatures(model, measureId, {featureId}),
                   "stage feature measure input feature");
    dvatest::check(setMeasureActive(model, measureId, true),
                   "activate feature measure");

    dvatest::check(!setPointActive(model, pointId, false),
                   "reject deactivating active feature measure defining point");
    dvatest::check(model.parts[0].points[0].active,
                   "invalid feature measure defining point deactivation leaves value");
    dvatest::check(setMeasureActive(model, measureId, false),
                   "deactivate feature measure before deactivating defining point");
    dvatest::check(setPointActive(model, pointId, false),
                   "allow inactive feature measure defining point deactivation");
}

TEST("model editing: feature measure activation requires active defining points") {
    Model model;
    const PartId partId = addPart(model, "Part");
    const PointId pointId = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    const FeatureId featureId = model.parts[0].features[0].id;
    const MeasureId measureId = addPointPointMeasure(model, 0.0, 1.0);
    dvatest::check(setMeasureActive(model, measureId, false),
                   "deactivate measure before staging feature measure");
    dvatest::check(setMeasureType(model, measureId, MeasureType::FeatureMeasure),
                   "stage feature measure type");
    dvatest::check(setMeasureInputPoints(model, measureId, {}),
                   "feature measure uses feature inputs instead of point inputs");
    dvatest::check(setMeasureInputFeatures(model, measureId, {featureId}),
                   "stage feature measure input feature");
    dvatest::check(setPointActive(model, pointId, false),
                   "stage inactive defining point while measure inactive");

    dvatest::check(!setMeasureActive(model, measureId, true),
                   "reject activating feature measure with inactive defining point");
    dvatest::check(!model.measures[0].def.active,
                   "invalid feature measure activation leaves value");
    dvatest::check(setPointActive(model, pointId, true),
                   "reactivate defining point before measure activation");
    dvatest::check(setMeasureActive(model, measureId, true),
                   "allow feature measure activation with active defining point");
}

TEST("model editing: active feature measure inputs require active defining points") {
    Model model;
    const PartId partId = addPart(model, "Part");
    const PointId activePointId = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    const FeatureId activeFeatureId = model.parts[0].features[0].id;
    const PointId inactivePointId =
        addCoordinatePoint(model, {20.0, 0.0, 0.0}, partId);
    const FeatureId inactiveFeatureId = model.parts[0].features[2].id;
    const MeasureId measureId = addPointPointMeasure(model, 0.0, 1.0);
    dvatest::check(setMeasureActive(model, measureId, false),
                   "deactivate measure before staging feature measure");
    dvatest::check(setMeasureType(model, measureId, MeasureType::FeatureMeasure),
                   "stage feature measure type");
    dvatest::check(setMeasureInputPoints(model, measureId, {}),
                   "feature measure uses feature inputs instead of point inputs");
    dvatest::check(setPointActive(model, inactivePointId, false),
                   "stage inactive candidate feature point");
    dvatest::check(setMeasureInputFeatures(model, measureId, {activeFeatureId}),
                   "stage active feature measure input");
    dvatest::check(setMeasureActive(model, measureId, true),
                   "activate feature measure with active input feature");

    dvatest::check(!setMeasureInputFeatures(model, measureId, {inactiveFeatureId}),
                   "reject active feature measure input with inactive defining point");
    dvatest::check(model.measures[0].def.inputFeatures.size() == 1 &&
                       model.measures[0].def.inputFeatures[0] == activeFeatureId,
                   "invalid feature measure inputs leave value");
    dvatest::check(setMeasureActive(model, measureId, false),
                   "deactivate feature measure before inactive input edit");
    dvatest::check(setMeasureInputFeatures(model, measureId, {inactiveFeatureId}),
                   "allow inactive feature measure input with inactive defining point");
    dvatest::check(activePointId != inactivePointId,
                   "test fixture uses distinct candidate points");
}

TEST("model editing: active tolerance keeps defining point active") {
    Model model;
    const PartId partId = addPart(model, "Part");
    const PointId pointId = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    addLinearTolerance(model, 0.5, partId);

    dvatest::check(!setPointActive(model, pointId, false),
                   "reject deactivating active tolerance defining point");
    dvatest::check(model.parts[0].points[0].active,
                   "invalid tolerance defining point deactivation leaves value");
    dvatest::check(setToleranceActive(model, model.parts[0].tolerances[0].id,
                                      false),
                   "deactivate tolerance before deactivating defining point");
    dvatest::check(setPointActive(model, pointId, false),
                   "allow inactive tolerance defining point deactivation");
}

TEST("model editing: active point position edits preserve measure inputs") {
    Model model;
    const PartId partId = addPart(model, "Part");
    const PointId p1 = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const PointId p2 = addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    addPointPointMeasure(model, 0.0, 12.0);

    model.measures[0].def.inputPoints = {p1, p1};

    dvatest::check(!setPointPosition(model, p1, {1.0, 2.0, 3.0}),
                   "reject active point position edit with invalid measure inputs");
    dvatest::checkNear(model.parts[0].points[0].position.x, 0.0, 1e-12,
                       "invalid point position edit leaves x");
    dvatest::checkNear(model.parts[0].points[0].position.y, 0.0, 1e-12,
                       "invalid point position edit leaves y");
    dvatest::checkNear(model.parts[0].points[0].position.z, 0.0, 1e-12,
                       "invalid point position edit leaves z");

    model.measures[0].def.inputPoints = {p1, p2};
    dvatest::check(setPointPosition(model, p1, {1.0, 2.0, 3.0}),
                   "allow active point position edit after restoring measure inputs");
}

TEST("model editing: active point direction edits preserve measure inputs") {
    Model model;
    const PartId partId = addPart(model, "Part");
    const PointId p1 = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const PointId p2 = addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    addPointPointMeasure(model, 0.0, 12.0);

    model.measures[0].def.inputPoints = {p1, p1};

    dvatest::check(!setPointDirection(model, p1, {1.0, 0.0, 0.0}),
                   "reject active point direction edit with invalid measure inputs");
    dvatest::checkNear(model.parts[0].points[0].ijk.x, 0.0, 1e-12,
                       "invalid point direction edit leaves x");
    dvatest::checkNear(model.parts[0].points[0].ijk.y, 0.0, 1e-12,
                       "invalid point direction edit leaves y");
    dvatest::checkNear(model.parts[0].points[0].ijk.z, 1.0, 1e-12,
                       "invalid point direction edit leaves z");

    model.measures[0].def.inputPoints = {p1, p2};
    dvatest::check(setPointDirection(model, p1, {1.0, 0.0, 0.0}),
                   "allow active point direction edit after restoring measure inputs");
}

TEST("model editing: active point diameter edits preserve measure inputs") {
    Model model;
    const PartId partId = addPart(model, "Part");
    const PointId p1 = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const PointId p2 = addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    addPointPointMeasure(model, 0.0, 12.0);

    model.measures[0].def.inputPoints = {p1, p1};

    dvatest::check(!setPointDiameter(model, p1, 4.5),
                   "reject active point diameter edit with invalid measure inputs");
    dvatest::checkNear(model.parts[0].points[0].diameter, 0.0, 1e-12,
                       "invalid point diameter edit leaves value");

    model.measures[0].def.inputPoints = {p1, p2};
    dvatest::check(setPointDiameter(model, p1, 4.5),
                   "allow active point diameter edit after restoring measure inputs");
}

TEST("model editing: active feature kind edits preserve tolerance state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const FeatureId featureId = model.parts[0].features[0].id;
    addLinearTolerance(model, 0.5, partId);

    model.parts[0].tolerances[0].ir.rangeScale = 0.0;

    dvatest::check(!setFeatureKind(model, featureId, FeatureKind::Sphere),
                   "reject active feature kind edit with invalid tolerance state");
    dvatest::check(model.parts[0].features[0].kind == FeatureKind::PointBased,
                   "invalid feature kind edit leaves value");

    model.parts[0].tolerances[0].ir.rangeScale = 1.0;
    dvatest::check(setFeatureKind(model, featureId, FeatureKind::Sphere),
                   "allow active feature kind edit after restoring tolerance state");
}

TEST("model editing: active tolerance direction keeps ref point active") {
    Model model;
    const PartId partId = addPart(model, "Part");
    const PointId directionPointId =
        addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const PointId featurePointId =
        addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, partId);
    model.parts[0].tolerances[0].ir.direction.type = DirectionType::TwoPoints;
    model.parts[0].tolerances[0].ir.direction.refPoints = {directionPointId,
                                                            featurePointId};

    dvatest::check(!setPointActive(model, directionPointId, false),
                   "reject deactivating active tolerance direction ref point");
    dvatest::check(model.parts[0].points[0].active,
                   "invalid tolerance direction ref deactivation leaves value");
    dvatest::check(setToleranceActive(model, toleranceId, false),
                   "deactivate tolerance before direction ref deactivation");
    dvatest::check(setPointActive(model, directionPointId, false),
                   "allow inactive tolerance direction ref deactivation");
}

TEST("model editing: tolerance activation requires active defining points") {
    Model model;
    const PartId partId = addPart(model, "Part");
    const PointId pointId = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, partId);
    dvatest::check(setToleranceActive(model, toleranceId, false),
                   "deactivate tolerance before staging inactive point");
    dvatest::check(setPointActive(model, pointId, false),
                   "stage inactive defining point while tolerance inactive");

    dvatest::check(!setToleranceActive(model, toleranceId, true),
                   "reject activating tolerance with inactive defining point");
    dvatest::check(!model.parts[0].tolerances[0].active,
                   "invalid tolerance activation leaves value");
    dvatest::check(setPointActive(model, pointId, true),
                   "reactivate defining point before tolerance activation");
    dvatest::check(setToleranceActive(model, toleranceId, true),
                   "allow tolerance activation with active defining point");
}

TEST("model editing: active tolerance features require active defining points") {
    Model model;
    const PartId partId = addPart(model, "Part");
    const PointId activePointId = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const FeatureId activeFeatureId = model.parts[0].features[0].id;
    const PointId inactivePointId =
        addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    const FeatureId inactiveFeatureId = model.parts[0].features[1].id;
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, partId);
    dvatest::check(setToleranceActive(model, toleranceId, false),
                   "deactivate tolerance before staging inactive feature point");
    dvatest::check(setPointActive(model, inactivePointId, false),
                   "stage inactive candidate feature point");
    dvatest::check(setToleranceFeatures(model, toleranceId, {activeFeatureId}),
                   "stage active tolerance feature");
    dvatest::check(setToleranceActive(model, toleranceId, true),
                   "activate tolerance with active feature");

    dvatest::check(!setToleranceFeatures(model, toleranceId, {inactiveFeatureId}),
                   "reject active tolerance feature with inactive defining point");
    dvatest::check(model.parts[0].tolerances[0].features.size() == 1 &&
                       model.parts[0].tolerances[0].features[0] ==
                           activeFeatureId,
                   "invalid tolerance features leave value");
    dvatest::check(setToleranceActive(model, toleranceId, false),
                   "deactivate tolerance before inactive feature edit");
    dvatest::check(setToleranceFeatures(model, toleranceId, {inactiveFeatureId}),
                   "allow inactive tolerance feature with inactive defining point");
    dvatest::check(activePointId != inactivePointId,
                   "test fixture uses distinct points");
}

TEST("model editing: active tolerance requires random variables") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, partId);
    const FeatureId featureId = model.parts[0].tolerances[0].features[0];

    dvatest::check(!setToleranceFeatures(model, toleranceId, {}),
                   "reject active tolerance without controlled features");
    dvatest::check(model.parts[0].tolerances[0].features.size() == 1 &&
                       model.parts[0].tolerances[0].features[0] == featureId,
                   "invalid active tolerance features leave value");

    dvatest::check(!setToleranceRandomVariables(model, toleranceId, {}),
                   "reject active tolerance without random variables");
    dvatest::check(model.parts[0].tolerances[0].ir.rands.size() == 1,
                   "invalid active tolerance random variables leave value");
    dvatest::check(setToleranceActive(model, toleranceId, false),
                   "deactivate tolerance before clearing random variables");
    dvatest::check(setToleranceRandomVariables(model, toleranceId, {}),
                   "allow inactive tolerance without random variables");
    dvatest::check(!setToleranceActive(model, toleranceId, true),
                   "reject activating tolerance without random variables");
    dvatest::check(!model.parts[0].tolerances[0].active,
                   "invalid tolerance random variable activation leaves value");
}

TEST("model editing: tolerance activation rejects invalid staged state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, partId);

    dvatest::check(setToleranceActive(model, toleranceId, false),
                   "deactivate tolerance before staging invalid state");
    model.parts[0].tolerances[0].ir.rangeScale = 0.0;
    dvatest::check(!setToleranceActive(model, toleranceId, true),
                   "reject activating tolerance with invalid range scale");
    dvatest::check(!model.parts[0].tolerances[0].active,
                   "invalid range-scale activation leaves value");

    model.parts[0].tolerances[0].ir.rangeScale = 1.0;
    model.parts[0].tolerances[0].ir.rands[0].range = -0.1;
    dvatest::check(!setToleranceActive(model, toleranceId, true),
                   "reject activating tolerance with negative rand range");
    dvatest::check(!model.parts[0].tolerances[0].active,
                   "negative-rand activation leaves value");

    model.parts[0].tolerances[0].ir.rands[0].range = 0.5;
    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 0.0;
    dvatest::check(!setToleranceActive(model, toleranceId, true),
                   "reject activating tolerance with invalid sigma");
    dvatest::check(!model.parts[0].tolerances[0].active,
                   "invalid-sigma activation leaves value");

    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 3.0;
    model.parts[0].tolerances[0].ir.truncation.active = true;
    model.parts[0].tolerances[0].ir.truncation.minTrunc = 1.0;
    model.parts[0].tolerances[0].ir.truncation.maxTrunc = -1.0;
    dvatest::check(!setToleranceActive(model, toleranceId, true),
                   "reject activating tolerance with misordered truncation");
    dvatest::check(!model.parts[0].tolerances[0].active,
                   "misordered-truncation activation leaves value");

    model.parts[0].tolerances[0].ir.truncation.active = false;
    model.parts[0].tolerances[0].ir.direction.ijk = {0.0, 0.0, 0.0};
    dvatest::check(!setToleranceActive(model, toleranceId, true),
                   "reject activating tolerance with zero direction");
    dvatest::check(!model.parts[0].tolerances[0].active,
                   "zero-direction activation leaves value");
}

TEST("model editing: active tolerance distribution rejects invalid staged state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, partId);

    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 0.0;
    dvatest::check(!setToleranceDistribution(model, toleranceId,
                                             DistributionType::Uniform),
                   "reject active tolerance distribution with invalid rands");
    dvatest::check(model.parts[0].tolerances[0].ir.rands[0].distribution ==
                       DistributionType::Normal,
                   "invalid active tolerance distribution leaves value");

    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 3.0;
    dvatest::check(setToleranceDistribution(model, toleranceId,
                                            DistributionType::Uniform),
                   "allow active tolerance distribution after restoring staged state");
}

TEST("model editing: active tolerance range rejects invalid staged state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, partId);

    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 0.0;
    dvatest::check(!setToleranceRange(model, toleranceId, 0.75),
                   "reject active tolerance range with invalid rands");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.rands[0].range, 0.5,
                       1e-12, "invalid active tolerance range leaves value");

    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 3.0;
    dvatest::check(setToleranceRange(model, toleranceId, 0.75),
                   "allow active tolerance range after restoring staged state");
}

TEST("model editing: active tolerance offset rejects invalid staged state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, partId);

    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 0.0;
    dvatest::check(!setToleranceOffset(model, toleranceId, 0.125),
                   "reject active tolerance offset with invalid rands");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.rands[0].offset, 0.0,
                       1e-12, "invalid active tolerance offset leaves value");

    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 3.0;
    dvatest::check(setToleranceOffset(model, toleranceId, 0.125),
                   "allow active tolerance offset after restoring staged state");
}

TEST("model editing: active tolerance sigma number rejects invalid staged state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, partId);

    model.parts[0].tolerances[0].ir.rands[0].offset =
        std::numeric_limits<double>::infinity();
    dvatest::check(!setToleranceSigmaNumber(model, toleranceId, 4.0),
                   "reject active tolerance sigma with invalid rands");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.rands[0].sigmaNum, 3.0,
                       1e-12, "invalid active tolerance sigma leaves value");

    model.parts[0].tolerances[0].ir.rands[0].offset = 0.0;
    dvatest::check(setToleranceSigmaNumber(model, toleranceId, 4.0),
                   "allow active tolerance sigma after restoring staged state");
}

TEST("model editing: active tolerance random variables reject invalid staged state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, partId);

    RandSpec rand;
    rand.distribution = DistributionType::Uniform;
    rand.range = 0.75;
    rand.offset = 0.125;
    rand.sigmaNum = 4.0;

    model.parts[0].tolerances[0].ir.direction.ijk = {0.0, 0.0, 0.0};
    dvatest::check(!setToleranceRandomVariables(model, toleranceId, {rand}),
                   "reject active tolerance random variables with invalid direction");
    dvatest::check(model.parts[0].tolerances[0].ir.rands.size() == 1,
                   "invalid active tolerance random variables leave size");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.rands[0].range, 0.5,
                       1e-12, "invalid active tolerance random variables leave range");

    model.parts[0].tolerances[0].ir.direction.ijk = {0.0, 0.0, 1.0};
    dvatest::check(setToleranceRandomVariables(model, toleranceId, {rand}),
                   "allow active tolerance random variables after restoring staged state");
}

TEST("model editing: active tolerance geom rule rejects invalid staged state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, partId);

    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 0.0;
    dvatest::check(!setToleranceGeomRule(model, toleranceId,
                                         GeomRule::RotateAboutLocatorPoint),
                   "reject active tolerance geom rule with invalid rands");
    dvatest::check(model.parts[0].tolerances[0].ir.geomRule ==
                       GeomRule::TranslateAlongVector,
                   "invalid active tolerance geom rule leaves value");

    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 3.0;
    dvatest::check(setToleranceGeomRule(model, toleranceId,
                                        GeomRule::RotateAboutLocatorPoint),
                   "allow active tolerance geom rule after restoring staged state");
}

TEST("model editing: active tolerance sample path rejects invalid staged state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, partId);

    model.parts[0].tolerances[0].ir.rands[0].userDefinedSamplePath = "old.smp";
    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 0.0;
    dvatest::check(!setToleranceUserDefinedSamplePath(model, toleranceId,
                                                      "new.smp"),
                   "reject active tolerance sample path with invalid rands");
    dvatest::check(model.parts[0].tolerances[0]
                       .ir.rands[0]
                       .userDefinedSamplePath == "old.smp",
                   "invalid active tolerance sample path leaves value");

    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 3.0;
    dvatest::check(setToleranceUserDefinedSamplePath(model, toleranceId,
                                                     "new.smp"),
                   "allow active tolerance sample path after restoring staged state");
}

TEST("model editing: active tolerance truncation rejects invalid staged state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, partId);

    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 0.0;
    dvatest::check(!setToleranceTruncation(model, toleranceId, -1.0, 1.0, true),
                   "reject active tolerance truncation with invalid rands");
    dvatest::check(!model.parts[0].tolerances[0].ir.truncation.active,
                   "invalid active tolerance truncation leaves value");

    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 3.0;
    dvatest::check(setToleranceTruncation(model, toleranceId, -1.0, 1.0, true),
                   "allow active tolerance truncation after restoring staged state");
}

TEST("model editing: active tolerance range scale rejects invalid staged state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, partId);

    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 0.0;
    dvatest::check(!setToleranceRangeScale(model, toleranceId, 1.25),
                   "reject active tolerance range scale with invalid rands");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.rangeScale, 1.0, 1e-12,
                       "invalid active tolerance range scale leaves value");

    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 3.0;
    dvatest::check(setToleranceRangeScale(model, toleranceId, 1.25),
                   "allow active tolerance range scale after restoring staged state");
}

TEST("model editing: active tolerance direction rejects invalid staged state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, partId);

    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 0.0;
    dvatest::check(!setToleranceDirection(model, toleranceId, {1.0, 0.0, 0.0}),
                   "reject active tolerance direction with invalid rands");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.direction.ijk.x, 0.0,
                       1e-12, "invalid active tolerance direction leaves x");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.direction.ijk.z, 1.0,
                       1e-12, "invalid active tolerance direction leaves z");

    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 3.0;
    dvatest::check(setToleranceDirection(model, toleranceId, {1.0, 0.0, 0.0}),
                   "allow active tolerance direction after restoring staged state");
}

TEST("model editing: active tolerance rename rejects invalid staged state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, partId);

    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 0.0;
    const std::string oldName = model.parts[0].tolerances[0].name;
    dvatest::check(!renameTolerance(model, toleranceId, "Blocked Tol"),
                   "reject active tolerance rename with invalid rands");
    dvatest::check(model.parts[0].tolerances[0].name == oldName,
                   "invalid active tolerance rename leaves value");

    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 3.0;
    dvatest::check(renameTolerance(model, toleranceId, "Allowed Tol"),
                   "allow active tolerance rename after restoring staged state");
}

TEST("model editing: active tolerance feature edits require active defining points") {
    Model model;
    const PartId partId = addPart(model, "Part");
    const PointId activePointId = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const FeatureId featureId = model.parts[0].features[0].id;
    const PointId inactivePointId =
        addCoordinatePoint(model, {0.0, 20.0, 0.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, partId);
    dvatest::check(setToleranceFeatures(model, toleranceId, {featureId}),
                   "stage active tolerance feature");
    dvatest::check(setPointActive(model, inactivePointId, false),
                   "stage inactive candidate defining point");

    dvatest::check(!setFeatureDefiningPoints(model, featureId, {inactivePointId}),
                   "reject active tolerance feature edit with inactive point");
    dvatest::check(model.parts[0].features[0].definingPoints.size() == 1 &&
                       model.parts[0].features[0].definingPoints[0] ==
                           activePointId,
                   "invalid active tolerance feature edit leaves value");
    dvatest::check(setToleranceActive(model, toleranceId, false),
                   "deactivate tolerance before inactive feature edit");
    dvatest::check(setFeatureDefiningPoints(model, featureId, {inactivePointId}),
                   "allow inactive tolerance feature edit with inactive point");
}

TEST("model editing: active gdt keeps controlled feature point active") {
    Model model;
    const PartId partId = addPart(model, "Part");
    const PointId pointId = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    addGdt(model, GdtType::Flatness, 0.3, false, partId);

    dvatest::check(!setPointActive(model, pointId, false),
                   "reject deactivating active gdt feature point");
    dvatest::check(model.parts[0].points[0].active,
                   "invalid gdt feature point deactivation leaves value");
    dvatest::check(setGdtActive(model, model.parts[0].gdts[0].id, false),
                   "deactivate gdt before deactivating controlled feature point");
    dvatest::check(setPointActive(model, pointId, false),
                   "allow inactive gdt feature point deactivation");
}

TEST("model editing: gdt activation requires active controlled feature points") {
    Model model;
    const PartId partId = addPart(model, "Part");
    const PointId pointId = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const GdtId gdtId = addGdt(model, GdtType::Flatness, 0.3, false, partId);
    dvatest::check(setGdtActive(model, gdtId, false),
                   "deactivate gdt before staging inactive feature point");
    dvatest::check(setPointActive(model, pointId, false),
                   "stage inactive controlled feature point while gdt inactive");

    dvatest::check(!setGdtActive(model, gdtId, true),
                   "reject activating gdt with inactive controlled feature point");
    dvatest::check(!model.parts[0].gdts[0].active,
                   "invalid gdt activation leaves value");
    dvatest::check(setPointActive(model, pointId, true),
                   "reactivate controlled feature point before gdt activation");
    dvatest::check(setGdtActive(model, gdtId, true),
                   "allow gdt activation with active controlled feature point");
}

TEST("model editing: gdt activation rejects invalid staged state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const GdtId gdtId = addGdt(model, GdtType::Flatness, 0.3, false, partId);
    const FeatureId featureId = model.parts[0].features[0].id;

    dvatest::check(setGdtActive(model, gdtId, false),
                   "deactivate gdt before staging invalid state");
    model.parts[0].gdts[0].range = -0.1;
    dvatest::check(!setGdtActive(model, gdtId, true),
                   "reject activating gdt with negative range");
    dvatest::check(!model.parts[0].gdts[0].active,
                   "negative-range activation leaves value");

    model.parts[0].gdts[0].range = 0.3;
    model.parts[0].gdts[0].features.clear();
    dvatest::check(!setGdtActive(model, gdtId, true),
                   "reject activating gdt without features");
    dvatest::check(!model.parts[0].gdts[0].active,
                   "missing-feature activation leaves value");

    model.parts[0].gdts[0].features = {featureId, featureId};
    dvatest::check(!setGdtActive(model, gdtId, true),
                   "reject activating gdt with duplicate features");
    dvatest::check(!model.parts[0].gdts[0].active,
                   "duplicate-feature activation leaves value");

    model.parts[0].gdts[0].features = {featureId};
    model.parts[0].gdts[0].type = GdtType::Position;
    model.parts[0].gdts[0].drf = {kInvalidId, featureId, kInvalidId};
    dvatest::check(!setGdtActive(model, gdtId, true),
                   "reject activating gdt with non-contiguous datum refs");
    dvatest::check(!model.parts[0].gdts[0].active,
                   "invalid-drf activation leaves value");
}

TEST("model editing: active gdt type switch rejects invalid staged state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const GdtId gdtId = addGdt(model, GdtType::Flatness, 0.3, false, partId);

    model.parts[0].gdts[0].range = -0.1;
    dvatest::check(!setGdtType(model, gdtId, GdtType::SurfaceProfile),
                   "reject active gdt type switch with invalid range");
    dvatest::check(model.parts[0].gdts[0].type == GdtType::Flatness,
                   "invalid active gdt type switch leaves value");

    model.parts[0].gdts[0].range = 0.3;
    dvatest::check(setGdtType(model, gdtId, GdtType::SurfaceProfile),
                   "allow active gdt type switch after restoring staged state");
}

TEST("model editing: active gdt rename rejects invalid staged state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const GdtId gdtId = addGdt(model, GdtType::Flatness, 0.3, false, partId);

    model.parts[0].gdts[0].range = -0.1;
    const std::string oldName = model.parts[0].gdts[0].name;
    dvatest::check(!renameGdt(model, gdtId, "Blocked GDT"),
                   "reject active gdt rename with invalid range");
    dvatest::check(model.parts[0].gdts[0].name == oldName,
                   "invalid active gdt rename leaves value");

    model.parts[0].gdts[0].range = 0.3;
    dvatest::check(renameGdt(model, gdtId, "Allowed GDT"),
                   "allow active gdt rename after restoring staged state");
}

TEST("model editing: active gdt range rejects invalid staged state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const GdtId gdtId = addGdt(model, GdtType::Flatness, 0.3, false, partId);

    model.parts[0].gdts[0].features.clear();
    dvatest::check(!setGdtRange(model, gdtId, 0.4),
                   "reject active gdt range with missing features");
    dvatest::checkNear(model.parts[0].gdts[0].range, 0.3, 1e-12,
                       "invalid active gdt range leaves value");

    model.parts[0].gdts[0].features = {model.parts[0].features[0].id};
    dvatest::check(setGdtRange(model, gdtId, 0.4),
                   "allow active gdt range after restoring staged state");
}

TEST("model editing: active gdt diametrical rejects invalid staged state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const GdtId gdtId = addGdt(model, GdtType::Flatness, 0.3, false, partId);

    dvatest::check(!model.parts[0].gdts[0].diametrical,
                   "starter gdt is non-diametrical");
    model.parts[0].gdts[0].features.clear();
    dvatest::check(!setGdtDiametrical(model, gdtId, true),
                   "reject active gdt diametrical with missing features");
    dvatest::check(!model.parts[0].gdts[0].diametrical,
                   "invalid active gdt diametrical leaves value");

    model.parts[0].gdts[0].features = {model.parts[0].features[0].id};
    dvatest::check(setGdtDiametrical(model, gdtId, true),
                   "allow active gdt diametrical after restoring staged state");
}

TEST("model editing: active gdt drf rejects invalid staged state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const FeatureId featureId = model.parts[0].features[0].id;
    const GdtId gdtId = addGdt(model, GdtType::Flatness, 0.3, false, partId);

    model.parts[0].gdts[0].type = GdtType::Position;
    model.parts[0].gdts[0].features.clear();
    dvatest::check(!setGdtDrf(model, gdtId, {featureId, kInvalidId,
                                             kInvalidId}),
                   "reject active gdt drf with missing features");
    dvatest::check(model.parts[0].gdts[0].drf.primary == kInvalidId,
                   "invalid active gdt drf leaves value");

    model.parts[0].gdts[0].features = {featureId};
    dvatest::check(setGdtDrf(model, gdtId, {featureId, kInvalidId,
                                            kInvalidId}),
                   "allow active gdt drf after restoring staged state");
}

TEST("model editing: active gdt features reject invalid staged state") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const FeatureId featureId = model.parts[0].features[0].id;
    const GdtId gdtId = addGdt(model, GdtType::Flatness, 0.3, false, partId);

    model.parts[0].gdts[0].type = GdtType::Position;
    model.parts[0].gdts[0].features.clear();
    dvatest::check(!setGdtFeatures(model, gdtId, {featureId}),
                   "reject active gdt features with missing required datum");
    dvatest::check(model.parts[0].gdts[0].features.empty(),
                   "invalid active gdt features leave value");

    model.parts[0].gdts[0].drf = {featureId, kInvalidId, kInvalidId};
    dvatest::check(setGdtFeatures(model, gdtId, {featureId}),
                   "allow active gdt features after restoring staged state");
}

TEST("model editing: active gdt features require active defining points") {
    Model model;
    const PartId partId = addPart(model, "Part");
    const PointId activePointId =
        addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const FeatureId activeFeatureId = model.parts[0].features[0].id;
    const PointId inactivePointId =
        addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    const FeatureId inactiveFeatureId = model.parts[0].features[1].id;
    const GdtId gdtId = addGdt(model, GdtType::Flatness, 0.3, false, partId);
    dvatest::check(setGdtActive(model, gdtId, false),
                   "deactivate gdt before staging inactive feature point");
    dvatest::check(setPointActive(model, inactivePointId, false),
                   "stage inactive candidate feature point");
    dvatest::check(setGdtFeatures(model, gdtId, {activeFeatureId}),
                   "stage active controlled feature");
    dvatest::check(setGdtActive(model, gdtId, true),
                   "activate gdt with active controlled feature");

    dvatest::check(!setGdtFeatures(model, gdtId, {}),
                   "reject active gdt without controlled features");
    dvatest::check(model.parts[0].gdts[0].features.size() == 1 &&
                       model.parts[0].gdts[0].features[0] == activeFeatureId,
                   "empty active gdt features leave value");

    dvatest::check(!setGdtFeatures(model, gdtId, {inactiveFeatureId}),
                   "reject active gdt feature with inactive defining point");
    dvatest::check(model.parts[0].gdts[0].features.size() == 1 &&
                       model.parts[0].gdts[0].features[0] == activeFeatureId,
                   "invalid gdt features leave value");
    dvatest::check(setGdtActive(model, gdtId, false),
                   "deactivate gdt before inactive feature edit");
    dvatest::check(setGdtFeatures(model, gdtId, {inactiveFeatureId}),
                   "allow inactive gdt feature with inactive defining point");
    dvatest::check(activePointId != inactivePointId,
                   "test fixture uses distinct points");
}

TEST("model editing: active gdt keeps datum feature point active") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const GdtId gdtId = addGdt(model, GdtType::Position, 0.3, false, partId);
    const PointId datumPointId =
        addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    const FeatureId datumFeatureId = model.parts[0].features[1].id;
    dvatest::check(setGdtDrf(model, gdtId, {datumFeatureId, kInvalidId,
                                            kInvalidId}),
                   "set active gdt primary datum");

    dvatest::check(!setPointActive(model, datumPointId, false),
                   "reject deactivating active gdt datum point");
    dvatest::check(model.parts[0].points[1].active,
                   "invalid gdt datum point deactivation leaves value");
    dvatest::check(setGdtActive(model, gdtId, false),
                   "deactivate gdt before deactivating datum point");
    dvatest::check(setPointActive(model, datumPointId, false),
                   "allow inactive gdt datum point deactivation");
}

TEST("model editing: active gdt drf requires active datum points") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const GdtId gdtId = addGdt(model, GdtType::Position, 0.3, false, partId);
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    const FeatureId activeDatumFeatureId = model.parts[0].features[1].id;
    const PointId inactiveDatumPointId =
        addCoordinatePoint(model, {0.0, 0.0, 20.0}, partId);
    const FeatureId inactiveDatumFeatureId = model.parts[0].features[2].id;
    dvatest::check(setGdtActive(model, gdtId, false),
                   "deactivate gdt before staging datum refs");
    dvatest::check(setPointActive(model, inactiveDatumPointId, false),
                   "stage inactive candidate datum point");
    dvatest::check(setGdtDrf(model, gdtId,
                             {activeDatumFeatureId, kInvalidId, kInvalidId}),
                   "stage active datum reference");
    dvatest::check(setGdtActive(model, gdtId, true),
                   "activate gdt with active datum reference");

    dvatest::check(!setGdtDrf(model, gdtId,
                              {kInvalidId, kInvalidId, kInvalidId}),
                   "reject active gdt drf missing required datum");
    dvatest::check(model.parts[0].gdts[0].drf.primary == activeDatumFeatureId,
                   "missing-required-datum gdt drf leaves primary datum");

    dvatest::check(!setGdtDrf(model, gdtId,
                              {inactiveDatumFeatureId, kInvalidId, kInvalidId}),
                   "reject active gdt drf with inactive datum point");
    dvatest::check(model.parts[0].gdts[0].drf.primary == activeDatumFeatureId,
                   "invalid gdt drf leaves primary datum");
    dvatest::check(setGdtActive(model, gdtId, false),
                   "deactivate gdt before inactive datum edit");
    dvatest::check(setGdtDrf(model, gdtId,
                             {inactiveDatumFeatureId, kInvalidId, kInvalidId}),
                   "allow inactive gdt drf with inactive datum point");
}

TEST("model editing: tolerance userdefined sample path can be edited") {
    Model model = createStarterModel();
    const ToleranceId toleranceId = model.parts[0].tolerances[0].id;

    dvatest::check(setToleranceUserDefinedSamplePath(
                       model, toleranceId, "samples/offsets.smp"),
                   "set userdefined sample path");
    dvatest::check(model.parts[0].tolerances[0]
                       .ir.rands[0]
                       .userDefinedSamplePath == "samples/offsets.smp",
                   "userdefined sample path stored");

    dvatest::check(!setToleranceUserDefinedSamplePath(model, 9999, "bad.smp"),
                   "missing tolerance sample path rejected");
    dvatest::check(model.parts[0].tolerances[0]
                       .ir.rands[0]
                       .userDefinedSamplePath == "samples/offsets.smp",
                   "failed sample path edit leaves value unchanged");

    dvatest::check(setToleranceUserDefinedSamplePath(model, toleranceId, ""),
                   "clear userdefined sample path");
    dvatest::check(model.parts[0].tolerances[0]
                       .ir.rands[0]
                       .userDefinedSamplePath.empty(),
                   "userdefined sample path cleared");
}

TEST("model editing: update missing ids returns false") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId = addTransformMove(model, {1.0, 0.0, 0.0},
                                           baseId, targetId);
    const PointId pointId = addCoordinatePoint(model, {0.0, 0.0, 0.0}, baseId);
    const PointId secondPointId =
        addCoordinatePoint(model, {0.0, 0.0, 10.0}, baseId);
    const FeatureId featureId = model.parts[0].features[0].id;
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, baseId);
    const FeatureId toleranceFeatureId = model.parts[0].tolerances[0].features[0];
    const GdtId gdtId = addGdt(model, GdtType::Flatness, 0.25, false, baseId);
    const FeatureId gdtFeatureId = model.parts[0].gdts[0].features[0];
    const MeasureId measureId = addPointPointMeasure(model, 0.0, 1.0);

    dvatest::check(!renamePart(model, 42, "Missing"), "missing part");
    dvatest::check(!setPointPosition(model, 42, {1.0, 2.0, 3.0}), "missing point");
    dvatest::check(!setPointActive(model, 42, true), "missing point active");
    dvatest::check(!setPointKind(model, 42, PointKind::Feature),
                   "missing point kind");
    dvatest::check(!setPointHoleType(model, 42, HoleType::Hole),
                   "missing point hole type");
    dvatest::check(!setPointDiameter(model, 42, 1.0), "missing point diameter");
    dvatest::check(!setPointDirection(model, 42, {0.0, 0.0, 1.0}),
                   "missing point direction");
    dvatest::check(!setFeatureKind(model, 42, FeatureKind::Sphere),
                   "missing feature kind");
    dvatest::check(!setFeatureDefiningPoints(model, 42, {101}),
                   "missing feature defining points");
    dvatest::check(!setFeatureDefiningPoints(model, featureId, {9999}),
                   "missing defining point rejected");
    dvatest::check(model.parts[0].features[0].definingPoints.size() == 1 &&
                       model.parts[0].features[0].definingPoints[0] == pointId,
                   "invalid defining points leave feature unchanged");
    dvatest::check(!renameTolerance(model, 42, "Missing"), "missing tolerance name");
    dvatest::check(!setToleranceActive(model, 42, true), "missing tolerance active");
    dvatest::check(!setToleranceDistribution(model, 42, DistributionType::Uniform),
                   "missing tolerance distribution");
    dvatest::check(!setToleranceRange(model, 42, 1.0), "missing tolerance range");
    dvatest::check(!setToleranceOffset(model, 42, 0.05),
                   "missing tolerance offset");
    dvatest::check(!setToleranceSigmaNumber(model, 42, 4.0),
                   "missing tolerance sigma number");
    dvatest::check(!setToleranceGeomRule(model, 42,
                                         GeomRule::RotateAboutLocatorPoint),
                   "missing tolerance geometry rule");
    dvatest::check(!setToleranceRangeScale(model, 42, 1.5),
                   "missing tolerance range scale");
    dvatest::check(!setToleranceDirection(model, 42, {0.0, 0.0, 1.0}),
                   "missing tolerance direction");
    model.parts[0].tolerances[0].ir.direction.type = DirectionType::TwoPoints;
    model.parts[0].tolerances[0].ir.direction.refPoints = {pointId};
    dvatest::check(setToleranceDirection(model, toleranceId, {0.0, 1.0, 0.0}),
                   "tolerance direction can be updated");
    dvatest::check(model.parts[0].tolerances[0].ir.direction.type ==
                           DirectionType::TypeIn &&
                       model.parts[0].tolerances[0].ir.direction.refPoints.empty(),
                   "tolerance direction update clears stale point references");
    dvatest::check(!setToleranceTruncation(model, 42, -0.5, 0.5, true),
                   "missing tolerance truncation");
    dvatest::check(setToleranceTruncation(model, toleranceId, -0.25, 0.25, true),
                   "valid tolerance truncation");
    dvatest::check(!setToleranceTruncation(model, toleranceId, 0.5, -0.5, true),
                   "misordered active tolerance truncation rejected");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.truncation.minTrunc,
                       -0.25, 1e-12,
                       "invalid tolerance truncation leaves minimum unchanged");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.truncation.maxTrunc,
                       0.25, 1e-12,
                       "invalid tolerance truncation leaves maximum unchanged");
    dvatest::check(!setToleranceFeatures(model, 42, {201}),
                   "missing tolerance features");
    dvatest::check(!setToleranceFeatures(model, toleranceId, {9999}),
                   "missing tolerance feature reference rejected");
    dvatest::check(model.parts[0].tolerances[0].features.size() == 1 &&
                       model.parts[0].tolerances[0].features[0] == toleranceFeatureId,
                   "invalid tolerance features leave tolerance unchanged");
    dvatest::check(!setToleranceRandomVariables(model, 42, {RandSpec{}}),
                   "missing tolerance random variables");
    dvatest::check(!renameGdt(model, 42, "Missing"), "missing gdt name");
    dvatest::check(!setGdtType(model, 42, GdtType::Position), "missing gdt type");
    dvatest::check(!setGdtActive(model, 42, true), "missing gdt active");
    dvatest::check(!setGdtRange(model, 42, 1.0), "missing gdt range");
    dvatest::check(!setGdtDiametrical(model, 42, true), "missing gdt diametrical");
    dvatest::check(!setGdtDrf(model, 42, {201, 202, 203}), "missing gdt drf");
    dvatest::check(!setGdtFeatures(model, 42, {201}),
                   "missing gdt controlled features");
    dvatest::check(!setGdtDrf(model, gdtId, {featureId, 9999, kInvalidId}),
                   "missing gdt drf reference rejected");
    dvatest::check(!setGdtDrf(model, gdtId,
                              {kInvalidId, featureId, kInvalidId}),
                   "gdt drf gap rejected");
    dvatest::check(!setGdtDrf(model, gdtId,
                              {featureId, featureId, kInvalidId}),
                   "duplicate gdt drf refs rejected");
    dvatest::check(!setGdtType(model, gdtId, GdtType::Position),
                   "reject active gdt type requiring missing datum");
    dvatest::check(model.parts[0].gdts[0].type == GdtType::Flatness,
                   "invalid gdt type leaves value unchanged");
    dvatest::check(setGdtActive(model, gdtId, false),
                   "deactivate gdt before datum-required type");
    dvatest::check(setGdtType(model, gdtId, GdtType::Position),
                   "allow inactive gdt type requiring missing datum");
    dvatest::check(!setGdtActive(model, gdtId, true),
                   "reject activating gdt type requiring missing datum");
    dvatest::check(!model.parts[0].gdts[0].active,
                   "invalid gdt activation leaves value unchanged");
    dvatest::check(model.parts[0].gdts[0].drf.primary == kInvalidId &&
                       model.parts[0].gdts[0].drf.secondary == kInvalidId &&
                       model.parts[0].gdts[0].drf.tertiary == kInvalidId,
                   "invalid gdt drf leaves gdt unchanged");
    dvatest::check(setGdtDrf(model, gdtId,
                             {featureId, kInvalidId, kInvalidId}),
                   "set required gdt datum");
    dvatest::check(setGdtActive(model, gdtId, true),
                   "activate datum-complete gdt");
    dvatest::check(!setGdtFeatures(model, gdtId, {9999}),
                   "missing gdt feature reference rejected");
    dvatest::check(model.parts[0].gdts[0].features.size() == 1 &&
                       model.parts[0].gdts[0].features[0] == gdtFeatureId,
                   "invalid gdt features leave gdt unchanged");
    dvatest::check(!renameMove(model, 42, "Missing"), "missing move name");
    dvatest::check(!setMoveType(model, 42, MoveType::ThermalScaling),
                   "missing move type");
    dvatest::check(!setMoveActive(model, 42, true), "missing move active");
    dvatest::check(!setMoveNominalBuild(model, 42, true),
                   "missing move nominal build");
    dvatest::check(!setTransformMoveTranslation(model, 42, {1.0, 0.0, 0.0}),
                   "missing move translation");
    dvatest::check(!setMovePairs(model, 42, {MovePair{}}), "missing move pairs");
    dvatest::check(!setMovePairDirection(model, 42, 0, {0.0, 0.0, 1.0}),
                   "missing move pair direction");
    dvatest::check(!setMovePairObjectPoint(model, 42, 0, {0.0, 0.0, 0.0}),
                   "missing move pair object point");
    dvatest::check(!setMovePairTargetPoint(model, 42, 0, {0.0, 0.0, 0.0}),
                   "missing move pair target point");
    model.moves[0].inputs.pairs[0].direction.type = DirectionType::TwoPoints;
    model.moves[0].inputs.pairs[0].direction.refPoints = {pointId};
    dvatest::check(setTransformMoveTranslation(model, moveId, {0.0, 2.0, 0.0}),
                   "transform translation can be updated");
    dvatest::check(model.moves[0].inputs.pairs[0].direction.type ==
                           DirectionType::TypeIn &&
                       model.moves[0].inputs.pairs[0].direction.refPoints.empty(),
                   "transform translation update clears stale point references");
    dvatest::check(!setMovePairObjectPoint(model, moveId, 99, {0.0, 0.0, 0.0}),
                   "out of range move pair object point");
    dvatest::check(!setMovePairTargetPoint(model, moveId, 99, {0.0, 0.0, 0.0}),
                   "out of range move pair target point");
    model.moves[0].inputs.pairs[0].direction.type = DirectionType::TwoPoints;
    model.moves[0].inputs.pairs[0].direction.refPoints = {pointId};
    dvatest::check(setMovePairDirection(model, moveId, 0, {0.0, 1.0, 0.0}),
                   "move pair direction can be updated");
    dvatest::check(model.moves[0].inputs.pairs[0].direction.type ==
                           DirectionType::TypeIn &&
                       model.moves[0].inputs.pairs[0].direction.refPoints.empty(),
                   "move pair direction update clears stale point references");
    dvatest::check(!setMoveParts(model, 42, {1, 2}), "missing move parts");
    dvatest::check(!setMoveUserDllRoutine(model, 42, "externalMove"),
                   "missing move user-dll routine");
    dvatest::check(!setMoveParts(model, moveId, {baseId, 9999}),
                   "missing move part reference rejected");
    dvatest::check(model.moves[0].moveParts.size() == 2 &&
                       model.moves[0].moveParts[0] == baseId &&
                       model.moves[0].moveParts[1] == targetId,
                   "invalid move parts leave move unchanged");
    MovePair invalidPair;
    invalidPair.direction.type = DirectionType::TwoPoints;
    invalidPair.direction.refPoints = {pointId, 9999};
    dvatest::check(!setMovePairs(model, moveId, {invalidPair}),
                   "missing move pair direction reference rejected");
    dvatest::check(model.moves[0].inputs.pairs.size() == 1 &&
                       model.moves[0].inputs.pairs[0].direction.refPoints.empty(),
                   "invalid move pairs leave move unchanged");
    dvatest::check(!setMoveSearchAccuracy(model, 42, 1e-4),
                   "missing move search accuracy");
    dvatest::check(!setMoveMaxIterations(model, 42, 25),
                   "missing move max iterations");
    dvatest::check(!setMoveFloatActive(model, 42, true),
                   "missing move float active");
    dvatest::check(!setMoveFloatSigmaNumber(model, 42, 5),
                   "missing move float sigma number");
    dvatest::check(!setMoveFloatRangeScale(model, 42, 1.25),
                   "missing move float range scale");
    dvatest::check(!setMoveFloatAngleRange(model, 42, 120.0),
                   "missing move float angle range");
    dvatest::check(!setMoveFloatAngleOffset(model, 42, -15.0),
                   "missing move float angle offset");
    dvatest::check(!renameMeasure(model, 42, "Missing"), "missing measure name");
    dvatest::check(!setMeasureSpec(model, 42, 0.0, 1.0, true, true),
                   "missing measure spec");
    dvatest::check(!setMeasureType(model, 42, MeasureType::NominalPoint),
                   "missing measure type");
    dvatest::check(!setMeasureInputPoints(model, 42, {101}),
                   "missing measure input points");
    dvatest::check(!setMeasureInputFeatures(model, 42, {201}),
                   "missing measure input features");
    dvatest::check(!setMeasureInputPoints(model, measureId, {pointId, 9999}),
                   "missing measure input point reference rejected");
    dvatest::check(model.measures[0].def.inputPoints.size() == 2 &&
                       model.measures[0].def.inputPoints[0] == pointId &&
                       model.measures[0].def.inputPoints[1] == secondPointId,
                   "invalid measure input points leave measure unchanged");
    dvatest::check(!setMeasureInputFeatures(model, measureId, {9999}),
                   "missing measure input feature reference rejected");
    dvatest::check(model.measures[0].def.inputFeatures.empty(),
                   "invalid measure input features leave measure unchanged");
    dvatest::check(!setMeasureSpecMode(model, 42, SpecMode::RelativeToNominal),
                   "missing measure spec mode");
    dvatest::check(!setMeasureDirectionMode(model, 42,
                                            DirectionMode::ProjectedOnPlane),
                   "missing measure direction mode");
    dvatest::check(!setMeasureDirection(model, 42, {0.0, 0.0, 1.0}),
                   "missing measure direction");
    model.measures[0].def.direction.type = DirectionType::TwoPoints;
    model.measures[0].def.direction.refPoints = {pointId};
    dvatest::check(setMeasureDirection(model, measureId, {1.0, 0.0, 0.0}),
                   "measure direction can be updated");
    dvatest::check(model.measures[0].def.direction.type == DirectionType::TypeIn &&
                       model.measures[0].def.direction.refPoints.empty(),
                   "measure direction update clears stale point references");
    dvatest::check(!setMeasureScale(model, 42, 2.5),
                   "missing measure scale");
    dvatest::check(!setMeasureEquation(model, 42, "P1X"),
                   "missing measure equation");
    dvatest::check(!setMeasureValues(model, 42, {1.0}),
                   "missing measure values");
    dvatest::check(!setMeasureLslActive(model, 42, true),
                   "missing measure lsl active");
    dvatest::check(!setMeasureUslActive(model, 42, true),
                   "missing measure usl active");
    dvatest::check(!setMeasureActive(model, 42, true), "missing measure active");
    dvatest::check(!setMeasureAsOutput(model, 42, true),
                   "missing measure output");
}

TEST("model editing: tolerance enum setters reject invalid values") {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, partId);

    dvatest::check(!setToleranceDistribution(
                       model, toleranceId, static_cast<DistributionType>(999)),
                   "reject invalid tolerance distribution");
    dvatest::check(model.parts[0].tolerances[0].ir.rands[0].distribution ==
                       DistributionType::Normal,
                   "invalid tolerance distribution leaves value");
    dvatest::check(!setToleranceGeomRule(model, toleranceId,
                                         static_cast<GeomRule>(999)),
                   "reject invalid tolerance geom rule");
    dvatest::check(model.parts[0].tolerances[0].ir.geomRule ==
                       GeomRule::TranslateAlongVector,
                   "invalid tolerance geom rule leaves value");
}

TEST("model editing: object enum setters reject invalid values") {
    Model model;
    const PartId partId = addPart(model, "Part");
    const PointId pointId = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const FeatureId featureId = model.parts[0].features[0].id;
    const GdtId gdtId = addGdt(model, GdtType::Flatness, 0.25, false, partId);

    dvatest::check(!setPointKind(model, pointId, static_cast<PointKind>(999)),
                   "reject invalid point kind");
    dvatest::check(model.parts[0].points[0].kind == PointKind::Coordinate,
                   "invalid point kind leaves value");
    dvatest::check(!setPointHoleType(model, pointId, static_cast<HoleType>(999)),
                   "reject invalid point hole type");
    dvatest::check(model.parts[0].points[0].holeType == HoleType::None,
                   "invalid point hole type leaves value");
    dvatest::check(!setFeatureKind(model, featureId,
                                   static_cast<FeatureKind>(999)),
                   "reject invalid feature kind");
    dvatest::check(model.parts[0].features[0].kind == FeatureKind::PointBased,
                   "invalid feature kind leaves value");
    dvatest::check(!setGdtType(model, gdtId, static_cast<GdtType>(999)),
                   "reject invalid gdt type");
    dvatest::check(model.parts[0].gdts[0].type == GdtType::Flatness,
                   "invalid gdt type leaves value");
}

TEST("model editing: move and measure enum setters reject invalid values") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, baseId);
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, baseId);
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);
    const MeasureId measureId = addPointPointMeasure(model, 0.0, 1.0);

    dvatest::check(!setMoveType(model, moveId, static_cast<MoveType>(999)),
                   "reject invalid move type");
    dvatest::check(model.moves[0].inputs.type == MoveType::Transform,
                   "invalid move type leaves value");
    dvatest::check(!setMeasureType(model, measureId,
                                   static_cast<MeasureType>(999)),
                   "reject invalid measure type");
    dvatest::check(model.measures[0].def.type == MeasureType::PointPoint,
                   "invalid measure type leaves value");
}

TEST("model editing: move activation rejects invalid staged state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, baseId);
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, baseId);
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(setMoveActive(model, moveId, false),
                   "deactivate move before staging invalid state");
    model.moves[0].inputs.searchAccuracy = 0.0;
    dvatest::check(!setMoveActive(model, moveId, true),
                   "reject activating move with invalid search accuracy");
    dvatest::check(!model.moves[0].active,
                   "invalid move activation leaves value");

    model.moves[0].inputs.searchAccuracy = 1e-5;
    model.moves[0].moveParts = {baseId, baseId};
    dvatest::check(!setMoveActive(model, moveId, true),
                   "reject activating move with duplicate parts");
    dvatest::check(!model.moves[0].active,
                   "duplicate-part activation leaves value");

    model.moves[0].moveParts = {baseId, targetId};
    model.moves[0].inputs.pairs[0].direction.ijk = {0.0, 0.0, 0.0};
    dvatest::check(!setMoveActive(model, moveId, true),
                   "reject activating move with zero direction");
    dvatest::check(!model.moves[0].active,
                   "zero-direction activation leaves value");
}

TEST("model editing: active move type switch rejects invalid staged state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].inputs.searchAccuracy = 0.0;
    dvatest::check(!setMoveType(model, moveId, MoveType::ThermalScaling),
                   "reject active move type switch with invalid controls");
    dvatest::check(model.moves[0].inputs.type == MoveType::Transform,
                   "invalid active move type switch leaves value");

    model.moves[0].inputs.searchAccuracy = 1e-5;
    dvatest::check(setMoveType(model, moveId, MoveType::ThermalScaling),
                   "allow active move type switch after restoring staged state");
}

TEST("model editing: active move rename rejects invalid staged state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].inputs.searchAccuracy = 0.0;
    const std::string oldName = model.moves[0].name;
    dvatest::check(!renameMove(model, moveId, "Blocked Move"),
                   "reject active move rename with invalid controls");
    dvatest::check(model.moves[0].name == oldName,
                   "invalid active move rename leaves value");

    model.moves[0].inputs.searchAccuracy = 1e-5;
    dvatest::check(renameMove(model, moveId, "Allowed Move"),
                   "allow active move rename after restoring staged state");
}

TEST("model editing: active move nominal build rejects invalid staged state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].moveParts = {baseId, baseId};
    dvatest::check(!setMoveNominalBuild(model, moveId, true),
                   "reject active move nominal build with duplicate parts");
    dvatest::check(!model.moves[0].inputs.isNominalBuild,
                   "invalid active move nominal build leaves value");

    model.moves[0].moveParts = {baseId, targetId};
    dvatest::check(setMoveNominalBuild(model, moveId, true),
                   "allow active move nominal build after restoring staged state");
}

TEST("model editing: active transform move translation rejects invalid move state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].moveParts = {baseId, baseId};
    const Vec3 oldTarget = model.moves[0].inputs.pairs[0].targetPoint;
    dvatest::check(!setTransformMoveTranslation(model, moveId,
                                                {2.0, 0.0, 0.0}),
                   "reject active transform translation with duplicate parts");
    dvatest::checkNear(model.moves[0].inputs.pairs[0].targetPoint.x,
                       oldTarget.x, 1e-12,
                       "invalid active transform translation leaves target x");

    model.moves[0].moveParts = {baseId, targetId};
    dvatest::check(setTransformMoveTranslation(model, moveId,
                                               {2.0, 0.0, 0.0}),
                   "allow active transform translation after restoring move state");
}

TEST("model editing: active move pairs reject invalid staged state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].inputs.hole_pin_float.active = true;
    model.moves[0].inputs.hole_pin_float.sigmaNumber = 0;
    const Vec3 oldTarget = model.moves[0].inputs.pairs[0].targetPoint;
    MovePair replacement;
    replacement.objectPoint = {2.0, 0.0, 0.0};
    replacement.targetPoint = {3.0, 0.0, 0.0};
    replacement.direction.ijk = {1.0, 0.0, 0.0};
    dvatest::check(!setMovePairs(model, moveId, {replacement}),
                   "reject active move pairs with invalid controls");
    dvatest::checkNear(model.moves[0].inputs.pairs[0].targetPoint.x,
                       oldTarget.x, 1e-12,
                       "invalid active move pairs leave target x");

    model.moves[0].inputs.hole_pin_float.sigmaNumber = 3;
    dvatest::check(setMovePairs(model, moveId, {replacement}),
                   "allow active move pairs after restoring staged state");
}

TEST("model editing: active move pair direction rejects invalid staged state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].inputs.hole_pin_float.active = true;
    model.moves[0].inputs.hole_pin_float.sigmaNumber = 0;
    const Vec3 oldDirection = model.moves[0].inputs.pairs[0].direction.ijk;
    dvatest::check(!setMovePairDirection(model, moveId, 0, {0.0, 1.0, 0.0}),
                   "reject active move pair direction with invalid controls");
    dvatest::checkNear(model.moves[0].inputs.pairs[0].direction.ijk.x,
                       oldDirection.x, 1e-12,
                       "invalid active move pair direction leaves x");

    model.moves[0].inputs.hole_pin_float.sigmaNumber = 3;
    dvatest::check(setMovePairDirection(model, moveId, 0, {0.0, 1.0, 0.0}),
                   "allow active move pair direction after restoring staged state");
}

TEST("model editing: active move pair object point rejects invalid staged state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].inputs.hole_pin_float.active = true;
    model.moves[0].inputs.hole_pin_float.sigmaNumber = 0;
    const Vec3 oldObjectPoint = model.moves[0].inputs.pairs[0].objectPoint;
    dvatest::check(!setMovePairObjectPoint(model, moveId, 0,
                                           {2.0, 0.0, 0.0}),
                   "reject active move pair object point with invalid controls");
    dvatest::checkNear(model.moves[0].inputs.pairs[0].objectPoint.x,
                       oldObjectPoint.x, 1e-12,
                       "invalid active move pair object point leaves x");

    model.moves[0].inputs.hole_pin_float.sigmaNumber = 3;
    dvatest::check(setMovePairObjectPoint(model, moveId, 0, {2.0, 0.0, 0.0}),
                   "allow active move pair object point after restoring staged state");
}

TEST("model editing: active move pair target point rejects invalid staged state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].inputs.hole_pin_float.active = true;
    model.moves[0].inputs.hole_pin_float.sigmaNumber = 0;
    const Vec3 oldTargetPoint = model.moves[0].inputs.pairs[0].targetPoint;
    dvatest::check(!setMovePairTargetPoint(model, moveId, 0,
                                           {3.0, 0.0, 0.0}),
                   "reject active move pair target point with invalid controls");
    dvatest::checkNear(model.moves[0].inputs.pairs[0].targetPoint.x,
                       oldTargetPoint.x, 1e-12,
                       "invalid active move pair target point leaves x");

    model.moves[0].inputs.hole_pin_float.sigmaNumber = 3;
    dvatest::check(setMovePairTargetPoint(model, moveId, 0, {3.0, 0.0, 0.0}),
                   "allow active move pair target point after restoring staged state");
}

TEST("model editing: active move parts reject invalid staged state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].inputs.hole_pin_float.active = true;
    model.moves[0].inputs.hole_pin_float.sigmaNumber = 0;
    const PartId oldFirstPart = model.moves[0].moveParts[0];
    dvatest::check(!setMoveParts(model, moveId, {targetId, baseId}),
                   "reject active move parts with invalid controls");
    dvatest::check(model.moves[0].moveParts[0] == oldFirstPart,
                   "invalid active move parts leave first part");

    model.moves[0].inputs.hole_pin_float.sigmaNumber = 3;
    dvatest::check(setMoveParts(model, moveId, {targetId, baseId}),
                   "allow active move parts after restoring staged state");
}

TEST("model editing: active move float toggle rejects invalid staged state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].inputs.hole_pin_float.sigmaNumber = 0;
    dvatest::check(!setMoveFloatActive(model, moveId, true),
                   "reject active move float toggle with invalid controls");
    dvatest::check(!model.moves[0].inputs.hole_pin_float.active,
                   "invalid active move float toggle leaves value");

    model.moves[0].inputs.hole_pin_float.sigmaNumber = 3;
    dvatest::check(setMoveFloatActive(model, moveId, true),
                   "allow active move float toggle after restoring staged state");
}

TEST("model editing: active move float toggle rejects invalid move state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].moveParts = {baseId, baseId};
    dvatest::check(!setMoveFloatActive(model, moveId, true),
                   "reject active move float toggle with duplicate parts");
    dvatest::check(!model.moves[0].inputs.hole_pin_float.active,
                   "invalid active move float toggle leaves value");

    model.moves[0].moveParts = {baseId, targetId};
    dvatest::check(setMoveFloatActive(model, moveId, true),
                   "allow active move float toggle after restoring move state");
}

TEST("model editing: active move search accuracy rejects invalid staged state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].inputs.hole_pin_float.active = true;
    model.moves[0].inputs.hole_pin_float.sigmaNumber = 0;
    dvatest::check(!setMoveSearchAccuracy(model, moveId, 2e-5),
                   "reject active move search accuracy with invalid controls");
    dvatest::checkNear(model.moves[0].inputs.searchAccuracy, 1e-5, 1e-12,
                       "invalid active move search accuracy leaves value");

    model.moves[0].inputs.hole_pin_float.sigmaNumber = 3;
    dvatest::check(setMoveSearchAccuracy(model, moveId, 2e-5),
                   "allow active move search accuracy after restoring staged state");
}

TEST("model editing: active move max iterations rejects invalid staged state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].inputs.hole_pin_float.active = true;
    model.moves[0].inputs.hole_pin_float.sigmaNumber = 0;
    const int oldMaxIterations = model.moves[0].inputs.maxIterations;
    dvatest::check(!setMoveMaxIterations(model, moveId, 50),
                   "reject active move max iterations with invalid controls");
    dvatest::check(model.moves[0].inputs.maxIterations == oldMaxIterations,
                   "invalid active move max iterations leaves value");

    model.moves[0].inputs.hole_pin_float.sigmaNumber = 3;
    dvatest::check(setMoveMaxIterations(model, moveId, 50),
                   "allow active move max iterations after restoring staged state");
}

TEST("model editing: active move float sigma rejects invalid staged state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].inputs.hole_pin_float.active = true;
    model.moves[0].inputs.hole_pin_float.rangeScale = 0.0;
    const int oldSigmaNumber =
        model.moves[0].inputs.hole_pin_float.sigmaNumber;
    dvatest::check(!setMoveFloatSigmaNumber(model, moveId, 4),
                   "reject active move float sigma with invalid controls");
    dvatest::check(model.moves[0].inputs.hole_pin_float.sigmaNumber ==
                       oldSigmaNumber,
                   "invalid active move float sigma leaves value");

    model.moves[0].inputs.hole_pin_float.rangeScale = 1.0;
    dvatest::check(setMoveFloatSigmaNumber(model, moveId, 4),
                   "allow active move float sigma after restoring staged state");
}

TEST("model editing: active move float sigma rejects invalid move state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].inputs.hole_pin_float.active = true;
    model.moves[0].moveParts = {baseId, baseId};
    const int oldSigmaNumber =
        model.moves[0].inputs.hole_pin_float.sigmaNumber;
    dvatest::check(!setMoveFloatSigmaNumber(model, moveId, 4),
                   "reject active move float sigma with duplicate parts");
    dvatest::check(model.moves[0].inputs.hole_pin_float.sigmaNumber ==
                       oldSigmaNumber,
                   "invalid active move float sigma leaves value");

    model.moves[0].moveParts = {baseId, targetId};
    dvatest::check(setMoveFloatSigmaNumber(model, moveId, 4),
                   "allow active move float sigma after restoring move state");
}

TEST("model editing: active move float range scale rejects invalid staged state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].inputs.hole_pin_float.active = true;
    model.moves[0].inputs.hole_pin_float.sigmaNumber = 0;
    const double oldRangeScale =
        model.moves[0].inputs.hole_pin_float.rangeScale;
    dvatest::check(!setMoveFloatRangeScale(model, moveId, 1.5),
                   "reject active move float range scale with invalid controls");
    dvatest::checkNear(model.moves[0].inputs.hole_pin_float.rangeScale,
                       oldRangeScale, 1e-12,
                       "invalid active move float range scale leaves value");

    model.moves[0].inputs.hole_pin_float.sigmaNumber = 3;
    dvatest::check(setMoveFloatRangeScale(model, moveId, 1.5),
                   "allow active move float range scale after restoring staged state");
}

TEST("model editing: active move float range scale rejects invalid move state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].inputs.hole_pin_float.active = true;
    model.moves[0].moveParts = {baseId, baseId};
    const double oldRangeScale =
        model.moves[0].inputs.hole_pin_float.rangeScale;
    dvatest::check(!setMoveFloatRangeScale(model, moveId, 1.5),
                   "reject active move float range scale with duplicate parts");
    dvatest::checkNear(model.moves[0].inputs.hole_pin_float.rangeScale,
                       oldRangeScale, 1e-12,
                       "invalid active move float range scale leaves value");

    model.moves[0].moveParts = {baseId, targetId};
    dvatest::check(setMoveFloatRangeScale(model, moveId, 1.5),
                   "allow active move float range scale after restoring move state");
}

TEST("model editing: active move float angle range rejects invalid staged state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].inputs.hole_pin_float.active = true;
    model.moves[0].inputs.hole_pin_float.sigmaNumber = 0;
    const double oldAngleRange =
        model.moves[0].inputs.hole_pin_float.angleRangeDeg;
    dvatest::check(!setMoveFloatAngleRange(model, moveId, 180.0),
                   "reject active move float angle range with invalid controls");
    dvatest::checkNear(model.moves[0].inputs.hole_pin_float.angleRangeDeg,
                       oldAngleRange, 1e-12,
                       "invalid active move float angle range leaves value");

    model.moves[0].inputs.hole_pin_float.sigmaNumber = 3;
    dvatest::check(setMoveFloatAngleRange(model, moveId, 180.0),
                   "allow active move float angle range after restoring staged state");
}

TEST("model editing: active move float angle range rejects invalid move state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].inputs.hole_pin_float.active = true;
    model.moves[0].moveParts = {baseId, baseId};
    const double oldAngleRange =
        model.moves[0].inputs.hole_pin_float.angleRangeDeg;
    dvatest::check(!setMoveFloatAngleRange(model, moveId, 180.0),
                   "reject active move float angle range with duplicate parts");
    dvatest::checkNear(model.moves[0].inputs.hole_pin_float.angleRangeDeg,
                       oldAngleRange, 1e-12,
                       "invalid active move float angle range leaves value");

    model.moves[0].moveParts = {baseId, targetId};
    dvatest::check(setMoveFloatAngleRange(model, moveId, 180.0),
                   "allow active move float angle range after restoring move state");
}

TEST("model editing: active move float angle offset rejects invalid staged state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].inputs.hole_pin_float.active = true;
    model.moves[0].inputs.hole_pin_float.sigmaNumber = 0;
    const double oldAngleOffset =
        model.moves[0].inputs.hole_pin_float.angleOffsetDeg;
    dvatest::check(!setMoveFloatAngleOffset(model, moveId, -20.0),
                   "reject active move float angle offset with invalid controls");
    dvatest::checkNear(model.moves[0].inputs.hole_pin_float.angleOffsetDeg,
                       oldAngleOffset, 1e-12,
                       "invalid active move float angle offset leaves value");

    model.moves[0].inputs.hole_pin_float.sigmaNumber = 3;
    dvatest::check(setMoveFloatAngleOffset(model, moveId, -20.0),
                   "allow active move float angle offset after restoring staged state");
}

TEST("model editing: active move float angle offset rejects invalid move state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    dvatest::check(moveId != kInvalidId, "move created");
    model.moves[0].inputs.hole_pin_float.active = true;
    model.moves[0].moveParts = {baseId, baseId};
    const double oldAngleOffset =
        model.moves[0].inputs.hole_pin_float.angleOffsetDeg;
    dvatest::check(!setMoveFloatAngleOffset(model, moveId, -20.0),
                   "reject active move float angle offset with duplicate parts");
    dvatest::checkNear(model.moves[0].inputs.hole_pin_float.angleOffsetDeg,
                       oldAngleOffset, 1e-12,
                       "invalid active move float angle offset leaves value");

    model.moves[0].moveParts = {baseId, targetId};
    dvatest::check(setMoveFloatAngleOffset(model, moveId, -20.0),
                   "allow active move float angle offset after restoring move state");
}

TEST("model editing: measure mode enum setters reject invalid values") {
    Model model;
    addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0});
    addCoordinatePoint(model, {0.0, 0.0, 10.0});
    const MeasureId measureId = addPointPointMeasure(model, 0.0, 1.0);

    dvatest::check(!setMeasureSpecMode(model, measureId,
                                       static_cast<SpecMode>(999)),
                   "reject invalid measure spec mode");
    dvatest::check(model.measures[0].def.spec.mode == SpecMode::Absolute,
                   "invalid measure spec mode leaves value");
    dvatest::check(!setMeasureDirectionMode(
                       model, measureId, static_cast<DirectionMode>(999)),
                   "reject invalid measure direction mode");
    dvatest::check(model.measures[0].def.dirMode ==
                       DirectionMode::ProjectedOnVector,
                   "invalid measure direction mode leaves value");
}

TEST("model editing: numeric setters reject invalid values") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const PointId pointId = addCoordinatePoint(model, {0.0, 0.0, 0.0}, baseId);
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, baseId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, baseId);
    const GdtId gdtId = addGdt(model, GdtType::Flatness, 0.25, false, baseId);
    const MoveId moveId = addTransformMove(model, {1.0, 0.0, 0.0},
                                           baseId, targetId);
    const MeasureId measureId = addPointPointMeasure(model, 0.0, 1.0);

    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    dvatest::check(!setPointDiameter(model, pointId, -1.0),
                   "reject negative point diameter");
    dvatest::checkNear(model.parts[0].points[0].diameter, 0.0, 1e-12,
                       "invalid point diameter leaves value");
    dvatest::check(!setPointDiameter(model, pointId, nan),
                   "reject non-finite point diameter");

    dvatest::check(!setToleranceRange(model, toleranceId, -0.1),
                   "reject negative tolerance range");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.rands[0].range, 0.5,
                       1e-12, "invalid tolerance range leaves value");
    dvatest::check(!setToleranceSigmaNumber(model, toleranceId, 0.0),
                   "reject non-positive tolerance sigma");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.rands[0].sigmaNum, 3.0,
                       1e-12, "invalid tolerance sigma leaves value");
    dvatest::check(!setToleranceRangeScale(model, toleranceId, inf),
                   "reject non-finite tolerance range scale");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.rangeScale, 1.0, 1e-12,
                       "invalid tolerance range scale leaves value");

    dvatest::check(!setGdtRange(model, gdtId, -0.1), "reject negative gdt range");
    dvatest::checkNear(model.parts[0].gdts[0].range, 0.25, 1e-12,
                       "invalid gdt range leaves value");

    dvatest::check(!setMoveSearchAccuracy(model, moveId, 0.0),
                   "reject non-positive move search accuracy");
    dvatest::checkNear(model.moves[0].inputs.searchAccuracy, 1e-5, 1e-12,
                       "invalid move search accuracy leaves value");
    dvatest::check(!setMoveFloatSigmaNumber(model, moveId, 0),
                   "reject non-positive move float sigma");
    dvatest::check(model.moves[0].inputs.hole_pin_float.sigmaNumber == 3,
                   "invalid move float sigma leaves value");
    dvatest::check(!setMoveFloatSigmaNumber(model, moveId, 9),
                   "reject too large move float sigma");
    dvatest::check(model.moves[0].inputs.hole_pin_float.sigmaNumber == 3,
                   "too large move float sigma leaves value");
    dvatest::check(!setMoveFloatRangeScale(model, moveId, -1.0),
                   "reject non-positive move float range scale");
    dvatest::checkNear(model.moves[0].inputs.hole_pin_float.rangeScale, 1.0,
                       1e-12, "invalid move float range scale leaves value");
    dvatest::check(!setMoveFloatAngleRange(model, moveId, -1.0),
                   "reject negative move float angle range");
    dvatest::checkNear(model.moves[0].inputs.hole_pin_float.angleRangeDeg, 360.0,
                       1e-12, "invalid move float angle range leaves value");
    dvatest::check(!setMoveType(model, moveId, MoveType::UserDll),
                   "reject active unsupported move type");
    dvatest::check(model.moves[0].inputs.type == MoveType::Transform,
                   "invalid active move type leaves value");
    dvatest::check(!setMoveType(model, moveId, MoveType::SixPlane),
                   "reject active move type requiring more pairs");
    dvatest::check(model.moves[0].inputs.type == MoveType::Transform,
                   "invalid underconstrained move type leaves value");
    dvatest::check(!setMoveParts(model, moveId, {baseId}),
                   "reject active move with too few parts");
    dvatest::check(model.moves[0].moveParts.size() == 2 &&
                       model.moves[0].moveParts[0] == baseId &&
                       model.moves[0].moveParts[1] == targetId,
                   "invalid active move parts leave value");
    dvatest::check(setMoveActive(model, moveId, false),
                   "deactivate move before unsupported type");
    dvatest::check(setMoveParts(model, moveId, {baseId}),
                   "allow inactive move with too few parts");
    dvatest::check(!setMoveActive(model, moveId, true),
                   "reject activating move with too few parts");
    dvatest::check(!model.moves[0].active,
                   "invalid move parts activation leaves value");
    dvatest::check(setMoveParts(model, moveId, {baseId, targetId}),
                   "restore valid move parts");
    dvatest::check(setMovePairs(model, moveId, {}),
                   "clear inactive move pairs before empty iteration");
    dvatest::check(setMoveType(model, moveId, MoveType::Iteration),
                   "allow inactive empty iteration reference move type");
    dvatest::check(setMoveActive(model, moveId, true),
                   "activate empty iteration reference move");
    dvatest::check(!hasBlockingValidationIssue(model),
                   "active empty iteration reference move remains runnable");
    dvatest::check(setMoveActive(model, moveId, false),
                   "deactivate empty iteration before restoring transform");
    dvatest::check(setMoveType(model, moveId, MoveType::Transform),
                   "restore inactive transform move type");
    dvatest::check(setMovePairs(model, moveId, {MovePair{}}),
                   "restore inactive transform move pair");
    dvatest::check(setMoveType(model, moveId, MoveType::SixPlane),
                   "allow inactive move type requiring more pairs");
    dvatest::check(!setMoveActive(model, moveId, true),
                   "reject activating move type requiring more pairs");
    dvatest::check(!model.moves[0].active,
                   "invalid underconstrained move activation leaves value");
    MovePair pairA;
    pairA.direction.ijk = {1.0, 0.0, 0.0};
    MovePair pairB;
    pairB.direction.ijk = {0.0, 1.0, 0.0};
    MovePair pairC;
    pairC.direction.ijk = {0.0, 0.0, 1.0};
    dvatest::check(setMovePairs(model, moveId, {pairA, pairB, pairC}),
                   "set enough pairs for six-plane move");
    dvatest::check(setMoveActive(model, moveId, true),
                   "activate move with enough pairs");
    dvatest::check(setMoveActive(model, moveId, false),
                   "deactivate move before unsupported type");
    dvatest::check(setMoveType(model, moveId, MoveType::UserDll),
                   "allow inactive unsupported move type");
    dvatest::check(!setMoveActive(model, moveId, true),
                   "reject activating unsupported move type");
    dvatest::check(!model.moves[0].active,
                   "invalid unsupported move activation leaves value");
    dvatest::check(setMoveUserDllRoutine(model, moveId, "externalMove"),
                   "set bound user-dll routine before activation");
    opendva::plugin::PluginHost host;
    host.registerRoutine("externalMove", &boundUserDllRoutine, dcsCalTypeMove);
    opendva::plugin::PluginHost::ActiveScope scope(&host);
    dvatest::check(setMoveActive(model, moveId, true),
                   "activate bound user-dll move");
    dvatest::check(!hasBlockingValidationIssue(model),
                   "bound user-dll move remains runnable");
    dvatest::check(setMoveActive(model, moveId, false),
                   "deactivate bound user-dll move before measure checks");

    dvatest::check(setMeasureActive(model, measureId, false),
                   "deactivate measure before staged invalid state");
    model.measures[0].def.inputPoints = {pointId, pointId};
    dvatest::check(!setMeasureActive(model, measureId, true),
                   "reject activating measure with duplicate input points");
    dvatest::check(!model.measures[0].def.active,
                   "duplicate-point measure activation leaves value");

    model.measures[0].def.inputPoints = {pointId, 102};
    model.measures[0].def.direction.ijk = {0.0, 0.0, 0.0};
    dvatest::check(!setMeasureActive(model, measureId, true),
                   "reject activating measure with zero direction");
    dvatest::check(!model.measures[0].def.active,
                   "zero-direction measure activation leaves value");

    model.measures[0].def.direction.ijk = {0.0, 0.0, 1.0};
    model.measures[0].def.values = {1.0, nan};
    dvatest::check(!setMeasureActive(model, measureId, true),
                   "reject activating measure with non-finite value");
    dvatest::check(!model.measures[0].def.active,
                   "non-finite-value measure activation leaves value");

    model.measures[0].def.values.clear();
    model.measures[0].def.spec.lsl = nan;
    dvatest::check(!setMeasureActive(model, measureId, true),
                   "reject activating measure with non-finite spec");
    dvatest::check(!model.measures[0].def.active,
                   "non-finite-spec measure activation leaves value");

    model.measures[0].def.spec.lsl = 0.0;
    dvatest::check(setMeasureActive(model, measureId, true),
                   "reactivate measure after restoring staged state");
    dvatest::check(!setMeasureScale(model, measureId, nan),
                   "reject non-finite measure scale");
    dvatest::checkNear(model.measures[0].def.scale, 1.0, 1e-12,
                       "invalid measure scale leaves value");
    dvatest::check(!setMeasureScale(model, measureId, 0.0),
                   "reject zero measure scale");
    dvatest::checkNear(model.measures[0].def.scale, 1.0, 1e-12,
                       "zero measure scale leaves value");
    dvatest::check(!setMeasureSpec(model, measureId, 12.0, 10.0, true, true),
                   "reject misordered active measure spec limits");
    dvatest::checkNear(model.measures[0].def.spec.lsl, 0.0, 1e-12,
                       "invalid measure spec leaves lsl");
    dvatest::checkNear(model.measures[0].def.spec.usl, 1.0, 1e-12,
                       "invalid measure spec leaves usl");
    dvatest::check(setMeasureSpec(model, measureId, 12.0, 10.0, true, false),
                   "allow one-sided misordered measure spec");
    dvatest::check(!setMeasureUslActive(model, measureId, true),
                   "reject activating misordered measure upper spec");
    dvatest::check(!model.measures[0].def.spec.uslActive,
                   "invalid upper spec activation leaves value");
    dvatest::check(setMeasureSpec(model, measureId, 12.0, 10.0, false, true),
                   "allow alternate one-sided misordered measure spec");
    dvatest::check(!setMeasureLslActive(model, measureId, true),
                   "reject activating misordered measure lower spec");
    dvatest::check(!model.measures[0].def.spec.lslActive,
                   "invalid lower spec activation leaves value");
    dvatest::check(!setMeasureType(model, measureId, MeasureType::UserDll),
                   "reject active unsupported measure type");
    dvatest::check(model.measures[0].def.type == MeasureType::PointPoint,
                   "invalid active measure type leaves value");
    dvatest::check(!setMeasureType(model, measureId, MeasureType::PlanePlane),
                   "reject active measure type requiring more input points");
    dvatest::check(model.measures[0].def.type == MeasureType::PointPoint,
                   "invalid underconstrained measure type leaves value");
    dvatest::check(!setMeasureType(model, measureId, MeasureType::FeatureMeasure),
                   "reject active feature measure type without input features");
    dvatest::check(model.measures[0].def.type == MeasureType::PointPoint,
                   "invalid feature-input measure type leaves value");
    dvatest::check(setMeasureActive(model, measureId, false),
                   "deactivate measure before unsupported type");
    dvatest::check(setMeasureType(model, measureId, MeasureType::PlanePlane),
                   "allow inactive measure type requiring more input points");
    dvatest::check(!setMeasureActive(model, measureId, true),
                   "reject activating measure type requiring more input points");
    dvatest::check(!model.measures[0].def.active,
                   "invalid underconstrained measure activation leaves value");
    dvatest::check(setMeasureType(model, measureId, MeasureType::PointPoint),
                   "restore valid measure type");
    dvatest::check(setMeasureActive(model, measureId, true),
                   "reactivate measure with valid input points");
    dvatest::check(!setPointActive(model, pointId, false),
                   "reject deactivating active measure input point");
    dvatest::check(model.parts[0].points[0].active,
                   "invalid point deactivation leaves value");
    dvatest::check(!setMeasureInputPoints(model, measureId, {pointId}),
                   "reject active measure with too few input points");
    dvatest::check(model.measures[0].def.inputPoints.size() == 2 &&
                       model.measures[0].def.inputPoints[0] == 101 &&
                       model.measures[0].def.inputPoints[1] == 102,
                   "invalid active measure input points leave value");
    const FeatureId featureId = model.parts[0].features[0].id;
    dvatest::check(setMeasureActive(model, measureId, false),
                   "deactivate measure before feature measure");
    dvatest::check(setMeasureType(model, measureId, MeasureType::FeatureMeasure),
                   "allow inactive feature measure type");
    dvatest::check(setMeasureInputFeatures(model, measureId, {featureId}),
                   "set feature measure input feature");
    dvatest::check(setMeasureActive(model, measureId, true),
                   "activate feature measure with input feature");
    dvatest::check(!setMeasureInputFeatures(model, measureId, {}),
                   "reject active feature measure without input features");
    dvatest::check(model.measures[0].def.inputFeatures.size() == 1 &&
                       model.measures[0].def.inputFeatures[0] == featureId,
                   "invalid active feature measure inputs leave value");
    dvatest::check(setMeasureActive(model, measureId, false),
                   "deactivate measure before unsupported type");
    dvatest::check(setMeasureInputFeatures(model, measureId, {}),
                   "allow inactive feature measure without input features");
    dvatest::check(!setMeasureActive(model, measureId, true),
                   "reject activating feature measure without input features");
    dvatest::check(!model.measures[0].def.active,
                   "invalid feature measure activation leaves value");
    dvatest::check(setMeasureType(model, measureId, MeasureType::UserDll),
                   "allow inactive unsupported measure type");
    dvatest::check(!setMeasureActive(model, measureId, true),
                   "reject activating unsupported measure type");
    dvatest::check(!model.measures[0].def.active,
                   "invalid unsupported measure activation leaves value");
    dvatest::check(setMeasureEquation(model, measureId, "externalMeasure"),
                   "set user-dll measure routine name");
    plugin::PluginHost measureHost;
    measureHost.registerRoutine("externalMeasure", &boundUserDllRoutine,
                                dcsCalTypeMeas);
    {
        plugin::PluginHost::ActiveScope measureScope(&measureHost);
        dvatest::check(setMeasureActive(model, measureId, true),
                       "activate bound user-dll measure");
        dvatest::check(!hasBlockingValidationIssue(model),
                       "bound user-dll measure passes validation");
    }
    dvatest::check(setMeasureActive(model, measureId, false),
                   "deactivate bound user-dll measure");
}

TEST("model editing: active measure type switch rejects invalid staged state") {
    Model model = createStarterModel();
    const MeasureId measureId = model.measures[0].id;
    const double nan = std::numeric_limits<double>::quiet_NaN();

    dvatest::check(model.measures[0].def.active,
                   "starter measure is active");
    model.measures[0].def.equation = std::string(401, '1');
    dvatest::check(!setMeasureType(model, measureId, MeasureType::Equation),
                   "reject active equation type switch with invalid equation");
    dvatest::check(model.measures[0].def.type == MeasureType::PointPoint,
                   "invalid equation type switch leaves value");

    model.measures[0].def.equation = "P1X + P2X";
    model.measures[0].def.values = {1.0, nan};
    dvatest::check(!setMeasureType(model, measureId, MeasureType::Equation),
                   "reject active equation type switch with non-finite values");
    dvatest::check(model.measures[0].def.type == MeasureType::PointPoint,
                   "invalid value type switch leaves value");

    model.measures[0].def.values.clear();
    dvatest::check(setMeasureType(model, measureId, MeasureType::Equation),
                   "allow active equation type switch after restoring staged state");
}

TEST("model editing: active measure scale rejects invalid staged state") {
    Model model = createStarterModel();
    const MeasureId measureId = model.measures[0].id;

    dvatest::check(model.measures[0].def.active,
                   "starter measure is active");
    const std::vector<PointId> originalInputPoints =
        model.measures[0].def.inputPoints;
    model.measures[0].def.inputPoints = {originalInputPoints.front(),
                                         originalInputPoints.front()};
    const double oldScale = model.measures[0].def.scale;
    dvatest::check(!setMeasureScale(model, measureId, 2.0),
                   "reject active measure scale with invalid input points");
    dvatest::checkNear(model.measures[0].def.scale, oldScale, 1e-12,
                       "invalid active measure scale leaves value");

    model.measures[0].def.inputPoints = originalInputPoints;
    dvatest::check(setMeasureScale(model, measureId, 2.0),
                   "allow active measure scale after restoring staged state");
}

TEST("model editing: active measure direction rejects invalid staged state") {
    Model model = createStarterModel();
    const MeasureId measureId = model.measures[0].id;

    dvatest::check(model.measures[0].def.active,
                   "starter measure is active");
    const std::vector<PointId> originalInputPoints =
        model.measures[0].def.inputPoints;
    model.measures[0].def.inputPoints = {originalInputPoints.front(),
                                         originalInputPoints.front()};
    const Vec3 oldDirection = model.measures[0].def.direction.ijk;
    dvatest::check(!setMeasureDirection(model, measureId, {1.0, 0.0, 0.0}),
                   "reject active measure direction with invalid input points");
    dvatest::checkNear(model.measures[0].def.direction.ijk.x, oldDirection.x,
                       1e-12, "invalid active measure direction leaves x");
    dvatest::checkNear(model.measures[0].def.direction.ijk.y, oldDirection.y,
                       1e-12, "invalid active measure direction leaves y");
    dvatest::checkNear(model.measures[0].def.direction.ijk.z, oldDirection.z,
                       1e-12, "invalid active measure direction leaves z");

    model.measures[0].def.inputPoints = originalInputPoints;
    dvatest::check(setMeasureDirection(model, measureId, {1.0, 0.0, 0.0}),
                   "allow active measure direction after restoring staged state");
}

TEST("model editing: active measure spec mode rejects invalid staged state") {
    Model model = createStarterModel();
    const MeasureId measureId = model.measures[0].id;

    dvatest::check(model.measures[0].def.active,
                   "starter measure is active");
    const std::vector<PointId> originalInputPoints =
        model.measures[0].def.inputPoints;
    model.measures[0].def.inputPoints = {originalInputPoints.front(),
                                         originalInputPoints.front()};
    const SpecMode oldMode = model.measures[0].def.spec.mode;
    dvatest::check(!setMeasureSpecMode(model, measureId,
                                       SpecMode::RelativeToNominal),
                   "reject active measure spec mode with invalid input points");
    dvatest::check(model.measures[0].def.spec.mode == oldMode,
                   "invalid active measure spec mode leaves value");

    model.measures[0].def.inputPoints = originalInputPoints;
    dvatest::check(setMeasureSpecMode(model, measureId,
                                      SpecMode::RelativeToNominal),
                   "allow active measure spec mode after restoring staged state");
}

TEST("model editing: active measure direction mode rejects invalid staged state") {
    Model model = createStarterModel();
    const MeasureId measureId = model.measures[0].id;

    dvatest::check(model.measures[0].def.active,
                   "starter measure is active");
    const std::vector<PointId> originalInputPoints =
        model.measures[0].def.inputPoints;
    model.measures[0].def.inputPoints = {originalInputPoints.front(),
                                         originalInputPoints.front()};
    const DirectionMode oldMode = model.measures[0].def.dirMode;
    dvatest::check(!setMeasureDirectionMode(model, measureId,
                                            DirectionMode::TrueDistance),
                   "reject active measure direction mode with invalid input points");
    dvatest::check(model.measures[0].def.dirMode == oldMode,
                   "invalid active measure direction mode leaves value");

    model.measures[0].def.inputPoints = originalInputPoints;
    dvatest::check(setMeasureDirectionMode(model, measureId,
                                           DirectionMode::TrueDistance),
                   "allow active measure direction mode after restoring staged state");
}

TEST("model editing: active measure equation rejects invalid staged state") {
    Model model = createStarterModel();
    const MeasureId measureId = model.measures[0].id;

    dvatest::check(model.measures[0].def.active,
                   "starter measure is active");
    const std::vector<PointId> originalInputPoints =
        model.measures[0].def.inputPoints;
    model.measures[0].def.inputPoints = {originalInputPoints.front(),
                                         originalInputPoints.front()};
    const std::string oldEquation = model.measures[0].def.equation;
    dvatest::check(!setMeasureEquation(model, measureId, "P1X + P2X"),
                   "reject active measure equation with invalid input points");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "invalid active measure equation leaves value");

    model.measures[0].def.inputPoints = originalInputPoints;
    dvatest::check(setMeasureEquation(model, measureId, "P1X + P2X"),
                   "allow active measure equation after restoring staged state");
    dvatest::check(setMeasureType(model, measureId, MeasureType::Equation),
                   "switch active measure to equation before value-index check");

    dvatest::check(!setMeasureEquation(model, measureId, "[VAL:2]+1"),
                   "reject active measure equation with missing value index");
    dvatest::check(model.measures[0].def.equation == "P1X + P2X",
                   "missing value index equation leaves value");

    dvatest::check(!setMeasureEquation(model, measureId, "[MS:2]+1"),
                   "reject active measure equation with missing measure index");
    dvatest::check(model.measures[0].def.equation == "P1X + P2X",
                   "missing measure index equation leaves value");

    dvatest::check(!setMeasureEquation(model, measureId, "2+3\n[STR:2]+1"),
                   "reject active measure equation with missing string index");
    dvatest::check(model.measures[0].def.equation == "P1X + P2X",
                   "missing string index equation leaves value");

    dvatest::check(!setMeasureEquation(model, measureId, "[P1X:3]+1"),
                   "reject active measure equation with missing point index");
    dvatest::check(model.measures[0].def.equation == "P1X + P2X",
                   "missing point index equation leaves value");
}

TEST("model editing: measure equation rejects malformed or unsupported variables") {
    Model model = createStarterModel();
    const MeasureId measureId = model.measures[0].id;
    const std::string oldEquation = model.measures[0].def.equation;

    dvatest::check(!setMeasureEquation(model, measureId, "[VAL:1+1"),
                   "reject measure equation with unterminated variable");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "unterminated variable equation leaves value");

    dvatest::check(!setMeasureEquation(model, measureId, "1]+[VAL:1]"),
                   "reject measure equation with unmatched close bracket");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "unmatched close bracket equation leaves value");

    dvatest::check(!setMeasureEquation(model, measureId, "[BAD:1]+1"),
                   "reject measure equation with unsupported variable");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "unsupported variable equation leaves value");

    dvatest::check(!setMeasureEquation(model, measureId, "[P3X:1]+1"),
                   "reject measure equation with invalid point variable");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "invalid point variable equation leaves value");

    dvatest::check(!setMeasureEquation(model, measureId, "[DRI:2]+1"),
                   "reject measure equation with invalid direction variable");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "invalid direction variable equation leaves value");

    dvatest::check(!setMeasureEquation(model, measureId, "[VAL:0]+1"),
                   "reject measure equation with zero scalar index");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "zero scalar index equation leaves value");

    dvatest::check(!setMeasureEquation(model, measureId, "1==1"),
                   "reject measure equation with top-level equality");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "top-level equality equation leaves value");

    dvatest::check(!setMeasureEquation(model, measureId, "1=1"),
                   "reject measure equation with single top-level equals");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "single top-level equals equation leaves value");

    dvatest::check(!setMeasureEquation(model, measureId, "2+"),
                   "reject measure equation with trailing operator");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "trailing operator equation leaves value");

    dvatest::check(!setMeasureEquation(model, measureId, "-1+2"),
                   "reject measure equation with bare negative constant");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "bare negative constant equation leaves value");

    dvatest::check(!setMeasureEquation(model, measureId, "1e309"),
                   "reject measure equation with non-finite numeric literal");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "non-finite numeric literal equation leaves value");

    dvatest::check(!setMeasureEquation(model, measureId, "1e308*1e308"),
                   "reject measure equation with non-finite numeric product");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "non-finite numeric product equation leaves value");
    dvatest::check(!setMeasureEquation(model, measureId, "1e308+1e308"),
                   "reject measure equation with non-finite numeric sum");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "non-finite numeric sum equation leaves value");
    dvatest::check(!setMeasureEquation(model, measureId, "(1e308)*(1e308)"),
                   "reject measure equation with parenthesized non-finite numeric product");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "parenthesized non-finite numeric product equation leaves value");
    dvatest::check(!setMeasureEquation(model, measureId, "pow(1e308,2)"),
                   "reject measure equation with non-finite function result");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "non-finite function result equation leaves value");
    dvatest::check(!setMeasureEquation(model, measureId, "pow((-1),0.5)"),
                   "reject measure equation with parenthesized non-finite pow result");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "parenthesized non-finite pow result equation leaves value");
    dvatest::check(!setMeasureEquation(model, measureId, "exp(1000)"),
                   "reject measure equation with non-finite exp result");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "non-finite exp result equation leaves value");
    dvatest::check(!setMeasureEquation(model, measureId, "inch2mm(1e308)"),
                   "reject measure equation with non-finite inch conversion result");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "non-finite inch conversion result equation leaves value");

    dvatest::check(setMeasureEquation(model, measureId, "rad2deg(1e306)"),
                   "allow finite radian conversion result with large literal");
    dvatest::check(model.measures[0].def.equation == "rad2deg(1e306)",
                   "large finite radian conversion equation is stored");
    dvatest::check(setMeasureEquation(model, measureId, oldEquation),
                   "restore old equation after large finite conversion check");

    dvatest::check(!setMeasureEquation(model, measureId, "sin(7)"),
                   "reject measure equation with out-of-range trig input");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "out-of-range trig input equation leaves value");
    dvatest::check(!setMeasureEquation(
                       model, measureId, "tan(1.5707963267948966)"),
                   "reject measure equation with tan boundary input");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "tan boundary input equation leaves value");
    dvatest::check(!setMeasureEquation(model, measureId, "asin(2)"),
                   "reject measure equation with out-of-range inverse trig input");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "out-of-range inverse trig input equation leaves value");
    dvatest::check(!setMeasureEquation(model, measureId, "log(0)"),
                   "reject measure equation with invalid function-domain input");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "invalid function-domain input equation leaves value");
    dvatest::check(!setMeasureEquation(model, measureId, "sqrt((-1))"),
                   "reject measure equation with invalid parenthesized sqrt input");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "invalid parenthesized sqrt input equation leaves value");
    dvatest::check(!setMeasureEquation(model, measureId, "mod(10,0)"),
                   "reject measure equation with zero mod divisor");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "zero mod divisor equation leaves value");
    dvatest::check(!setMeasureEquation(model, measureId, "sqrt(1,2)"),
                   "reject measure equation with invalid function arity");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "invalid function arity equation leaves value");
    dvatest::check(!setMeasureEquation(model, measureId, "sqrt()"),
                   "reject measure equation with empty function arguments");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "empty function arguments equation leaves value");
    dvatest::check(!setMeasureEquation(model, measureId, "pow(,2)"),
                   "reject measure equation with empty function argument slot");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "empty function argument slot equation leaves value");
    dvatest::check(!setMeasureEquation(model, measureId, "if_then_else(1,2)"),
                   "reject measure equation with invalid conditional arity");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "invalid conditional arity equation leaves value");
    dvatest::check(!setMeasureEquation(model, measureId, "MIN()"),
                   "reject measure equation with empty MIN arguments");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "empty MIN arguments equation leaves value");
    dvatest::check(!setMeasureEquation(model, measureId, "MAX()"),
                   "reject measure equation with empty MAX arguments");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "empty MAX arguments equation leaves value");

    dvatest::check(!setMeasureEquation(model, measureId, "SIN(1)"),
                   "reject measure equation with uppercase math operator");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "uppercase math operator equation leaves value");
    dvatest::check(!setMeasureEquation(model, measureId, "foo(1)"),
                   "reject measure equation with unknown math operator");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "unknown math operator equation leaves value");

    dvatest::check(!setMeasureEquation(model, measureId, "MIN(MIN(1,2),3)"),
                   "reject measure equation with nested MIN operator");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "nested MIN operator equation leaves value");

    dvatest::check(
        !setMeasureEquation(model, measureId,
                            "if_then_else(if_then_else(1,1,0),2,3)"),
        "reject measure equation with nested conditional operator");
    dvatest::check(model.measures[0].def.equation == oldEquation,
                   "nested conditional operator equation leaves value");
}

TEST("model editing: active measure values reject invalid staged state") {
    Model model = createStarterModel();
    const MeasureId measureId = model.measures[0].id;

    dvatest::check(model.measures[0].def.active,
                   "starter measure is active");
    const std::vector<PointId> originalInputPoints =
        model.measures[0].def.inputPoints;
    model.measures[0].def.inputPoints = {originalInputPoints.front(),
                                         originalInputPoints.front()};
    const std::vector<double> oldValues = model.measures[0].def.values;
    dvatest::check(!setMeasureValues(model, measureId, {2.0, 3.0}),
                   "reject active measure values with invalid input points");
    dvatest::check(model.measures[0].def.values == oldValues,
                   "invalid active measure values leave value");

    model.measures[0].def.inputPoints = originalInputPoints;
    dvatest::check(setMeasureValues(model, measureId, {2.0, 3.0}),
                   "allow active measure values after restoring staged state");
}

TEST("model editing: active measure spec rejects invalid staged state") {
    Model model = createStarterModel();
    const MeasureId measureId = model.measures[0].id;

    dvatest::check(model.measures[0].def.active, "starter measure is active");
    const std::vector<PointId> originalInputPoints =
        model.measures[0].def.inputPoints;
    model.measures[0].def.inputPoints = {originalInputPoints.front(),
                                         originalInputPoints.front()};
    const SpecLimits oldSpec = model.measures[0].def.spec;
    dvatest::check(!setMeasureSpec(model, measureId, 9.0, 10.5, true, true),
                   "reject active measure spec with invalid input points");
    dvatest::checkNear(model.measures[0].def.spec.lsl, oldSpec.lsl, 1e-12,
                       "invalid active measure spec leaves lsl");
    dvatest::checkNear(model.measures[0].def.spec.usl, oldSpec.usl, 1e-12,
                       "invalid active measure spec leaves usl");
    dvatest::check(model.measures[0].def.spec.lslActive == oldSpec.lslActive,
                   "invalid active measure spec leaves lsl active");
    dvatest::check(model.measures[0].def.spec.uslActive == oldSpec.uslActive,
                   "invalid active measure spec leaves usl active");

    model.measures[0].def.inputPoints = originalInputPoints;
    dvatest::check(setMeasureSpec(model, measureId, 9.0, 10.5, true, true),
                   "allow active measure spec after restoring staged state");
}

TEST("model editing: active measure lsl active rejects invalid staged state") {
    Model model = createStarterModel();
    const MeasureId measureId = model.measures[0].id;

    dvatest::check(model.measures[0].def.active, "starter measure is active");
    const std::vector<PointId> originalInputPoints =
        model.measures[0].def.inputPoints;
    model.measures[0].def.inputPoints = {originalInputPoints.front(),
                                         originalInputPoints.front()};
    const bool oldLslActive = model.measures[0].def.spec.lslActive;
    const bool newLslActive = !oldLslActive;
    dvatest::check(!setMeasureLslActive(model, measureId, newLslActive),
                   "reject active measure lsl active with invalid input points");
    dvatest::check(model.measures[0].def.spec.lslActive == oldLslActive,
                   "invalid active measure lsl active leaves value");

    model.measures[0].def.inputPoints = originalInputPoints;
    dvatest::check(setMeasureLslActive(model, measureId, newLslActive),
                   "allow active measure lsl active after restoring staged state");
}

TEST("model editing: active measure usl active rejects invalid staged state") {
    Model model = createStarterModel();
    const MeasureId measureId = model.measures[0].id;

    dvatest::check(model.measures[0].def.active, "starter measure is active");
    const std::vector<PointId> originalInputPoints =
        model.measures[0].def.inputPoints;
    model.measures[0].def.inputPoints = {originalInputPoints.front(),
                                         originalInputPoints.front()};
    const bool oldUslActive = model.measures[0].def.spec.uslActive;
    const bool newUslActive = !oldUslActive;
    dvatest::check(!setMeasureUslActive(model, measureId, newUslActive),
                   "reject active measure usl active with invalid input points");
    dvatest::check(model.measures[0].def.spec.uslActive == oldUslActive,
                   "invalid active measure usl active leaves value");

    model.measures[0].def.inputPoints = originalInputPoints;
    dvatest::check(setMeasureUslActive(model, measureId, newUslActive),
                   "allow active measure usl active after restoring staged state");
}

TEST("model editing: active measure input points reject invalid staged state") {
    Model model = createStarterModel();
    const MeasureId measureId = model.measures[0].id;

    dvatest::check(model.measures[0].def.active, "starter measure is active");
    const std::vector<PointId> oldInputPoints =
        model.measures[0].def.inputPoints;
    const double oldScale = model.measures[0].def.scale;
    model.measures[0].def.scale = 0.0;
    dvatest::check(!setMeasureInputPoints(model, measureId,
                                          {oldInputPoints[1],
                                           oldInputPoints[0]}),
                   "reject active measure input points with invalid scale");
    dvatest::check(model.measures[0].def.inputPoints == oldInputPoints,
                   "invalid active measure input points leave value");

    model.measures[0].def.scale = oldScale;
    dvatest::check(setMeasureInputPoints(model, measureId,
                                         {oldInputPoints[1],
                                          oldInputPoints[0]}),
                   "allow active measure input points after restoring staged state");
}

TEST("model editing: active measure input features reject invalid staged state") {
    Model model = createStarterModel();
    const MeasureId measureId = model.measures[0].id;
    const FeatureId featureId = model.parts[0].features[0].id;

    dvatest::check(setMeasureActive(model, measureId, false),
                   "deactivate measure before feature measure setup");
    dvatest::check(setMeasureType(model, measureId, MeasureType::FeatureMeasure),
                   "set feature measure type");
    dvatest::check(setMeasureInputFeatures(model, measureId, {featureId}),
                   "set feature measure input feature");
    dvatest::check(setMeasureActive(model, measureId, true),
                   "activate feature measure with valid input feature");

    const std::vector<FeatureId> oldInputFeatures =
        model.measures[0].def.inputFeatures;
    const double oldScale = model.measures[0].def.scale;
    model.measures[0].def.scale = 0.0;
    dvatest::check(!setMeasureInputFeatures(model, measureId, {featureId}),
                   "reject active measure input features with invalid scale");
    dvatest::check(model.measures[0].def.inputFeatures == oldInputFeatures,
                   "invalid active measure input features leave value");

    model.measures[0].def.scale = oldScale;
    dvatest::check(setMeasureInputFeatures(model, measureId, {featureId}),
                   "allow active measure input features after restoring staged state");
}

TEST("model editing: active measure output flag rejects invalid staged state") {
    Model model = createStarterModel();
    const MeasureId measureId = model.measures[0].id;

    dvatest::check(model.measures[0].def.active, "starter measure is active");
    const std::vector<PointId> originalInputPoints =
        model.measures[0].def.inputPoints;
    model.measures[0].def.inputPoints = {originalInputPoints.front(),
                                         originalInputPoints.front()};
    const bool oldAsOutput = model.measures[0].def.asOutput;
    const bool newAsOutput = !oldAsOutput;
    dvatest::check(!setMeasureAsOutput(model, measureId, newAsOutput),
                   "reject active measure output flag with invalid input points");
    dvatest::check(model.measures[0].def.asOutput == oldAsOutput,
                   "invalid active measure output flag leaves value");

    model.measures[0].def.inputPoints = originalInputPoints;
    dvatest::check(setMeasureAsOutput(model, measureId, newAsOutput),
                   "allow active measure output flag after restoring staged state");
}

TEST("model editing: active measure rename rejects invalid staged state") {
    Model model = createStarterModel();
    const MeasureId measureId = model.measures[0].id;

    dvatest::check(model.measures[0].def.active, "starter measure is active");
    const std::vector<PointId> originalInputPoints =
        model.measures[0].def.inputPoints;
    model.measures[0].def.inputPoints = {originalInputPoints.front(),
                                         originalInputPoints.front()};
    const std::string oldName = model.measures[0].name;
    dvatest::check(!renameMeasure(model, measureId, "Blocked Rename"),
                   "reject active measure rename with invalid input points");
    dvatest::check(model.measures[0].name == oldName,
                   "invalid active measure rename leaves value");

    model.measures[0].def.inputPoints = originalInputPoints;
    dvatest::check(renameMeasure(model, measureId, "Allowed Rename"),
                   "allow active measure rename after restoring staged state");
}

TEST("model editing: geometry setters reject non-finite vectors") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PartId targetId = addPart(model, "Target");
    const PointId pointId = addCoordinatePoint(model, {1.0, 2.0, 3.0}, baseId);
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, baseId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, baseId);
    const MoveId moveId = addTransformMove(model, {1.0, 0.0, 0.0},
                                           baseId, targetId);
    const MeasureId measureId = addPointPointMeasure(model, 0.0, 1.0);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    dvatest::check(!setPointPosition(model, pointId, {nan, 0.0, 0.0}),
                   "reject non-finite point position");
    dvatest::checkNear(model.parts[0].points[0].position.x, 1.0, 1e-12,
                       "invalid point position leaves value");
    dvatest::check(!setPointDirection(model, pointId, {0.0, inf, 1.0}),
                   "reject non-finite point direction");
    dvatest::checkNear(model.parts[0].points[0].ijk.z, 1.0, 1e-12,
                       "invalid point direction leaves value");
    dvatest::check(!setToleranceDirection(model, toleranceId, {nan, 1.0, 0.0}),
                   "reject non-finite tolerance direction");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.direction.ijk.z, 1.0,
                       1e-12, "invalid tolerance direction leaves value");
    dvatest::check(!setToleranceDirection(model, toleranceId, {0.0, 0.0, 0.0}),
                   "reject zero tolerance direction");
    dvatest::checkNear(model.parts[0].tolerances[0].ir.direction.ijk.z, 1.0,
                       1e-12, "zero tolerance direction leaves value");
    dvatest::check(!setTransformMoveTranslation(model, moveId, {0.0, inf, 0.0}),
                   "reject non-finite transform translation");
    dvatest::checkNear(model.moves[0].inputs.pairs[0].targetPoint.x, 1.0,
                       1e-12, "invalid transform translation leaves value");
    dvatest::check(!setMovePairDirection(model, moveId, 0, {nan, 1.0, 0.0}),
                   "reject non-finite move pair direction");
    dvatest::check(!setMovePairDirection(model, moveId, 0, {0.0, 0.0, 0.0}),
                   "reject zero move pair direction");
    dvatest::checkNear(model.moves[0].inputs.pairs[0].direction.ijk.x, 1.0,
                       1e-12, "zero move pair direction leaves value");
    std::vector<MovePair> movePairs = model.moves[0].inputs.pairs;
    movePairs[0].direction.ijk = {0.0, 0.0, 0.0};
    dvatest::check(!setMovePairs(model, moveId, movePairs),
                   "reject zero move pair direction list");
    dvatest::checkNear(model.moves[0].inputs.pairs[0].direction.ijk.x, 1.0,
                       1e-12, "zero move pair direction list leaves value");
    dvatest::check(!setMovePairObjectPoint(model, moveId, 0, {nan, 0.0, 0.0}),
                   "reject non-finite move object point");
    dvatest::check(!setMovePairTargetPoint(model, moveId, 0, {0.0, inf, 0.0}),
                   "reject non-finite move target point");
    dvatest::check(!setMeasureDirection(model, measureId, {1.0, nan, 0.0}),
                   "reject non-finite measure direction");
    dvatest::check(!setMeasureDirection(model, measureId, {0.0, 0.0, 0.0}),
                   "reject zero measure direction");
    dvatest::checkNear(model.measures[0].def.direction.ijk.z, 1.0, 1e-12,
                       "zero measure direction leaves value");
}

TEST("model editing: reorder moves by id") {
    Model model;
    MoveDef moveA;
    moveA.id = 601;
    moveA.name = "Move A";
    MoveDef moveB;
    moveB.id = 602;
    moveB.name = "Move B";
    MoveDef moveC;
    moveC.id = 603;
    moveC.name = "Move C";
    model.moves = {moveA, moveB, moveC};

    dvatest::check(reorderMove(model, 603, 0), "move C to front");
    dvatest::check(model.moves[0].id == 603, "C now first");
    dvatest::check(model.moves[1].id == 601, "A shifted second");
    dvatest::check(model.moves[2].id == 602, "B shifted third");

    dvatest::check(reorderMove(model, 603, 99), "out of range clamps to end");
    dvatest::check(model.moves[0].id == 601, "A now first");
    dvatest::check(model.moves[1].id == 602, "B now second");
    dvatest::check(model.moves[2].id == 603, "C now last");

    dvatest::check(!reorderMove(model, 999, 0), "missing move fails");
}

TEST("model editing: delete objects cleans dependent references") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PointId p1 = addCoordinatePoint(model, {0.0, 0.0, 0.0}, baseId);
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, baseId);
    const FeatureId survivingFeatureId = model.parts[0].features[0].id;
    const FeatureId featureId = addFeature(model, FeatureKind::Plane, baseId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, baseId);
    const GdtId gdtId = addGdt(model, GdtType::Flatness, 0.25, false, baseId);
    dvatest::check(setGdtFeatures(model, gdtId, {survivingFeatureId}),
                   "gdt controls surviving feature");
    dvatest::check(setGdtDrf(model, gdtId,
                             {featureId, kInvalidId, kInvalidId}),
                   "gdt datum references feature");
    const MeasureId measureId = addPointPointMeasure(model, 8.0, 12.0);
    dvatest::check(setMeasureInputFeatures(model, measureId, {featureId}),
                   "measure targets feature");
    const PartId targetId = addPart(model, "Target");
    const MoveId moveId = addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);
    addOrReplaceModelVariant(model, captureActiveVariant(model, "Baseline"));

    dvatest::check(deleteFeature(model, featureId), "feature deleted");
    dvatest::check(model.parts[0].features.size() == 2, "feature removed");
    dvatest::check(model.parts[0].tolerances.empty(),
                   "tolerance targeting feature removed");
    dvatest::check(model.parts[0].gdts.empty(),
                   "gdt referencing feature removed");
    dvatest::check(model.measures.empty(), "measure targeting feature removed");
    dvatest::check(model.variants[0].tolerances.empty(),
                   "variant tolerance reference removed");
    dvatest::check(model.variants[0].measures.empty(),
                   "variant measure reference removed");

    dvatest::check(!deleteMeasure(model, measureId),
                   "already removed measure delete fails");

    dvatest::check(deleteMove(model, moveId), "move deleted");
    dvatest::check(model.moves.empty(), "move removed");
    dvatest::check(model.variants[0].moves.empty(), "variant move reference removed");

    dvatest::check(deletePoint(model, p1), "point deleted");
    dvatest::check(model.parts[0].points.size() == 1, "point removed");

    dvatest::check(deletePart(model, targetId), "part deleted");
    dvatest::check(model.parts.size() == 1, "target part removed");

    dvatest::check(!deletePart(model, 999), "missing part delete fails");
    dvatest::check(!deletePoint(model, 999), "missing point delete fails");
    dvatest::check(!deleteFeature(model, 999), "missing feature delete fails");
    dvatest::check(!deleteTolerance(model, toleranceId),
                   "already removed tolerance delete fails");
    dvatest::check(!deleteGdt(model, gdtId), "already removed gdt delete fails");
    dvatest::check(!deleteMove(model, moveId), "already removed move delete fails");
}

TEST("model editing: deleting point cleans derived feature references") {
    Model model;
    const PartId partId = addPart(model, "Base");
    const PointId deletedPointId = addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const FeatureId deletedFeatureId = model.parts[0].features.back().id;
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    addCoordinatePoint(model, {10.0, 0.0, 0.0}, partId);
    const FeatureId survivingFeatureId = model.parts[0].features.back().id;

    const GdtId gdtId = addGdt(model, GdtType::Position, 0.25, false, partId);
    dvatest::check(setGdtActive(model, gdtId, false),
                   "deactivate gdt before staged delete fixture setup");
    dvatest::check(setGdtFeatures(model, gdtId, {survivingFeatureId}),
                   "gdt controls surviving feature");
    dvatest::check(setGdtDrf(model, gdtId,
                             {deletedFeatureId, kInvalidId, kInvalidId}),
                   "gdt datum references deleted point feature");
    dvatest::check(setGdtActive(model, gdtId, true),
                   "reactivate staged delete fixture gdt");

    const MeasureId measureId = addPointPointMeasure(model, 8.0, 12.0);
    dvatest::check(setMeasureInputPoints(model, measureId, {102, 103}),
                   "measure avoids deleted point directly");
    dvatest::check(setMeasureInputFeatures(model, measureId, {deletedFeatureId}),
                   "measure references deleted point feature");
    addOrReplaceModelVariant(model, captureActiveVariant(model, "Baseline"));

    dvatest::check(deletePoint(model, deletedPointId), "point deleted");

    dvatest::check(model.parts[0].features.size() == 2,
                   "point-derived feature removed");
    dvatest::check(model.parts[0].gdts.empty(),
                   "gdt datum referencing point-derived feature removed");
    dvatest::check(model.measures.empty(),
                   "measure referencing point-derived feature removed");
    dvatest::check(model.variants[0].measures.empty(),
                   "variant measure reference removed");
}

TEST("model editing: deleting point cleans direction references") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    const PointId p1 = addCoordinatePoint(model, {0.0, 0.0, 0.0}, baseId);
    const PointId p2 = addCoordinatePoint(model, {0.0, 0.0, 10.0}, baseId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, baseId);
    const PointId directionPoint =
        addCoordinatePoint(model, {10.0, 0.0, 0.0}, baseId);
    const PartId targetId = addPart(model, "Target");
    addCoordinatePoint(model, {20.0, 0.0, 0.0}, targetId);

    const MeasureId measureId = addPointPointMeasure(model, 8.0, 12.0);
    dvatest::check(setMeasureInputPoints(model, measureId, {p1, p2}),
                   "measure avoids deleted direction point as input");
    const MoveId moveId = addTransformMove(model, {1.0, 0.0, 0.0},
                                           baseId, targetId);

    model.parts[0].tolerances[0].ir.direction.type = DirectionType::TwoPoints;
    model.parts[0].tolerances[0].ir.direction.refPoints = {directionPoint};
    model.measures[0].def.direction.type = DirectionType::TwoPoints;
    model.measures[0].def.direction.refPoints = {directionPoint};
    model.moves[0].inputs.pairs[0].direction.type = DirectionType::TwoPoints;
    model.moves[0].inputs.pairs[0].direction.refPoints = {directionPoint};
    addOrReplaceModelVariant(model, captureActiveVariant(model, "Baseline"));

    dvatest::check(deletePoint(model, directionPoint), "direction point deleted");

    dvatest::check(model.parts[0].tolerances.empty(),
                   "tolerance using deleted direction point removed");
    dvatest::check(model.measures.empty(),
                   "measure using deleted direction point removed");
    dvatest::check(model.moves.empty(),
                   "move using deleted direction point removed");
    dvatest::check(model.variants[0].tolerances.empty(),
                   "variant tolerance reference removed");
    dvatest::check(model.variants[0].measures.empty(),
                   "variant measure reference removed");
    dvatest::check(model.variants[0].moves.empty(),
                   "variant move reference removed");
    (void)toleranceId;
    (void)moveId;
}

TEST("model editing: deleting part cleans external feature references") {
    Model model;
    const PartId deletedPartId = addPart(model, "Deleted");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, deletedPartId);
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, deletedPartId);
    const FeatureId deletedFeatureId = model.parts[0].features.back().id;

    const PartId survivingPartId = addPart(model, "Survivor");
    addCoordinatePoint(model, {10.0, 0.0, 0.0}, survivingPartId);
    addCoordinatePoint(model, {10.0, 0.0, 10.0}, survivingPartId);
    const FeatureId survivingFeatureId = model.parts[1].features.back().id;

    const GdtId gdtId = addGdt(model, GdtType::Position, 0.25, false,
                               survivingPartId);
    dvatest::check(setGdtActive(model, gdtId, false),
                   "deactivate gdt before staged delete fixture setup");
    dvatest::check(setGdtFeatures(model, gdtId, {survivingFeatureId}),
                   "gdt controls surviving feature");
    dvatest::check(setGdtDrf(model, gdtId,
                             {deletedFeatureId, kInvalidId, kInvalidId}),
                   "gdt datum references deleted part feature");
    dvatest::check(setGdtActive(model, gdtId, true),
                   "reactivate staged delete fixture gdt");

    const MeasureId measureId = addPointPointMeasure(model, 8.0, 12.0);
    dvatest::check(setMeasureInputPoints(model, measureId, {103, 104}),
                   "measure avoids deleted part points directly");
    dvatest::check(setMeasureInputFeatures(model, measureId, {deletedFeatureId}),
                   "measure references deleted part feature");
    addOrReplaceModelVariant(model, captureActiveVariant(model, "Baseline"));

    dvatest::check(deletePart(model, deletedPartId), "part deleted");

    dvatest::check(model.parts.size() == 1, "deleted part removed");
    dvatest::check(model.parts[0].id == survivingPartId, "survivor remains");
    dvatest::check(model.parts[0].gdts.empty(),
                   "gdt datum referencing deleted part feature removed");
    dvatest::check(model.measures.empty(),
                   "measure referencing deleted part feature removed");
    dvatest::check(model.variants[0].measures.empty(),
                   "variant measure reference removed");
}

TEST("model editing: deleting part cleans external direction references") {
    Model model;
    const PartId deletedPartId = addPart(model, "Deleted");
    const PointId directionPoint =
        addCoordinatePoint(model, {0.0, 0.0, 0.0}, deletedPartId);
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, deletedPartId);

    const PartId survivorId = addPart(model, "Survivor");
    const PointId p1 = addCoordinatePoint(model, {10.0, 0.0, 0.0}, survivorId);
    const PointId p2 = addCoordinatePoint(model, {10.0, 0.0, 10.0}, survivorId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.5, survivorId);
    const MeasureId measureId = addPointPointMeasure(model, 8.0, 12.0);
    dvatest::check(setMeasureInputPoints(model, measureId, {p1, p2}),
                   "measure avoids deleted part points as inputs");
    const PartId targetId = addPart(model, "Target");
    addCoordinatePoint(model, {20.0, 0.0, 0.0}, targetId);
    const MoveId moveId = addTransformMove(model, {1.0, 0.0, 0.0},
                                           survivorId, targetId);

    model.parts[1].tolerances[0].ir.direction.type = DirectionType::TwoPoints;
    model.parts[1].tolerances[0].ir.direction.refPoints = {directionPoint};
    model.measures[0].def.direction.type = DirectionType::TwoPoints;
    model.measures[0].def.direction.refPoints = {directionPoint};
    model.moves[0].inputs.pairs[0].direction.type = DirectionType::TwoPoints;
    model.moves[0].inputs.pairs[0].direction.refPoints = {directionPoint};
    addOrReplaceModelVariant(model, captureActiveVariant(model, "Baseline"));

    dvatest::check(deletePart(model, deletedPartId), "part deleted");

    dvatest::check(model.parts.size() == 2, "only deleted part removed");
    dvatest::check(model.parts[0].id == survivorId, "survivor remains");
    dvatest::check(model.parts[0].tolerances.empty(),
                   "tolerance using deleted part direction point removed");
    dvatest::check(model.measures.empty(),
                   "measure using deleted part direction point removed");
    dvatest::check(model.moves.empty(),
                   "move using deleted part direction point removed");
    dvatest::check(model.variants[0].tolerances.empty(),
                   "variant tolerance reference removed");
    dvatest::check(model.variants[0].measures.empty(),
                   "variant measure reference removed");
    dvatest::check(model.variants[0].moves.empty(),
                   "variant move reference removed");
    (void)toleranceId;
    (void)moveId;
}

TEST("model editing: capture apply replace and delete model variants") {
    Model model;
    const PartId partId = addPart(model, "Base");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    const PartId targetId = addPart(model, "Target");
    addCoordinatePoint(model, {1.0, 0.0, 0.0}, targetId);
    const ToleranceId tolA = addLinearTolerance(model, 0.4, partId);
    const ToleranceId tolB = addLinearTolerance(model, 0.8, partId);
    const MeasureId measureA = addPointPointMeasure(model, 8.0, 12.0);
    const MeasureId measureB = addPointPointMeasure(model, 9.0, 11.0);

    const MoveId moveA = addTransformMove(model, {1.0, 0.0, 0.0},
                                          partId, targetId);
    const MoveId moveB = addTransformMove(model, {2.0, 0.0, 0.0},
                                          partId, targetId);
    dvatest::check(renameMove(model, moveA, "Move A"), "rename move A");
    dvatest::check(renameMove(model, moveB, "Move B"), "rename move B");
    dvatest::check(setMoveActive(model, moveB, false), "deactivate move B");

    setToleranceActive(model, tolA, true);
    setToleranceActive(model, tolB, false);
    model.measures[0].def.active = true;
    model.measures[1].def.active = false;

    const ModelVariant baseline = captureActiveVariant(model, "Baseline");

    dvatest::check(baseline.name == "Baseline", "variant name captured");
    dvatest::check(baseline.moves.size() == 1 && baseline.moves[0] == moveA,
                   "active move captured");
    dvatest::check(baseline.tolerances.size() == 1 &&
                       baseline.tolerances[0] == tolA,
                   "active tolerance captured");
    dvatest::check(baseline.measures.size() == 1 &&
                       baseline.measures[0] == measureA,
                   "active measure captured");

    dvatest::check(addOrReplaceModelVariant(model, baseline), "variant added");
    dvatest::check(model.variants.size() == 1, "one variant stored");
    dvatest::check(renameModelVariant(model, "Baseline", "Baseline Edited"),
                   "variant renamed");
    dvatest::check(model.variants[0].name == "Baseline Edited",
                   "variant renamed value");
    dvatest::check(setModelVariantActive(model, "Baseline Edited", true),
                   "variant active edited");
    dvatest::check(model.variants[0].active, "variant active edited value");
    dvatest::check(setModelVariantMoves(model, "Baseline Edited", {moveB}),
                   "variant moves edited");
    dvatest::check(setModelVariantTolerances(model, "Baseline Edited", {}),
                   "variant tolerances edited");
    dvatest::check(setModelVariantMeasures(model, "Baseline Edited", {measureB}),
                   "variant measures edited");
    dvatest::check(model.variants[0].moves.size() == 1 &&
                       model.variants[0].moves[0] == moveB,
                   "variant moves edited value");
    dvatest::check(model.variants[0].tolerances.empty(),
                   "variant tolerances edited value");
    dvatest::check(model.variants[0].measures.size() == 1 &&
                       model.variants[0].measures[0] == measureB,
                   "variant measures edited value");
    dvatest::check(!renameModelVariant(model, "Baseline Edited", ""),
                   "variant empty rename rejected");
    dvatest::check(!setModelVariantMoves(model, "Missing", {moveA}),
                   "missing variant moves rejected");
    dvatest::check(!setModelVariantMoves(model, "Baseline Edited", {9999}),
                   "missing move variant reference rejected");
    dvatest::check(!setModelVariantTolerances(model, "Baseline Edited", {9999}),
                   "missing tolerance variant reference rejected");
    dvatest::check(!setModelVariantMeasures(model, "Baseline Edited", {9999}),
                   "missing measure variant reference rejected");
    ModelVariant invalidRefs = baseline;
    invalidRefs.name = "InvalidRefs";
    invalidRefs.moves = {9999};
    invalidRefs.tolerances = {9999};
    invalidRefs.measures = {9999};
    dvatest::check(!addOrReplaceModelVariant(model, invalidRefs),
                   "variant with missing references rejected");
    dvatest::check(model.variants.size() == 1,
                   "invalid variant not stored");
    ModelVariant alternate = baseline;
    alternate.name = "Alternate";
    alternate.active = true;
    alternate.moves = {moveA};
    dvatest::check(addOrReplaceModelVariant(model, alternate),
                   "active variant added");
    dvatest::check(model.variants.size() == 2, "two variants stored");
    dvatest::check(!model.variants[0].active && model.variants[1].active,
                   "adding active variant clears previous active");
    dvatest::check(deleteModelVariant(model, "Alternate"),
                   "temporary active variant deleted");
    dvatest::check(setModelVariantActive(model, "Baseline Edited", true),
                   "baseline variant reactivated");

    setToleranceActive(model, tolA, false);
    setToleranceActive(model, tolB, true);
    model.moves[0].active = false;
    model.moves[1].active = true;
    model.measures[0].def.active = false;
    model.measures[1].def.active = true;

    dvatest::check(applyModelVariant(model, "Baseline Edited"), "variant applied");
    dvatest::check(!model.moves[0].active && model.moves[1].active,
                   "move activity restored");
    dvatest::check(!model.parts[0].tolerances[0].active &&
                       !model.parts[0].tolerances[1].active,
                   "tolerance activity restored");
    dvatest::check(!model.measures[0].def.active && model.measures[1].def.active,
                   "measure activity restored");
    dvatest::check(model.variants[0].active, "variant active flag set");

    ModelVariant replacement = captureActiveVariant(model, "Baseline Edited");
    replacement.measures = {measureB};
    dvatest::check(addOrReplaceModelVariant(model, replacement),
                   "same-name variant replaced");
    dvatest::check(model.variants.size() == 1, "replace keeps one variant");
    dvatest::check(model.variants[0].measures[0] == measureB,
                   "replacement stored");

    dvatest::check(deleteModelVariant(model, "Baseline Edited"), "variant deleted");
    dvatest::check(model.variants.empty(), "variant list empty");
    dvatest::check(!applyModelVariant(model, "Baseline Edited"),
                   "missing apply fails");
    dvatest::check(!deleteModelVariant(model, "Baseline Edited"),
                   "missing delete fails");
}

TEST("model editing: applying model variant rejects invalid active state") {
    Model model;
    const PartId partId = addPart(model, "Base");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.4, partId);
    dvatest::check(addOrReplaceModelVariant(
                       model, captureActiveVariant(model, "Baseline")),
                   "baseline variant stored");

    dvatest::check(setToleranceActive(model, toleranceId, false),
                   "deactivate tolerance before staging invalid state");
    model.parts[0].tolerances[0].ir.rangeScale = 0.0;

    dvatest::check(!applyModelVariant(model, "Baseline"),
                   "reject applying variant with invalid active tolerance");
    dvatest::check(!model.parts[0].tolerances[0].active,
                   "invalid variant apply leaves tolerance inactive");
    dvatest::check(!model.variants[0].active,
                   "invalid variant apply leaves variant inactive");
}

TEST("model editing: activating model variant rejects invalid active state") {
    Model model;
    const PartId partId = addPart(model, "Base");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.4, partId);
    dvatest::check(addOrReplaceModelVariant(
                       model, captureActiveVariant(model, "Baseline")),
                   "baseline variant stored");

    dvatest::check(setToleranceActive(model, toleranceId, false),
                   "deactivate tolerance before staging invalid state");
    model.parts[0].tolerances[0].ir.rangeScale = 0.0;

    dvatest::check(!setModelVariantActive(model, "Baseline", true),
                   "reject activating variant with invalid tolerance state");
    dvatest::check(!model.variants[0].active,
                   "failed variant activation leaves variant inactive");
}

TEST("model editing: adding active model variant rejects invalid active state") {
    Model model;
    const PartId partId = addPart(model, "Base");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.4, partId);

    dvatest::check(setToleranceActive(model, toleranceId, false),
                   "deactivate tolerance before staging invalid state");
    model.parts[0].tolerances[0].ir.rangeScale = 0.0;

    ModelVariant baseline;
    baseline.name = "Baseline";
    baseline.active = true;
    baseline.tolerances = {toleranceId};

    dvatest::check(!addOrReplaceModelVariant(model, baseline),
                   "reject active variant with invalid tolerance state");
    dvatest::check(model.variants.empty(),
                   "failed active variant add leaves variants unchanged");
}

TEST("model editing: adding active model variant rejects invalid active gdt state") {
    Model model;
    const PartId partId = addPart(model, "Base");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    addGdt(model, GdtType::Flatness, 0.3, false, partId);
    model.parts[0].gdts[0].range = -0.1;

    ModelVariant baseline;
    baseline.name = "Baseline";
    baseline.active = true;

    dvatest::check(!addOrReplaceModelVariant(model, baseline),
                   "reject active variant with invalid gdt state");
    dvatest::check(model.variants.empty(),
                   "failed active variant gdt add leaves variants unchanged");
}

TEST("model editing: active model variant tolerance edits reject invalid state") {
    Model model;
    const PartId partId = addPart(model, "Base");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.4, partId);

    ModelVariant baseline;
    baseline.name = "Baseline";
    baseline.active = true;
    dvatest::check(addOrReplaceModelVariant(model, baseline),
                   "active baseline variant stored");

    dvatest::check(setToleranceActive(model, toleranceId, false),
                   "deactivate tolerance before staging invalid state");
    model.parts[0].tolerances[0].ir.rangeScale = 0.0;

    dvatest::check(!setModelVariantTolerances(model, "Baseline", {toleranceId}),
                   "reject active variant tolerance edit with invalid state");
    dvatest::check(model.variants[0].tolerances.empty(),
                   "failed active variant tolerance edit leaves refs unchanged");
}

TEST("model editing: active model variant measure edits reject invalid state") {
    Model model;
    const PartId partId = addPart(model, "Base");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    const MeasureId measureId = addPointPointMeasure(model, 8.0, 12.0);

    ModelVariant baseline;
    baseline.name = "Baseline";
    baseline.active = true;
    dvatest::check(addOrReplaceModelVariant(model, baseline),
                   "active baseline variant stored");

    dvatest::check(setMeasureActive(model, measureId, false),
                   "deactivate measure before staging invalid state");
    model.measures[0].def.scale = 0.0;

    dvatest::check(!setModelVariantMeasures(model, "Baseline", {measureId}),
                   "reject active variant measure edit with invalid state");
    dvatest::check(model.variants[0].measures.empty(),
                   "failed active variant measure edit leaves refs unchanged");
}

TEST("model editing: active model variant move edits reject invalid state") {
    Model model;
    const PartId baseId = addPart(model, "Base");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, baseId);
    const PartId targetId = addPart(model, "Target");
    addCoordinatePoint(model, {1.0, 0.0, 0.0}, targetId);
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, baseId, targetId);

    ModelVariant baseline;
    baseline.name = "Baseline";
    baseline.active = true;
    dvatest::check(addOrReplaceModelVariant(model, baseline),
                   "active baseline variant stored");

    dvatest::check(setMoveActive(model, moveId, false),
                   "deactivate move before staging invalid state");
    dvatest::check(setMoveType(model, moveId, MoveType::UserDll),
                   "stage inactive unbound user-dll move");

    dvatest::check(!setModelVariantMoves(model, "Baseline", {moveId}),
                   "reject active variant move edit with invalid state");
    dvatest::check(model.variants[0].moves.empty(),
                   "failed active variant move edit leaves refs unchanged");
}

TEST("model editing: model variants reject duplicate references") {
    Model model;
    const PartId partId = addPart(model, "Base");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    addCoordinatePoint(model, {0.0, 0.0, 10.0}, partId);
    const ToleranceId toleranceId = addLinearTolerance(model, 0.4, partId);
    const MeasureId measureId = addPointPointMeasure(model, 8.0, 12.0);
    MoveDef move;
    move.id = 601;
    move.name = "Move A";
    model.moves = {move};

    ModelVariant duplicate;
    duplicate.name = "Duplicate Refs";
    duplicate.moves = {move.id, move.id};
    duplicate.tolerances = {toleranceId, toleranceId};
    duplicate.measures = {measureId, measureId};
    dvatest::check(!addOrReplaceModelVariant(model, duplicate),
                   "duplicate variant refs rejected");
    dvatest::check(model.variants.empty(), "duplicate variant not stored");

    ModelVariant baseline;
    baseline.name = "Baseline";
    baseline.moves = {move.id};
    baseline.tolerances = {toleranceId};
    baseline.measures = {measureId};
    dvatest::check(addOrReplaceModelVariant(model, baseline), "baseline stored");
    dvatest::check(!setModelVariantMoves(model, "Baseline", {move.id, move.id}),
                   "duplicate variant moves rejected");
    dvatest::check(!setModelVariantTolerances(
                       model, "Baseline", {toleranceId, toleranceId}),
                   "duplicate variant tolerances rejected");
    dvatest::check(!setModelVariantMeasures(
                       model, "Baseline", {measureId, measureId}),
                   "duplicate variant measures rejected");
    dvatest::check(model.variants[0].moves.size() == 1 &&
                       model.variants[0].moves[0] == move.id,
                   "failed duplicate move edit leaves value unchanged");
    dvatest::check(model.variants[0].tolerances.size() == 1 &&
                       model.variants[0].tolerances[0] == toleranceId,
                   "failed duplicate tolerance edit leaves value unchanged");
    dvatest::check(model.variants[0].measures.size() == 1 &&
                       model.variants[0].measures[0] == measureId,
                   "failed duplicate measure edit leaves value unchanged");
}
