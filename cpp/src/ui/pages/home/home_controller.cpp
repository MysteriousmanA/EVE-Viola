#include "ui/pages/home/home_controller.h"
#include "config/config_validation.h"
#include "ui/pages/home/home_page.h"
#include "features/region_preset/preset_service.h"
#include <cmath>
#include <QInputDialog>
#include <QMessageBox>
#include <QLineEdit>
HomeController::HomeController(HomePage &page, AppConfig &config, QWidget *host)
    : QObject(&page), page_(page), config_(config), host_(host)
{
    connect(&page_, &HomePage::closeAllRequested, this, [this] {
        if (replicaCount_ == 0) {
            return;
        }
        if (QMessageBox::question(host_, QStringLiteral("关闭所有小窗"),
                                  QStringLiteral("确定关闭全部 %1 个小窗？").arg(replicaCount_)) ==
            QMessageBox::Yes) {
            emit closeAllRequested();
        }
    });
    connect(&page_, &HomePage::activateSourceChanged, this, [this](bool checked) {
        config_.activateSourceOnColorLogicTrigger = checked;
        emit activateSourceChanged(checked);
        emit persistRequested();
    });
    connect(&page_, &HomePage::keepInTrayChanged, this, [this](bool checked) {
        config_.keepInTray = checked;
        emit keepInTrayChanged(checked);
        emit persistRequested();
    });
    connect(&page_, &HomePage::restoreWithGameChanged, this, [this](bool checked) {
        config_.restoreWindowsWithGame = checked;
        emit restoreWithGameChanged(checked);
        emit persistRequested();
    });
    connect(&page_, &HomePage::raiseOnClickChanged, this, [this](bool checked) {
        config_.raiseReplicaOnClick = checked;
        emit raiseOnClickChanged(checked);
        emit persistRequested();
    });
    connect(&page_, &HomePage::cooldownChanged, this, [this](double seconds) {
        const int milliseconds = static_cast<int>(std::lround(seconds * 1000.0));
        if (!isValidAutomaticActivationCooldownMs(milliseconds)) {
            return;
        }
        config_.automaticActivationCooldownMs = milliseconds;
        emit cooldownChanged(milliseconds);
        emit persistRequested();
    });
    connect(&page_, &HomePage::mouseQuietPeriodChanged, this, [this](double seconds) {
        const int milliseconds = static_cast<int>(std::lround(seconds * 1000.0));
        if (!isValidAutomaticActivationMouseQuietPeriodMs(milliseconds)) {
            return;
        }
        config_.automaticActivationMouseQuietPeriodMs = milliseconds;
        emit mouseQuietPeriodChanged(milliseconds);
        emit persistRequested();
    });
    connect(&page_, &HomePage::renamePresetRequested, this, &HomeController::renamePresetAt);
    connect(&page_, &HomePage::savePresetRequested, this, &HomeController::savePresetTo);
    connect(&page_, &HomePage::loadPresetRequested, this, &HomeController::loadPreset);
    connect(&page_, &HomePage::deletePresetRequested, this, &HomeController::deletePresetAt);
    connect(&page_, &HomePage::defaultPresetRequested, this, &HomeController::setDefaultPresetSlot);
}
void HomeController::savePresetTo(int slot)
{
    if (slot < 0 || slot >= kPresetCount) {
        return;
    }
    emit snapshotRequested();
    static_cast<void>(PresetService::save(config_, slot, config_.replicas));
    emit persistRequested();
    page_.setPresets(config_);
}
void HomeController::renamePresetAt(int slot)
{
    if (slot < 0 || slot >= kPresetCount) {
        return;
    }
    bool accepted = false;
    const QString name =
        QInputDialog::getText(host_, QStringLiteral("重命名预设"), QStringLiteral("预设名称"),
                              QLineEdit::Normal, config_.presets[slot].name, &accepted)
            .trimmed();
    if (!accepted || name.isEmpty()) {
        return;
    }
    config_.presets[slot].name = name;
    emit persistRequested();
    page_.setPresets(config_);
}
void HomeController::deletePresetAt(int slot)
{
    if (slot < 0 || slot >= kPresetCount) {
        return;
    }
    if (QMessageBox::question(host_, QStringLiteral("删除预设"),
                              QStringLiteral("确定删除“%1”？")
                                  .arg(config_.presets[slot].name.isEmpty()
                                           ? QStringLiteral("预设 %1").arg(slot + 1)
                                           : config_.presets[slot].name)) != QMessageBox::Yes) {
        return;
    }
    static_cast<void>(PresetService::remove(config_, slot));
    emit persistRequested();
    page_.setPresets(config_);
}
void HomeController::setDefaultPresetSlot(int slot)
{
    if (slot < 0 || slot >= kPresetCount) {
        return;
    }
    config_.defaultPresetSlot = slot + 1;
    emit persistRequested();
    page_.setPresets(config_);
}
void HomeController::loadPreset(int slot)
{
    if (slot < 0 || slot >= kPresetCount || !config_.presets[slot].hasConfig) {
        return;
    }
    if (!PresetService::load(config_, slot)) {
        return;
    }
    emit rebuildRequested();
    emit persistRequested();
    page_.setPresets(config_);
}
