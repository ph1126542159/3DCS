// Pure C++ visualization extraction used by the Qt3D desktop viewport.
#include <limits>
#include <map>

#include "dva_test.h"
#include "opendva/ISimulationEngine.h"
#include "opendva/domain/ModelVisualization.h"

using namespace opendva;

TEST("model visualization: point point measures produce line segments") {
    Model model;
    model.parts.push_back(Part{});
    model.parts[0].points.push_back(Point{101, PointKind::Coordinate, {1.0, 2.0, 3.0}});
    model.parts[0].points.push_back(Point{102, PointKind::Coordinate, {4.0, 6.0, 8.0}});

    MeasureRecord measure;
    measure.id = 401;
    measure.name = "Gap";
    measure.def.type = MeasureType::PointPoint;
    measure.def.inputPoints = {101, 102};
    model.measures.push_back(measure);

    const std::vector<LineSegment> segments = measurementLineSegments(model);

    dvatest::check(segments.size() == 1, "one point-point measure line");
    dvatest::check(segments[0].measureId == 401, "measure id carried");
    dvatest::checkNear(segments[0].start.x, 1.0, 1e-12, "start x");
    dvatest::checkNear(segments[0].start.y, 2.0, 1e-12, "start y");
    dvatest::checkNear(segments[0].start.z, 3.0, 1e-12, "start z");
    dvatest::checkNear(segments[0].end.x, 4.0, 1e-12, "end x");
    dvatest::checkNear(segments[0].end.y, 6.0, 1e-12, "end y");
    dvatest::checkNear(segments[0].end.z, 8.0, 1e-12, "end z");
}

TEST("model visualization: inactive and unresolved measures are skipped") {
    Model model;
    model.parts.push_back(Part{});
    model.parts[0].points.push_back(Point{101, PointKind::Coordinate, {0.0, 0.0, 0.0}});
    model.parts[0].points.push_back(Point{102, PointKind::Coordinate, {0.0, 0.0, 10.0}});

    MeasureRecord inactive;
    inactive.id = 401;
    inactive.def.type = MeasureType::PointPoint;
    inactive.def.inputPoints = {101, 102};
    inactive.def.active = false;

    MeasureRecord missingPoint;
    missingPoint.id = 402;
    missingPoint.def.type = MeasureType::PointPoint;
    missingPoint.def.inputPoints = {101, 999};

    MeasureRecord unsupported;
    unsupported.id = 403;
    unsupported.def.type = MeasureType::PointPlane;
    unsupported.def.inputPoints = {101, 102};

    model.measures = {inactive, missingPoint, unsupported};

    dvatest::check(measurementLineSegments(model).empty(),
                   "skip inactive unresolved and unsupported measures");
}

TEST("model visualization: features produce simplified glyphs") {
    Model model;
    Part part;
    part.points.push_back(Point{101, PointKind::Coordinate, {0.0, 0.0, 0.0}});
    part.points.push_back(Point{102, PointKind::Coordinate, {4.0, 0.0, 0.0}});
    part.points.push_back(Point{103, PointKind::Coordinate, {0.0, 2.0, 0.0}});

    Feature plane;
    plane.id = 201;
    plane.kind = FeatureKind::Plane;
    plane.definingPoints = {101, 102, 103};

    Feature unresolved;
    unresolved.id = 202;
    unresolved.kind = FeatureKind::Cylinder;
    unresolved.definingPoints = {999};

    part.features = {plane, unresolved};
    model.parts = {part};

    const std::vector<FeatureGlyph> glyphs = featureGlyphs(model);

    dvatest::check(glyphs.size() == 1, "one resolved feature glyph");
    dvatest::check(glyphs[0].featureId == 201, "feature id carried");
    dvatest::check(glyphs[0].kind == FeatureKind::Plane, "feature kind carried");
    dvatest::checkNear(glyphs[0].center.x, 4.0 / 3.0, 1e-12, "center x");
    dvatest::checkNear(glyphs[0].center.y, 2.0 / 3.0, 1e-12, "center y");
    dvatest::checkNear(glyphs[0].center.z, 0.0, 1e-12, "center z");
    dvatest::check(glyphs[0].radius >= 1.0, "feature radius has useful size");
}

TEST("model visualization: point deviation cloud maps values to contour colors") {
    Model model;
    Part part;
    part.points.push_back(Point{101, PointKind::Coordinate, {0.0, 0.0, 0.0}});
    part.points.push_back(Point{102, PointKind::Coordinate, {1.0, 0.0, 0.0}});
    part.points.push_back(Point{103, PointKind::Coordinate, {2.0, 0.0, 0.0}});
    Point inactive{104, PointKind::Coordinate, {3.0, 0.0, 0.0}};
    inactive.active = false;
    part.points.push_back(inactive);
    model.parts = {part};

    const std::map<PointId, double> deviations = {
        {101, 0.0},
        {102, 5.0},
        {103, 10.0},
        {104, 10.0},
    };

    const std::vector<ContourPoint> cloud =
        contourPointCloud(model, deviations, 0.0, 10.0);

    dvatest::check(cloud.size() == 3, "active points with contour data returned");
    dvatest::check(cloud[0].pointId == 101, "model point order preserved");
    dvatest::check(cloud[0].color.b > 200 && cloud[0].color.r == 0,
                   "low deviation is blue");
    dvatest::check(cloud[1].color.g > 200 && cloud[1].color.r == 0 &&
                       cloud[1].color.b == 0,
                   "mid deviation is green");
    dvatest::check(cloud[2].color.r > 200 && cloud[2].color.b == 0,
                   "high deviation is red");
}

