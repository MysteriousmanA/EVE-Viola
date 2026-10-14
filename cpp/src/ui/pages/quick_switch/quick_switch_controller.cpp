#include "ui/pages/quick_switch/quick_switch_controller.h"
#include "ui/pages/quick_switch/quick_switch_page.h"
#include "ui/account_switch_controller.h"
#include "features/quick_switch/quick_switch_service.h"
#include "platform/window_catalog.h"
#include "runtime/runtime_logger.h"
#include <exception>
#include <QJsonObject>
QuickSwitchController::QuickSwitchController(QuickSwitchPage &page, AppConfig &config,
                                             QuickSwitchService &service, WindowCatalog &catalog,
                                             AccountSwitchController &accounts)
    : QObject(&page), quickSwitchPage_(&page), config_(config), quickSwitchService_(&service),
      windowCatalog_(&catalog), accountSwitchController_(&accounts)
{
    connect(quickSwitchPage_, &QuickSwitchPage::captureRequested, quickSwitchService_,
            [this](bool enabled) {
                accountSwitchController_->setInputCaptureActive(enabled);
                if (enabled) {
                    accountSwitchController_->cancelRadialSwitchInteraction();
                    quickSwitchService_->beginCapture();
                } else {
                    quickSwitchService_->cancelCapture();
                }
            });
    connect(quickSwitchService_, &QuickSwitchService::inputCaptured, quickSwitchPage_,
            &QuickSwitchPage::captureInput);
    connect(quickSwitchPage_, &QuickSwitchPage::keysChanged, this,
            [this](const QStringList &previousKeys, const QStringList &nextKeys) {
                if (previousKeys == config_.previousClientKeys &&
                    nextKeys == config_.nextClientKeys) {
                    return;
                }
                config_.previousClientKeys = previousKeys;
                config_.nextClientKeys = nextKeys;
                quickSwitchService_->updateKeys(previousKeys, nextKeys);
                emit persistRequested();
            });
    connect(quickSwitchPage_, &QuickSwitchPage::radialSwitchKeyChanged, this,
            [this](const QString &key) {
                if (key == config_.radialSwitchKey) {
                    return;
                }
                config_.radialSwitchKey = key;
                quickSwitchService_->updateRadialSwitchKey(key);
                emit persistRequested();
            });
    connect(quickSwitchPage_, &QuickSwitchPage::radialAccountConfigsChanged, this,
            [this](const QList<RadialAccountConfig> &accounts) {
                if (accounts == config_.radialAccounts) {
                    return;
                }
                config_.radialAccounts = accounts;
                emit persistRequested();
            });
    connect(quickSwitchPage_, &QuickSwitchPage::radialAccountsRefreshRequested, this, [this] {
        try {
            const QList<WindowInfo> clients = windowCatalog_->windows();
            QStringList titles;
            titles.reserve(clients.size());
            for (const auto &client : clients) {
                titles.append(client.title);
            }
            quickSwitchPage_->setRadialAccounts(titles, config_.radialAccounts);
        } catch (const std::exception &error) {
            RuntimeLogger::writeEvent(
                QStringLiteral("radial_accounts_refresh_failed"),
                QJsonObject{{QStringLiteral("reason"), QString::fromUtf8(error.what())}});
        }
    });
    connect(quickSwitchPage_, &QuickSwitchPage::radialAccountPreviewRequested,
            accountSwitchController_, &AccountSwitchController::showRadialAccountPositionPreview);
}
void QuickSwitchController::refresh()
{
    quickSwitchPage_->setKeys(config_.previousClientKeys, config_.nextClientKeys);
    quickSwitchPage_->setRadialSwitchKey(config_.radialSwitchKey);
    QStringList titles;
    try {
        for (const auto &client : windowCatalog_->windows()) {
            titles.append(client.title);
        }
    } catch (const std::exception &) {
    }
    quickSwitchPage_->setRadialAccounts(titles, config_.radialAccounts);
}
