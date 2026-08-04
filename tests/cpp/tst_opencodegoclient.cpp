// SPDX-License-Identifier: GPL-2.0-or-later

#include "opencodegoclient.h"

#include <QDateTime>
#include <QTest>

class OpenCodeGoClientTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesRefreshContract();
    void parseUsageHtmlExtractsThreeWindows();
    void parseUsageHtmlRejectsMissingWindow();
    void parseUsageHtmlRejectsBadPercent();
    void parseUsageHtmlRejectsNoMatch();
};

static qint64 ms(int year, int month, int day, int hour)
{
    return QDateTime(QDate(year, month, day), QTime(hour, 0), Qt::UTC).toMSecsSinceEpoch();
}

static void assertPercent(const QVariantList &plans, int index, double expected)
{
    const double actual = plans.at(index).toMap().value(QStringLiteral("used")).toDouble();
    QVERIFY2(qAbs(actual - expected) < 1e-6,
             qPrintable(QStringLiteral("plan %1: got %2, want %3")
                            .arg(index).arg(actual).arg(expected)));
}

void OpenCodeGoClientTest::exposesRefreshContract()
{
    const QMetaObject &metaObject = OpenCodeGoClient::staticMetaObject;

    QVERIFY(metaObject.indexOfProperty("snapshot") >= 0);
    QVERIFY(metaObject.indexOfProperty("loading") >= 0);
    QVERIFY(metaObject.indexOfProperty("credentialConfigured") >= 0);
    QVERIFY(metaObject.indexOfMethod("refresh()") >= 0);
    QVERIFY(metaObject.indexOfMethod("saveCredential(QString,QString)") >= 0);
    QVERIFY(metaObject.indexOfMethod("clearCredential()") >= 0);
}

void OpenCodeGoClientTest::parseUsageHtmlExtractsThreeWindows()
{
    // SolidJS 序列化片段（键名无引号），模拟控制台页面
    const QByteArray html =
        "<html><body>"
        "window.usage={rollingUsage:$R[12]={usagePercent:25,resetInSec:3600},"
        "weeklyUsage:$R[13]={usagePercent:50,resetInSec:36000},"
        "monthlyUsage:$R[14]={usagePercent:75,resetInSec:86400}}"
        "</body></html>";
    const qint64 nowMs = ms(2026, 8, 4, 12);
    const QVariantList plans = OpenCodeGoClient::parseUsageHtml(html, nowMs);

    QCOMPARE(plans.size(), 3);
    QCOMPARE(plans.at(0).toMap().value(QStringLiteral("planId")).toString(),
             QStringLiteral("five-hour"));
    assertPercent(plans, 0, 25.0);
    assertPercent(plans, 1, 50.0);
    assertPercent(plans, 2, 75.0);
    // resetInSec=3600 → nowMs + 1h
    const qint64 expectedReset = nowMs + 3600 * 1000;
    QCOMPARE(QDateTime::fromMSecsSinceEpoch(expectedReset).toLocalTime().toString(QStringLiteral("MM-dd HH:mm")),
             plans.at(0).toMap().value(QStringLiteral("resetText")).toString());
}

void OpenCodeGoClientTest::parseUsageHtmlRejectsMissingWindow()
{
    const QByteArray html =
        "<html><body>"
        "rollingUsage:$R[1]={usagePercent:10},"
        "monthlyUsage:$R[3]={usagePercent:30}"
        "</body></html>";
    // 缺 weekly → 整体返回空（调用方显示不可用）
    QVERIFY(OpenCodeGoClient::parseUsageHtml(html, ms(2026, 8, 4, 12)).isEmpty());
}

void OpenCodeGoClientTest::parseUsageHtmlRejectsBadPercent()
{
    const QByteArray html =
        "<html><body>"
        "rollingUsage:$R[1]={usagePercent:\"n/a\"},"
        "weeklyUsage:$R[2]={usagePercent:20},"
        "monthlyUsage:$R[3]={usagePercent:30}"
        "</body></html>";
    QVERIFY(OpenCodeGoClient::parseUsageHtml(html, ms(2026, 8, 4, 12)).isEmpty());
}

void OpenCodeGoClientTest::parseUsageHtmlRejectsNoMatch()
{
    const QByteArray html = "<html><body>no usage data here</body></html>";
    QVERIFY(OpenCodeGoClient::parseUsageHtml(html, ms(2026, 8, 4, 12)).isEmpty());
}

QTEST_GUILESS_MAIN(OpenCodeGoClientTest)

#include "tst_opencodegoclient.moc"
