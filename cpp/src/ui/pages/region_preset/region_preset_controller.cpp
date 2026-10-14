#include "ui/pages/region_preset/region_preset_controller.h"
#include "features/region_preset/region_preset_config.h"
#include "config/config_validation.h"
#include <algorithm>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
namespace
{
QString nextRegionPresetName(const QList<RegionPreset> &presets)
{
    for (int number = 1;; ++number) {
        const QString candidate = QStringLiteral("区域 %1").arg(number);
        const bool exists =
            std::any_of(presets.cbegin(), presets.cend(), [&candidate](const RegionPreset &preset) {
                return preset.name.compare(candidate, Qt::CaseInsensitive) == 0;
            });
        if (!exists) {
            return candidate;
        }
    }
}
} // namespace
RegionPresetController::RegionPresetController(AppConfig &config, QWidget *host)
    : QObject(host), config_(config), host_(host)
{
}
void RegionPresetController::saveRegionPreset(const QRect &region, int statisticsIntervalMs,
                                              bool confirmReplacementOnDuplicate)
{
    QString defaultName = nextRegionPresetName(config_.regionPresets);
    QString name;
    auto existingPreset = config_.regionPresets.end();
    while (true) {
        bool accepted = false;
        name =
            QInputDialog::getText(host_, QStringLiteral("保存区域预设"), QStringLiteral("预设名称"),
                                  QLineEdit::Normal, defaultName, &accepted)
                .trimmed();
        if (!accepted) {
            return;
        }
        if (name.isEmpty()) {
            QMessageBox::warning(host_, QStringLiteral("无法保存预设"),
                                 QStringLiteral("预设名称不能为空。"));
            defaultName = nextRegionPresetName(config_.regionPresets);
            continue;
        }

        existingPreset =
            std::find_if(config_.regionPresets.begin(), config_.regionPresets.end(),
                         [&name](const RegionPreset &preset) {
                             return preset.name.compare(name, Qt::CaseInsensitive) == 0;
                         });
        if (existingPreset != config_.regionPresets.end() && confirmReplacementOnDuplicate) {
            const auto answer = QMessageBox::question(
                host_, QStringLiteral("匹配到重名预设"),
                QStringLiteral("匹配到重名预设，是否要替换区域？注意：替换后，原配置会丢失"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                return;
            }
        }
        break;
    }
    const RegionPreset replacement{
        existingPreset == config_.regionPresets.end() ? name : existingPreset->name,
        region,
        existingPreset != config_.regionPresets.end() && confirmReplacementOnDuplicate
            ? existingPreset->statisticsIntervalMs
            : statisticsIntervalMs,
    };
    const bool replacing = existingPreset != config_.regionPresets.end();
    if (replacing) {
        const QString existingName = existingPreset->name;
        emit presetReplaced(existingName, replacement);
        *existingPreset = replacement;
    } else if (!addRegionPreset(config_, replacement)) {
        QMessageBox::warning(host_, QStringLiteral("无法保存预设"),
                             QStringLiteral("预设名称已存在。"));
        return;
    }
    emit presetsChanged();
    emit persistRequested();
    emit statusMessage(
        replacing ? QStringLiteral("区域预设已更新") : QStringLiteral("已新增区域预设"), 3000);
}
void RegionPresetController::renameRegionPresetAt(int row)
{
    if (row < 0 || row >= config_.regionPresets.size()) {
        return;
    }
    bool accepted = false;
    const QString name =
        QInputDialog::getText(host_, QStringLiteral("重命名区域预设"), QStringLiteral("预设名称"),
                              QLineEdit::Normal, config_.regionPresets[row].name, &accepted)
            .trimmed();
    if (!accepted || name.isEmpty()) {
        return;
    }
    if (!renameRegionPreset(config_, row, name)) {
        QMessageBox::warning(host_, QStringLiteral("无法重命名"),
                             QStringLiteral("预设名称已存在。"));
        return;
    }
    emit presetsChanged();
    emit persistRequested();
}
void RegionPresetController::deleteRegionPresetAt(int row)
{
    if (row < 0 || row >= config_.regionPresets.size()) {
        return;
    }
    if (QMessageBox::question(
            host_, QStringLiteral("删除区域预设"),
            QStringLiteral("确定删除“%1”？").arg(config_.regionPresets[row].name)) !=
        QMessageBox::Yes) {
        return;
    }
    if (removeRegionPreset(config_, row)) {
        emit presetsChanged();
        emit persistRequested();
    }
}
void RegionPresetController::updateRegionPresetInterval(int row, int milliseconds)
{
    if (row < 0 || row >= config_.regionPresets.size() ||
        !isValidStatisticsIntervalMs(milliseconds)) {
        return;
    }
    RegionPreset &preset = config_.regionPresets[row];
    preset.statisticsIntervalMs = milliseconds;
    emit intervalChanged(preset.name, milliseconds);
    emit presetsChanged();
    emit replicaPersistRequested();
}
