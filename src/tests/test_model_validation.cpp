// Domain model validation for desktop run gating.
#include <limits>

#include "dva_test.h"
#include "opendva/dcs_plugin_api.h"
#include "opendva/domain/ModelEditing.h"
#include "opendva/domain/ModelValidation.h"
#include "opendva/plugin/PluginHost.h"

using namespace opendva;

namespace {

void boundMeasureRoutine(dcsDataPtr) {}
void boundMoveRoutine(dcsDataPtr) {}

bool hasIssue(const std::vector<ModelIssue>& issues, const std::string& code) {
    for (const ModelIssue& issue : issues) {
        if (issue.code == code) return true;
    }
    return false;
}

ModelIssueCategory issueCategory(const std::vector<ModelIssue>& issues,
                                 const std::string& code) {
    for (const ModelIssue& issue : issues) {
        if (issue.code == code) return issue.category;
    }
    return ModelIssueCategory::Model;
}

std::string issueMessage(const std::vector<ModelIssue>& issues,
                         const std::string& code) {
    for (const ModelIssue& issue : issues) {
        if (issue.code == code) return issue.message;
    }
    return {};
}

Model makeRunnableModel() {
    return createStarterModel();
}

}  // namespace

TEST("model validation: runnable starter model has no blocking issues") {
    const Model model = makeRunnableModel();

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(!hasBlockingIssues(issues), "runnable model has no blocking issues");
}

TEST("model validation: empty model reports missing parts and measures") {
    const Model model;

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues), "empty model is blocked");
    dvatest::check(hasIssue(issues, "model.no_parts"), "missing parts issue");
    dvatest::check(hasIssue(issues, "model.no_active_measures"), "missing measures issue");
    dvatest::check(issueCategory(issues, "model.no_parts") == ModelIssueCategory::Model,
                   "missing parts is a model issue");
}

TEST("model validation: point-point measure requires existing input points") {
    Model model = makeRunnableModel();
    model.measures[0].def.inputPoints = {101, 999};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues), "dangling point blocks run");
    dvatest::check(hasIssue(issues, "measure.input_point_missing"),
                   "missing input point issue");
    dvatest::check(issueCategory(issues, "measure.input_point_missing") ==
                       ModelIssueCategory::Measure,
                   "missing input point is a measure issue");
}

TEST("model validation: active measure input points require distinct refs") {
    Model model = makeRunnableModel();
    const PointId pointId = model.parts[0].points[0].id;
    model.measures[0].def.type = MeasureType::PointPoint;
    model.measures[0].def.inputPoints = {pointId, pointId};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "duplicate measure input points block run");
    dvatest::check(hasIssue(issues, "measure.input_points_not_distinct"),
                   "duplicate measure input point issue");
    dvatest::check(issueCategory(issues, "measure.input_points_not_distinct") ==
                       ModelIssueCategory::Measure,
                   "duplicate measure input point is a measure issue");
}

TEST("model validation: point-line measure requires enough input points") {
    Model model = makeRunnableModel();
    model.measures[0].def.type = MeasureType::PointLine;
    model.measures[0].def.inputPoints = {model.parts[0].points[0].id,
                                         model.parts[0].points[1].id};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "underconstrained point-line measure blocks run");
    dvatest::check(hasIssue(issues, "measure.not_enough_input_points"),
                   "not enough measure input points issue");
    dvatest::check(issueCategory(issues, "measure.not_enough_input_points") ==
                       ModelIssueCategory::Measure,
                   "not enough measure input points is a measure issue");
}

TEST("model validation: feature angle measure requires two feature vectors") {
    Model model = makeRunnableModel();
    const PointId thirdPoint =
        addCoordinatePoint(model, {1.0, 0.0, 0.0}, model.parts[0].id);
    dvatest::check(thirdPoint != kInvalidId, "third point created");
    model.measures[0].def.type = MeasureType::FeatureAngle;
    model.measures[0].def.inputPoints = {model.parts[0].points[0].id,
                                         model.parts[0].points[1].id,
                                         thirdPoint};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "underconstrained feature angle blocks run");
    dvatest::check(hasIssue(issues, "measure.not_enough_input_points"),
                   "not enough feature angle input points issue");
    dvatest::check(issueCategory(issues, "measure.not_enough_input_points") ==
                       ModelIssueCategory::Measure,
                   "feature angle point-count issue is a measure issue");
}

TEST("model validation: feature angle can use feature inputs") {
    Model model;
    Part part;
    part.id = 1;
    part.dcsName = "P";
    Point x0;
    x0.id = 101;
    x0.position = {0, 0, 0};
    Point x1;
    x1.id = 102;
    x1.position = {1, 0, 0};
    Point y0;
    y0.id = 103;
    y0.position = {0, 0, 0};
    Point y1;
    y1.id = 104;
    y1.position = {0, 1, 0};
    part.points = {x0, x1, y0, y1};
    Feature featureX;
    featureX.id = 201;
    featureX.kind = FeatureKind::Edge;
    featureX.definingPoints = {101, 102};
    Feature featureY;
    featureY.id = 202;
    featureY.kind = FeatureKind::Edge;
    featureY.definingPoints = {103, 104};
    part.features = {featureX, featureY};
    model.parts = {part};

    MeasureRecord angle;
    angle.id = 401;
    angle.name = "feature_angle";
    angle.def.type = MeasureType::FeatureAngle;
    angle.def.inputFeatures = {201, 202};
    angle.def.direction.ijk = {0, 0, 1};
    model.measures = {angle};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(!hasBlockingIssues(issues),
                   "feature angle with feature inputs does not block run");
    dvatest::check(!hasIssue(issues, "measure.not_enough_input_points"),
                   "feature angle feature inputs satisfy point-count rule");
}

TEST("model validation: active measure cannot reference inactive input points") {
    Model model = makeRunnableModel();
    model.parts[0].points[0].active = false;
    model.measures[0].def.inputPoints = {model.parts[0].points[0].id,
                                         model.parts[0].points[1].id};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "inactive point reference blocks run");
    dvatest::check(hasIssue(issues, "measure.input_point_inactive"),
                   "inactive input point issue");
    dvatest::check(issueCategory(issues, "measure.input_point_inactive") ==
                       ModelIssueCategory::Measure,
                   "inactive input point is a measure issue");
}

TEST("model validation: active measure feature requires active defining points") {
    Model model = makeRunnableModel();
    const FeatureId measureFeatureId = model.parts[0].features[0].id;
    const PointId inactivePointId =
        model.parts[0].features[0].definingPoints[0];
    for (Point& point : model.parts[0].points) {
        if (point.id == inactivePointId) point.active = false;
    }
    model.measures[0].def.type = MeasureType::FeatureMeasure;
    model.measures[0].def.inputPoints.clear();
    model.measures[0].def.inputFeatures = {measureFeatureId};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "inactive feature point blocks measure run");
    dvatest::check(hasIssue(issues, "measure.input_feature_point_inactive"),
                   "inactive measure feature point issue");
    dvatest::check(issueCategory(issues, "measure.input_feature_point_inactive") ==
                       ModelIssueCategory::Measure,
                   "inactive measure feature point is a measure issue");
}

TEST("model validation: feature measure requires input features") {
    Model model = makeRunnableModel();
    model.measures[0].def.type = MeasureType::FeatureMeasure;
    model.measures[0].def.inputPoints.clear();
    model.measures[0].def.inputFeatures.clear();

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "feature measure without features blocks run");
    dvatest::check(hasIssue(issues, "measure.no_input_features"),
                   "missing measure feature issue");
    dvatest::check(issueCategory(issues, "measure.no_input_features") ==
                       ModelIssueCategory::Measure,
                   "missing measure feature is a measure issue");
}

TEST("model validation: feature measure input features require distinct refs") {
    Model model = makeRunnableModel();
    const FeatureId featureId = model.parts[0].features[0].id;
    model.measures[0].def.type = MeasureType::FeatureMeasure;
    model.measures[0].def.inputPoints.clear();
    model.measures[0].def.inputFeatures = {featureId, featureId};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "duplicate feature measure inputs block run");
    dvatest::check(hasIssue(issues, "measure.input_features_not_distinct"),
                   "duplicate feature measure input issue");
    dvatest::check(issueCategory(issues, "measure.input_features_not_distinct") ==
                       ModelIssueCategory::Measure,
                   "duplicate feature measure input is a measure issue");
}

TEST("model validation: active tolerance requires a feature and random variable") {
    Model model = makeRunnableModel();
    model.parts[0].tolerances[0].features.clear();
    model.parts[0].tolerances[0].ir.rands.clear();

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues), "invalid tolerance blocks run");
    dvatest::check(hasIssue(issues, "tolerance.no_features"), "missing feature issue");
    dvatest::check(hasIssue(issues, "tolerance.no_random_variables"),
                   "missing random variable issue");
    dvatest::check(issueCategory(issues, "tolerance.no_features") ==
                       ModelIssueCategory::Tolerance,
                   "missing controlled feature is a tolerance issue");
}

