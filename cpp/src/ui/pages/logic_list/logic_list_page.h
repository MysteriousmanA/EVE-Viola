#pragma once
#include <QWidget>
#include "config/config_types.h"
#include "platform/native_gateway.h"
class QComboBox;
class QTableWidget;
class LogicListPage final : public QWidget {
    Q_OBJECT
public:
    explicit LogicListPage(QWidget* parent = nullptr);
    void setConfiguration(const QList<ColorRule>& rules, const QList<LogicBindingConfig>& bindings,
        const QList<WindowInfo>& sources, const QHash<QString, QString>& statuses);
signals:
    void addRequested(int ruleNumber);
    void addForSourcesRequested(int ruleNumber, const QList<WindowInfo>& sources);
    void removeRequested(const QString& id);
    void sourceChanged(const QString& id, const WindowInfo& source);
    void enabledChanged(const QString& id, bool enabled);
    void refreshRequested();
private:
    void applyFilters();
    void chooseSourcesForAddition();
    QComboBox* rules_{};
    QComboBox* sourceFilter_{};
    QComboBox* logicFilter_{};
    QTableWidget* table_{};
    QList<ColorRule> lastRules_;
    QList<LogicBindingConfig> lastBindings_;
    QList<WindowInfo> lastSources_;
    QHash<QString, QString> lastStatuses_;
};
