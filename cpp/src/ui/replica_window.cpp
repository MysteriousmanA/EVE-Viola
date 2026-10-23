#include "ui/replica_window.h"

#include <utility>

#include <QCloseEvent>
#include <QFont>
#include <QLabel>
#include <QPalette>
#include <QSizeGrip>
#include <QTimer>
#include <QVBoxLayout>

ReplicaWindow::ReplicaWindow(
    ReplicaId replicaId,
    NativeGateway& gateway,
    StatisticsCoordinator& statisticsCoordinator,
    AudioFeedbackManager& audioFeedbackManager,
    ClosedCallback onClosed,
    QWidget* parent)
    : QWidget(parent)
    , replicaId_(replicaId)
    , gateway_(gateway)
    , statisticsCoordinator_(statisticsCoordinator)
    , audioFeedbackManager_(audioFeedbackManager)
    , onClosed_(std::move(onClosed))
{
    setObjectName(QStringLiteral("replicaWindow"));
    setWindowTitle(QStringLiteral("EVE-Viola 小窗 #%1").arg(replicaId_));
    setWindowFlags(
        Qt::FramelessWindowHint
        | Qt::WindowStaysOnTopHint
        | Qt::Tool);
    updateMinimumSize();
    resize(360, 240);
    setFocusPolicy(Qt::StrongFocus);
    applyColorLogicBorder();

    emptyLabel_ = new QLabel(QStringLiteral("右键选择完整窗口"), this);
    emptyLabel_->setObjectName(QStringLiteral("emptyLabel"));
    emptyLabel_->setAlignment(Qt::AlignCenter);
    emptyLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    emptyLabel_->setStyleSheet(QStringLiteral("color: #91A7B3; font-size: 14px;"));

    auto* resizeGrip = new QSizeGrip(this);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 3, 3);
    layout->addWidget(emptyLabel_, 1);
    layout->addWidget(resizeGrip, 0, Qt::AlignRight | Qt::AlignBottom);

    sourceNameOverlay_ = new QLabel(this);
    sourceNameOverlay_->setObjectName(QStringLiteral("sourceNameOverlay"));
    sourceNameOverlay_->setAlignment(Qt::AlignCenter);
    sourceNameOverlay_->setAttribute(Qt::WA_TransparentForMouseEvents);
    sourceNameOverlay_->setMargin(8);
    QPalette overlayPalette = sourceNameOverlay_->palette();
    overlayPalette.setColor(QPalette::WindowText, QColor(255, 140, 0));
    sourceNameOverlay_->setPalette(overlayPalette);
    QFont overlayFont = sourceNameOverlay_->font();
    overlayFont.setBold(true);
    overlayFont.setPointSize(18);
    sourceNameOverlay_->setFont(overlayFont);
    sourceNameOverlay_->hide();

    initializeStatistics();
    initializeRegionCapture();
    initializeInput();
    initializeFlash();
    initializeSourceMonitoring();
    initializeAudioFeedback();
}

ReplicaWindow::~ReplicaWindow()
{
    cancelAdvancedLogic();
    statisticsCoordinator_.cancelFrame(replicaId_);
    gateway_.cancelAutomaticActivation(replicaId_);
    statisticsCoordinator_.remove(replicaId_);
    audioFeedbackManager_.cancelReplica(replicaId_);
}

ReplicaId ReplicaWindow::replicaId() const noexcept
{
    return replicaId_;
}

QLabel* ReplicaWindow::emptyLabel() const noexcept
{
    return emptyLabel_;
}

void ReplicaWindow::closeEvent(QCloseEvent* event)
{
    cancelAdvancedLogic();
    frameCapturePending_ = false;
    statisticsCoordinator_.cancelFrame(replicaId_);
    sourceCheckTimer_->stop();
    statisticsCoordinator_.remove(replicaId_);
    audioFeedbackManager_.cancelReplica(replicaId_);
    thumbnail_.reset();
    source_.reset();
    if (!closedNotified_) {
        closedNotified_ = true;
        if (onClosed_) {
            onClosed_(replicaId_);
        }
        emit closed(replicaId_);
    }
    event->accept();
}