TEST("model validation: active tolerance features require distinct refs") {
    Model model = makeRunnableModel();
    const FeatureId featureId = model.parts[0].tolerances[0].features[0];
    model.parts[0].tolerances[0].features = {featureId, featureId};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "duplicate tolerance feature refs block run");
    dvatest::check(hasIssue(issues, "tolerance.features_not_distinct"),
                   "duplicate tolerance feature refs issue");
    dvatest::check(issueCategory(issues, "tolerance.features_not_distinct") ==
                       ModelIssueCategory::Tolerance,
                   "duplicate tolerance feature refs is a tolerance issue");
}

TEST("model validation: active tolerance truncation must be finite and ordered") {
    Model model = makeRunnableModel();
    model.parts[0].tolerances[0].ir.truncation.active = true;
    model.parts[0].tolerances[0].ir.truncation.minTrunc =
        std::numeric_limits<double>::quiet_NaN();

    std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "non-finite truncation blocks run");
    dvatest::check(hasIssue(issues, "tolerance.truncation_non_finite"),
                   "non-finite truncation issue");
    dvatest::check(issueCategory(issues, "tolerance.truncation_non_finite") ==
                       ModelIssueCategory::Tolerance,
                   "non-finite truncation is a tolerance issue");

    model.parts[0].tolerances[0].ir.truncation.minTrunc = 0.5;
    model.parts[0].tolerances[0].ir.truncation.maxTrunc = -0.5;
    issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "misordered truncation blocks run");
    dvatest::check(hasIssue(issues, "tolerance.truncation_misordered"),
                   "misordered truncation issue");
}

TEST("model validation: active tolerance random offset must be finite") {
    Model model = makeRunnableModel();
    model.parts[0].tolerances[0].ir.rands[0].offset =
        std::numeric_limits<double>::infinity();

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "non-finite random offset blocks run");
    dvatest::check(hasIssue(issues, "tolerance.offset_non_finite"),
                   "non-finite random offset issue");
    dvatest::check(issueCategory(issues, "tolerance.offset_non_finite") ==
                       ModelIssueCategory::Tolerance,
                   "non-finite random offset is a tolerance issue");
}

TEST("model validation: active tolerance feature requires active defining points") {
    Model model = makeRunnableModel();
    const FeatureId toleranceFeatureId = model.parts[0].tolerances[0].features[0];
    PointId inactivePointId = kInvalidId;
    for (const Feature& feature : model.parts[0].features) {
        if (feature.id == toleranceFeatureId) {
            inactivePointId = feature.definingPoints[0];
        }
    }
    for (Point& point : model.parts[0].points) {
        if (point.id == inactivePointId) point.active = false;
    }

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "inactive feature point blocks tolerance run");
    dvatest::check(hasIssue(issues, "tolerance.feature_point_inactive"),
                   "inactive tolerance feature point issue");
    dvatest::check(issueCategory(issues, "tolerance.feature_point_inactive") ==
                       ModelIssueCategory::Tolerance,
                   "inactive tolerance feature point is a tolerance issue");
}

TEST("model validation: active gdt requires existing datum features") {
    Model model = makeRunnableModel();
    const GdtId gdtId = addGdt(model, GdtType::Position, 0.25, false,
                               model.parts[0].id);
    dvatest::check(gdtId != kInvalidId, "gdt created");
    model.parts[0].gdts[0].drf.primary = 999;

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues), "dangling gdt datum blocks run");
    dvatest::check(hasIssue(issues, "gdt.datum_missing"),
                   "missing gdt datum issue");
    dvatest::check(issueCategory(issues, "gdt.datum_missing") ==
                       ModelIssueCategory::Gdt,
                   "missing datum is a gdt issue");
}

TEST("model validation: active gdt type requiring drf requires datum") {
    Model model = makeRunnableModel();
    const GdtId gdtId = addGdt(model, GdtType::Position, 0.25, false,
                               model.parts[0].id);
    dvatest::check(gdtId != kInvalidId, "gdt created");
    model.parts[0].gdts[0].drf = {};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "position gdt without datum blocks run");
    dvatest::check(hasIssue(issues, "gdt.datum_required"),
                   "missing required gdt datum issue");
    dvatest::check(issueCategory(issues, "gdt.datum_required") ==
                       ModelIssueCategory::Gdt,
                   "missing required gdt datum is a gdt issue");
}

TEST("model validation: active gdt feature requires active defining points") {
    Model model = makeRunnableModel();
    const GdtId gdtId = addGdt(model, GdtType::Position, 0.25, false,
                               model.parts[0].id);
    dvatest::check(gdtId != kInvalidId, "gdt created");
    const FeatureId gdtFeatureId = model.parts[0].gdts[0].features[0];
    PointId inactivePointId = kInvalidId;
    for (const Feature& feature : model.parts[0].features) {
        if (feature.id == gdtFeatureId) inactivePointId = feature.definingPoints[0];
    }
    for (Point& point : model.parts[0].points) {
        if (point.id == inactivePointId) point.active = false;
    }

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "inactive feature point blocks gdt run");
    dvatest::check(hasIssue(issues, "gdt.feature_point_inactive"),
                   "inactive gdt feature point issue");
    dvatest::check(issueCategory(issues, "gdt.feature_point_inactive") ==
                       ModelIssueCategory::Gdt,
                   "inactive gdt feature point is a gdt issue");
}

TEST("model validation: active gdt features require distinct refs") {
    Model model = makeRunnableModel();
    const GdtId gdtId = addGdt(model, GdtType::Position, 0.25, false,
                               model.parts[0].id);
    dvatest::check(gdtId != kInvalidId, "gdt created");
    const FeatureId featureId = model.parts[0].gdts[0].features[0];
    model.parts[0].gdts[0].features = {featureId, featureId};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "duplicate gdt feature refs block run");
    dvatest::check(hasIssue(issues, "gdt.features_not_distinct"),
                   "duplicate gdt feature refs issue");
    dvatest::check(issueCategory(issues, "gdt.features_not_distinct") ==
                       ModelIssueCategory::Gdt,
                   "duplicate gdt feature refs is a gdt issue");
}

TEST("model validation: active gdt datum requires active defining points") {
    Model model = makeRunnableModel();
    const GdtId gdtId = addGdt(model, GdtType::Position, 0.25, false,
                               model.parts[0].id);
    dvatest::check(gdtId != kInvalidId, "gdt created");
    const FeatureId datumFeatureId = model.parts[0].features[0].id;
    model.parts[0].gdts[0].drf.primary = datumFeatureId;
    const PointId inactivePointId =
        model.parts[0].features[0].definingPoints[0];
    for (Point& point : model.parts[0].points) {
        if (point.id == inactivePointId) point.active = false;
    }

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "inactive datum point blocks gdt run");
    dvatest::check(hasIssue(issues, "gdt.datum_point_inactive"),
                   "inactive gdt datum point issue");
    dvatest::check(issueCategory(issues, "gdt.datum_point_inactive") ==
                       ModelIssueCategory::Gdt,
                   "inactive gdt datum point is a gdt issue");
}

TEST("model validation: active gdt datums require distinct refs") {
    Model model = makeRunnableModel();
    const GdtId gdtId = addGdt(model, GdtType::Position, 0.25, false,
                               model.parts[0].id);
    dvatest::check(gdtId != kInvalidId, "gdt created");
    const FeatureId datumFeatureId = model.parts[0].features[0].id;
    model.parts[0].gdts[0].drf.primary = datumFeatureId;
    model.parts[0].gdts[0].drf.secondary = datumFeatureId;

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "duplicate gdt datum refs block run");
    dvatest::check(hasIssue(issues, "gdt.datums_not_distinct"),
                   "duplicate gdt datum refs issue");
    dvatest::check(issueCategory(issues, "gdt.datums_not_distinct") ==
                       ModelIssueCategory::Gdt,
                   "duplicate gdt datum refs is a gdt issue");
}

TEST("model validation: active gdt datums require contiguous refs") {
    Model model = makeRunnableModel();
    const GdtId gdtId = addGdt(model, GdtType::Position, 0.25, false,
                               model.parts[0].id);
    dvatest::check(gdtId != kInvalidId, "gdt created");
    model.parts[0].gdts[0].drf.primary = kInvalidId;
    model.parts[0].gdts[0].drf.secondary = model.parts[0].features[0].id;

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "non-contiguous gdt datum refs block run");
    dvatest::check(hasIssue(issues, "gdt.datum_order_invalid"),
                   "non-contiguous gdt datum refs issue");
    dvatest::check(issueCategory(issues, "gdt.datum_order_invalid") ==
                       ModelIssueCategory::Gdt,
                   "non-contiguous gdt datum refs is a gdt issue");
}

