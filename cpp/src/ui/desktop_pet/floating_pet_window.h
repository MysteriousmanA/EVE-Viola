#pragma once

#include <QImage>
#include <QPointF>
#include <QStringList>
#include <QWidget>

#include "ui/desktop_pet/desktop_pet_constants.h"
#include "features/desktop_pet/desktop_pet_mode.h"

class QLabel;
class QTimer;
class QMouseEvent;
class QWheelEvent;
class QContextMenuEvent;
class QMenu;
class QTextBrowser;

class FloatingPetWindow final : public QWidget {
    Q_OBJECT

public:
    explicit FloatingPetWindow();

    void setAvatar(const QImage& image, int size);

    void showMessage(const QString& message);
    void appendTriggerRecord(const QString& record);
    void toggleTriggerHistory();

    void hideByUser();
    void clearMessage();
    void setMode(DesktopPetMode mode);
    DesktopPetMode mode() const { return mode_; }
    QMenu* buildContextMenu();

signals:
    void hiddenByUser();
    void modeChanged(DesktopPetMode mode);
    void stopSoftwareRequested();
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
    void showBubbleText(const QString& text);
    void refreshTriggerHistory();

    DesktopPetMode mode_{DesktopPetMode::Resident};
    void keepWindowOnScreen();

    void collapseMessageBubble();

    [[nodiscard]] QRect avatarRect() const;

    void updateWindowRegion();

    QImage image_;
    QPoint dragOffset_;
    QPoint pressGlobalPosition_;
    QLabel* bubble_{};
    QTextBrowser* historyBubble_{};
    QStringList triggerHistory_;
    QTimer* messageTimer_{};
    int avatarSize_{128};
    int bubbleWidth_{244};
    int bubbleHeight_{64};
    int bubbleRegionHeight_{DesktopPetUi::kBubbleRegionHeight};
    bool dragging_{false};
    bool dragged_{false};
    bool messageVisible_{false};
    bool historyVisible_{false};
    bool autoHideAfterMessage_{false};
    int wheelDeltaRemainder_{0};
    int historyWheelRemainder_{0};
};
