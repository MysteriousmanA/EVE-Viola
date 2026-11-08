#pragma once
#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>
#include <QSet>
#include "features/color_rule/window_color_logic_engine.h"
#include "features/desktop_pet/desktop_pet_mode.h"
class ReplicaWindow;
class AudioFeedbackManager;

class WindowColorLogicRuntime final : public QObject {
    Q_OBJECT
public:
    WindowColorLogicRuntime(AudioFeedbackManager& audio,
        QObject* parent = nullptr);
    void configure(const QList<ColorRule>& rules,
        const QHash<QString, QPair<int, int>>& middleClickSwitches = {});
    void attach(const QString& id, ReplicaWindow* window);
    void detach(const QString& id);
    QString idForWindow(const ReplicaWindow* window) const;
    void setMode(DesktopPetMode mode);
    void stop();
signals:
    void accountTriggersChanged(const QHash<quintptr, QStringList>& triggers);
private:
    friend class MainWindowTest;
    struct Pending { ColorRule rule; QString target; bool executing{}; };
    struct RecentTrigger { ColorRule rule; quintptr hwnd{}; qint64 expiresAt{}; };
    void refreshAccountTriggers();
    void observe(const QString& id, const PixelStatistics& statistics);
    void invalidate(const QString& id);
    void refresh();
    void execute(quint64 token);
    void finish(quint64 token, bool success);
    void cancel(quint64 token);
    void toggleRulesForWindow(const QString& id);
    AudioFeedbackManager& audio_;
    WindowColorLogicEngine engine_;
    QList<ColorRule> rules_;
    QHash<QString, QPair<int, int>> middleClickSwitches_;
    QSet<int> triggeredRules_;
    QHash<quintptr, QStringList> accountTriggers_;
    QHash<int, RecentTrigger> recentAccountTriggers_;
    QHash<QString, QPointer<ReplicaWindow>> windows_;
    QHash<quint64, Pending> pending_;
    QElapsedTimer clock_;
    QTimer timer_;
    DesktopPetMode mode_{DesktopPetMode::Resident};
    bool stopped_{};
    bool batchingRefresh_{};
};