TEST("model validation: active directions require existing ref points") {
    Model model = makeRunnableModel();
    model.parts[0].tolerances[0].ir.direction.type = DirectionType::TwoPoints;
    model.parts[0].tolerances[0].ir.direction.refPoints = {101, 999};
    model.measures[0].def.direction.type = DirectionType::TwoPoints;
    model.measures[0].def.direction.refPoints = {101, 998};

    const PartId targetPartId = addPart(model, "Target");
    MoveDef move;
    move.id = 601;
    move.name = "Move With Direction";
    move.active = true;
    move.inputs.type = MoveType::Transform;
    MovePair pair;
    pair.direction.type = DirectionType::TwoPoints;
    pair.direction.refPoints = {101, 997};
    move.inputs.pairs = {pair};
    move.moveParts = {model.parts[0].id, targetPartId};
    model.moves = {move};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues), "dangling direction points block run");
    dvatest::check(hasIssue(issues, "tolerance.direction_point_missing"),
                   "missing tolerance direction point issue");
    dvatest::check(hasIssue(issues, "measure.direction_point_missing"),
                   "missing measure direction point issue");
    dvatest::check(hasIssue(issues, "move.direction_point_missing"),
                   "missing move direction point issue");
}

TEST("model validation: active directions require active ref points") {
    Model model = makeRunnableModel();
    model.parts[0].points[0].active = false;

    model.parts[0].tolerances[0].ir.direction.type = DirectionType::TwoPoints;
    model.parts[0].tolerances[0].ir.direction.refPoints = {
        model.parts[0].points[0].id, model.parts[0].points[1].id};
    model.measures[0].def.direction.type = DirectionType::TwoPoints;
    model.measures[0].def.direction.refPoints = {model.parts[0].points[0].id,
                                                 model.parts[0].points[1].id};

    const PartId targetPartId = addPart(model, "Target");
    MoveDef move;
    move.id = 602;
    move.name = "Move With Inactive Direction Point";
    move.active = true;
    move.inputs.type = MoveType::Transform;
    MovePair pair;
    pair.direction.type = DirectionType::TwoPoints;
    pair.direction.refPoints = {model.parts[0].points[0].id,
                                model.parts[0].points[1].id};
    move.inputs.pairs = {pair};
    move.moveParts = {model.parts[0].id, targetPartId};
    model.moves = {move};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "inactive direction points block run");
    dvatest::check(hasIssue(issues, "tolerance.direction_point_inactive"),
                   "inactive tolerance direction point issue");
    dvatest::check(hasIssue(issues, "measure.direction_point_inactive"),
                   "inactive measure direction point issue");
    dvatest::check(hasIssue(issues, "move.direction_point_inactive"),
                   "inactive move direction point issue");
}

TEST("model validation: two-point directions require two ref points") {
    Model model = makeRunnableModel();
    model.parts[0].tolerances[0].ir.direction.type = DirectionType::TwoPoints;
    model.parts[0].tolerances[0].ir.direction.refPoints = {101};
    model.measures[0].def.direction.type = DirectionType::TwoPoints;
    model.measures[0].def.direction.refPoints = {101};

    const PartId targetPartId = addPart(model, "Target");
    MoveDef move;
    move.id = 603;
    move.name = "Move With Incomplete TwoPoint Direction";
    move.active = true;
    move.inputs.type = MoveType::Transform;
    MovePair pair;
    pair.direction.type = DirectionType::TwoPoints;
    pair.direction.refPoints = {101};
    move.inputs.pairs = {pair};
    move.moveParts = {model.parts[0].id, targetPartId};
    model.moves = {move};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "incomplete two-point directions block run");
    dvatest::check(hasIssue(issues, "tolerance.direction_point_count"),
                   "tolerance two-point count issue");
    dvatest::check(hasIssue(issues, "measure.direction_point_count"),
                   "measure two-point count issue");
    dvatest::check(hasIssue(issues, "move.direction_point_count"),
                   "move two-point count issue");
}

TEST("model validation: normal and pick directions require one ref point") {
    Model model = makeRunnableModel();
    model.parts[0].tolerances[0].ir.direction.type = DirectionType::Normal;
    model.parts[0].tolerances[0].ir.direction.refPoints = {};
    model.measures[0].def.direction.type = DirectionType::PickPtDir;
    model.measures[0].def.direction.refPoints = {};

    const PartId targetPartId = addPart(model, "Target");
    MoveDef move;
    move.id = 604;
    move.name = "Move With Incomplete Normal Direction";
    move.active = true;
    move.inputs.type = MoveType::Transform;
    MovePair pair;
    pair.direction.type = DirectionType::Normal;
    pair.direction.refPoints = {};
    move.inputs.pairs = {pair};
    move.moveParts = {model.parts[0].id, targetPartId};
    model.moves = {move};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "incomplete normal/pick directions block run");
    dvatest::check(hasIssue(issues, "tolerance.direction_point_count"),
                   "tolerance normal count issue");
    dvatest::check(hasIssue(issues, "measure.direction_point_count"),
                   "measure pick count issue");
    dvatest::check(hasIssue(issues, "move.direction_point_count"),
                   "move normal count issue");
}

TEST("model validation: two-point directions require distinct ref points") {
    Model model = makeRunnableModel();
    model.parts[0].tolerances[0].ir.direction.type = DirectionType::TwoPoints;
    model.parts[0].tolerances[0].ir.direction.refPoints = {101, 101};
    model.measures[0].def.direction.type = DirectionType::TwoPoints;
    model.measures[0].def.direction.refPoints = {101, 101};

    const PartId targetPartId = addPart(model, "Target");
    MoveDef move;
    move.id = 605;
    move.name = "Move With Degenerate TwoPoint Direction";
    move.active = true;
    move.inputs.type = MoveType::Transform;
    MovePair pair;
    pair.direction.type = DirectionType::TwoPoints;
    pair.direction.refPoints = {101, 101};
    move.inputs.pairs = {pair};
    move.moveParts = {model.parts[0].id, targetPartId};
    model.moves = {move};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "duplicate two-point directions block run");
    dvatest::check(hasIssue(issues, "tolerance.direction_points_not_distinct"),
                   "tolerance two-point distinct issue");
    dvatest::check(hasIssue(issues, "measure.direction_points_not_distinct"),
                   "measure two-point distinct issue");
    dvatest::check(hasIssue(issues, "move.direction_points_not_distinct"),
                   "move two-point distinct issue");
}

TEST("model validation: type-in directions require nonzero vector") {
    Model model = makeRunnableModel();
    model.parts[0].tolerances[0].ir.direction.type = DirectionType::TypeIn;
    model.parts[0].tolerances[0].ir.direction.ijk = {0, 0, 0};
    model.measures[0].def.direction.type = DirectionType::TypeIn;
    model.measures[0].def.direction.ijk = {0, 0, 0};

    const PartId targetPartId = addPart(model, "Target");
    MoveDef move;
    move.id = 606;
    move.name = "Move With Zero Direction";
    move.active = true;
    move.inputs.type = MoveType::Transform;
    MovePair pair;
    pair.direction.type = DirectionType::TypeIn;
    pair.direction.ijk = {0, 0, 0};
    move.inputs.pairs = {pair};
    move.moveParts = {model.parts[0].id, targetPartId};
    model.moves = {move};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "zero type-in directions block run");
    dvatest::check(hasIssue(issues, "tolerance.direction_zero"),
                   "zero tolerance direction issue");
    dvatest::check(hasIssue(issues, "measure.direction_zero"),
                   "zero measure direction issue");
    dvatest::check(hasIssue(issues, "move.direction_zero"),
                   "zero move direction issue");
}

TEST("model validation: point directions require nonzero vector") {
    Model model = makeRunnableModel();
    model.parts[0].points[0].ijk = {0, 0, 0};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "zero point direction blocks run");
    dvatest::check(hasIssue(issues, "point.direction_zero"),
                   "zero point direction issue");
    dvatest::check(issueCategory(issues, "point.direction_zero") ==
                       ModelIssueCategory::Feature,
                   "zero point direction is a feature issue");
}

TEST("model validation: feature defining points require distinct refs") {
    Model model = makeRunnableModel();
    const PointId pointId = model.parts[0].features[0].definingPoints[0];
    model.parts[0].features[0].definingPoints = {pointId, pointId};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "duplicate feature defining points block run");
    dvatest::check(hasIssue(issues, "feature.points_not_distinct"),
                   "duplicate feature defining point issue");
    dvatest::check(issueCategory(issues, "feature.points_not_distinct") ==
                       ModelIssueCategory::Feature,
                   "duplicate feature defining point is a feature issue");
}

TEST("model validation: active move requires existing distinct parts") {
    Model model = makeRunnableModel();
    MoveDef move;
    move.id = 601;
    move.name = "Bad Move";
    move.active = true;
    move.inputs.type = MoveType::Transform;
    move.moveParts = {model.parts[0].id, 999};
    model.moves = {move};

    std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues), "missing move part blocks run");
    dvatest::check(hasIssue(issues, "move.part_missing"), "missing move part issue");
    dvatest::check(issueCategory(issues, "move.part_missing") ==
                       ModelIssueCategory::Move,
                   "missing move part is a move issue");

    model.moves[0].moveParts = {model.parts[0].id, model.parts[0].id};
    issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues), "same object and target blocks run");
    dvatest::check(hasIssue(issues, "move.same_object_target"),
                   "same part move issue");
}

