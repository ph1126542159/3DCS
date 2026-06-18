#include "opendva/domain/ModelVisualization.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace opendva {
namespace {

const Point* findPoint(const Model& model, PointId id) {
    for (const Part& part : model.parts) {
        for (const Point& point : part.points) {
            if (point.id == id) {
                return &point;
            }
        }
    }
    return nullptr;
}

const ToleranceDef* findTolerance(const Model& model, ToleranceId id) {
    for (const Part& part : model.parts) {
        for (const ToleranceDef& tolerance : part.tolerances) {
            if (tolerance.id == id) {
                return &tolerance;
            }
        }
    }
    return nullptr;
}

const Feature* findFeature(const Model& model, FeatureId id) {
    for (const Part& part : model.parts) {
        for (const Feature& feature : part.features) {
            if (feature.id == id) {
                return &feature;
            }
        }
    }
    return nullptr;
}

double distance(const Vec3& a, const Vec3& b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double clampd(double x, double lo, double hi) {
    return std::max(lo, std::min(hi, x));
}

std::uint8_t colorByte(double v) {
    return static_cast<std::uint8_t>(std::lround(clampd(v, 0.0, 1.0) * 255.0));
}

ViewportColor contourRamp(double t) {
    t = clampd(t, 0.0, 1.0);
    const double s = t * 4.0;
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    if (s < 1.0) {
        b = 1.0;
        g = s;
    } else if (s < 2.0) {
        g = 1.0;
        b = 2.0 - s;
    } else if (s < 3.0) {
        g = 1.0;
        r = s - 2.0;
    } else {
        r = 1.0;
        g = 4.0 - s;
    }
    return ViewportColor{colorByte(r), colorByte(g), colorByte(b)};
}

ViewportColor contourColor(double deviation, double minDev, double maxDev) {
    const double span = maxDev - minDev;
    if (!std::isfinite(deviation) || !std::isfinite(minDev) || !std::isfinite(maxDev) ||
        !std::isfinite(span) || span <= 0.0) {
        return contourRamp(0.5);
    }
    return contourRamp((deviation - minDev) / span);
}

ViewportColor defaultPointColor(bool active) {
    return active ? ViewportColor{245, 210, 90} : ViewportColor{130, 130, 130};
}

bool validContourRange(double minDev, double maxDev) {
    return std::isfinite(minDev) && std::isfinite(maxDev) && minDev <= maxDev;
}

bool finiteContourBounds(const std::map<PointId, double>& pointDeviations,
                         double& minDev,
                         double& maxDev) {
    bool found = false;
    for (const auto& [pointId, deviation] : pointDeviations) {
        (void)pointId;
        if (!std::isfinite(deviation)) continue;

        if (!found) {
            minDev = deviation;
            maxDev = deviation;
            found = true;
        } else {
            minDev = std::min(minDev, deviation);
            maxDev = std::max(maxDev, deviation);
        }
    }
    return found;
}

}  // namespace

std::vector<LineSegment> measurementLineSegments(const Model& model) {
    std::vector<LineSegment> segments;
    for (const MeasureRecord& measure : model.measures) {
        if (!measure.def.active || measure.def.type != MeasureType::PointPoint ||
            measure.def.inputPoints.size() < 2) {
            continue;
        }

        const Point* start = findPoint(model, measure.def.inputPoints[0]);
        const Point* end = findPoint(model, measure.def.inputPoints[1]);
        if (start == nullptr || end == nullptr) {
            continue;
        }

        segments.push_back(LineSegment{measure.id, start->position, end->position});
    }
    return segments;
}

std::vector<FeatureGlyph> featureGlyphs(const Model& model) {
    std::vector<FeatureGlyph> glyphs;
    for (const Part& part : model.parts) {
        for (const Feature& feature : part.features) {
            if (feature.definingPoints.empty()) continue;

            Vec3 center{};
            std::vector<Vec3> positions;
            positions.reserve(feature.definingPoints.size());
            bool resolved = true;
            for (const PointId pointId : feature.definingPoints) {
                const Point* point = findPoint(model, pointId);
                if (point == nullptr) {
                    resolved = false;
                    break;
                }
                positions.push_back(point->position);
                center.x += point->position.x;
                center.y += point->position.y;
                center.z += point->position.z;
            }
            if (!resolved || positions.empty()) continue;

            const double scale = 1.0 / static_cast<double>(positions.size());
            center.x *= scale;
            center.y *= scale;
            center.z *= scale;

            double radius = 0.0;
            for (const Vec3& position : positions) {
                radius = std::max(radius, distance(center, position));
            }
            radius = std::max(radius, 1.0);

            glyphs.push_back(FeatureGlyph{feature.id, feature.kind, center, radius});
        }
    }
    return glyphs;
}

std::vector<ContourPoint> contourPointCloud(
    const Model& model,
    const std::map<PointId, double>& pointDeviations,
    double manualMin,
    double manualMax) {
    std::vector<ContourPoint> cloud;
    if (pointDeviations.empty()) return cloud;

    double minDev = manualMin;
    double maxDev = manualMax;
    if (!validContourRange(manualMin, manualMax) &&
        !finiteContourBounds(pointDeviations, minDev, maxDev)) {
        return cloud;
    }

    for (const Part& part : model.parts) {
        for (const Point& point : part.points) {
            if (!point.active) continue;

            const auto deviationIt = pointDeviations.find(point.id);
            if (deviationIt == pointDeviations.end()) continue;

            const double deviation = deviationIt->second;
            cloud.push_back(ContourPoint{point.id, point.position, deviation,
                                         contourColor(deviation, minDev, maxDev)});
        }
    }
    return cloud;
}

std::vector<ContourLegendEntry> contourLegend(double minDev, double maxDev, int sampleCount) {
    std::vector<ContourLegendEntry> legend;
    if (sampleCount <= 0) return legend;

    legend.reserve(static_cast<std::size_t>(sampleCount));
    if (sampleCount == 1) {
        legend.push_back(ContourLegendEntry{minDev, contourColor(minDev, minDev, maxDev)});
        return legend;
    }

    const double step = (maxDev - minDev) / static_cast<double>(sampleCount - 1);
    for (int i = 0; i < sampleCount; ++i) {
        const double value = minDev + step * static_cast<double>(i);
        legend.push_back(ContourLegendEntry{value, contourColor(value, minDev, maxDev)});
    }
    return legend;
}

ContourLegendScale contourLegendScale(const std::map<PointId, double>& pointDeviations,
                                      int sampleCount) {
    if (pointDeviations.empty() || sampleCount <= 0) {
        return ContourLegendScale{};
    }

    ContourLegendScale scale;
    if (!finiteContourBounds(pointDeviations, scale.minValue, scale.maxValue)) {
        return scale;
    }

    scale.visible = true;
    scale.entries = contourLegend(scale.minValue, scale.maxValue, sampleCount);
    return scale;
}

ContourRange resolveContourRange(const std::map<PointId, double>& pointDeviations,
                                 bool autoScale,
                                 double manualMin,
                                 double manualMax) {
    if (pointDeviations.empty()) {
        return ContourRange{};
    }

    if (!autoScale && validContourRange(manualMin, manualMax)) {
        return ContourRange{true, manualMin, manualMax};
    }

    double minDev = 0.0;
    double maxDev = 0.0;
    if (!finiteContourBounds(pointDeviations, minDev, maxDev)) {
        return ContourRange{};
    }
    return ContourRange{true, minDev, maxDev};
}

std::vector<ViewportPointMarker> viewportPointMarkers(
    const Model& model,
    const std::map<PointId, double>& pointDeviations,
    double manualMin,
    double manualMax) {
    std::vector<ViewportPointMarker> markers;

    double minDev = manualMin;
    double maxDev = manualMax;
    if (!pointDeviations.empty() && !validContourRange(manualMin, manualMax) &&
        !finiteContourBounds(pointDeviations, minDev, maxDev)) {
        minDev = 0.0;
        maxDev = 0.0;
    }

    for (const Part& part : model.parts) {
        for (const Point& point : part.points) {
            const auto deviationIt = pointDeviations.find(point.id);
            const bool useContour = point.active && deviationIt != pointDeviations.end();
            const ViewportColor color =
                useContour ? contourColor(deviationIt->second, minDev, maxDev)
                           : defaultPointColor(point.active);
            markers.push_back(
                ViewportPointMarker{point.id, point.position, point.active, useContour, color});
        }
    }
    return markers;
}

std::map<PointId, double> pointDeviationsFromContributors(
    const Model& model,
    const std::vector<ContributorRow>& contributors) {
    std::map<PointId, double> deviations;
    for (const ContributorRow& contributor : contributors) {
        const ToleranceDef* tolerance = findTolerance(model, contributor.contributor);
        if (tolerance == nullptr) continue;

        for (const FeatureId featureId : tolerance->features) {
            const Feature* feature = findFeature(model, featureId);
            if (feature == nullptr) continue;

            for (const PointId pointId : feature->definingPoints) {
                const auto existing = deviations.find(pointId);
                if (existing == deviations.end() ||
                    std::fabs(contributor.sixSigma) > std::fabs(existing->second)) {
                    deviations[pointId] = contributor.sixSigma;
                }
            }
        }
    }
    return deviations;
}

}  // namespace opendva
