#pragma once

#include <optional>
#include <QWidget>
#include "features/replica/pixel_statistics.h"

class QComboBox;
class QSpinBox;

class PixelColorEditor final : public QWidget {
public:
    explicit PixelColorEditor(const PixelColorTarget& target, bool allowNone,
                              QWidget* parent = nullptr);
    [[nodiscard]] std::optional<PixelColorTarget> target() const;
    [[nodiscard]] QComboBox* combo() const { return combo_; }
    [[nodiscard]] QSpinBox* tolerance() const { return tolerance_; }

private:
    QComboBox* combo_{};
    QSpinBox* tolerance_{};
};

[[nodiscard]] std::optional<QList<PixelColorTarget>> promptStatisticsColors(
    QWidget* parent, const QList<PixelColorTarget>& targets);
