#pragma once

#include <array>

#include <QWidget>

#include "config/config_types.h"

class QPushButton;
class QComboBox;
class QCheckBox;
class QHBoxLayout;
class QLabel;
class QTableWidget;

class ColorRulePage final : public QWidget
{
    Q_OBJECT

  public:
    explicit ColorRulePage(QWidget *parent = nullptr);
    void setRules(const QList<ColorRule> &rules);

  signals:
    void addRequested();
    void editRequested(int number);
    void deleteRequested(int number);

  private:
    [[nodiscard]] int selectedRuleNumber() const;
    void refreshActionState();

    QTableWidget *table_{};
    QPushButton *editButton_{};
    QPushButton *deleteButton_{};
};
