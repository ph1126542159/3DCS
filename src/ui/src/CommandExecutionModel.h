#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace opendva::ui {

class CommandExecutionModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(int executionCount READ executionCount NOTIFY executionCountChanged)
    Q_PROPERTY(QVariantMap lastRecord READ lastRecord NOTIFY lastRecordChanged)

public:
    explicit CommandExecutionModel(QObject* parent = nullptr);

    int executionCount() const;
    QVariantMap lastRecord() const;

    Q_INVOKABLE QVariantMap execute(const QVariantMap& workspace,
                                    const QVariantMap& commandView,
                                    const QString& commandName);

signals:
    void executionCountChanged();
    void lastRecordChanged();

private:
    QString riskLevel(const QVariantMap& workspace,
                      const QVariantMap& commandView) const;

    int executionCount_ = 0;
    QVariantMap lastRecord_;
};

}  // namespace opendva::ui
