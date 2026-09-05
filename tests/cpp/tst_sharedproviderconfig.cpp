// SPDX-License-Identifier: GPL-2.0-or-later

#include "sharedproviderconfig.h"

#include <QSignalSpy>
#include <QTest>
#include <QUuid>

class SharedProviderConfigTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void savesAndReloadsValidProviders();
    void rejectsInvalidProvidersWithoutOverwriting();
    void notifiesOtherInstances();
};

const QString validProviders = QStringLiteral(
    R"([{"id":"codex","plans":[{"id":"weekly"}]}])");

QString temporaryConfigName()
{
    return QStringLiteral("aiquotapilot-test-%1rc")
        .arg(QUuid::createUuid().toString(QUuid::Id128));
}

void SharedProviderConfigTest::savesAndReloadsValidProviders()
{
    const QString path = temporaryConfigName();

    SharedProviderConfig writer(path);
    QVERIFY(writer.ensure(validProviders));
    QCOMPARE(writer.providers(), validProviders);
    QVERIFY(writer.ensure(QStringLiteral(
        R"([{"id":"minimax","plans":[]}])")));
    QCOMPARE(writer.providers(), validProviders);

    SharedProviderConfig reader(path);
    QCOMPARE(reader.providers(), validProviders);
}

void SharedProviderConfigTest::rejectsInvalidProvidersWithoutOverwriting()
{
    SharedProviderConfig store(temporaryConfigName());
    QVERIFY(store.save(validProviders));

    QVERIFY(!store.save(QStringLiteral("{broken")));
    QVERIFY(!store.save(QStringLiteral("{}")));
    QVERIFY(!store.save(QStringLiteral(R"([{"id":"codex"}])")));
    QCOMPARE(store.providers(), validProviders);
}

void SharedProviderConfigTest::notifiesOtherInstances()
{
    const QString path = temporaryConfigName();
    SharedProviderConfig first(path);
    SharedProviderConfig second(path);
    QSignalSpy changed(&second, &SharedProviderConfig::providersChanged);

    QVERIFY(first.save(validProviders));
    // KConfigWatcher 依赖 inotify；容器文件系统（overlayfs/tmpfs）上变更
    // 通知可能延迟甚至丢失。QTRY 轮询避免一次信号的时序脆弱。
    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 5000);
    QCOMPARE(second.providers(), validProviders);
}

QTEST_GUILESS_MAIN(SharedProviderConfigTest)

#include "tst_sharedproviderconfig.moc"