TEST("model validation: active move parts require distinct refs") {
    Model model = makeRunnableModel();
    const PartId targetPartId = addPart(model, "Target");
    dvatest::check(targetPartId != kInvalidId, "target part created");
    MoveDef move;
    move.id = 602;
    move.name = "Move With Duplicate Part";
    move.active = true;
    move.inputs.type = MoveType::Transform;
    move.inputs.pairs = {MovePair{}};
    move.moveParts = {model.parts[0].id, targetPartId, model.parts[0].id};
    model.moves = {move};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "duplicate move part refs block run");
    dvatest::check(hasIssue(issues, "move.parts_not_distinct"),
                   "duplicate move part refs issue");
    dvatest::check(issueCategory(issues, "move.parts_not_distinct") ==
                       ModelIssueCategory::Move,
                   "duplicate move part refs is a move issue");
}

TEST("model validation: active move requires enough solver pairs") {
    Model model = makeRunnableModel();
    const PartId targetPartId = addPart(model, "Target");
    MoveDef move;
    move.id = 603;
    move.name = "Underconstrained Three Point";
    move.active = true;
    move.inputs.type = MoveType::ThreePoint;
    move.inputs.pairs = {MovePair{}, MovePair{}};
    move.moveParts = {model.parts[0].id, targetPartId};
    model.moves = {move};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "underconstrained move blocks run");
    dvatest::check(hasIssue(issues, "move.not_enough_pairs"),
                   "not enough move pairs issue");
    dvatest::check(issueCategory(issues, "move.not_enough_pairs") ==
                       ModelIssueCategory::Move,
                   "not enough move pairs is a move issue");
}

TEST("model validation: unsupported active move types block simulation") {
    Model model = makeRunnableModel();
    const PartId targetPartId = addPart(model, "Target");
    MoveDef move;
    move.id = 601;
    move.name = "External Move";
    move.active = true;
    move.inputs.type = MoveType::UserDll;
    move.moveParts = {model.parts[0].id, targetPartId};
    model.moves = {move};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues), "unsupported move blocks run");
    dvatest::check(hasIssue(issues, "move.unsupported_type"),
                   "unsupported move issue");
    dvatest::check(issueCategory(issues, "move.unsupported_type") ==
                       ModelIssueCategory::Move,
                   "unsupported move is a move issue");
    const std::string message = issueMessage(issues, "move.unsupported_type");
    dvatest::check(message.find("UserDll") != std::string::npos,
                   "unsupported UserDll move validation names the move type");
    dvatest::check(message.find("plugin host") != std::string::npos,
                   "unsupported UserDll move validation names missing plugin host");

    move.inputs.type = MoveType::AutoBend;
    model.moves = {move};

    const std::vector<ModelIssue> autoBendIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(autoBendIssues),
                   "unsupported AutoBend move blocks run");
    const std::string autoBendMessage =
        issueMessage(autoBendIssues, "move.unsupported_type");
    dvatest::check(autoBendMessage.find("AutoBend") != std::string::npos,
                   "unsupported AutoBend move validation names the move type");
    dvatest::check(autoBendMessage.find("missing bend/FEA") !=
                       std::string::npos,
                   "unsupported AutoBend move validation names missing bend/FEA infrastructure");
}

TEST("model validation: bound user-dll move can pass plugin validation") {
    opendva::plugin::PluginHost host;
    host.registerRoutine("externalMove", &boundMoveRoutine, dcsCalTypeMove);
    opendva::plugin::PluginHost::ActiveScope scope(&host);

    Model model = makeRunnableModel();
    const PartId targetPartId = addPart(model, "Target");
    MoveDef move;
    move.id = 605;
    move.name = "External Move";
    move.active = true;
    move.inputs.type = MoveType::UserDll;
    move.inputs.userDllRoutine = "externalMove";
    move.moveParts = {model.parts[0].id, targetPartId};
    model.moves = {move};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(!hasIssue(issues, "move.unsupported_type"),
                   "bound UserDll move does not report missing plugin host");
    dvatest::check(!hasBlockingIssues(issues),
                   "bound UserDll move does not block run");
}

TEST("model validation: empty iteration move is allowed but nonempty needs infrastructure") {
    Model model = makeRunnableModel();
    const PartId targetPartId = addPart(model, "Target");
    MoveDef move;
    move.id = 602;
    move.name = "Iteration Move";
    move.active = true;
    move.inputs.type = MoveType::Iteration;
    move.moveParts = {model.parts[0].id, targetPartId};
    model.moves = {move};

    std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(!hasIssue(issues, "move.unsupported_type"),
                   "empty Iteration reference move does not need nested infrastructure");
    dvatest::check(!hasBlockingIssues(issues),
                   "empty Iteration reference move passes pre-run validation");

    MovePair pair;
    pair.direction.ijk = {1.0, 0.0, 0.0};
    move.inputs.pairs = {pair};
    model.moves = {move};

    issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "nonempty Iteration move blocks without nested infrastructure");
    dvatest::check(hasIssue(issues, "move.unsupported_type"),
                   "nonempty Iteration move reports unsupported infrastructure");
    const std::string message = issueMessage(issues, "move.unsupported_type");
    dvatest::check(message.find("Iteration") != std::string::npos,
                   "nonempty Iteration validation names the move type");
    dvatest::check(message.find("missing") != std::string::npos,
                   "nonempty Iteration validation names missing infrastructure");
    dvatest::check(message.find("nested move-sequence") != std::string::npos,
                   "nonempty Iteration validation names missing nested sequence infrastructure");
}

TEST("model validation: active move numeric controls must be valid") {
    Model model = makeRunnableModel();
    const PartId targetPartId = addPart(model, "Target");
    MoveDef move;
    move.id = 601;
    move.name = "Invalid Controls Move";
    move.active = true;
    move.inputs.type = MoveType::BestFit;
    move.inputs.searchAccuracy = 0.0;
    move.inputs.maxIterations = 0;
    move.moveParts = {model.parts[0].id, targetPartId};
    model.moves = {move};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues), "invalid move controls block run");
    dvatest::check(hasIssue(issues, "move.invalid_search_accuracy"),
                   "invalid move search accuracy issue");
    dvatest::check(hasIssue(issues, "move.invalid_max_iterations"),
                   "invalid move max iterations issue");
    dvatest::check(issueCategory(issues, "move.invalid_search_accuracy") ==
                       ModelIssueCategory::Move,
                   "invalid search accuracy is a move issue");
}

TEST("model validation: active move float controls must be valid") {
    Model model = makeRunnableModel();
    const PartId targetPartId = addPart(model, "Target");
    MoveDef move;
    move.id = 604;
    move.name = "Invalid Float Move";
    move.active = true;
    move.inputs.type = MoveType::Transform;
    move.inputs.pairs = {MovePair{}};
    move.inputs.hole_pin_float.active = true;
    move.inputs.hole_pin_float.sigmaNumber = 0;
    move.inputs.hole_pin_float.rangeScale =
        std::numeric_limits<double>::infinity();
    move.inputs.hole_pin_float.angleOffsetDeg =
        std::numeric_limits<double>::quiet_NaN();
    move.moveParts = {model.parts[0].id, targetPartId};
    model.moves = {move};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "invalid move float controls block run");
    dvatest::check(hasIssue(issues, "move.invalid_float_sigma_number"),
                   "invalid float sigma issue");
    dvatest::check(hasIssue(issues, "move.float_range_scale_non_finite"),
                   "non-finite float range scale issue");
    dvatest::check(hasIssue(issues, "move.float_angle_offset_non_finite"),
                   "non-finite float angle offset issue");
    dvatest::check(issueCategory(issues, "move.invalid_float_sigma_number") ==
                       ModelIssueCategory::Move,
                   "invalid float sigma is a move issue");
}

TEST("model validation: user-dll measure blocks simulation without plugin binding") {
    Model model = makeRunnableModel();
    model.measures[0].def.type = MeasureType::UserDll;

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "unbound user-dll measure blocks run");
    dvatest::check(hasIssue(issues, "measure.userdll_unbound"),
                   "unbound user-dll measure issue");
    dvatest::check(issueCategory(issues, "measure.userdll_unbound") ==
                       ModelIssueCategory::Measure,
                   "unbound user-dll measure is a measure issue");
    const std::string message = issueMessage(issues, "measure.userdll_unbound");
    dvatest::check(message.find("UserDll") != std::string::npos,
                   "unbound UserDll measure validation names the measure type");
    dvatest::check(message.find("plugin host") != std::string::npos,
                   "unbound UserDll measure validation names missing plugin host");
}

