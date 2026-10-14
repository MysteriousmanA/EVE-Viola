#pragma once

#include <QList>
#include <QPoint>
#include <QRect>
#include <QWidget>
#include <cstdint>

#include <functional>

#include "platform/native_gateway.h"

class QKeyEvent;
class QMouseEvent;
class QTimer;

class RadialSwitchOverlay final : public QWidget {
public:
    explicit RadialSwitchOverlay(QWidget* parent = nullptr);

    [[nodiscard]] bool showForClients(
        const QList<WindowInfo>& clients,
        const QPoint& globalAnchor,
        bool interactivePreview = false,
        std::function<void(int)> previewSelectionHandler = {},
        int centerImageDiameter = 0);
    [[nodiscard]] bool isPetRingVisible() const noexcept;
    void movePetCenter(const QPoint& globalCenter, int centerImageDiameter);
    void flashPetAccount(std::uintptr_t hwnd);
    void updateCursorPosition(const QPoint& globalPosition);
    [[nodiscard]] int selectedIndex() const noexcept;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    [[nodiscard]] int indexAt(const QPoint& globalPosition) const noexcept;
    void updateRadii();
    void updateOverlayGeometry();
    void updatePetRingMask();

    QList<WindowInfo> clients_;
    QRect monitorGeometry_;
    QRect screenGeometry_;
    QPoint anchorGlobal_;
    QPoint centerGlobal_;
    QPoint cursorGlobal_;
    qreal outerRadius_{};
    qreal innerRadius_{};
    int centerImageDiameter_{};
    int selectedIndex_{-1};
    bool interactivePreview_{};
    bool petRingMode_{};
    int flashingIndex_{-1};
    int flashTicksRemaining_{};
    bool flashPhase_{false};
    QTimer* flashTimer_{};
    std::function<void(int)> previewSelectionHandler_;
};
