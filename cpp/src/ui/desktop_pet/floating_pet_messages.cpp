#include "ui/desktop_pet/floating_pet_window.h"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QLabel>
#include <QScreen>
#include <QTimer>

using namespace DesktopPetUi;

void FloatingPetWindow::initializeMessageBubble()
{
    bubble_ = new QLabel(this);
    bubble_->setObjectName(QStringLiteral("desktopPetSpeechBubble"));
    bubble_->setAlignment(Qt::AlignCenter);
    bubble_->setWordWrap(true);
    bubble_->setTextFormat(Qt::PlainText);
    bubble_->setMargin(0);
    bubble_->setAttribute(Qt::WA_TransparentForMouseEvents);
    bubble_->setStyleSheet(QStringLiteral(
        "QLabel#desktopPetSpeechBubble {"
        "color: #F4F8FA; background: #142B36; border: 1px solid #62C4E8;"
        "border-radius: 10px; padding: 8px 12px; font-size: 13px; }"));
    bubble_->hide();
    messageTimer_ = new QTimer(this);
    messageTimer_->setSingleShot(true);
    connect(messageTimer_, &QTimer::timeout, this, &FloatingPetWindow::clearMessage);
}

void FloatingPetWindow::showMessage(const QString& message)
{
    if (message.trimmed().isEmpty() || mode_ == DesktopPetMode::DoNotDisturb) {
        return;
    }
    if (!isVisible()) {
        autoHideAfterMessage_ = true;
    }
    messageVisible_ = true;
    bubble_->setText(message);
    bubble_->ensurePolished();
    const QFontMetrics metrics(bubble_->font());
    int longestLineWidth = 0;
    const QStringList lines = message.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        longestLineWidth = qMax(longestLineWidth, metrics.horizontalAdvance(line));
    }

    const QPoint avatarCenter = mapToGlobal(QPoint(width() / 2, avatarSize_ / 2));
    const QScreen* screen = QGuiApplication::screenAt(avatarCenter);
    const QRect availableGeometry = screen != nullptr
        ? screen->availableGeometry()
        : QRect(0, 0, 800, 800);
    const int maxBubbleWidth = qMax(
        1, qMin(kBubbleMaximumWidth, availableGeometry.width() - 24));
    const int minBubbleWidth = qMin(kBubbleMinimumWidth, maxBubbleWidth);
    bubbleWidth_ = qBound(
        minBubbleWidth,
        longestLineWidth + kBubbleHorizontalChrome + kBubbleHorizontalSlack,
        maxBubbleWidth);

    const int maxBubbleHeight = qMax(
        44, availableGeometry.height() - avatarSize_ - 24);
    const auto measureBubbleHeight = [this, &metrics, &message, maxBubbleHeight](int width) {
        const int labelHeight = bubble_->heightForWidth(width);
        if (labelHeight > 0) {
            return labelHeight;
        }
        const QRect textBounds = metrics.boundingRect(
            QRect(0, 0,
                  qMax(1, width - kBubbleHorizontalChrome),
                  qMax(20, maxBubbleHeight - kBubbleVerticalChrome)),
            Qt::TextWordWrap | Qt::TextWrapAnywhere,
            message);
        return textBounds.height() + kBubbleVerticalChrome;
    };
    int measuredBubbleHeight = measureBubbleHeight(bubbleWidth_);
    if (measuredBubbleHeight > maxBubbleHeight && bubbleWidth_ < maxBubbleWidth) {
        bubbleWidth_ = maxBubbleWidth;
        measuredBubbleHeight = measureBubbleHeight(bubbleWidth_);
    }
    bubbleHeight_ = qBound(
        44, measuredBubbleHeight + kBubbleVerticalSlack, maxBubbleHeight);
    bubbleRegionHeight_ = qMax(kBubbleRegionHeight, bubbleHeight_ + 16);
    updateWindowRegion();
    // Changing the bubble width must not move the centered avatar on screen.
    move(avatarCenter - QPoint(width() / 2, avatarSize_ / 2));
    keepWindowOnScreen();
    bubble_->show();
    if (!isVisible()) {
        show();
    }
    raise();
    messageTimer_->start(kMessageDisplayDurationMs);
}

void FloatingPetWindow::hideByUser()
{
    messageTimer_->stop();
    autoHideAfterMessage_ = false;
    collapseMessageBubble();
    hide();
    emit hiddenByUser();
}

void FloatingPetWindow::clearMessage()
{
    messageTimer_->stop();
    if (!messageVisible_) { return; }
    collapseMessageBubble();
    if (autoHideAfterMessage_) {
        autoHideAfterMessage_ = false;
        hide();
        emit autoHiddenByMessage();
    }
    emit messageExpired();
}

void FloatingPetWindow::collapseMessageBubble()
{
    bubbleRegionHeight_ = kBubbleRegionHeight;
    messageVisible_ = false;
    bubble_->hide();
    updateWindowRegion();
    emit positionChanged(pos());
}
