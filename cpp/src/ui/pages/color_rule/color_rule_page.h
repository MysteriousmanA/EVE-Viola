#pragma once

#include <array>
#include <QHash>

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
    void setReplicas(const QList<ReplicaConfig>& replicas, const QHash<QString, QString>& presetNames = {}) {
        replicas_ = replicas; presetNames_ = presetNames;
    }

  signals:
    void addRequested();
    void colorTolerancesRequested();
    void editRequested(int number);
    void deleteRequested(int number);

  private:
    [[nodiscard]] int selectedRuleNumber() const;
    void refreshActionState();

    QTableWidget *table_{};
    QList<ReplicaConfig> replicas_;
    QHash<QString, QString> presetNames_;
    QPushButton *editButton_{};
    QPushButton *deleteButton_{};
};
