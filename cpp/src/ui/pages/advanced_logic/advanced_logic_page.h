#pragma once
#include <QWidget>
#include "config/config_types.h"

class QCheckBox;
class QLabel;
class QPushButton;
class QTableWidget;

class AdvancedLogicPage final : public QWidget {
    Q_OBJECT
public:
    explicit AdvancedLogicPage(QWidget* parent = nullptr);
    void setConfiguration(const AdvancedLogicConfig& config, const QList<ColorRule>& rules);
    void showDecision(int ruleNumber, bool execute);
    [[nodiscard]] const AdvancedLogicConfig& configuration() const { return config_; }

signals:
    void configurationChanged();
    void priorityChanged(int ruleNumber, ColorRulePriority priority);

private:
    void editSequence(bool create);
    void refreshSequences();
    void refreshActions();
    void moveSequence(int direction);
    AdvancedLogicConfig config_;
    QList<ColorRule> rules_;
    QCheckBox* enabled_{};
    QTableWidget* sequences_{};
    QTableWidget* priorities_{};
    QPushButton* add_{};
    QPushButton* edit_{};
    QPushButton* remove_{};
    QPushButton* up_{};
    QPushButton* down_{};
    QLabel* status_{};
};
