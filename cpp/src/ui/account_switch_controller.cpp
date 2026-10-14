#include "ui/account_switch_controller.h"
#include <algorithm>
#include <exception>
#include <utility>
#include <QCursor>
#include <QTimer>
#include <QWidget>
#include "features/quick_switch/quick_switch_service.h"
#include "runtime/runtime_logger.h"
#include "platform/window_catalog.h"
#include "ui/radial_switch_overlay.h"
namespace {
QString displayAccountName(QString windowTitle)
{
    windowTitle = windowTitle.trimmed();
    const QString evePrefix = QStringLiteral("EVE -");
    if (windowTitle.startsWith(evePrefix, Qt::CaseInsensitive)) {
        windowTitle.remove(0, evePrefix.size());
    }
    return windowTitle.trimmed();
}

struct RadialAccountClients {
    QList<WindowInfo> orderedClients;
    QList<WindowInfo> displayClients;
};

RadialAccountClients buildRadialAccountClients(
    const QList<WindowInfo>& liveClients,
    const QList<RadialAccountConfig>& accountConfigs)
{
    QStringList liveTitles;
    liveTitles.reserve(liveClients.size());
    for (const WindowInfo& client : liveClients) {
        liveTitles.append(client.title);
    }

    const QList<QString> orderedTitles = orderedRadialAccountTitles(
        liveTitles, accountConfigs);
    RadialAccountClients result;
    result.orderedClients.reserve(orderedTitles.size());
    result.displayClients.reserve(orderedTitles.size());
    for (const QString& title : orderedTitles) {
        const auto client = std::find_if(
            liveClients.cbegin(), liveClients.cend(), [&title](const WindowInfo& candidate) {
                return candidate.title == title;
            });
        if (client == liveClients.cend()) {
            continue;
        }

        result.orderedClients.append(*client);
        WindowInfo displayClient = *client;
        const auto account = std::find_if(
            accountConfigs.cbegin(), accountConfigs.cend(),
            [&title](const RadialAccountConfig& candidate) {
                return candidate.windowTitle == title;
            });
        displayClient.title = account != accountConfigs.cend()
                && !account->nickname.trimmed().isEmpty()
            ? account->nickname.trimmed()
            : displayAccountName(title);
        result.displayClients.append(std::move(displayClient));
    }
    return result;
}

} // namespace
AccountSwitchController::AccountSwitchController(NativeGateway& gateway, WindowCatalog& catalog,
    const AppConfig& config, QuickSwitchService& service, QWidget* host)
    : QObject(host), gateway_(gateway), catalog_(catalog), config_(config), quickSwitchService_(service)
{
    radialSwitchOverlay_ = new RadialSwitchOverlay(host);
    radialSwitchHoldTimer_ = new QTimer(this);
    radialSwitchHoldTimer_->setSingleShot(true);
    radialSwitchHoldTimer_->setInterval(kRadialSwitchHoldDurationMs);
    connect(radialSwitchHoldTimer_, &QTimer::timeout, this, [this] {
        if (radialSwitchKeyHeld_ && !radialSwitchCancelled_) { showRadialSwitchWheel(); }
    });
    radialSwitchCursorTimer_ = new QTimer(this);
    radialSwitchCursorTimer_->setInterval(16);
    connect(radialSwitchCursorTimer_, &QTimer::timeout, this, &AccountSwitchController::updateRadialSwitchWheelCursor);
}
AccountSwitchController::~AccountSwitchController() { stop(); }
void AccountSwitchController::stop() { stopped_ = true; cancelRadialSwitchInteraction(); }
void AccountSwitchController::setInputCaptureActive(bool active)
{
    quickSwitchCaptureActive_ = active;
    if (active) { cancelRadialSwitchInteraction(); }
}
RadialSwitchOverlay* AccountSwitchController::overlay() const noexcept { return radialSwitchOverlay_; }
void AccountSwitchController::switchEveClient(bool next, quintptr foregroundHwnd)
{
    if (stopped_) {
        return;
    }

    try {
        const QList<WindowInfo> clients = catalog_.windows();
        if (clients.size() < 2) {
            return;
        }

        const auto current = std::find_if(
            clients.cbegin(), clients.cend(), [foregroundHwnd](const WindowInfo& client) {
                return client.hwnd == static_cast<std::uintptr_t>(foregroundHwnd);
            });
        if (current == clients.cend()) {
            return;
        }

        const qsizetype currentIndex = std::distance(clients.cbegin(), current);
        const qsizetype targetIndex =
            (currentIndex + (next ? 1 : clients.size() - 1)) % clients.size();
        static_cast<void>(gateway_.activateWindow(clients[targetIndex].hwnd));
    } catch (const std::exception& error) {
        RuntimeLogger::writeEvent(
            QStringLiteral("quick_switch_failed"),
            QJsonObject{
                {QStringLiteral("direction"), next ? QStringLiteral("next")
                                                   : QStringLiteral("previous")},
                {QStringLiteral("foreground_hwnd"), RuntimeLogger::formatWindowHandle(
                     static_cast<std::uintptr_t>(foregroundHwnd))},
                {QStringLiteral("reason"), QString::fromUtf8(error.what())},
            });
    }
}

