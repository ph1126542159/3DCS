// Pure domain extraction for geometry shown by the desktop viewport.
#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "opendva/ISimulationEngine.h"
#include "opendva/domain/Model.h"

namespace opendva {

struct LineSegment {
    MeasureId measureId{kInvalidId};
    Vec3 start{};
    Vec3 end{};
};

struct FeatureGlyph {
    FeatureId featureId{kInvalidId};
    FeatureKind kind{FeatureKind::PointBased};
    Vec3 center{};
    double radius{1.0};
};

struct ViewportColor {
    std::uint8_t r{0};
    std::uint8_t g{0};
    std::uint8_t b{0};
};

struct ContourPoint {
    PointId pointId{kInvalidId};
    Vec3 position{};
    double deviation{0.0};
    ViewportColor color{};
};

struct ContourLegendEntry {
    double value{0.0};
    ViewportColor color{};
};

struct ContourLegendScale {
    bool visible{false};
    double minValue{0.0};
    double maxValue{0.0};
    std::vector<ContourLegendEntry> entries;
};

struct ContourRange {
    bool valid{false};
    double minValue{0.0};
    double maxValue{0.0};
};

struct ViewportPointMarker {
    PointId pointId{kInvalidId};
    Vec3 position{};
    bool active{false};
    bool hasContour{false};
    ViewportColor color{};
};

std::vector<LineSegment> measurementLineSegments(const Model& model);
std::vector<FeatureGlyph> featureGlyphs(const Model& model);
std::vector<ContourPoint> contourPointCloud(
    const Model& model,
    const std::map<PointId, double>& pointDeviations,
    double manualMin,
    double manualMax);
std::vector<ContourLegendEntry> contourLegend(double minDev, double maxDev, int sampleCount);
ContourLegendScale contourLegendScale(const std::map<PointId, double>& pointDeviations,
                                      int sampleCount);
ContourRange resolveContourRange(const std::map<PointId, double>& pointDeviations,
                                 bool autoScale,
                                 double manualMin,
                                 double manualMax);
std::vector<ViewportPointMarker> viewportPointMarkers(
    const Model& model,
    const std::map<PointId, double>& pointDeviations,
    double manualMin,
    double manualMax);
std::map<PointId, double> pointDeviationsFromContributors(
    const Model& model,
    const std::vector<ContributorRow>& contributors);

}  // namespace opendva
