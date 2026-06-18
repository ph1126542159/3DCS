// Qt3D model viewport for the OpenDVA desktop MVP.
#pragma once

#include <map>

#include <QWidget>

#include "opendva/domain/ModelVisualization.h"

class QString;

namespace Qt3DCore {
class QEntity;
}

namespace Qt3DExtras {
class Qt3DWindow;
}

namespace opendva::ui {

class ColorContourLegendWidget;

class ModelViewport final : public QWidget {
    Q_OBJECT

public:
    explicit ModelViewport(QWidget* parent = nullptr);

    void setModel(const Model& model);
    void setPointDeviations(const std::map<PointId, double>& pointDeviations,
                            double manualMin,
                            double manualMax);
    bool saveSnapshot(const QString& path);

private:
    void refreshContourLegend();
    void rebuildScene(const Model& model);
    Qt3DCore::QEntity* createPointEntity(Qt3DCore::QEntity* parent,
                                         const ViewportPointMarker& marker,
                                         const QColor& color);
    Qt3DCore::QEntity* createAxisEntity(Qt3DCore::QEntity* parent,
                                        const QVector3D& position,
                                        const QVector3D& rotation,
                                        float length,
                                        const QColor& color);
    Qt3DCore::QEntity* createLineSegmentEntity(Qt3DCore::QEntity* parent,
                                               const Vec3& start,
                                               const Vec3& end,
                                               const QColor& color);
    Qt3DCore::QEntity* createFeatureGlyphEntity(Qt3DCore::QEntity* parent,
                                                const FeatureGlyph& glyph,
                                                const QColor& color);

    Qt3DExtras::Qt3DWindow* view_{};
    Qt3DCore::QEntity* root_{};
    ColorContourLegendWidget* legend_{};
    Model model_{};
    std::map<PointId, double> pointDeviations_;
    bool contourAutoScale_{true};
    double contourManualMin_{0.0};
    double contourManualMax_{0.0};
    double contourMin_{1.0};
    double contourMax_{0.0};
};

}  // namespace opendva::ui
