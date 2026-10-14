#include <QtTest>

#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTableWidget>

#include "ui/page_partition_page.h"

class PagePartitionPageTest final : public QObject {
    Q_OBJECT

private slots:
    void rendersPersistedLinesAndStableControls()
    {
        PagePartitionConfig config;
        config.visible = false;
        config.defaultWidth = 4;
        config.defaultColor = QColor(QStringLiteral("#123456"));
        config.selectedColor = QColor(QStringLiteral("#ff00aa"));
        config.lines = {
            {QStringLiteral("horizontal-id"), QPoint(-300, 40), QPoint(500, 40),
             QColor(QStringLiteral("#0088ff")), 2, QStringLiteral("上边界")},
            {QStringLiteral("vertical-id"), QPoint(120, -50), QPoint(120, 600),
             QColor(QStringLiteral("#00cc88")), 3, QStringLiteral("侧边界")},
        };

        PagePartitionPage page(config);
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("pagePartitionTable"));
        auto* width = page.findChild<QSpinBox*>(QStringLiteral("pagePartitionDefaultWidth"));
        auto* redraw = page.findChild<QPushButton*>(QStringLiteral("redrawPagePartitionLine"));
        auto* visibility = page.findChild<QPushButton*>(
            QStringLiteral("togglePagePartitionVisibility"));
        QVERIFY(table != nullptr);
        QVERIFY(width != nullptr);
        QVERIFY(redraw != nullptr);
        QVERIFY(visibility != nullptr);
        QCOMPARE(table->rowCount(), 2);
        QCOMPARE(table->item(0, 1)->text(), QStringLiteral("水平"));
        QCOMPARE(table->item(1, 1)->text(), QStringLiteral("垂直"));
        QCOMPARE(width->value(), 4);
        QCOMPARE(visibility->text(), QStringLiteral("显示线条"));
        QVERIFY(!redraw->isEnabled());

        table->setCurrentCell(1, 0);
        QVERIFY(redraw->isEnabled());
    }

    void settingsChangesUpdateConfigurationAndEmit()
    {
        PagePartitionConfig config;
        config.visible = false;
        PagePartitionPage page(config);
        QSignalSpy changed(&page, &PagePartitionPage::configurationChanged);
        auto* width = page.findChild<QSpinBox*>(QStringLiteral("pagePartitionDefaultWidth"));
        auto* visibility = page.findChild<QPushButton*>(
            QStringLiteral("togglePagePartitionVisibility"));
        QVERIFY(width != nullptr);
        QVERIFY(visibility != nullptr);

        width->setValue(7);
        QCOMPARE(page.partitionConfig().defaultWidth, 7);
        QCOMPARE(changed.count(), 1);

        visibility->click();
        QVERIFY(page.partitionConfig().visible);
        QCOMPARE(changed.count(), 2);
        QCOMPARE(visibility->text(), QStringLiteral("隐藏线条"));
    }
};

QTEST_MAIN(PagePartitionPageTest)

#include "page_partition_page_test.moc"