void AccountSwitchController::beginRadialSwitchHold(
    quint64 sessionId,
    quintptr foregroundHwnd)
{
    if (stopped_ || quickSwitchCaptureActive_) {
        return;
    }
    cancelRadialSwitchInteraction();
    radialSwitchSessionId_ = sessionId;
    radialSwitchForegroundHwnd_ = foregroundHwnd;
    radialSwitchAnchor_ = QCursor::pos();
    radialSwitchKeyHeld_ = true;
    radialSwitchCancelled_ = false;
    radialSwitchHoldTimer_->start();
    radialSwitchCursorTimer_->start();
}

void AccountSwitchController::endRadialSwitchHold(
    quint64 sessionId,
    bool heldLongEnough,
    bool foregroundStillMatches,
    bool cancelled,
    quintptr foregroundHwnd)
{
    if (sessionId == 0 || sessionId != radialSwitchSessionId_) {
        return;
    }

    radialSwitchHoldTimer_->stop();
    radialSwitchCursorTimer_->stop();
    const bool validSession = radialSwitchKeyHeld_
        && heldLongEnough
        && foregroundStillMatches
        && !cancelled
        && !radialSwitchCancelled_
        && !quickSwitchCaptureActive_
        && foregroundHwnd == radialSwitchForegroundHwnd_
        && quickSwitchService_.isForegroundWindow(radialSwitchForegroundHwnd_);

    int selectedIndex = -1;
    WindowInfo selectedClient;
    bool hasSelection = false;
    if (validSession && radialSwitchWheelVisible_ && radialSwitchOverlay_ != nullptr) {
        radialSwitchOverlay_->updateCursorPosition(QCursor::pos());
        selectedIndex = radialSwitchOverlay_->selectedIndex();
        if (selectedIndex >= 0 && selectedIndex < radialSwitchClients_.size()) {
            selectedClient = radialSwitchClients_.at(selectedIndex);
            hasSelection = true;
        }
    }

    const quintptr sourceHwnd = radialSwitchForegroundHwnd_;
    cancelRadialSwitchInteraction();
    if (validSession && hasSelection) {
        activateRadialSwitchClient(selectedClient, sourceHwnd);
    }
}

void AccountSwitchController::showRadialSwitchWheel()
{
    if (!radialSwitchKeyHeld_ || radialSwitchCancelled_
        || quickSwitchCaptureActive_ || radialSwitchOverlay_ == nullptr) {
        return;
    }
    if (!quickSwitchService_.isForegroundWindow(radialSwitchForegroundHwnd_)) {
        radialSwitchCancelled_ = true;
        radialSwitchHoldTimer_->stop();
        return;
    }

    try {
        const QList<WindowInfo> clients = catalog_.windows();
        const bool sourceStillAvailable = std::any_of(
            clients.cbegin(), clients.cend(), [this](const WindowInfo& client) {
                return client.hwnd == static_cast<std::uintptr_t>(
                    radialSwitchForegroundHwnd_);
            });
        if (!sourceStillAvailable) {
            radialSwitchCancelled_ = true;
            radialSwitchHoldTimer_->stop();
            return;
        }

        const RadialAccountClients radialClients = buildRadialAccountClients(
            clients, config_.radialAccounts);
        if (radialSwitchOverlay_->showForClients(
                radialClients.displayClients, radialSwitchAnchor_)) {
            radialSwitchClients_ = radialClients.orderedClients;
            radialSwitchWheelVisible_ = true;
            radialSwitchOverlay_->updateCursorPosition(QCursor::pos());
        }
    } catch (const std::exception& error) {
        radialSwitchCancelled_ = true;
        radialSwitchHoldTimer_->stop();
        RuntimeLogger::writeEvent(
            QStringLiteral("quick_switch_radial_failed"),
            QJsonObject{{QStringLiteral("reason"), QString::fromUtf8(error.what())}});
    }
}

void AccountSwitchController::showDesktopPetAccountWheel(const QPoint& petCenter, int avatarSize)
{
    if (stopped_ || quickSwitchCaptureActive_ || radialSwitchOverlay_ == nullptr) {
        return;
    }

    if (radialSwitchOverlay_->isPetRingVisible()) {
        radialSwitchOverlay_->hide();
        return;
    }

    cancelRadialSwitchInteraction();
    try {
        const QList<WindowInfo> clients = catalog_.windows();
        RadialAccountClients radialClients = buildRadialAccountClients(
            clients, config_.radialAccounts);

        static_cast<void>(radialSwitchOverlay_->showForClients(
            radialClients.displayClients,
            petCenter,
            true,
            [this, orderedClients = std::move(radialClients.orderedClients)](int chosenIndex) {
                if (chosenIndex >= 0 && chosenIndex < orderedClients.size()) {
                    activateRadialSwitchClient(orderedClients.at(chosenIndex), 0);
                }
            },
            avatarSize));
    } catch (const std::exception& error) {
        RuntimeLogger::writeEvent(
            QStringLiteral("desktop_pet_radial_switch_failed"),
            QJsonObject{{QStringLiteral("reason"), QString::fromUtf8(error.what())}});
    }
}

