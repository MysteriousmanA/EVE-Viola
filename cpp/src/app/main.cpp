#include <exception>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QMessageBox>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTimer>

#include "platform/native_gateway.h"
#include "runtime/runtime_logger.h"
#include "runtime/performance_recorder.h"
#include "platform/single_instance.h"
#include "ui/main_window.h"

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("EVE-Viola"));
    QApplication::setApplicationVersion(QStringLiteral("1.1.1"));
    QApplication::setApplicationDisplayName(QStringLiteral("EVE-Viola"));
    QApplication::setOrganizationName(QStringLiteral("EVE-Viola"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/viola.png")));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption smokeExitOption(
        QStringLiteral("smoke-exit-ms"),
        QStringLiteral("Internal deployment smoke-test timeout."),
        QStringLiteral("milliseconds"));
    parser.addOption(smokeExitOption);
    QCommandLineOption instanceKeyOption(
        QStringLiteral("single-instance-key"),
        QStringLiteral("Internal single-instance test key."),
        QStringLiteral("key"));
    parser.addOption(instanceKeyOption);
    const QCommandLineOption performanceOption(
        QStringLiteral("performance-log"),
        QStringLiteral("Write phased performance summaries to a JSONL file."),
        QStringLiteral("path"));
    parser.addOption(performanceOption);
    parser.process(application);

    const bool smoke = parser.isSet(smokeExitOption);
    QString instanceKey = parser.value(instanceKeyOption).trimmed();
    if (instanceKey.isEmpty()) {
        instanceKey = smoke
            ? QStringLiteral("EVE-Viola.Smoke.%1").arg(QCoreApplication::applicationPid())
            : singleInstanceKeyForApplication(QCoreApplication::applicationFilePath());
    }
    SingleInstance singleInstance(instanceKey);
    if (singleInstance.state() == SingleInstance::State::Secondary) {
        return singleInstance.notifyPrimary() ? 0 : 4;
    }
    if (singleInstance.state() == SingleInstance::State::Error) {
        QMessageBox::critical(
            nullptr,
            QStringLiteral("EVE-Viola 启动失败"),
            QStringLiteral("无法建立单实例服务：%1").arg(singleInstance.errorString()));
        return 4;
    }

    RuntimeLogger::initialize(
        QCoreApplication::applicationDirPath() + QStringLiteral("/logs"));
    const auto flushRuntimeLog = qScopeGuard([] {
        if (!RuntimeLogger::flush()) {
            qWarning() << "Runtime log flush failed";
        }
    });
    QTimer runtimeLogTimer;
    runtimeLogTimer.setInterval(2000);
    QObject::connect(&runtimeLogTimer, &QTimer::timeout, &application, [] {
        static_cast<void>(RuntimeLogger::flush());
    });
    runtimeLogTimer.start();

    const QString performancePath = parser.isSet(performanceOption)
        ? parser.value(performanceOption)
        : qEnvironmentVariable("EVE_VIOLA_PERFORMANCE_LOG");
    QTimer performanceTimer;
    const auto flushPerformance = qScopeGuard([] {
        QString error;
        if (!PerformanceRecorder::flush(&error)) {
            qWarning().noquote() << "Performance log flush failed:" << error;
        }
    });
    if (!performancePath.isEmpty()) {
        QString error;
        if (!PerformanceRecorder::initialize(performancePath, &error)) {
            QMessageBox::critical(nullptr, QStringLiteral("性能日志无法打开"), error);
            return 2;
        }
        performanceTimer.setInterval(5000);
        QObject::connect(&performanceTimer, &QTimer::timeout, &application, [] {
            QString error;
            if (!PerformanceRecorder::flush(&error)) {
                qWarning().noquote() << "Performance log flush failed:" << error;
            }
        });
        performanceTimer.start();
    }

    QString configPath = QCoreApplication::applicationDirPath()
        + QStringLiteral("/cache");
    QTemporaryDir smokeConfigDirectory;
    if (smoke) {
        if (!smokeConfigDirectory.isValid()) {
            return 3;
        }
        configPath = smokeConfigDirectory.path();
    }

    try {
        NativeGateway gateway;
        MainWindow window(gateway, configPath);
        singleInstance.setActivationHandler([&window] {
            window.showAndActivate();
        });
        window.show();

        if (smoke) {
            bool valid = false;
            const auto milliseconds = parser.value(smokeExitOption).toInt(&valid);
            if (!valid || milliseconds < 1) {
                return 2;
            }
            static_cast<void>(window.createReplica());
            QTimer::singleShot(milliseconds, &application, &QApplication::quit);
        }

        return application.exec();
    } catch (const std::exception& error) {
        QMessageBox::critical(
            nullptr,
            QStringLiteral("EVE-Viola 启动失败"),
            QString::fromUtf8(error.what()));
        return 1;
    }
}
