#pragma once
#include <QWidget>
#include <QSet>
#include "config/config_types.h"
#include "platform/native_gateway.h"
class QComboBox;
class QTableWidget;
class QPushButton;
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
    struct SourceGroup {
        QString key;
        QString title;
        QList<int> bindings;
        int headerRow{};
        QPushButton* toggle{};
    };
    QComboBox* rules_{};
    QComboBox* sourceFilter_{};
    QComboBox* logicFilter_{};
    QTableWidget* table_{};
    QList<SourceGroup> groups_;
    QList<int> bindingRows_;
    QSet<QString> collapsedGroups_;
    QList<ColorRule> lastRules_;
    QList<LogicBindingConfig> lastBindings_;
    QList<WindowInfo> lastSources_;
    QHash<QString, QString> lastStatuses_;
};
