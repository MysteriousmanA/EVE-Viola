#pragma once

#include <functional>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QWheelEvent>

namespace ReplicaRowUi {

class ScrollPassthroughCombo final : public QComboBox {
public:
    using QComboBox::QComboBox;

    std::function<void()> onShowPopup;

protected:
    void showPopup() override
    {
        if (onShowPopup) {
            onShowPopup();
        }
        QComboBox::showPopup();
    }

    void wheelEvent(QWheelEvent* event) override
    {
        event->ignore();
    }
};

class ScrollPassthroughDoubleSpinBox final : public QDoubleSpinBox {
public:
    using QDoubleSpinBox::QDoubleSpinBox;

protected:
    void wheelEvent(QWheelEvent* event) override
    {
        event->ignore();
    }
};

} // namespace ReplicaRowUi
