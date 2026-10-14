#include "ui/replica_window.h"

#include <memory>
#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>

QMenu* ReplicaWindow::buildContextMenu()
{
    auto* menu = new QMenu;
    auto* sourceMenu = menu->addMenu(QStringLiteral("选择完整窗口"));
    try {
        const auto windows = gateway_.listEveWindows();
        if (windows.isEmpty()) {
            auto* empty = sourceMenu->addAction(QStringLiteral("未找到 EVE 窗口"));
            empty->setEnabled(false);
        }
        for (const auto& window : windows) {
            auto* action = sourceMenu->addAction(window.title);
            action->setToolTip(QStringLiteral("HWND 0x%1").arg(window.hwnd, 0, 16));
            connect(action, &QAction::triggered, this, [this, window] {
                selectSourceFull(window);
            });
        }
    } catch (const NativeException& error) {
        auto* unavailable = sourceMenu->addAction(QString::fromUtf8(error.what()));
        unavailable->setEnabled(false);
    }

    auto* captureAction = menu->addAction(QStringLiteral("截取区域"));
    captureAction->setEnabled(
        thumbnail_.has_value() && !capturingRegion_ && !sourceNameVisible_);
    connect(captureAction, &QAction::triggered, this, &ReplicaWindow::beginRegionCapture);

    auto* presetsMenu = menu->addMenu(QStringLiteral("应用区域预设"));
    presetsMenu->setEnabled(source_.has_value() && !regionPresets_.isEmpty());
    for (const auto& preset : regionPresets_) {
        auto* action = presetsMenu->addAction(preset.name);
        action->setToolTip(
            QStringLiteral("%1×%2 @ (%3,%4)")
                .arg(preset.region.width())
                .arg(preset.region.height())
                .arg(preset.region.x())
                .arg(preset.region.y()));
        connect(action, &QAction::triggered, this, [this, preset] {
            applyRegionPreset(preset);
        });
    }

    menu->addSeparator();
    auto* closeAction = menu->addAction(QStringLiteral("关闭小窗"));
    connect(closeAction, &QAction::triggered, this, &QWidget::close);
    return menu;
}

void ReplicaWindow::contextMenuEvent(QContextMenuEvent* event)
{
    const std::unique_ptr<QMenu> menu(buildContextMenu());
    menu->exec(event->globalPos());
}