void AccountSwitchController::moveDesktopPetAccountWheel(const QPoint& petCenter, int avatarSize)
{
    if (radialSwitchOverlay_ != nullptr && radialSwitchOverlay_->isPetRingVisible()) {
        radialSwitchOverlay_->movePetCenter(petCenter, avatarSize);
    }
}

void AccountSwitchController::dismissDesktopPetAccountWheel()
{
    if (radialSwitchOverlay_ != nullptr && radialSwitchOverlay_->isPetRingVisible()) {
        radialSwitchOverlay_->hide();
    }
}

void AccountSwitchController::showRadialAccountPositionPreview(const QString& windowTitle)
{
    if (stopped_ || radialSwitchOverlay_ == nullptr) {
        return;
    }
    try {
        const QList<WindowInfo> clients = catalog_.windows();
        const RadialAccountClients radialClients = buildRadialAccountClients(
            clients, config_.radialAccounts);
        const auto target = std::find_if(
            radialClients.orderedClients.cbegin(), radialClients.orderedClients.cend(),
            [&windowTitle](const WindowInfo& client) {
                return client.title == windowTitle;
            });
        if (target == radialClients.orderedClients.cend()
            || radialClients.displayClients.isEmpty()) {
            return;
        }
        const QPoint anchor = QCursor::pos();
        const bool shown = radialSwitchOverlay_->showForClients(
            radialClients.displayClients,
            anchor,
            true,
            [this, windowTitle](int chosenIndex) {
                emit accountPositionSelected(windowTitle, chosenIndex);
            });
        if (!shown) {
            return;
        }
    } catch (const std::exception& error) {
        RuntimeLogger::writeEvent(
            QStringLiteral("radial_account_preview_failed"),
            QJsonObject{{QStringLiteral("reason"), QString::fromUtf8(error.what())}});
    }
}

void AccountSwitchController::updateRadialSwitchWheelCursor()
{
    if (!radialSwitchKeyHeld_ || radialSwitchCancelled_) {
        radialSwitchCursorTimer_->stop();
        return;
    }
    if (!quickSwitchService_.isForegroundWindow(radialSwitchForegroundHwnd_)) {
        radialSwitchCancelled_ = true;
        radialSwitchHoldTimer_->stop();
        if (radialSwitchOverlay_ != nullptr) {
            radialSwitchOverlay_->hide();
        }
        radialSwitchWheelVisible_ = false;
        radialSwitchClients_.clear();
        radialSwitchCursorTimer_->stop();
        return;
    }
    if (radialSwitchWheelVisible_ && radialSwitchOverlay_ != nullptr) {
        radialSwitchOverlay_->updateCursorPosition(QCursor::pos());
    }
}

void AccountSwitchController::cancelRadialSwitchInteraction()
{
    radialSwitchHoldTimer_->stop();
    radialSwitchCursorTimer_->stop();
    if (radialSwitchOverlay_ != nullptr) {
        radialSwitchOverlay_->hide();
    }
    radialSwitchClients_.clear();
    radialSwitchWheelVisible_ = false;
    radialSwitchKeyHeld_ = false;
    radialSwitchCancelled_ = true;
    radialSwitchSessionId_ = 0;
    radialSwitchForegroundHwnd_ = 0;
}

void AccountSwitchController::activateRadialSwitchClient(
    const WindowInfo& client,
    quintptr sourceHwnd)
{
    if (stopped_
        || (sourceHwnd != 0 && !quickSwitchService_.isForegroundWindow(sourceHwnd))) {
        return;
    }
    try {
        const QList<WindowInfo> available = catalog_.windows(true);
        const bool stillAvailable = std::any_of(
            available.cbegin(), available.cend(), [&client](const WindowInfo& current) {
                return current.hwnd == client.hwnd;
            });
        if (stillAvailable) {
            static_cast<void>(gateway_.activateWindow(client.hwnd));
        }
    } catch (const std::exception& error) {
        RuntimeLogger::writeEvent(
            QStringLiteral("quick_switch_radial_failed"),
            QJsonObject{
                {QStringLiteral("target_hwnd"), RuntimeLogger::formatWindowHandle(
                     client.hwnd)},
                {QStringLiteral("reason"), QString::fromUtf8(error.what())},
            });
    }
}
