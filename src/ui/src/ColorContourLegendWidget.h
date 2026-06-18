// Compact color contour legend shown beside the Qt3D viewport.
#pragma once

#include <QWidget>

#include "opendva/domain/ModelVisualization.h"

class QCheckBox;
class QDoubleSpinBox;
class QPushButton;

namespace opendva::ui {

class ColorContourRampWidget;

class ColorContourLegendWidget final : public QWidget {
    Q_OBJECT

public:
    explicit ColorContourLegendWidget(QWidget* parent = nullptr);

    void setLegend(const ContourLegendScale& scale);
    void setRangeControls(bool autoScale, double manualMin, double manualMax);

signals:
    void rangeSettingsChanged(bool autoScale, double manualMin, double manualMax);
    void clearRequested();

private:
    void emitRangeSettings();
    void updateManualControlState();

    ColorContourRampWidget* ramp_{};
    QCheckBox* autoScaleCheck_{};
    QDoubleSpinBox* minSpin_{};
    QDoubleSpinBox* maxSpin_{};
    QPushButton* clearButton_{};
};

}  // namespace opendva::ui
