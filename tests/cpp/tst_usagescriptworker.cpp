// SPDX-License-Identifier: GPL-2.0-or-later

#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTest>

#ifndef USAGE_SCRIPT_WORKER_TEST_PATH
#error USAGE_SCRIPT_WORKER_TEST_PATH is required
#endif

class UsageScriptWorkerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void extractsRequestConfiguration();
    void executesResponseExtractor();

private:
    static QJsonObject runTask(const QJsonObject &task);
};

QJsonObject UsageScriptWorkerTest::runTask(const QJsonObject &task)
{
    QProcess process;
    process.setProgram(QStringLiteral(USAGE_SCRIPT_WORKER_TEST_PATH));
    process.start();
    if (!process.waitForStarted(2000)) {
        return {};
    }
    process.write(QJsonDocument(task).toJson(QJsonDocument::Compact));
    process.closeWriteChannel();
    if (!process.waitForFinished(5000)
        || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return {};
    }
    const QJsonDocument result = QJsonDocument::fromJson(process.readAllStandardOutput());
    return result.isObject() ? result.object() : QJsonObject{};
}

void UsageScriptWorkerTest::extractsRequestConfiguration()
{
    const QString script = QStringLiteral(
        "({ request: { url: 'https://example.com/usage', method: 'GET', headers: {} }, "
        "extractor: function(response) { return { used: response.used, limit: response.limit }; } })");
    const QJsonObject result = runTask(
        {{QStringLiteral("mode"), QStringLiteral("request")},
         {QStringLiteral("script"), script}});

    QVERIFY(!result.isEmpty());
    QVERIFY(result.value(QStringLiteral("ok")).toBool());
    QCOMPARE(result.value(QStringLiteral("value")).toObject().value(QStringLiteral("url")).toString(),
             QStringLiteral("https://example.com/usage"));
}

void UsageScriptWorkerTest::executesResponseExtractor()
{
    const QString script = QStringLiteral(
        "({ request: { url: 'https://example.com/usage', method: 'GET', headers: {} }, "
        "extractor: function(response) { return { used: response.data.used, limit: response.data.limit }; } })");
    const QJsonObject result = runTask(
        {{QStringLiteral("mode"), QStringLiteral("extract")},
         {QStringLiteral("script"), script},
         {QStringLiteral("response"),
          QJsonObject{{QStringLiteral("data"),
                       QJsonObject{{QStringLiteral("used"), 37},
                                   {QStringLiteral("limit"), 100}}}}}});

    QVERIFY(!result.isEmpty());
    QVERIFY(result.value(QStringLiteral("ok")).toBool());
    const QJsonObject value = result.value(QStringLiteral("value")).toObject();
    QCOMPARE(value.value(QStringLiteral("used")).toInt(), 37);
    QCOMPARE(value.value(QStringLiteral("limit")).toInt(), 100);
}

QTEST_GUILESS_MAIN(UsageScriptWorkerTest)

#include "tst_usagescriptworker.moc"
