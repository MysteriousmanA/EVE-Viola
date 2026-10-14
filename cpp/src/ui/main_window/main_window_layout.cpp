#include "ui/main_window.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include "ui/expedition_timing_page.h"
#include "ui/theme.h"

namespace {

QWidget* makeScrollablePage(QWidget* page, const QString& objectName)
{
    auto* scrollArea = new QScrollArea;
    scrollArea->setObjectName(objectName);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setWidget(page);
    return scrollArea;
}

} // namespace

void MainWindow::buildWindowLayout()
{
    setWindowTitle(QStringLiteral("EVE-Viola"));
    resize(980, 680);
    setMinimumSize(720, 480);
    setStyleSheet(kAppStyle);

    auto* root = new QWidget;
    root->setObjectName(QStringLiteral("managerRoot"));
    setCentralWidget(root);
    auto* outer = new QVBoxLayout(root);
    outer->setContentsMargins(26, 24, 26, 24);
    outer->setSpacing(18);

    auto* tabs = new QTabWidget;
    tabs->setObjectName(QStringLiteral("mainTabs"));
    tabs->addTab(buildHomePage(), QStringLiteral("首页"));
    tabs->addTab(buildReplicaListPage(), QStringLiteral("小窗列表"));
    tabs->addTab(
        makeScrollablePage(buildRegionPresetPage(), QStringLiteral("regionPresetPageScroll")),
        QStringLiteral("区域预设"));
    tabs->addTab(
        makeScrollablePage(buildColorRulePage(), QStringLiteral("colorRulePageScroll")),
        QStringLiteral("图色逻辑"));
    tabs->addTab(
        makeScrollablePage(buildQuickSwitchPage(), QStringLiteral("quickSwitchPageScroll")),
        QStringLiteral("快捷切屏"));
    tabs->addTab(
        makeScrollablePage(buildPagePartitionPage(), QStringLiteral("pagePartitionPageScroll")),
        QStringLiteral("页面分区"));
    auto* expeditionTimingPage = static_cast<ExpeditionTimingPage*>(
        buildExpeditionTimingPage());
    const int expeditionTimingTab = tabs->addTab(
        makeScrollablePage(
            expeditionTimingPage, QStringLiteral("expeditionTimingPageScroll")),
        QStringLiteral("远征计时"));
    connect(
        tabs->tabBar(), &QTabBar::tabBarClicked,
        expeditionTimingPage,
        [expeditionTimingPage, expeditionTimingTab](int clickedTab) {
            if (clickedTab == expeditionTimingTab) {
                expeditionTimingPage->syncCurrentTime();
            }
        });
    tabs->addTab(buildDesktopPetPage(), QStringLiteral("摸鱼桌宠"));
    outer->addWidget(tabs);

    auto* bottomBar = new QWidget;
    bottomBar->setObjectName(QStringLiteral("bottomBar"));
    bottomBar->setMinimumHeight(48);
    auto* bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(8);
    auto* forceQuitButton = new QPushButton(QStringLiteral("强制退出"));
    forceQuitButton->setObjectName(QStringLiteral("forceQuitButton"));
    forceQuitButton->setCursor(Qt::PointingHandCursor);
    forceQuitButton->setToolTip(QStringLiteral("无论是否保留到托盘，立即退出软件并关闭所有小窗"));
    connect(forceQuitButton, &QPushButton::clicked, this, &MainWindow::forceQuit);
    bottomLayout->addWidget(forceQuitButton);
    bottomLayout->addStretch(1);
    outer->addWidget(bottomBar, 0);
}

QWidget* MainWindow::buildExpeditionTimingPage()
{
    return new ExpeditionTimingPage;
}