TEST("model validation: bound user-dll measure can pass plugin validation") {
    opendva::plugin::PluginHost host;
    host.registerRoutine("calcGap", &boundMeasureRoutine, dcsCalTypeMeas);
    opendva::plugin::PluginHost::ActiveScope scope(&host);

    Model model = makeRunnableModel();
    model.measures[0].def.type = MeasureType::UserDll;
    model.measures[0].def.equation = "calcGap";

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(!hasIssue(issues, "measure.userdll_unbound"),
                   "bound UserDll measure does not report missing plugin host");
    dvatest::check(!hasBlockingIssues(issues),
                   "bound UserDll measure does not block run");
}

TEST("model validation: equation measure lines cannot exceed 400 characters") {
    Model model = makeRunnableModel();
    model.measures[0].def.type = MeasureType::Equation;
    model.measures[0].def.equation = std::string(401, '1');

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "overlong equation line blocks run");
    dvatest::check(hasIssue(issues, "measure.equation_line_too_long"),
                   "overlong equation line issue");
    dvatest::check(issueCategory(issues, "measure.equation_line_too_long") ==
                       ModelIssueCategory::Measure,
                   "overlong equation line is a measure issue");
}

TEST("model validation: equation SETCFG directives must be supported") {
    Model model = makeRunnableModel();
    model.measures[0].def.type = MeasureType::Equation;
    model.measures[0].def.equation = "SETCFG=BOGUSDATA\n2+3";

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "invalid SETCFG directive blocks run");
    dvatest::check(hasIssue(issues, "measure.equation_setcfg_invalid"),
                   "invalid SETCFG directive issue");
    dvatest::check(issueCategory(issues, "measure.equation_setcfg_invalid") ==
                       ModelIssueCategory::Measure,
                   "invalid SETCFG directive is a measure issue");
}

TEST("model validation: equation SETCFG directives must be uppercase") {
    Model model = makeRunnableModel();
    model.measures[0].def.type = MeasureType::Equation;
    model.measures[0].def.equation = "setcfg=lengthdata\n2+3";

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "lowercase SETCFG directive blocks run");
    dvatest::check(hasIssue(issues, "measure.equation_setcfg_invalid"),
                   "lowercase SETCFG directive issue");
    dvatest::check(issueCategory(issues, "measure.equation_setcfg_invalid") ==
                       ModelIssueCategory::Measure,
                   "lowercase SETCFG directive is a measure issue");
}

TEST("model validation: equation REM comment cannot be the last line") {
    Model model = makeRunnableModel();
    model.measures[0].def.type = MeasureType::Equation;
    model.measures[0].def.equation = "2+3\n  REM trailing comment";

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "trailing REM comment blocks run");
    dvatest::check(hasIssue(issues, "measure.equation_trailing_rem"),
                   "trailing REM comment issue");
    dvatest::check(issueCategory(issues, "measure.equation_trailing_rem") ==
                       ModelIssueCategory::Measure,
                   "trailing REM comment is a measure issue");

    model.measures[0].def.equation =
        "2+3\nREM ignored string\nSETCFG=LENGTHDATA";
    const std::vector<ModelIssue> setCfgAfterRemIssues =
        validateModelForSimulation(model);

    dvatest::check(!hasIssue(setCfgAfterRemIssues,
                             "measure.equation_trailing_rem"),
                   "SETCFG after REM is not a trailing REM issue");
    dvatest::check(!hasBlockingIssues(setCfgAfterRemIssues),
                   "SETCFG after REM does not block run");
}

TEST("model validation: equation cannot use top-level equality") {
    Model model = makeRunnableModel();
    model.measures[0].def.type = MeasureType::Equation;
    model.measures[0].def.equation = "1==1";

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "top-level equality blocks run");
    dvatest::check(hasIssue(issues, "measure.equation_top_level_equality"),
                   "top-level equality issue");
    dvatest::check(issueCategory(issues, "measure.equation_top_level_equality") ==
                       ModelIssueCategory::Measure,
                   "top-level equality is a measure issue");

    model.measures[0].def.equation = "1=1";
    const std::vector<ModelIssue> singleEqualsIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(singleEqualsIssues),
                   "single top-level equals blocks run");
    dvatest::check(hasIssue(singleEqualsIssues,
                            "measure.equation_top_level_equality"),
                   "single top-level equals issue");

    model.measures[0].def.equation = "2+";
    const std::vector<ModelIssue> trailingOperatorIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(trailingOperatorIssues),
                   "trailing equation operator blocks run");
    dvatest::check(hasIssue(trailingOperatorIssues,
                            "measure.equation_malformed"),
                   "trailing equation operator issue");

    model.measures[0].def.equation = "-1+2";
    const std::vector<ModelIssue> bareNegativeIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(bareNegativeIssues),
                   "bare negative constant blocks run");
    dvatest::check(hasIssue(bareNegativeIssues,
                            "measure.equation_bare_negative"),
                   "bare negative constant issue");

    model.measures[0].def.equation = "1e309";
    const std::vector<ModelIssue> nonFiniteLiteralIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(nonFiniteLiteralIssues),
                   "non-finite numeric literal blocks run");
    dvatest::check(hasIssue(nonFiniteLiteralIssues,
                            "measure.equation_non_finite_numeric"),
                   "non-finite numeric literal issue");

    model.measures[0].def.equation = "1e308*1e308";
    const std::vector<ModelIssue> nonFiniteProductIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(nonFiniteProductIssues),
                   "non-finite numeric product blocks run");
    dvatest::check(hasIssue(nonFiniteProductIssues,
                            "measure.equation_non_finite_numeric"),
                   "non-finite numeric product issue");

    model.measures[0].def.equation = "1e308+1e308";
    const std::vector<ModelIssue> nonFiniteSumIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(nonFiniteSumIssues),
                   "non-finite numeric sum blocks run");
    dvatest::check(hasIssue(nonFiniteSumIssues,
                            "measure.equation_non_finite_numeric"),
                   "non-finite numeric sum issue");

    model.measures[0].def.equation = "(1e308)*(1e308)";
    const std::vector<ModelIssue> parenthesizedNonFiniteProductIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(parenthesizedNonFiniteProductIssues),
                   "parenthesized non-finite numeric product blocks run");
    dvatest::check(hasIssue(parenthesizedNonFiniteProductIssues,
                            "measure.equation_non_finite_numeric"),
                   "parenthesized non-finite numeric product issue");

    model.measures[0].def.equation = "pow(1e308,2)";
    const std::vector<ModelIssue> nonFiniteFunctionIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(nonFiniteFunctionIssues),
                   "non-finite function result blocks run");
    dvatest::check(hasIssue(nonFiniteFunctionIssues,
                            "measure.equation_non_finite_numeric"),
                   "non-finite function result issue");

    model.measures[0].def.equation = "pow((-1),0.5)";
    const std::vector<ModelIssue> parenthesizedNonFiniteFunctionIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(parenthesizedNonFiniteFunctionIssues),
                   "parenthesized non-finite function result blocks run");
    dvatest::check(hasIssue(parenthesizedNonFiniteFunctionIssues,
                            "measure.equation_non_finite_numeric"),
                   "parenthesized non-finite function result issue");

    model.measures[0].def.equation = "exp(1000)";
    const std::vector<ModelIssue> nonFiniteExpIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(nonFiniteExpIssues),
                   "non-finite exp result blocks run");
    dvatest::check(hasIssue(nonFiniteExpIssues,
                            "measure.equation_non_finite_numeric"),
                   "non-finite exp result issue");

    model.measures[0].def.equation = "inch2mm(1e308)";
    const std::vector<ModelIssue> nonFiniteConversionIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(nonFiniteConversionIssues),
                   "non-finite inch conversion result blocks run");
    dvatest::check(hasIssue(nonFiniteConversionIssues,
                            "measure.equation_non_finite_numeric"),
                   "non-finite inch conversion result issue");

    model.measures[0].def.equation = "rad2deg(1e306)";
    const std::vector<ModelIssue> finiteRadConversionIssues =
        validateModelForSimulation(model);

    dvatest::check(!hasIssue(finiteRadConversionIssues,
                             "measure.equation_non_finite_numeric"),
                   "finite large radian conversion result has no non-finite issue");

    model.measures[0].def.equation = "sin(7)";
    const std::vector<ModelIssue> trigRangeIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(trigRangeIssues),
                   "out-of-range trig input blocks run");
    dvatest::check(hasIssue(trigRangeIssues, "measure.equation_trig_range"),
                   "out-of-range trig input issue");

    model.measures[0].def.equation = "tan(1.5707963267948966)";
    const std::vector<ModelIssue> tanBoundaryIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(tanBoundaryIssues),
                   "tan boundary input blocks run");
    dvatest::check(hasIssue(tanBoundaryIssues,
                            "measure.equation_trig_range"),
                   "tan boundary input issue");

    model.measures[0].def.equation = "asin(2)";
    const std::vector<ModelIssue> inverseTrigRangeIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(inverseTrigRangeIssues),
                   "out-of-range inverse trig input blocks run");
    dvatest::check(hasIssue(inverseTrigRangeIssues,
                            "measure.equation_trig_range"),
                   "out-of-range inverse trig input issue");

    model.measures[0].def.equation = "log(0)";
    const std::vector<ModelIssue> functionDomainIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(functionDomainIssues),
                   "invalid function-domain input blocks run");
    dvatest::check(hasIssue(functionDomainIssues,
                            "measure.equation_function_domain"),
                   "invalid function-domain input issue");

    model.measures[0].def.equation = "sqrt((-1))";
    const std::vector<ModelIssue> sqrtDomainIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(sqrtDomainIssues),
                   "invalid parenthesized sqrt input blocks run");
    dvatest::check(hasIssue(sqrtDomainIssues,
                            "measure.equation_function_domain"),
                   "invalid parenthesized sqrt input issue");

    model.measures[0].def.equation = "mod(10,0)";
    const std::vector<ModelIssue> modDomainIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(modDomainIssues),
                   "zero mod divisor blocks run");
    dvatest::check(hasIssue(modDomainIssues,
                            "measure.equation_function_domain"),
                   "zero mod divisor issue");

    model.measures[0].def.equation = "sqrt(1,2)";
    const std::vector<ModelIssue> functionArityIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(functionArityIssues),
                   "invalid function arity blocks run");
    dvatest::check(hasIssue(functionArityIssues,
                            "measure.equation_function_arity"),
                   "invalid function arity issue");

    model.measures[0].def.equation = "sqrt()";
    const std::vector<ModelIssue> emptyFunctionArgIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(emptyFunctionArgIssues),
                   "empty function arguments block run");
    dvatest::check(hasIssue(emptyFunctionArgIssues,
                            "measure.equation_function_arity"),
                   "empty function arguments issue");

    model.measures[0].def.equation = "pow(,2)";
    const std::vector<ModelIssue> emptyFunctionSlotIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(emptyFunctionSlotIssues),
                   "empty function argument slot blocks run");
    dvatest::check(hasIssue(emptyFunctionSlotIssues,
                            "measure.equation_function_arity"),
                   "empty function argument slot issue");

    model.measures[0].def.equation = "if_then_else(1,2)";
    const std::vector<ModelIssue> conditionalArityIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(conditionalArityIssues),
                   "invalid conditional arity blocks run");
    dvatest::check(hasIssue(conditionalArityIssues,
                            "measure.equation_function_arity"),
                   "invalid conditional arity issue");

    model.measures[0].def.equation = "MIN()";
    const std::vector<ModelIssue> emptyMinArgIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(emptyMinArgIssues),
                   "empty MIN arguments block run");
    dvatest::check(hasIssue(emptyMinArgIssues,
                            "measure.equation_function_arity"),
                   "empty MIN arguments issue");

    model.measures[0].def.equation = "MAX()";
    const std::vector<ModelIssue> emptyMaxArgIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(emptyMaxArgIssues),
                   "empty MAX arguments block run");
    dvatest::check(hasIssue(emptyMaxArgIssues,
                            "measure.equation_function_arity"),
                   "empty MAX arguments issue");

    model.measures[0].def.equation = "SIN(1)";
    const std::vector<ModelIssue> uppercaseMathIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(uppercaseMathIssues),
                   "uppercase math operator blocks run");
    dvatest::check(hasIssue(uppercaseMathIssues,
                            "measure.equation_math_operator_case"),
                   "uppercase math operator issue");

    model.measures[0].def.equation = "foo(1)";
    const std::vector<ModelIssue> unknownFunctionIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(unknownFunctionIssues),
                   "unknown math operator blocks run");
    dvatest::check(hasIssue(unknownFunctionIssues,
                            "measure.equation_function_unknown"),
                   "unknown math operator issue");

    model.measures[0].def.equation = "MIN(MIN(1,2),3)";
    const std::vector<ModelIssue> nestedMinIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(nestedMinIssues),
                   "nested MIN operator blocks run");
    dvatest::check(hasIssue(nestedMinIssues,
                            "measure.equation_multi_entry_nested"),
                   "nested MIN operator issue");

    model.measures[0].def.equation =
        "if_then_else(if_then_else(1,1,0),2,3)";
    const std::vector<ModelIssue> nestedConditionalIssues =
        validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(nestedConditionalIssues),
                   "nested conditional operator blocks run");
    dvatest::check(hasIssue(nestedConditionalIssues,
                            "measure.equation_conditional_nested"),
                   "nested conditional operator issue");
}

