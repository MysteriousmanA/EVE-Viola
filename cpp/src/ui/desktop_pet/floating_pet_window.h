#pragma once

#include <QImage>
#include <QPointF>
#include <QWidget>

#include "ui/desktop_pet/desktop_pet_constants.h"

class QLabel;
class QTimer;
class QMouseEvent;
class QWheelEvent;
class QContextMenuEvent;

class FloatingPetWindow final : public QWidget {
    Q_OBJECT

public:
    explicit FloatingPetWindow();

    void setAvatar(const QImage& image, int size);

    void showMessage(const QString& message);

    void hideByUser();

signals:
    void hiddenByUser();
    void fishModeToggled(bool enabled);
    void autoHiddenByMessage();
    void positionChanged(const QPoint& position);
    void avatarGeometryChanged(const QPoint& petCenter, int avatarSize);
    void sizeChanged(int size);
    void messageExpired();
    void leftClicked(const QPoint& petCenter, int avatarSize);

protected:
    void paintEvent(QPaintEvent*) override;

    void mousePressEvent(QMouseEvent* event) override;

    void mouseMoveEvent(QMouseEvent* event) override;

    void mouseReleaseEvent(QMouseEvent* event) override;

    void wheelEvent(QWheelEvent* event) override;

    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void initializeMessageBubble();

    bool fishModeEnabled_{};
    void keepWindowOnScreen();

    void collapseMessageBubble();

    [[nodiscard]] QRect avatarRect() const;

    void updateWindowRegion();

    QImage image_;
    QPoint dragOffset_;
    QPoint pressGlobalPosition_;
    QLabel* bubble_{};
    QTimer* messageTimer_{};
    int avatarSize_{128};
    int bubbleWidth_{244};
    int bubbleHeight_{64};
    int bubbleRegionHeight_{DesktopPetUi::kBubbleRegionHeight};
    bool dragging_{false};
    bool dragged_{false};
    bool messageVisible_{false};
    bool autoHideAfterMessage_{false};
    int wheelDeltaRemainder_{0};
};