TEST("model visualization: contour legend samples scale in display order") {
    const std::vector<ContourLegendEntry> legend = contourLegend(-2.0, 2.0, 5);

    dvatest::check(legend.size() == 5, "five legend samples");
    dvatest::checkNear(legend[0].value, -2.0, 1e-12, "legend minimum value");
    dvatest::checkNear(legend[1].value, -1.0, 1e-12, "legend quarter value");
    dvatest::checkNear(legend[2].value, 0.0, 1e-12, "legend midpoint value");
    dvatest::checkNear(legend[4].value, 2.0, 1e-12, "legend maximum value");
    dvatest::check(legend[0].color.b > 200 && legend[0].color.r == 0,
                   "legend starts blue");
    dvatest::check(legend[2].color.g > 200 && legend[2].color.r == 0 &&
                       legend[2].color.b == 0,
                   "legend midpoint is green");
    dvatest::check(legend[4].color.r > 200 && legend[4].color.b == 0,
                   "legend ends red");
}

TEST("model visualization: contour legend scale is derived from point deviations") {
    const std::map<PointId, double> deviations = {
        {101, -2.0},
        {102, 0.5},
        {103, 2.0},
    };

    const ContourLegendScale emptyScale = contourLegendScale({}, 5);
    dvatest::check(!emptyScale.visible, "empty contour legend scale is hidden");
    dvatest::check(emptyScale.entries.empty(), "empty contour legend has no entries");

    const ContourLegendScale scale = contourLegendScale(deviations, 5);

    dvatest::check(scale.visible, "non-empty contour legend scale is visible");
    dvatest::checkNear(scale.minValue, -2.0, 1e-12, "legend scale minimum");
    dvatest::checkNear(scale.maxValue, 2.0, 1e-12, "legend scale maximum");
    dvatest::check(scale.entries.size() == 5, "legend scale samples requested entry count");
    dvatest::checkNear(scale.entries.front().value, -2.0, 1e-12,
                       "legend scale first entry");
    dvatest::checkNear(scale.entries.back().value, 2.0, 1e-12,
                       "legend scale last entry");
}

TEST("model visualization: contour range resolves auto manual and invalid manual bounds") {
    const std::map<PointId, double> deviations = {
        {101, -2.0},
        {102, 0.5},
        {103, 2.0},
    };

    const ContourRange empty = resolveContourRange({}, true, -99.0, 99.0);
    dvatest::check(!empty.valid, "empty contour range is invalid");

    const ContourRange automatic = resolveContourRange(deviations, true, 99.0, 100.0);
    dvatest::check(automatic.valid, "auto contour range is valid");
    dvatest::checkNear(automatic.minValue, -2.0, 1e-12, "auto range minimum");
    dvatest::checkNear(automatic.maxValue, 2.0, 1e-12, "auto range maximum");

    const ContourRange manual = resolveContourRange(deviations, false, -1.5, 3.0);
    dvatest::check(manual.valid, "manual contour range is valid");
    dvatest::checkNear(manual.minValue, -1.5, 1e-12, "manual range minimum");
    dvatest::checkNear(manual.maxValue, 3.0, 1e-12, "manual range maximum");

    const ContourRange invalidManual = resolveContourRange(deviations, false, 3.0, -1.5);
    dvatest::check(invalidManual.valid, "invalid manual range falls back to auto");
    dvatest::checkNear(invalidManual.minValue, -2.0, 1e-12,
                       "invalid manual fallback minimum");
    dvatest::checkNear(invalidManual.maxValue, 2.0, 1e-12,
                       "invalid manual fallback maximum");
}

TEST("model visualization: contour scale ignores non finite deviations") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const std::map<PointId, double> deviations = {
        {101, nan},
        {102, -2.0},
        {103, inf},
        {104, 3.0},
    };

    const ContourRange automatic = resolveContourRange(deviations, true, 99.0, 100.0);
    dvatest::check(automatic.valid, "finite deviations produce valid range");
    dvatest::checkNear(automatic.minValue, -2.0, 1e-12,
                       "non finite values ignored for minimum");
    dvatest::checkNear(automatic.maxValue, 3.0, 1e-12,
                       "non finite values ignored for maximum");

    const ContourLegendScale scale = contourLegendScale(deviations, 3);
    dvatest::check(scale.visible, "finite deviations produce visible legend");
    dvatest::checkNear(scale.minValue, -2.0, 1e-12,
                       "legend ignores non finite minimum");
    dvatest::checkNear(scale.maxValue, 3.0, 1e-12,
                       "legend ignores non finite maximum");

    const ContourRange noFinite =
        resolveContourRange({{101, nan}, {102, inf}}, true, 0.0, 1.0);
    dvatest::check(!noFinite.valid, "all non finite deviations do not produce range");
    dvatest::check(!contourLegendScale({{101, nan}, {102, inf}}, 3).visible,
                   "all non finite deviations hide legend");
}

