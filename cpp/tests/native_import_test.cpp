#include <cstdint>

#include <QtTest>

#include "eve_viola_native.h"

class NativeImportTest final : public QObject {
    Q_OBJECT

private slots:
    void exposesExpectedApiVersion()
    {
        QCOMPARE(eve_viola_api_version(), std::uint32_t{3});
    }
};

QTEST_GUILESS_MAIN(NativeImportTest)

#include "native_import_test.moc"
