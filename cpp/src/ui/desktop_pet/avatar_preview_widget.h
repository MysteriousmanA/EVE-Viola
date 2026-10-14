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

class AvatarPreviewWidget final : public QWidget {
public:
    explicit AvatarPreviewWidget(QWidget* parent = nullptr);

    void setAvatar(const QImage& image);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QImage image_;
};