TEST("model visualization: non finite contour point colors are neutral") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    Model model;
    Part part;
    part.points.push_back(Point{101, PointKind::Coordinate, {0.0, 0.0, 0.0}});
    part.points.push_back(Point{102, PointKind::Coordinate, {1.0, 0.0, 0.0}});
    part.points.push_back(Point{103, PointKind::Coordinate, {2.0, 0.0, 0.0}});
    model.parts = {part};

    const std::map<PointId, double> deviations = {
        {101, nan},
        {102, 5.0},
        {103, inf},
    };

    const std::vector<ContourPoint> cloud =
        contourPointCloud(model, deviations, 0.0, 10.0);
    dvatest::check(cloud.size() == 3, "active contour points remain visible");
    dvatest::check(cloud[0].color.g > 200 && cloud[0].color.r == 0 &&
                       cloud[0].color.b == 0,
                   "nan point cloud color is neutral");
    dvatest::check(cloud[2].color.g > 200 && cloud[2].color.r == 0 &&
                       cloud[2].color.b == 0,
                   "infinite point cloud color is neutral");

    const std::vector<ViewportPointMarker> markers =
        viewportPointMarkers(model, deviations, 0.0, 10.0);
    dvatest::check(markers[0].hasContour, "nan marker still records contour data");
    dvatest::check(markers[0].color.g > 200 && markers[0].color.r == 0 &&
                       markers[0].color.b == 0,
                   "nan marker color is neutral");
    dvatest::check(markers[2].hasContour, "infinite marker still records contour data");
    dvatest::check(markers[2].color.g > 200 && markers[2].color.r == 0 &&
                       markers[2].color.b == 0,
                   "infinite marker color is neutral");
}

TEST("model visualization: viewport point markers apply contour overrides") {
    Model model;
    Part part;
    part.points.push_back(Point{101, PointKind::Coordinate, {0.0, 0.0, 0.0}});
    part.points.push_back(Point{102, PointKind::Coordinate, {1.0, 0.0, 0.0}});
    Point inactive{103, PointKind::Coordinate, {2.0, 0.0, 0.0}};
    inactive.active = false;
    part.points.push_back(inactive);
    model.parts = {part};

    const std::map<PointId, double> deviations = {
        {101, -1.0},
        {103, 1.0},
    };

    const std::vector<ViewportPointMarker> markers =
        viewportPointMarkers(model, deviations, -1.0, 1.0);

    dvatest::check(markers.size() == 3, "all model points remain visible");
    dvatest::check(markers[0].pointId == 101, "marker order follows model");
    dvatest::check(markers[0].hasContour, "active point with deviation uses contour");
    dvatest::check(markers[0].color.b > 200 && markers[0].color.r == 0,
                   "negative deviation is blue");
    dvatest::check(!markers[1].hasContour, "active point without deviation uses default");
    dvatest::check(markers[1].color.r > 200 && markers[1].color.g > 180 &&
                       markers[1].color.b < 120,
                   "default active point is yellow");
    dvatest::check(!markers[2].hasContour, "inactive point ignores deviation");
    dvatest::check(markers[2].color.r == markers[2].color.g &&
                       markers[2].color.g == markers[2].color.b,
                   "inactive point remains gray");
}

TEST("model visualization: contributor rows produce point deviations for color contour") {
    Model model;
    Part part;
    part.points.push_back(Point{101, PointKind::Coordinate, {0.0, 0.0, 0.0}});
    part.points.push_back(Point{102, PointKind::Coordinate, {1.0, 0.0, 0.0}});
    part.points.push_back(Point{103, PointKind::Coordinate, {2.0, 0.0, 0.0}});

    Feature feature;
    feature.id = 201;
    feature.kind = FeatureKind::Plane;
    feature.definingPoints = {101, 102};
    part.features.push_back(feature);

    ToleranceDef tolerance;
    tolerance.id = 301;
    tolerance.name = "Surface profile";
    tolerance.features = {201};
    part.tolerances = {tolerance};
    model.parts = {part};

    const std::vector<ContributorRow> contributors = {
        ContributorRow{401, 301, 1.0, 0.24, 80.0},
        ContributorRow{401, 999, 1.0, 99.0, 20.0},
    };

    const std::map<PointId, double> deviations =
        pointDeviationsFromContributors(model, contributors);

    dvatest::check(deviations.size() == 2, "controlled feature points receive deviations");
    dvatest::checkNear(deviations.at(101), 0.24, 1e-12, "first controlled point deviation");
    dvatest::checkNear(deviations.at(102), 0.24, 1e-12, "second controlled point deviation");
    dvatest::check(deviations.find(103) == deviations.end(),
                   "uncontrolled point has no deviation");
}