TEST("model validation: equation measure references require input measures") {
    Model model = makeRunnableModel();
    model.measures[0].def.type = MeasureType::Equation;
    model.measures[0].def.equation = "[MS:1]+2";
    model.measures[0].def.inputFeatures.clear();

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "equation measure reference without input measure blocks run");
    dvatest::check(hasIssue(issues, "measure.no_input_measures"),
                   "missing equation measure refs issue");
    dvatest::check(issueCategory(issues, "measure.no_input_measures") ==
                       ModelIssueCategory::Measure,
                   "missing equation measure refs is a measure issue");
}

TEST("model validation: equation measure reference indexes must exist") {
    Model model = makeRunnableModel();
    MeasureRecord equation = model.measures[0];
    equation.id = nextMeasureId(model);
    equation.name = "derived_gap";
    equation.def.type = MeasureType::Equation;
    equation.def.equation = "[MS:2]+1";
    equation.def.inputFeatures = {model.measures[0].id};
    model.measures.push_back(equation);

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "equation measure reference beyond input measures blocks run");
    dvatest::check(hasIssue(issues, "measure.input_measure_missing"),
                   "missing equation measure reference index issue");
    dvatest::check(issueCategory(issues, "measure.input_measure_missing") ==
                       ModelIssueCategory::Measure,
                   "missing equation measure reference index is a measure issue");
}

TEST("model validation: equation value indexes must exist") {
    Model model = makeRunnableModel();
    model.measures[0].def.type = MeasureType::Equation;
    model.measures[0].def.equation = "[VAL:2]+1";
    model.measures[0].def.values = {3.0};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "equation value reference beyond value list blocks run");
    dvatest::check(hasIssue(issues, "measure.value_missing"),
                   "missing equation value index issue");
    dvatest::check(issueCategory(issues, "measure.value_missing") ==
                       ModelIssueCategory::Measure,
                   "missing equation value index is a measure issue");
}

TEST("model validation: equation string indexes must exist") {
    Model model = makeRunnableModel();
    model.measures[0].def.type = MeasureType::Equation;
    model.measures[0].def.equation = "2+3\n[STR:2]+1";

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "equation string reference beyond string list blocks run");
    dvatest::check(hasIssue(issues, "measure.string_value_missing"),
                   "missing equation string index issue");
    dvatest::check(issueCategory(issues, "measure.string_value_missing") ==
                       ModelIssueCategory::Measure,
                   "missing equation string index is a measure issue");
}

TEST("model validation: equation point variable indexes must exist") {
    Model model = makeRunnableModel();
    model.measures[0].def.type = MeasureType::Equation;
    model.measures[0].def.inputPoints = {model.parts[0].points[0].id};
    model.measures[0].def.equation = "[P1X:2]+1";

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "equation point reference beyond input points blocks run");
    dvatest::check(hasIssue(issues, "measure.input_point_missing"),
                   "missing equation point input index issue");
    dvatest::check(issueCategory(issues, "measure.input_point_missing") ==
                       ModelIssueCategory::Measure,
                   "missing equation point input index is a measure issue");
}

TEST("model validation: equation point variables must be known") {
    Model model = makeRunnableModel();
    model.measures[0].def.type = MeasureType::Equation;
    model.measures[0].def.equation = "[P3X:1]+1";

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "unknown equation point variable blocks run");
    dvatest::check(hasIssue(issues, "measure.point_variable_invalid"),
                   "invalid equation point variable issue");
    dvatest::check(issueCategory(issues, "measure.point_variable_invalid") ==
                       ModelIssueCategory::Measure,
                   "invalid equation point variable is a measure issue");
}

TEST("model validation: equation variables must be supported") {
    Model model = makeRunnableModel();
    model.measures[0].def.type = MeasureType::Equation;
    model.measures[0].def.equation = "[BAD:1]+1";

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "unsupported equation variable blocks run");
    dvatest::check(hasIssue(issues, "measure.variable_unknown"),
                   "unknown equation variable issue");
    dvatest::check(issueCategory(issues, "measure.variable_unknown") ==
                       ModelIssueCategory::Measure,
                   "unknown equation variable is a measure issue");
}

TEST("model validation: equation variables must be closed") {
    Model model = makeRunnableModel();
    model.measures[0].def.type = MeasureType::Equation;
    model.measures[0].def.equation = "[VAL:1+1";
    model.measures[0].def.values = {2.0};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "malformed equation variable blocks run");
    dvatest::check(hasIssue(issues, "measure.variable_malformed"),
                   "malformed equation variable issue");
    dvatest::check(issueCategory(issues, "measure.variable_malformed") ==
                       ModelIssueCategory::Measure,
                   "malformed equation variable is a measure issue");
}

