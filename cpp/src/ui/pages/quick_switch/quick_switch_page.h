#pragma once

#include <array>

#include <QWidget>

#include "config/config_types.h"

class QPushButton;
class QComboBox;
class QCheckBox;
class QHBoxLayout;
class QLabel;
class QTableWidget;

class QuickSwitchPage final : public QWidget
{
    Q_OBJECT

  public:
    explicit QuickSwitchPage(QWidget *parent = nullptr);
    ~QuickSwitchPage() override;
    void setKeys(const QStringList &previousKeys, const QStringList &nextKeys);
    void setRadialSwitchKey(const QString &key);
    void setRadialAccounts(const QStringList &liveWindowTitles,
                           const QList<RadialAccountConfig> &accountConfigs);
    void setRadialAccountPosition(const QString &windowTitle, int targetIndex);
    void captureInput(const QString &key);

  signals:
    void keysChanged(const QStringList &previousKeys, const QStringList &nextKeys);
    void radialSwitchKeyChanged(const QString &key);
    void radialAccountConfigsChanged(const QList<RadialAccountConfig> &accountConfigs);
    void radialAccountsRefreshRequested();
    void radialAccountPreviewRequested(const QString &windowTitle);
    void captureRequested(bool enabled);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void beginListening(bool previousDirection);
    void beginListeningRadialKey();
    void finishListening();
    void bindKey(bool previousDirection, const QString &key);
    void removeCustomKey(bool previousDirection, const QString &key);
    void refreshCustomKeys();
    void bindingChanged(bool previousDirection, int keyIndex, bool checked);
    void rebuildRadialAccountTable();
    void updateRadialAccountNickname(const QString &windowTitle, const QString &nickname);
    void commitRadialAccountOrder(const QStringList &visibleOrder);

    std::array<QCheckBox *, 4> previousKeyChecks_{};
    std::array<QCheckBox *, 4> nextKeyChecks_{};
    QHBoxLayout *previousCustomKeysLayout_{};
    QHBoxLayout *nextCustomKeysLayout_{};
    QPushButton *previousListenButton_{};
    QPushButton *nextListenButton_{};
    QPushButton *radialListenButton_{};
    QLabel *radialKeyLabel_{};
    QLabel *radialAccountsSummary_{};
    QTableWidget *radialAccountsTable_{};
    QPushButton *activeListenButton_{};
    bool listeningPreviousDirection_{};
    bool listeningRadialKey_{};
    QStringList previousKeys_{kDefaultPreviousClientKey};
    QStringList nextKeys_{kDefaultNextClientKey};
    QString radialSwitchKey_{kDefaultRadialSwitchKey};
    QStringList liveRadialAccountTitles_;
    QList<RadialAccountConfig> radialAccountConfigs_;
};
