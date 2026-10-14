#pragma once

#include <memory>

#include <QObject>
#include <QString>
#include <QStringList>

inline constexpr int kRadialSwitchHoldDurationMs = 300;

class NativeGateway;

class QuickSwitchService final : public QObject {
    Q_OBJECT

public:
    explicit QuickSwitchService(NativeGateway& gateway, QObject* parent = nullptr);
    ~QuickSwitchService() override;

    QuickSwitchService(const QuickSwitchService&) = delete;
    QuickSwitchService& operator=(const QuickSwitchService&) = delete;

    void start(
        const QStringList& previousKeys,
        const QStringList& nextKeys,
        const QString& radialSwitchKey);
    void updateKeys(const QStringList& previousKeys, const QStringList& nextKeys);
    void updateRadialSwitchKey(const QString& key);
    void beginCapture();
    void cancelCapture();
    void stop();
    [[nodiscard]] bool isForegroundWindow(quintptr hwnd) const;

signals:
    void switchRequested(bool next, quintptr foregroundHwnd);
    void radialSwitchHoldStarted(quint64 sessionId, quintptr foregroundHwnd);
    void radialSwitchHoldEnded(
        quint64 sessionId,
        bool heldLongEnough,
        bool foregroundStillMatches,
        bool cancelled,
        quintptr foregroundHwnd);
    void inputCaptured(const QString& key);
    void hookError(const QString& message);

private:
    friend class QuickSwitchServiceTest;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
