#include <QtTest>

#include <QApplication>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QTemporaryDir>
#include <QTimer>
#include <QWheelEvent>

#include "features/desktop_pet/desktop_pet_settings_store.h"
#include "ui/desktop_pet/avatar_crop_widget.h"
#include "ui/desktop_pet/floating_pet_window.h"
#include "ui/desktop_pet_page.h"

namespace {

FloatingPetWindow* findPetWindow()
{
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (auto* pet = qobject_cast<FloatingPetWindow*>(widget)) {
            return pet;
        }
    }
    return nullptr;
}

void sendWheel(FloatingPetWindow& pet, int delta)
{
    const QPoint position(pet.width() / 2, 64);
    QWheelEvent event(QPointF(position), QPointF(pet.mapToGlobal(position)),
                     QPoint{}, QPoint(0, delta), Qt::NoButton, Qt::NoModifier,
                     Qt::NoScrollPhase, false);
    QApplication::sendEvent(&pet, &event);
}

} // namespace

class DesktopPetWidgetsTest final : public QObject {
    Q_OBJECT

private slots:
    void cropProducesCircularAvatarAndRespectsOffsets()
    {
        AvatarCropWidget crop;
        QVERIFY(crop.croppedImage().isNull());
        QImage source(400, 200, QImage::Format_ARGB32_Premultiplied);
        source.fill(Qt::red);
        {
            QPainter painter(&source);
            painter.fillRect(200, 0, 200, 200, Qt::blue);
        }
        QSignalSpy changed(&crop, &AvatarCropWidget::cropChanged);
        crop.setImage(source);
        QCOMPARE(changed.count(), 1);
        crop.setNormalizedOffset(QPointF(1, 0));
        const QImage left = crop.croppedImage();
        QCOMPARE(left.size(), QSize(256, 256));
        QCOMPARE(left.pixelColor(128, 128), QColor(Qt::red));
        QCOMPARE(left.pixelColor(0, 0).alpha(), 0);
        crop.setNormalizedOffset(QPointF(-1, 0));
        QCOMPARE(crop.croppedImage().pixelColor(128, 128), QColor(Qt::blue));
        crop.setZoomPercent(999);
        QCOMPARE(crop.zoomPercent(), 300);
        crop.setNormalizedOffset(QPointF(9, -9));
        QCOMPARE(crop.normalizedOffset(), QPointF(1, -1));
    }

    void wheelAccumulatesPartialStepsAndPreservesAvatarCenter()
    {
        FloatingPetWindow pet;
        pet.move(200, 100);
        const QPoint center = pet.mapToGlobal(QPoint(pet.width() / 2, 64));
        QSignalSpy resized(&pet, &FloatingPetWindow::sizeChanged);
        sendWheel(pet, 60);
        QCOMPARE(resized.count(), 0);
        sendWheel(pet, 60);
        QCOMPARE(resized.count(), 1);
        QCOMPARE(resized.first().first().toInt(), 140);
        QCOMPARE(pet.mapToGlobal(QPoint(pet.width() / 2, 70)), center);
        sendWheel(pet, 12000);
        QCOMPARE(resized.last().first().toInt(), 192);
        sendWheel(pet, -12000);
        QCOMPARE(resized.last().first().toInt(), 72);
    }

    void messageFitsBubbleAndOnlyTemporarilyRevealsHiddenPet()
    {
        FloatingPetWindow pet;
        auto* bubble = pet.findChild<QLabel*>(QStringLiteral("desktopPetSpeechBubble"));
        QVERIFY(bubble);
        QSignalSpy autoHidden(&pet, &FloatingPetWindow::autoHiddenByMessage);
        QSignalSpy expired(&pet, &FloatingPetWindow::messageExpired);
        const QString message = QStringLiteral("这是需要完整显示的桌宠提示。").repeated(8);
        pet.showMessage(message);
        QVERIFY(pet.isVisible());
        QVERIFY(bubble->isVisible());
        QCOMPARE(bubble->text(), message);
        QVERIFY(bubble->height() >= bubble->heightForWidth(bubble->width()));
        QVERIFY(pet.mask().contains(bubble->geometry().center()));
        auto* timer = pet.findChild<QTimer*>();
        QVERIFY(timer);
        timer->stop();
        QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
        QVERIFY(!pet.isVisible());
        QCOMPARE(autoHidden.count(), 1);
        QCOMPARE(expired.count(), 1);

        pet.show();
        pet.showMessage(QStringLiteral("已启用的桌宠提示结束后继续显示"));
        timer->stop();
        QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
        QVERIFY(pet.isVisible());
        QVERIFY(!bubble->isVisible());
        QCOMPARE(autoHidden.count(), 1);
    }

    void ruleMessagesMergeAccountsAndKeepDisabledSetting()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DesktopPetSettingsStore store(directory.path());
        QImage image(64, 64, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::green);
        QVERIFY(store.saveSourceImage(image));
        const QString message = QStringLiteral("条件已经满足");
        QVERIFY(store.save(QJsonObject{
            {QStringLiteral("enabled"), false},
            {QStringLiteral("triggerMessages"), QJsonObject{
                {QStringLiteral("1"), message}, {QStringLiteral("2"), message}}}}));
        {
            DesktopPetPage page(directory.path());
            ColorRule first;
            first.number = 1;
            ColorRule second;
            second.number = 2;
            page.setColorRules({first, second});
            auto* pet = findPetWindow();
            QVERIFY(pet);
            QVERIFY(!pet->isVisible());
            page.showRuleMessage(1, QStringLiteral("EVE - Alpha"));
            page.showRuleMessage(2, QStringLiteral("EVE - Beta"));
            page.showRuleMessage(1, QStringLiteral("EVE - Alpha"));
            auto* bubble = pet->findChild<QLabel*>(QStringLiteral("desktopPetSpeechBubble"));
            QVERIFY(bubble);
            QCOMPARE(bubble->text(), QStringLiteral("Alpha、Beta：条件已经满足"));
            QSignalSpy dismissed(&page, &DesktopPetPage::accountWheelDismissRequested);
            auto* timer = pet->findChild<QTimer*>();
            QVERIFY(timer);
            timer->stop();
            QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
            QCOMPARE(dismissed.count(), 1);
            QVERIFY(!pet->isVisible());
        }
        const auto saved = store.load();
        QVERIFY(saved.loaded);
        QVERIFY(!saved.settings.value(QStringLiteral("enabled")).toBool());
        QCOMPARE(saved.settings.value(QStringLiteral("triggerMessages")).toObject().size(), 2);
    }
};

QTEST_MAIN(DesktopPetWidgetsTest)
#include "desktop_pet_widgets_test.moc"
