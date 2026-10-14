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

class AvatarCropWidget final : public QWidget {
    Q_OBJECT

public:
    explicit AvatarCropWidget(QWidget* parent = nullptr);

    void setImage(const QImage& image);

    void setZoomPercent(int percent);

    void setNormalizedOffset(const QPointF& offset);

    [[nodiscard]] int zoomPercent() const;

    [[nodiscard]] QPointF normalizedOffset() const;

    [[nodiscard]] QImage croppedImage(int pixels = DesktopPetUi::kAvatarPixels) const;

signals:
    void cropChanged();
    void cropInteractionFinished();

protected:
    void paintEvent(QPaintEvent*) override;

    void mousePressEvent(QMouseEvent* event) override;

    void mouseMoveEvent(QMouseEvent* event) override;

    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    [[nodiscard]] QRectF cropRect() const;

    [[nodiscard]] QSizeF renderedImageSize() const;

    [[nodiscard]] QRectF renderedImageRect() const;

    void clampOffset();

    QImage image_;
    QPointF offset_;
    QPointF normalizedOffset_;
    QPointF lastPointer_;
    qreal zoom_{1.0};
    bool dragging_{false};
};
