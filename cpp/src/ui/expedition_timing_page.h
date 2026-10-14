#pragma once

#include <QWidget>

class QButtonGroup;
class QDateTimeEdit;
class QLabel;
class QPushButton;
class QSpinBox;

class ExpeditionTimingPage final : public QWidget {
public:
    explicit ExpeditionTimingPage(QWidget* parent = nullptr);
    void syncCurrentTime();

private:
    [[nodiscard]] QString selectedValue(const QButtonGroup* group) const;
    void updateDefaultDuration(const QString& expeditionType);
    void calculateExpiry();
    void copyResult();

    QButtonGroup* factionGroup_{};
    QButtonGroup* typeGroup_{};
    QDateTimeEdit* startTime_{};
    QSpinBox* deltaDays_{};
    QSpinBox* deltaHours_{};
    QSpinBox* deltaMinutes_{};
    QLabel* resultValue_{};
    QPushButton* copyButton_{};
    QString result_;
};