TEST("model validation: equation variable close brackets must be paired") {
    Model model = makeRunnableModel();
    model.measures[0].def.type = MeasureType::Equation;
    model.measures[0].def.equation = "1]+[VAL:1]";
    model.measures[0].def.values = {2.0};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "unmatched equation variable close bracket blocks run");
    dvatest::check(hasIssue(issues, "measure.variable_malformed"),
                   "unmatched equation variable close bracket issue");
    dvatest::check(issueCategory(issues, "measure.variable_malformed") ==
                       ModelIssueCategory::Measure,
                   "unmatched equation variable close bracket is a measure issue");
}

TEST("model validation: equation direction variables require index one") {
    Model model = makeRunnableModel();
    model.measures[0].def.type = MeasureType::Equation;
    model.measures[0].def.equation = "[DRI:2]+1";

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "equation direction variable with wrong index blocks run");
    dvatest::check(hasIssue(issues, "measure.direction_variable_invalid"),
                   "invalid equation direction variable issue");
    dvatest::check(issueCategory(issues, "measure.direction_variable_invalid") ==
                       ModelIssueCategory::Measure,
                   "invalid equation direction variable is a measure issue");
}

TEST("model validation: enabled measure spec limits must be ordered") {
    Model model = makeRunnableModel();
    model.measures[0].def.spec.lslActive = true;
    model.measures[0].def.spec.uslActive = true;
    model.measures[0].def.spec.lsl = 12.0;
    model.measures[0].def.spec.usl = 10.0;

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues), "misordered spec limits block run");
    dvatest::check(hasIssue(issues, "measure.spec_limits_misordered"),
                   "misordered spec limit issue");
    dvatest::check(issueCategory(issues, "measure.spec_limits_misordered") ==
                       ModelIssueCategory::Measure,
                   "misordered spec limits are a measure issue");
}

TEST("model validation: active measure scale must be nonzero") {
    Model model = makeRunnableModel();
    model.measures[0].def.scale = 0.0;

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues), "zero measure scale blocks run");
    dvatest::check(hasIssue(issues, "measure.invalid_scale"),
                   "zero measure scale issue");
    dvatest::check(issueCategory(issues, "measure.invalid_scale") ==
                       ModelIssueCategory::Measure,
                   "zero measure scale is a measure issue");
}

TEST("model validation: combination measure references existing measures") {
    Model model = makeRunnableModel();
    MeasureRecord base = model.measures[0];
    base.id = nextMeasureId(model);
    base.name = "second_gap";
    model.measures.push_back(base);

    MeasureRecord combo;
    combo.id = nextMeasureId(model);
    combo.name = "stack";
    combo.def.type = MeasureType::Combination;
    combo.def.inputFeatures = {model.measures[0].id, model.measures[1].id};
    model.measures.push_back(combo);

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(!hasIssue(issues, "measure.input_feature_missing"),
                   "combination measure ids are not treated as feature ids");
    dvatest::check(!hasBlockingIssues(issues),
                   "valid combination measure references do not block run");
}

TEST("model validation: combination in-spec requires measure references") {
    Model model = makeRunnableModel();

    MeasureRecord combo;
    combo.id = nextMeasureId(model);
    combo.name = "in_spec_rate";
    combo.def.type = MeasureType::Combination;
    combo.def.equation = "in_spec";
    combo.def.inputFeatures.clear();
    model.measures.push_back(combo);

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "combination in-spec without measure refs blocks run");
    dvatest::check(hasIssue(issues, "measure.no_input_measures"),
                   "missing combination measure refs issue");
    dvatest::check(issueCategory(issues, "measure.no_input_measures") ==
                       ModelIssueCategory::Measure,
                   "missing combination measure refs is a measure issue");
}

TEST("model validation: combination measure cannot reference later measures") {
    Model model = makeRunnableModel();

    MeasureRecord combo;
    combo.id = nextMeasureId(model);
    combo.name = "stack";
    combo.def.type = MeasureType::Combination;

    MeasureRecord later = model.measures[0];
    later.id = combo.id + 1;
    later.name = "later_gap";

    combo.def.inputFeatures = {later.id};
    model.measures.push_back(combo);
    model.measures.push_back(later);

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "forward measure reference blocks run");
    dvatest::check(hasIssue(issues, "measure.input_measure_not_previous"),
                   "forward measure reference issue");
    dvatest::check(issueCategory(issues, "measure.input_measure_not_previous") ==
                       ModelIssueCategory::Measure,
                   "forward measure reference is a measure issue");
}

TEST("model validation: combination measure cannot reference inactive measures") {
    Model model = makeRunnableModel();
    MeasureRecord inactive = model.measures[0];
    inactive.id = nextMeasureId(model);
    inactive.name = "inactive_gap";
    inactive.def.active = false;
    model.measures.push_back(inactive);

    MeasureRecord combo;
    combo.id = nextMeasureId(model);
    combo.name = "stack";
    combo.def.type = MeasureType::Combination;
    combo.def.inputFeatures = {inactive.id};
    model.measures.push_back(combo);

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "inactive measure reference blocks run");
    dvatest::check(hasIssue(issues, "measure.input_measure_inactive"),
                   "inactive measure reference issue");
    dvatest::check(issueCategory(issues, "measure.input_measure_inactive") ==
                       ModelIssueCategory::Measure,
                   "inactive measure reference is a measure issue");
}

TEST("model validation: variants require existing references") {
    Model model = makeRunnableModel();
    ModelVariant variant;
    variant.name = "Loaded Bad Variant";
    variant.moves = {999};
    variant.tolerances = {998};
    variant.measures = {997};
    model.variants = {variant};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues), "dangling variant refs block run");
    dvatest::check(hasIssue(issues, "variant.move_missing"),
                   "missing variant move issue");
    dvatest::check(hasIssue(issues, "variant.tolerance_missing"),
                   "missing variant tolerance issue");
    dvatest::check(hasIssue(issues, "variant.measure_missing"),
                   "missing variant measure issue");
    dvatest::check(issueCategory(issues, "variant.move_missing") ==
                       ModelIssueCategory::Model,
                   "missing variant move is a model issue");
}

TEST("model validation: variants require distinct references") {
    Model model = makeRunnableModel();
    const PartId targetPartId = addPart(model, "Target");
    const MoveId moveId =
        addTransformMove(model, {1.0, 0.0, 0.0}, model.parts[0].id, targetPartId);
    dvatest::check(moveId != kInvalidId, "move created");
    ModelVariant variant;
    variant.name = "Repeated MTM";
    variant.moves = {moveId, moveId};
    variant.tolerances = {model.parts[0].tolerances[0].id,
                          model.parts[0].tolerances[0].id};
    variant.measures = {model.measures[0].id, model.measures[0].id};
    model.variants = {variant};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "duplicate variant refs block run");
    dvatest::check(hasIssue(issues, "variant.moves_not_distinct"),
                   "duplicate variant move issue");
    dvatest::check(hasIssue(issues, "variant.tolerances_not_distinct"),
                   "duplicate variant tolerance issue");
    dvatest::check(hasIssue(issues, "variant.measures_not_distinct"),
                   "duplicate variant measure issue");
    dvatest::check(issueCategory(issues, "variant.moves_not_distinct") ==
                       ModelIssueCategory::Model,
                   "duplicate variant refs are model issues");
}

TEST("model validation: variants require unique names") {
    Model model = makeRunnableModel();
    ModelVariant first = captureActiveVariant(model, "Scenario");
    ModelVariant second = first;
    second.name = first.name;
    second.moves.clear();
    model.variants = {first, second};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "duplicate variant names block run");
    dvatest::check(hasIssue(issues, "variant.duplicate_name"),
                   "duplicate variant name issue");
    dvatest::check(issueCategory(issues, "variant.duplicate_name") ==
                       ModelIssueCategory::Model,
                   "duplicate variant name is a model issue");
}

TEST("model validation: variants require non-empty names") {
    Model model = makeRunnableModel();
    ModelVariant variant = captureActiveVariant(model, "Scenario");
    variant.name.clear();
    model.variants = {variant};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "empty variant name blocks run");
    dvatest::check(hasIssue(issues, "variant.name_empty"),
                   "empty variant name issue");
    dvatest::check(issueCategory(issues, "variant.name_empty") ==
                       ModelIssueCategory::Model,
                   "empty variant name is a model issue");
}

TEST("model validation: variants allow only one active scenario") {
    Model model = makeRunnableModel();
    ModelVariant first = captureActiveVariant(model, "Scenario A");
    ModelVariant second = captureActiveVariant(model, "Scenario B");
    first.active = true;
    second.active = true;
    model.variants = {first, second};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "multiple active variants block run");
    dvatest::check(hasIssue(issues, "variant.multiple_active"),
                   "multiple active variants issue");
    dvatest::check(issueCategory(issues, "variant.multiple_active") ==
                       ModelIssueCategory::Model,
                   "multiple active variants is a model issue");
}

