#pragma once

#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QWidget>

class QCloseEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QShowEvent;

class ScreenRegionCaptureOverlay final : public QWidget {
    Q_OBJECT

public:
    ScreenRegionCaptureOverlay(
        const QRect& screenGeometry,
        const QRectF& clientGeometry,
        const QSize& sourceSize,
        QWidget* parent = nullptr);

signals:
    void selectionFinished(const QRect& sourceRegion, bool accepted);

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    [[nodiscard]] QPoint clampToClient(const QPoint& point) const noexcept;
    [[nodiscard]] QRect mapSelectionToSource() const noexcept;
    void finish(bool accepted);

    QRect screenGeometry_;
    QRectF clientRect_;
    QSize sourceSize_;
    QPoint selectionOrigin_;
    QPoint selectionCurrent_;
    bool selecting_{};
    bool selectionReady_{};
    bool finished_{};
};
