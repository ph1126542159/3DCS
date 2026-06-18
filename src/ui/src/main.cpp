// A8 Qt/QML UI entry point.
#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTextStream>

#include "CommandExecutionModel.h"
#include "FeatureCatalogModel.h"
#include "WorkbenchModel.h"

namespace {

void writeQtLog(QtMsgType type,
                const QMessageLogContext& context,
                const QString& message) {
    QFile file(QCoreApplication::applicationDirPath() + "/opendva_gui.log");
    if (!file.open(QIODevice::Append | QIODevice::Text)) return;

    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODate) << ' ';
    switch (type) {
        case QtDebugMsg:
            stream << "DEBUG";
            break;
        case QtInfoMsg:
            stream << "INFO";
            break;
        case QtWarningMsg:
            stream << "WARN";
            break;
        case QtCriticalMsg:
            stream << "CRITICAL";
            break;
        case QtFatalMsg:
            stream << "FATAL";
            break;
    }
    stream << ' ' << message;
    if (context.file) stream << " (" << context.file << ':' << context.line << ')';
    stream << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QApplication app(argc, argv);
    QApplication::setOrganizationName("OpenDVA");
    QApplication::setApplicationName("OpenDVA");
    qInstallMessageHandler(writeQtLog);

    opendva::ui::FeatureCatalogModel featureCatalog;
    opendva::ui::WorkbenchModel workbenchModel;
    opendva::ui::CommandExecutionModel commandExecutor;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("featureCatalog", &featureCatalog);
    engine.rootContext()->setContextProperty("workbenchModel", &workbenchModel);
    engine.rootContext()->setContextProperty("commandExecutor", &commandExecutor);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    return app.exec();
}
