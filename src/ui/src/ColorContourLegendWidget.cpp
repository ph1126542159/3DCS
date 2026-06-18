#include "ColorContourLegendWidget.h"

#include <algorithm>

#include <QCheckBox>
#include <QColor>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <QString>
#include <QVBoxLayout>

namespace opendva::ui {
namespace {

QColor toColor(const ViewportColor& color) {
    return QColor(color.r, color.g, color.b);
}

QString formatValue(double value) {
    return QString::number(value, 'f', 3);
}

}  // namespace

class ColorContourRampWidget final : public QWidget {
    Q_OBJECT

public:
    explicit ColorContourRampWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(150);
    }

    void setLegend(const ContourLegendScale& scale) {
        scale_ = scale;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        QWidget::paintEvent(event);
        if (!scale_.visible || scale_.entries.empty()) return;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRect content = rect().adjusted(12, 12, -12, -10);
        if (content.width() <= 0 || content.height() <= 0) return;

        const int labelHeight = fontMetrics().height();
        const QRect titleRect(content.left(), content.top(), content.width(), labelHeight);
        painter.setPen(QColor(235, 238, 242));
        painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("Contour"));

        const int rampTop = titleRect.bottom() + 10;
        const int rampHeight = std::max(36, content.height() - labelHeight * 3 - 18);
        const QRect rampRect(content.left(), rampTop, 28, rampHeight);
        const int entryCount = static_cast<int>(scale_.entries.size());
        const double stepHeight = static_cast<double>(rampRect.height()) /
                                  static_cast<double>(entryCount);

        for (int i = 0; i < entryCount; ++i) {
            const int reversed = entryCount - 1 - i;
            const int y0 = rampRect.top() +
                           static_cast<int>(stepHeight * static_cast<double>(i));
            const int y1 = rampRect.top() +
                           static_cast<int>(stepHeight * static_cast<double>(i + 1));
            const QRect swatch(rampRect.left(), y0, rampRect.width(),
                               std::max(1, y1 - y0));
            painter.fillRect(
                swatch,
                toColor(scale_.entries[static_cast<std::size_t>(reversed)].color));
        }

        painter.setPen(QColor(95, 102, 112));
        painter.drawRect(rampRect.adjusted(0, 0, -1, -1));

        painter.setPen(QColor(235, 238, 242));
        const int textLeft = rampRect.right() + 10;
        const QRect maxRect(textLeft, rampRect.top() - labelHeight / 2,
                            content.right() - textLeft + 1, labelHeight);
        const QRect minRect(textLeft, rampRect.bottom() - labelHeight / 2,
                            content.right() - textLeft + 1, labelHeight);
        painter.drawText(maxRect, Qt::AlignLeft | Qt::AlignVCenter,
                         formatValue(scale_.maxValue));
        painter.drawText(minRect, Qt::AlignLeft | Qt::AlignVCenter,
                         formatValue(scale_.minValue));

        painter.setPen(QColor(170, 178, 188));
        const QRect unitRect(content.left(), rampRect.bottom() + 10, content.width(),
                             labelHeight);
        painter.drawText(unitRect, Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("6 sigma"));
    }

private:
    ContourLegendScale scale_{};
};

ColorContourLegendWidget::ColorContourLegendWidget(QWidget* parent) : QWidget(parent) {
    setFixedWidth(154);
    setMinimumHeight(260);
    setVisible(false);

    ramp_ = new ColorContourRampWidget(this);
    ramp_->setObjectName("contourRamp");

    autoScaleCheck_ = new QCheckBox(QStringLiteral("Auto scale"), this);
    autoScaleCheck_->setObjectName("autoScaleCheck");
    autoScaleCheck_->setChecked(true);

    minSpin_ = new QDoubleSpinBox(this);
    minSpin_->setObjectName("manualMinSpin");
    maxSpin_ = new QDoubleSpinBox(this);
    maxSpin_->setObjectName("manualMaxSpin");
    for (QDoubleSpinBox* spin : {minSpin_, maxSpin_}) {
        spin->setRange(-1000000000.0, 1000000000.0);
        spin->setDecimals(6);
        spin->setSingleStep(0.1);
    }

    clearButton_ = new QPushButton(QStringLiteral("Clear"), this);
    clearButton_->setObjectName("clearContourButton");

    auto* form = new QFormLayout();
    form->setContentsMargins(8, 0, 8, 0);
    form->setHorizontalSpacing(6);
    form->setVerticalSpacing(4);
    form->addRow(QStringLiteral("Min"), minSpin_);
    form->addRow(QStringLiteral("Max"), maxSpin_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 8);
    layout->setSpacing(6);
    layout->addWidget(ramp_);
    layout->addWidget(autoScaleCheck_);
    layout->addLayout(form);
    layout->addWidget(clearButton_);

    connect(autoScaleCheck_, &QCheckBox::toggled, this, [this]() {
        updateManualControlState();
        emitRangeSettings();
    });
    connect(minSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double) { emitRangeSettings(); });
    connect(maxSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double) { emitRangeSettings(); });
    connect(clearButton_, &QPushButton::clicked, this,
            &ColorContourLegendWidget::clearRequested);

    updateManualControlState();
}

void ColorContourLegendWidget::setLegend(const ContourLegendScale& scale) {
    ramp_->setLegend(scale);
    setVisible(scale.visible && !scale.entries.empty());
}

void ColorContourLegendWidget::setRangeControls(bool autoScale,
                                                double manualMin,
                                                double manualMax) {
    const QSignalBlocker autoBlocker(autoScaleCheck_);
    const QSignalBlocker minBlocker(minSpin_);
    const QSignalBlocker maxBlocker(maxSpin_);

    autoScaleCheck_->setChecked(autoScale);
    minSpin_->setValue(manualMin);
    maxSpin_->setValue(manualMax);
    updateManualControlState();
}

void ColorContourLegendWidget::emitRangeSettings() {
    emit rangeSettingsChanged(autoScaleCheck_->isChecked(),
                              minSpin_->value(),
                              maxSpin_->value());
}

void ColorContourLegendWidget::updateManualControlState() {
    const bool manualEnabled = !autoScaleCheck_->isChecked();
    minSpin_->setEnabled(manualEnabled);
    maxSpin_->setEnabled(manualEnabled);
}

}  // namespace opendva::ui

#include "ColorContourLegendWidget.moc"
