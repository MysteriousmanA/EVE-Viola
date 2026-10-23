#pragma once

#include <functional>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include "features/advanced_logic/advanced_logic_engine.h"

class AdvancedLogicService final : public QObject {
    Q_OBJECT
public:
    explicit AdvancedLogicService(QObject* parent = nullptr);
    void setConfiguration(const AdvancedLogicConfig& config, const QList<ColorRule>& rules);
    void submit(QObject* owner, quint64 ownerId, const QString& sourceKey,
        const QString& sourceTitle, int ruleNumber, std::function<void(bool)> callback);
    void cancelOwner(quint64 ownerId);
    void stop();

signals:
    void eventResolved(int ruleNumber, bool execute);

private:
    struct Pending {
        QPointer<QObject> owner;
        quint64 ownerId{};
        int ruleNumber{};
        std::function<void(bool)> callback;
    };
    void dispatch(const QList<AdvancedLogicDecision>& decisions);
    void schedule();
    AdvancedLogicEngine engine_;
    AdvancedLogicConfig config_;
    QList<ColorRule> rules_;
    QElapsedTimer clock_;
    QTimer timer_;
    QHash<quint64, Pending> pending_;
    QHash<quint64, QPointer<QObject>> owners_;
    quint64 nextId_{};
    bool stopped_{};
};
