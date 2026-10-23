#include "ui/main_window.h"

#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>
#include "ui/pages/home/home_page.h"
#include "ui/pages/replica_list/replica_list_page.h"
#include "ui/pages/quick_switch/quick_switch_controller.h"
#include "ui/page_partition_page.h"
#include "ui/replica_window.h"

bool MainWindow::loadConfiguration(QString& loadError)
{
    const bool loaded = configStore_.load(config_, &loadError);
    configurationWritable_ = loaded;
    if (config_.defaultPresetSlot >= 1 && config_.defaultPresetSlot <= kPresetCount
        && config_.presets[config_.defaultPresetSlot - 1].hasConfig) {
        const PresetConfig& preset = config_.presets[config_.defaultPresetSlot - 1];
        config_.replicas = preset.replicas;
    }
    return loaded;
}

void MainWindow::initializePersistenceTimer()
{
    persistTimer_ = new QTimer(this);
    persistTimer_->setSingleShot(true);
    persistTimer_->setInterval(500);
    connect(persistTimer_, &QTimer::timeout, this, [this] {
        if (!closing_) {
            persistConfiguration();
        }
    });
}

void MainWindow::showConfigurationLoadWarning(const QString& loadError)
{
    QTimer::singleShot(0, this, [this, loadError] {
        QMessageBox::warning(
            this,
            QStringLiteral("配置未加载"),
            loadError + QStringLiteral("\n原文件未被覆盖。"));
    });
}

void MainWindow::refreshConfigurationPages()
{
    gateway_.setAutomaticActivationMouseQuietPeriodMs(
        config_.automaticActivationMouseQuietPeriodMs);
    refreshRegionPresets();
    refreshColorRules();
    quickSwitchController_->refresh();
    pagePartitionPage_->setPartitionConfig(config_.pagePartition);
    for (auto* window : windows_) {
        window->setAutomaticActivationEnabled(
            runtimePolicy_.automaticActivation(config_.activateSourceOnColorLogicTrigger));
        window->setAutomaticActivationCooldownMs(
            config_.automaticActivationCooldownMs);
    }
    homePage_->setConfig(config_);
    refreshReplicaFilters();
    refreshReplicaGroups();
}

void MainWindow::persistConfiguration()
{
    if (!configurationWritable_) {
        statusBar()->showMessage(QStringLiteral("配置加载失败，自动保存已暂停；修复配置后请重新启动。"));
        return;
    }
    collectReplicaConfigs();
    QString error;
    if (!configStore_.save(config_, &error)) {
        QMessageBox::warning(this, QStringLiteral("配置未保存"), error);
    }
}

ReplicaConfig MainWindow::replicaConfigFor(ReplicaId id, ReplicaConfig config) const
{
    return replicaController_.contains(id) ? replicaController_.desiredConfig(id) : config;
}

void MainWindow::collectReplicaConfigs()
{
    config_.replicas = replicaController_.snapshot();
}

void MainWindow::schedulePersistReplicas()
{
    if (closing_ || restoringReplicas_) {
        return;
    }
    persistTimer_->start();
}
