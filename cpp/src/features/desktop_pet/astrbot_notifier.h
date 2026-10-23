#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>

struct AstrBotSettings {
    bool enabled{false};
    QString serverUrl{QStringLiteral("http://127.0.0.1:6185")};
    QString apiKey;
    QString umo;
    int cooldownSeconds{30};

    static AstrBotSettings fromJson(const QJsonObject& object);
    QJsonObject toJson() const;
    QUrl messageUrl() const;
    QString validationError() const;
};

class AstrBotNotifier final : public QObject {
    Q_OBJECT

public:
    explicit AstrBotNotifier(QObject* parent = nullptr);
    void cancelPending();
    // An empty deduplication key is used for explicit test messages.
    bool sendMessage(const AstrBotSettings& settings, const QString& message,
                     const QString& deduplicationKey = {});

signals:
    void deliveryFinished(bool success, const QString& detail);

private:
    void postMessage(const QUrl& url, const QByteArray& apiKey, const QByteArray& payload,
                     bool tryAlternatePath, qint64 deadline);
    QNetworkAccessManager network_;
    QElapsedTimer clock_;
    QHash<QString, qint64> lastAttempts_;
    int pending_{};
};
