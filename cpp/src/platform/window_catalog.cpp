#include "platform/window_catalog.h"
#include <chrono>
#include <utility>
#include <QTimer>

WindowCatalog::WindowCatalog(NativeGateway& gateway, QObject* parent)
    : WindowCatalog([&gateway] { return gateway.listEveWindows(); }, [] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }, parent)
{
}
WindowCatalog::WindowCatalog(Enumerate enumerate, Clock clock, QObject* parent)
    : QObject(parent), enumerate_(std::move(enumerate)), clock_(std::move(clock))
{
    timer_ = new QTimer(this);
    timer_->setInterval(5000);
    connect(timer_, &QTimer::timeout, this, [this] {
        try {
            static_cast<void>(windows(true));
            emit refreshed();
        } catch (const std::exception& error) {
            emit refreshFailed(QString::fromUtf8(error.what()));
        }
    });
}
QList<WindowInfo> WindowCatalog::windows(bool forceRefresh)
{
    const auto now = clock_();
    if (forceRefresh || !refreshedAt_ || now - *refreshedAt_ >= 100) {
        auto snapshot = enumerate_();
        windows_ = std::move(snapshot);
        refreshedAt_ = now;
    }
    return windows_;
}
void WindowCatalog::setMonitoring(bool enabled)
{
    if (enabled) { timer_->start(); }
    else { timer_->stop(); }
}
