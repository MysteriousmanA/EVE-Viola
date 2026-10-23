#include "features/desktop_pet/astrbot_notifier.h"

#include <algorithm>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

AstrBotSettings AstrBotSettings::fromJson(const QJsonObject& object)
{
    AstrBotSettings settings;
    settings.enabled = object.value(QStringLiteral("enabled")).toBool(false);
    settings.serverUrl = object.value(QStringLiteral("serverUrl")).toString(settings.serverUrl);
    settings.apiKey = object.value(QStringLiteral("apiKey")).toString();
    settings.umo = object.value(QStringLiteral("umo")).toString();
    settings.cooldownSeconds = std::clamp(
        object.value(QStringLiteral("cooldownSeconds")).toInt(30), 0, 600);
    return settings;
}

QJsonObject AstrBotSettings::toJson() const
{
    return {{QStringLiteral("enabled"), enabled},
            {QStringLiteral("serverUrl"), serverUrl.trimmed()},
            {QStringLiteral("apiKey"), apiKey.trimmed()},
            {QStringLiteral("umo"), umo.trimmed()},
            {QStringLiteral("cooldownSeconds"), cooldownSeconds}};
}

QUrl AstrBotSettings::messageUrl() const
{
    QUrl url(serverUrl.trimmed(), QUrl::StrictMode);
    QString path = url.path();
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    if (!path.endsWith(QStringLiteral("/api/v1/im/message"))
        && !path.endsWith(QStringLiteral("/api/v1/im/messages"))) {
        path += QStringLiteral("/api/v1/im/message");
    }
    url.setPath(path);
    return url;
}

QString AstrBotSettings::validationError() const
{
    const QUrl url(serverUrl.trimmed(), QUrl::StrictMode);
    if (!url.isValid() || url.host().isEmpty()
        || (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https"))
        || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()) {
        return QStringLiteral("请填写有效的 AstrBot HTTP/HTTPS 服务地址，不含账号密码、查询参数或片段。");
    }
    if (apiKey.trimmed().isEmpty() || apiKey.contains(QLatin1Char('\r'))
        || apiKey.contains(QLatin1Char('\n'))) {
        return QStringLiteral("请填写有效的 AstrBot API Key，并授予 im 权限。");
    }
    const auto parts = umo.trimmed().split(QLatin1Char(':'));
    if (parts.size() < 3 || parts[0].trimmed().isEmpty()
        || parts[1].trimmed().isEmpty() || parts.mid(2).join(QLatin1Char(':')).trimmed().isEmpty()) {
        return QStringLiteral("请填写接收会话 UMO，可在目标私聊或群聊中发送 /sid 获取。");
    }
    return {};
}

AstrBotNotifier::AstrBotNotifier(QObject* parent)
    : QObject(parent)
{
    clock_.start();
}

void AstrBotNotifier::cancelPending()
{
    const auto replies = network_.findChildren<QNetworkReply*>();
    for (auto* reply : replies) {
        if (!reply->isFinished()) { reply->abort(); }
    }
}

bool AstrBotNotifier::sendMessage(const AstrBotSettings& settings, const QString& message,
                                 const QString& deduplicationKey)
{
    const QString error = settings.validationError();
    if (!error.isEmpty() || message.trimmed().isEmpty()) {
        emit deliveryFinished(false, error.isEmpty() ? QStringLiteral("消息文案为空。") : error);
        return false;
    }
    const qint64 now = clock_.elapsed();
    // Bound the deduplication history even when source windows are repeatedly recreated.
    for (auto it = lastAttempts_.begin(); it != lastAttempts_.end();) {
        if (now - it.value() >= 600000) {
            it = lastAttempts_.erase(it);
        } else {
            ++it;
        }
    }
    const QString key = settings.messageUrl().toString() + QChar(0x1f)
        + settings.umo.trimmed() + QChar(0x1f) + deduplicationKey;
    if (!deduplicationKey.isEmpty() && lastAttempts_.contains(key)
        && now - lastAttempts_.value(key) < std::clamp(settings.cooldownSeconds, 0, 600) * 1000) {
        return false;
    }
    if (pending_ >= 4) {
        emit deliveryFinished(false, QStringLiteral("AstrBot 请求繁忙，本次推送已跳过。"));
        return false;
    }
    if (!deduplicationKey.isEmpty()) {
        if (lastAttempts_.size() >= 1024) {
            lastAttempts_.clear();
        }
        lastAttempts_.insert(key, now);
    }
    const QJsonObject payload{
        {QStringLiteral("umo"), settings.umo.trimmed()},
        {QStringLiteral("message"), QJsonArray{QJsonObject{
            {QStringLiteral("type"), QStringLiteral("plain")},
            {QStringLiteral("text"), message}}}}};
    postMessage(settings.messageUrl(), settings.apiKey.trimmed().toUtf8(),
                QJsonDocument(payload).toJson(QJsonDocument::Compact), true, now + 10000);
    return true;
}

void AstrBotNotifier::postMessage(const QUrl& url, const QByteArray& apiKey,
                                 const QByteArray& payload, bool tryAlternatePath, qint64 deadline)
{
    const int remaining = static_cast<int>(qMax(qint64(1), deadline - clock_.elapsed()));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", "Bearer " + apiKey);
    // Redirects must never forward the API key to another address.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(remaining);
    QNetworkReply* reply = network_.post(request, payload);
    ++pending_;
    // Also cap total request time when a server keeps a connection alive indefinitely.
    QTimer::singleShot(remaining, reply, [reply] {
        if (!reply->isFinished()) {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, url, apiKey, payload, tryAlternatePath, deadline] {
        --pending_;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // AstrBot's original OpenAPI uses /message; newer REST versions use /messages.
        // Only try the alternate route when this route is unavailable, never on delivery errors.
        if (tryAlternatePath && (status == 404 || status == 405) && clock_.elapsed() < deadline) {
            QUrl alternate = url;
            QString path = url.path();
            if (path.endsWith(QStringLiteral("/messages"))) {
                path.chop(1);
            } else {
                path += QLatin1Char('s');
            }
            alternate.setPath(path);
            reply->deleteLater();
            postMessage(alternate, apiKey, payload, false, deadline);
            return;
        }
        const QJsonObject body = QJsonDocument::fromJson(
            reply->isReadable() ? reply->readAll() : QByteArray{}).object();
        const QString result = body.value(QStringLiteral("status")).toString();
        const bool success = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300
            && (result == QStringLiteral("ok") || result == QStringLiteral("success"));
        QString detail;
        if (success) {
            detail = QStringLiteral("AstrBot 已接受消息，请在目标会话确认接收。");
        } else if (status == 401) {
            detail = QStringLiteral("AstrBot 身份验证失败（HTTP 401）。请填写 AstrBot 管理后台地址及其“设置 → OpenAPI”创建的 API Key；不要填写模型服务的 Base URL 或密钥。");
        } else if (status == 403) {
            detail = QStringLiteral("AstrBot 拒绝访问（HTTP 403）。请确认 OpenAPI API Key 已授予 im 权限；若经过反向代理，也请检查代理的访问限制。");
        } else if (status > 0) {
            detail = QStringLiteral("AstrBot 发送失败（HTTP %1），请检查服务版本、UMO 和平台连接。").arg(status);
        } else {
            detail = QStringLiteral("AstrBot 连接失败或请求超时，请检查地址、网络和服务状态。");
        }
        reply->deleteLater();
        emit deliveryFinished(success, detail);
    });
}
