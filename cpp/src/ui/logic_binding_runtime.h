#pragma once
#include <QObject>
#include <QHash>
#include <QStringList>
#include "config/config_types.h"
#include "platform/native_gateway.h"
class ReplicaWindow;
class WindowColorLogicRuntime;

class LogicBindingRuntime final : public QObject {
    Q_OBJECT
public:
    explicit LogicBindingRuntime(WindowColorLogicRuntime& logic, QObject* parent = nullptr);
    ~LogicBindingRuntime() override;
    void configure(const QList<ColorRule>& rules, const QList<LogicBindingConfig>& bindings,
        const QList<ReplicaConfig>& templates, const QList<WindowInfo>& sources,
        const QHash<QString, ReplicaWindow*>& replicas, bool force = false);
    QHash<QString, QString> statuses() const { return statuses_; }
    QList<ReplicaWindow*> windowsForBinding(const QString& id) const;
    void stop();
private:
    WindowColorLogicRuntime& logic_;
    QList<ColorRule> rules_;
    QList<LogicBindingConfig> bindings_;
    QList<ReplicaConfig> templates_;
    QHash<QString, WindowInfo> resolved_;
    QHash<QString, QString> statuses_;
    QHash<QString, ReplicaWindow*> replicas_;
    QHash<QString, QStringList> bindingWindowIds_;
    QList<ColorRule> canonicalRules_;
    QList<int> runtimeNumbers_;
    int nextRuntimeNumber_{1};
};
