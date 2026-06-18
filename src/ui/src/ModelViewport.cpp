#include "ModelViewport.h"

#include "ColorContourLegendWidget.h"

#include <QColor>
#include <QHBoxLayout>
#include <QPixmap>
#include <QQuaternion>
#include <QVector3D>

#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QCylinderMesh>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DRender/QCamera>

namespace opendva::ui {
namespace {

QVector3D toVector(const Vec3& v) {
    return QVector3D(static_cast<float>(v.x),
                     static_cast<float>(v.y),
                     static_cast<float>(v.z));
}

QColor toColor(const ViewportColor& color) {
    return QColor(color.r, color.g, color.b);
}

}  // namespace

ModelViewport::ModelViewport(QWidget* parent) : QWidget(parent) {
    view_ = new Qt3DExtras::Qt3DWindow();
    view_->defaultFrameGraph()->setClearColor(QColor(34, 38, 44));

    auto* container = QWidget::createWindowContainer(view_, this);
    container->setMinimumSize(480, 320);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(container);

    legend_ = new ColorContourLegendWidget(this);
    legend_->setObjectName("contourLegend");
    layout->addWidget(legend_);
    connect(legend_, &ColorContourLegendWidget::rangeSettingsChanged,
            this, [this](bool autoScale, double manualMin, double manualMax) {
                contourAutoScale_ = autoScale;
                contourManualMin_ = manualMin;
                contourManualMax_ = manualMax;
                refreshContourLegend();
                rebuildScene(model_);
            });
    connect(legend_, &ColorContourLegendWidget::clearRequested, this, [this]() {
        pointDeviations_.clear();
        refreshContourLegend();
        rebuildScene(model_);
    });

    root_ = new Qt3DCore::QEntity();
    view_->setRootEntity(root_);

    Qt3DRender::QCamera* camera = view_->camera();
    camera->lens()->setPerspectiveProjection(45.0f, 16.0f / 9.0f, 0.1f, 10000.0f);
    camera->setPosition(QVector3D(25.0f, -35.0f, 25.0f));
    camera->setViewCenter(QVector3D(0.0f, 0.0f, 5.0f));

    auto* controller = new Qt3DExtras::QOrbitCameraController(root_);
    controller->setCamera(camera);
    controller->setLinearSpeed(60.0f);
    controller->setLookSpeed(180.0f);
}

void ModelViewport::setModel(const Model& model) {
    model_ = model;
    rebuildScene(model_);
}

void ModelViewport::setPointDeviations(const std::map<PointId, double>& pointDeviations,
                                       double manualMin,
                                       double manualMax) {
    pointDeviations_ = pointDeviations;
    contourMin_ = manualMin;
    contourMax_ = manualMax;
    contourAutoScale_ = !(manualMin <= manualMax);
    contourManualMin_ = manualMin;
    contourManualMax_ = manualMax;
    refreshContourLegend();
    rebuildScene(model_);
}

bool ModelViewport::saveSnapshot(const QString& path) {
    return grab().save(path, "PNG");
}

void ModelViewport::refreshContourLegend() {
    ContourLegendScale scale;
    const ContourRange range = resolveContourRange(pointDeviations_, contourAutoScale_,
                                                   contourManualMin_,
                                                   contourManualMax_);
    if (range.valid) {
        contourMin_ = range.minValue;
        contourMax_ = range.maxValue;
        if (contourAutoScale_) {
            contourManualMin_ = range.minValue;
            contourManualMax_ = range.maxValue;
        }

        scale.visible = true;
        scale.minValue = range.minValue;
        scale.maxValue = range.maxValue;
        scale.entries = contourLegend(range.minValue, range.maxValue, 5);
    } else {
        contourMin_ = 1.0;
        contourMax_ = 0.0;
    }

    legend_->setRangeControls(contourAutoScale_, contourManualMin_, contourManualMax_);
    legend_->setLegend(scale);
}

void ModelViewport::rebuildScene(const Model& model) {
    const ContourRange range = resolveContourRange(pointDeviations_, contourAutoScale_,
                                                   contourManualMin_,
                                                   contourManualMax_);
    if (range.valid) {
        contourMin_ = range.minValue;
        contourMax_ = range.maxValue;
    }

    auto* newRoot = new Qt3DCore::QEntity();

    createAxisEntity(newRoot, QVector3D(6.0f, 0.0f, 0.0f),
                     QVector3D(0.0f, 0.0f, 90.0f), 12.0f, QColor(220, 80, 80));
    createAxisEntity(newRoot, QVector3D(0.0f, 6.0f, 0.0f),
                     QVector3D(90.0f, 0.0f, 0.0f), 12.0f, QColor(80, 190, 90));
    createAxisEntity(newRoot, QVector3D(0.0f, 0.0f, 6.0f),
                     QVector3D(0.0f, 0.0f, 0.0f), 12.0f, QColor(90, 130, 230));

    for (const ViewportPointMarker& marker :
         viewportPointMarkers(model, pointDeviations_, contourMin_, contourMax_)) {
        createPointEntity(newRoot, marker, toColor(marker.color));
    }

    for (const FeatureGlyph& glyph : featureGlyphs(model)) {
        createFeatureGlyphEntity(newRoot, glyph, QColor(100, 145, 185));
    }

    for (const LineSegment& segment : measurementLineSegments(model)) {
        createLineSegmentEntity(newRoot, segment.start, segment.end, QColor(60, 210, 230));
    }

    Qt3DRender::QCamera* camera = view_->camera();
    auto* controller = new Qt3DExtras::QOrbitCameraController(newRoot);
    controller->setCamera(camera);
    controller->setLinearSpeed(60.0f);
    controller->setLookSpeed(180.0f);

    view_->setRootEntity(newRoot);
    delete root_;
    root_ = newRoot;
}

Qt3DCore::QEntity* ModelViewport::createPointEntity(Qt3DCore::QEntity* parent,
                                                    const ViewportPointMarker& marker,
                                                    const QColor& color) {
    auto* entity = new Qt3DCore::QEntity(parent);

    auto* mesh = new Qt3DExtras::QSphereMesh(entity);
    mesh->setRadius(0.35f);
    mesh->setRings(16);
    mesh->setSlices(16);

    auto* material = new Qt3DExtras::QPhongMaterial(entity);
    material->setDiffuse(color);

    auto* transform = new Qt3DCore::QTransform(entity);
    transform->setTranslation(toVector(marker.position));

    entity->addComponent(mesh);
    entity->addComponent(material);
    entity->addComponent(transform);
    return entity;
}

Qt3DCore::QEntity* ModelViewport::createAxisEntity(Qt3DCore::QEntity* parent,
                                                   const QVector3D& position,
                                                   const QVector3D& rotation,
                                                   float length,
                                                   const QColor& color) {
    auto* entity = new Qt3DCore::QEntity(parent);

    auto* mesh = new Qt3DExtras::QCylinderMesh(entity);
    mesh->setRadius(0.06f);
    mesh->setLength(length);
    mesh->setRings(8);
    mesh->setSlices(16);

    auto* material = new Qt3DExtras::QPhongMaterial(entity);
    material->setDiffuse(color);

    auto* transform = new Qt3DCore::QTransform(entity);
    transform->setTranslation(position);
    transform->setRotationX(rotation.x());
    transform->setRotationY(rotation.y());
    transform->setRotationZ(rotation.z());

    entity->addComponent(mesh);
    entity->addComponent(material);
    entity->addComponent(transform);
    return entity;
}

Qt3DCore::QEntity* ModelViewport::createFeatureGlyphEntity(Qt3DCore::QEntity* parent,
                                                           const FeatureGlyph& glyph,
                                                           const QColor& color) {
    auto* entity = new Qt3DCore::QEntity(parent);

    auto* material = new Qt3DExtras::QPhongMaterial(entity);
    material->setDiffuse(color);

    auto* transform = new Qt3DCore::QTransform(entity);
    transform->setTranslation(toVector(glyph.center));

    const float radius = static_cast<float>(glyph.radius);
    if (glyph.kind == FeatureKind::Plane || glyph.kind == FeatureKind::SlotTab) {
        auto* mesh = new Qt3DExtras::QCylinderMesh(entity);
        mesh->setRadius(radius);
        mesh->setLength(0.08f);
        mesh->setRings(8);
        mesh->setSlices(32);
        entity->addComponent(mesh);
    } else if (glyph.kind == FeatureKind::Cylinder || glyph.kind == FeatureKind::Cone ||
               glyph.kind == FeatureKind::Edge) {
        auto* mesh = new Qt3DExtras::QCylinderMesh(entity);
        mesh->setRadius(std::max(0.12f, radius * 0.25f));
        mesh->setLength(std::max(1.0f, radius * 2.0f));
        mesh->setRings(8);
        mesh->setSlices(24);
        entity->addComponent(mesh);
    } else {
        auto* mesh = new Qt3DExtras::QSphereMesh(entity);
        mesh->setRadius(std::max(0.45f, radius * 0.35f));
        mesh->setRings(16);
        mesh->setSlices(16);
        entity->addComponent(mesh);
    }

    entity->addComponent(material);
    entity->addComponent(transform);
    return entity;
}

Qt3DCore::QEntity* ModelViewport::createLineSegmentEntity(Qt3DCore::QEntity* parent,
                                                          const Vec3& start,
                                                          const Vec3& end,
                                                          const QColor& color) {
    const QVector3D a = toVector(start);
    const QVector3D b = toVector(end);
    const QVector3D delta = b - a;
    const float length = delta.length();
    if (length <= 0.0001f) {
        return nullptr;
    }

    auto* entity = new Qt3DCore::QEntity(parent);

    auto* mesh = new Qt3DExtras::QCylinderMesh(entity);
    mesh->setRadius(0.045f);
    mesh->setLength(length);
    mesh->setRings(8);
    mesh->setSlices(16);

    auto* material = new Qt3DExtras::QPhongMaterial(entity);
    material->setDiffuse(color);

    auto* transform = new Qt3DCore::QTransform(entity);
    transform->setTranslation((a + b) * 0.5f);
    transform->setRotation(QQuaternion::rotationTo(QVector3D(0.0f, 1.0f, 0.0f),
                                                   delta.normalized()));

    entity->addComponent(mesh);
    entity->addComponent(material);
    entity->addComponent(transform);
    return entity;
}

}  // namespace opendva::ui
