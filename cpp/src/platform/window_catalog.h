#pragma once
#include <functional>
#include <QObject>
#include "platform/native_gateway.h"

class QTimer;
class WindowCatalog final : public QObject {
    Q_OBJECT
public:
    using Enumerate = std::function<QList<WindowInfo>()>;
    using Clock = std::function<qint64()>;
    explicit WindowCatalog(NativeGateway& gateway, QObject* parent = nullptr);
    WindowCatalog(Enumerate enumerate, Clock clock, QObject* parent = nullptr);
    [[nodiscard]] QList<WindowInfo> windows(bool forceRefresh = false);
    void setMonitoring(bool enabled);
signals:
    void refreshed();
    void refreshFailed(const QString& message);
private:
    Enumerate enumerate_;
    Clock clock_;
    QList<WindowInfo> windows_;
    std::optional<qint64> refreshedAt_;
    QTimer* timer_{};
};
