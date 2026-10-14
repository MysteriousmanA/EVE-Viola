#include "ui/desktop_pet/avatar_painting.h"

#include <QPainter>

namespace DesktopPetUi {

QPainterPath cropPath(const QRectF& rect, bool circular)
{
    QPainterPath path;
    if (circular) {
        path.addEllipse(rect);
    } else {
        path.addRect(rect);
    }
    return path;
}

void drawCheckerboard(QPainter& painter, const QRectF& rect, bool circular)
{
    painter.save();
    painter.setClipPath(cropPath(rect, circular));
    constexpr int tile = 14;
    for (int y = static_cast<int>(rect.top()); y < rect.bottom(); y += tile) {
        for (int x = static_cast<int>(rect.left()); x < rect.right(); x += tile) {
            const bool light = ((x - static_cast<int>(rect.left())) / tile
                                + (y - static_cast<int>(rect.top())) / tile)
                % 2 == 0;
            painter.fillRect(
                QRect(x, y, tile, tile),
                light ? QColor(QStringLiteral("#22333D"))
                      : QColor(QStringLiteral("#182731")));
        }
    }
    painter.restore();
}

} // namespace DesktopPetUi
