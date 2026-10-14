#pragma once

#include <QPainterPath>

class QPainter;

namespace DesktopPetUi {
QPainterPath cropPath(const QRectF& rect, bool circular);
void drawCheckerboard(QPainter& painter, const QRectF& rect, bool circular);
} // namespace DesktopPetUi