TEST("model validation: active variant applied state must be runnable") {
    Model model = makeRunnableModel();
    const ToleranceId toleranceId = model.parts[0].tolerances[0].id;
    model.parts[0].tolerances[0].active = false;
    model.parts[0].tolerances[0].ir.rangeScale = 0.0;

    ModelVariant variant = captureActiveVariant(model, "Loaded Scenario");
    variant.active = true;
    variant.tolerances = {toleranceId};
    model.variants = {variant};

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues),
                   "invalid active variant applied state blocks run");
    dvatest::check(hasIssue(issues, "tolerance.invalid_range_scale"),
                   "active variant tolerance issue returned");
    dvatest::check(issueCategory(issues, "tolerance.invalid_range_scale") ==
                       ModelIssueCategory::Tolerance,
                   "active variant tolerance issue remains categorized");
}

TEST("model validation: duplicate ids block simulation") {
    Model model = makeRunnableModel();
    Part duplicatePart = model.parts[0];
    model.parts.push_back(duplicatePart);
    model.parts[0].points.push_back(model.parts[0].points[0]);
    model.parts[0].features.push_back(model.parts[0].features[0]);
    model.parts[0].tolerances.push_back(model.parts[0].tolerances[0]);
    model.moves.push_back(model.moves.empty() ? MoveDef{} : model.moves[0]);
    model.measures.push_back(model.measures[0]);

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues), "duplicate ids block run");
    dvatest::check(hasIssue(issues, "model.duplicate_part_id"),
                   "duplicate part id issue");
    dvatest::check(hasIssue(issues, "model.duplicate_point_id"),
                   "duplicate point id issue");
    dvatest::check(hasIssue(issues, "model.duplicate_feature_id"),
                   "duplicate feature id issue");
    dvatest::check(hasIssue(issues, "model.duplicate_tolerance_id"),
                   "duplicate tolerance id issue");
    dvatest::check(hasIssue(issues, "model.duplicate_measure_id"),
                   "duplicate measure id issue");
}

TEST("model validation: invalid numeric values block simulation") {
    Model model = makeRunnableModel();
    model.parts[0].points[0].position.x =
        std::numeric_limits<double>::quiet_NaN();
    model.parts[0].points[0].diameter = -1.0;
    model.parts[0].tolerances[0].ir.rands[0].range = -0.5;
    model.parts[0].tolerances[0].ir.rands[0].sigmaNum = 0.0;
    model.parts[0].tolerances[0].ir.rangeScale =
        std::numeric_limits<double>::infinity();

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues), "invalid numeric values block run");
    dvatest::check(hasIssue(issues, "point.position_non_finite"),
                   "non-finite point position issue");
    dvatest::check(hasIssue(issues, "point.negative_diameter"),
                   "negative point diameter issue");
    dvatest::check(hasIssue(issues, "tolerance.negative_range"),
                   "negative tolerance range issue");
    dvatest::check(hasIssue(issues, "tolerance.invalid_sigma_number"),
                   "invalid tolerance sigma issue");
    dvatest::check(hasIssue(issues, "tolerance.range_scale_non_finite"),
                   "non-finite tolerance range scale issue");
}

TEST("model validation: non-finite MTM values block simulation") {
    Model model = makeRunnableModel();
    const PartId targetPartId = addPart(model, "Target");
    const MoveId moveId = addTransformMove(model, {1.0, 0.0, 0.0},
                                           model.parts[0].id, targetPartId);
    const GdtId gdtId = addGdt(model, GdtType::Position, 0.25, false,
                               model.parts[0].id);
    dvatest::check(moveId != kInvalidId, "move created");
    dvatest::check(gdtId != kInvalidId, "gdt created");

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    model.moves[0].inputs.pairs[0].objectPoint.x = nan;
    model.moves[0].inputs.pairs[0].targetPoint.y = inf;
    model.moves[0].inputs.pairs[0].direction.ijk.z = nan;
    model.parts[0].gdts[0].range = nan;
    model.measures[0].def.scale = inf;
    model.measures[0].def.values = {1.0, nan};
    model.measures[0].def.spec.lsl = nan;
    model.measures[0].def.spec.usl = inf;

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasBlockingIssues(issues), "non-finite MTM values block run");
    dvatest::check(hasIssue(issues, "move.object_point_non_finite"),
                   "non-finite move object point issue");
    dvatest::check(hasIssue(issues, "move.target_point_non_finite"),
                   "non-finite move target point issue");
    dvatest::check(hasIssue(issues, "move.direction_non_finite"),
                   "non-finite move direction issue");
    dvatest::check(hasIssue(issues, "gdt.range_non_finite"),
                   "non-finite gdt range issue");
    dvatest::check(hasIssue(issues, "measure.scale_non_finite"),
                   "non-finite measure scale issue");
    dvatest::check(hasIssue(issues, "measure.value_non_finite"),
                   "non-finite measure value issue");
    dvatest::check(hasIssue(issues, "measure.spec_non_finite"),
                   "non-finite measure spec issue");
}

TEST("model validation: unreferenced features are reported as warnings") {
    Model model = makeRunnableModel();
    const FeatureId extraFeature = addFeature(model, FeatureKind::Plane,
                                              model.parts[0].id);
    dvatest::check(extraFeature != kInvalidId, "extra feature created");

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasIssue(issues, "feature.unreferenced"),
                   "unreferenced feature warning");
    dvatest::check(!hasBlockingIssues(issues),
                   "unreferenced feature does not block simulation");
    dvatest::check(issueCategory(issues, "feature.unreferenced") ==
                       ModelIssueCategory::Feature,
                   "unreferenced feature is a feature issue");
}

TEST("model validation: non-userdefined smp path is reported as warning") {
    Model model = makeRunnableModel();
    model.parts[0].tolerances[0].ir.rands[0].distribution =
        DistributionType::Normal;
    model.parts[0].tolerances[0].ir.rands[0].userDefinedSamplePath =
        "samples/ignored.smp";

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasIssue(issues, "tolerance.userdefined_sample_path_ignored"),
                   "ignored userdefined sample path warning");
    dvatest::check(!hasBlockingIssues(issues),
                   "ignored sample path warning does not block simulation");
    dvatest::check(issueCategory(
                       issues, "tolerance.userdefined_sample_path_ignored") ==
                       ModelIssueCategory::Tolerance,
                   "ignored sample path is a tolerance issue");
}

TEST("model validation: userdefined without smp path is reported as warning") {
    Model model = makeRunnableModel();
    model.parts[0].tolerances[0].ir.rands[0].distribution =
        DistributionType::UserDefined;
    model.parts[0].tolerances[0].ir.rands[0].userDefinedSamplePath.clear();

    const std::vector<ModelIssue> issues = validateModelForSimulation(model);

    dvatest::check(hasIssue(issues, "tolerance.userdefined_sample_path_missing"),
                   "missing userdefined sample path warning");
    dvatest::check(!hasBlockingIssues(issues),
                   "missing userdefined sample path warning does not block simulation");
    dvatest::check(issueCategory(
                       issues, "tolerance.userdefined_sample_path_missing") ==
                       ModelIssueCategory::Tolerance,
                   "missing userdefined sample path is a tolerance issue");
}

TEST("model validation: issue summary groups severity and category") {
    const std::vector<ModelIssue> issues = {
        {ModelIssueSeverity::Error, ModelIssueCategory::Model, "model.err", "Model error"},
        {ModelIssueSeverity::Warning, ModelIssueCategory::Gdt, "gdt.warn", "GD&T warning"},
        {ModelIssueSeverity::Error, ModelIssueCategory::Measure, "measure.err",
         "Measure error"},
        {ModelIssueSeverity::Info, ModelIssueCategory::Feature, "feature.info",
         "Feature info"}};

    const ValidationSummary summary = summarizeIssues(issues);

    dvatest::check(summary.errorCount == 2, "two errors counted");
    dvatest::check(summary.warningCount == 1, "one warning counted");
    dvatest::check(summary.infoCount == 1, "one info counted");
    dvatest::check(summary.categoryCounts.at(ModelIssueCategory::Model) == 1,
                   "model category counted");
    dvatest::check(summary.categoryCounts.at(ModelIssueCategory::Measure) == 1,
                   "measure category counted");
    dvatest::check(summary.categoryCounts.at(ModelIssueCategory::Gdt) == 1,
                   "gdt category counted");
    dvatest::check(summary.categoryCounts.at(ModelIssueCategory::Feature) == 1,
                   "feature category counted");
    dvatest::check(std::string(issueSeverityName(ModelIssueSeverity::Warning)) == "Warning",
                   "warning name");
    dvatest::check(std::string(issueCategoryName(ModelIssueCategory::Gdt)) == "GD&T",
                   "gdt category name");
}
