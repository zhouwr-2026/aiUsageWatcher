// SPDX-License-Identifier: GPL-2.0-or-later

#include "codexzhresponseparser.h"

#include <QDate>
#include <QDateTime>
#include <QTest>
#include <QTime>

class CodexZhResponseParserTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesWeeklyUsdBudget();
    void extraTextUsesSixteenItemsInSpecOrder();
    void formattedFieldsTakePrecedenceOverRawNumbers();
    void missingSubscriptionRendersAsDash();
    void computesWeeklyResetTextFromFixedNow();
    void parsesWeeklyUsageSegments();
    void omitsSegmentsWhenTodayUsedIsMissingOrInvalid();
    void clampsTodayUsageToWeeklyUsage();
    void missingWeekCallsRendersAsDash();
    void hugeTokenCountRendersWithoutOverflow();
    void errorFieldFallbackWhenMessageMissing();
    void messageFieldStillWins();
};

void CodexZhResponseParserTest::parsesWeeklyUsdBudget()
{
    const QByteArray payload = R"({
        "success": true,
        "data": {
            "todayCalls": 204,
            "weekCalls": 204,
            "todayUsed": 30.020896,
            "todayTokens": 49782650,
            "weekUsed": 30.020896,
            "totalRequests": 1960,
            "analyticsTotalUsed": 305.099572,
            "totalTokens": 446034226,
            "dailyQuota": 127500000,
            "weeklyQuota": 127500000,
            "dailyBudget": 255,
            "weeklyBudget": 255,
            "remainQuota": 224.979104,
            "subscriptionStart": "2026-03-03 10:23:40",
            "subscriptionEnd": "2026-08-22 17:23:53"
        }
    })";

    const CodexZhParseResult result = CodexZhResponseParser::parse(payload);

    QVERIFY(result.ok);
    QCOMPARE(result.snapshot.plan.planName, QStringLiteral("周限额"));
    QCOMPARE(result.snapshot.plan.used, 30.020896);
    QCOMPARE(result.snapshot.plan.total, 255.0);
    QVERIFY(!result.snapshot.plan.resetText.isEmpty());
    QVERIFY(result.snapshot.plan.resetAtMs > 0);
}

void CodexZhResponseParserTest::extraTextUsesSixteenItemsInSpecOrder()
{
    const QByteArray payload = R"({
        "success": true,
        "data": {
            "todayCalls": 204,
            "weekCalls": 204,
            "todayUsed": 30.020896,
            "todayTokens": 49782650,
            "weekUsed": 30.020896,
            "totalRequests": 1960,
            "analyticsTotalUsed": 305.099572,
            "totalTokens": 446034226,
            "dailyQuota": 127500000,
            "weeklyQuota": 127500000,
            "dailyBudget": 255,
            "weeklyBudget": 255,
            "remainQuota": 224.979104,
            "subscriptionStart": "2026-03-03 10:23:40",
            "subscriptionEnd": "2026-08-22 17:23:53"
        }
    })";

    const CodexZhParseResult result = CodexZhResponseParser::parse(payload);
    QVERIFY(result.ok);

    const QStringList items =
        result.snapshot.plan.extraText.split(QStringLiteral(" | "));
    QCOMPARE(items.size(), 16);

    QCOMPARE(items.at(0), QStringLiteral("今日调用：204"));
    QCOMPARE(items.at(1), QStringLiteral("今日消费：$30.02"));
    QCOMPARE(items.at(2), QStringLiteral("今日 Token：49,782,650"));
    QCOMPARE(items.at(3), QStringLiteral("日限额度：$255.00"));
    QCOMPARE(items.at(4), QStringLiteral("今日剩余：$224.98"));
    QCOMPARE(items.at(5), QStringLiteral("本周调用：204"));
    QCOMPARE(items.at(6), QStringLiteral("本周消费：$30.02"));
    QCOMPARE(items.at(7), QStringLiteral("周限额度：$255.00"));
    QCOMPARE(items.at(8), QStringLiteral("实时剩余：$224.98"));
    QCOMPARE(items.at(9), QStringLiteral("总请求次数：1,960"));
    QCOMPARE(items.at(10), QStringLiteral("总使用额度：$305.10"));
    QCOMPARE(items.at(11), QStringLiteral("总使用 Token：446,034,226"));
    QCOMPARE(items.at(12), QStringLiteral("RPM：0"));
    QCOMPARE(items.at(13), QStringLiteral("TPM：0"));
    QCOMPARE(items.at(14), QStringLiteral("订阅开始：2026-03-03 10:23:40"));
    QCOMPARE(items.at(15), QStringLiteral("订阅到期：2026-08-22 17:23:53"));
}

void CodexZhResponseParserTest::formattedFieldsTakePrecedenceOverRawNumbers()
{
    const QByteArray payload = R"({
        "success": true,
        "data": {
            "weekUsed": 30.020896,
            "weeklyBudget": 255,
            "totalCalls": 9,
            "todayCalls": 7.6,
            "todayUsed": 11.11,
            "todayTokens": 9,
            "totalRequests": 1,
            "totalUsed": 5.5,
            "totalTokens": 100,
            "dailyBudget": 12,
            "dailyQuota": 0,
            "remainQuota": 0,
            "todayUsedFormatted": "11.11*",
            "weekUsedFormatted": "30.02*",
            "analyticsTotalUsed": 999,
            "totalUsedFormatted": "305.10*"
        }
    })";

    const CodexZhParseResult result = CodexZhResponseParser::parse(payload);
    QVERIFY(result.ok);

    const QStringList items =
        result.snapshot.plan.extraText.split(QStringLiteral(" | "));
    QCOMPARE(items.at(0), QStringLiteral("今日调用：8"));
    QCOMPARE(items.at(1), QStringLiteral("今日消费：$11.11*"));
    QCOMPARE(items.at(5), QStringLiteral("本周调用：-"));
    QCOMPARE(items.at(6), QStringLiteral("本周消费：$30.02*"));
    QCOMPARE(items.at(10), QStringLiteral("总使用额度：$305.10*"));
}

void CodexZhResponseParserTest::missingSubscriptionRendersAsDash()
{
    const QByteArray payload = R"({
        "success": true,
        "data": {
            "todayCalls": 1,
            "weekCalls": 1,
            "todayUsed": 0,
            "todayTokens": 0,
            "weekUsed": 0,
            "totalRequests": 1,
            "totalUsed": 0,
            "totalTokens": 0,
            "dailyBudget": 1,
            "weeklyBudget": 1,
            "dailyQuota": 0,
            "weeklyQuota": 0,
            "remainQuota": 1
        }
    })";

    const CodexZhParseResult result = CodexZhResponseParser::parse(payload);
    QVERIFY(result.ok);

    const QStringList items =
        result.snapshot.plan.extraText.split(QStringLiteral(" | "));
    QCOMPARE(items.size(), 16);
    QCOMPARE(items.at(14), QStringLiteral("订阅开始：-"));
    QCOMPARE(items.at(15), QStringLiteral("订阅到期：-"));
}

void CodexZhResponseParserTest::computesWeeklyResetTextFromFixedNow()
{
    const QDateTime now = QDateTime(QDate(2026, 7, 27), QTime(14, 0, 0));
    QCOMPARE(CodexZhResponseParser::nextWeeklyResetText(now),
             QStringLiteral("08-03 00:00"));
}

void CodexZhResponseParserTest::parsesWeeklyUsageSegments()
{
    const QByteArray payload = R"({
        "success": true,
        "data": {
            "weeklyBudget": 100,
            "weekUsed": 50,
            "todayUsed": "30",
            "todayUsedFormatted": "30.00"
        }
    })";

    const CodexZhParseResult result = CodexZhResponseParser::parse(payload);

    QVERIFY(result.ok);
    QCOMPARE(result.snapshot.plan.usageSegments.size(), 2);
    const CodexZhUsageSegment previous = result.snapshot.plan.usageSegments.at(0);
    QCOMPARE(previous.kind, QStringLiteral("previous"));
    QCOMPARE(previous.used, 20.0);
    QCOMPARE(previous.usedPercent, 20.0);
    const CodexZhUsageSegment today = result.snapshot.plan.usageSegments.at(1);
    QCOMPARE(today.kind, QStringLiteral("today"));
    QCOMPARE(today.used, 30.0);
    QCOMPARE(today.usedPercent, 30.0);
    QCOMPARE(today.formattedUsed, QStringLiteral("$30.00"));
}

void CodexZhResponseParserTest::omitsSegmentsWhenTodayUsedIsMissingOrInvalid()
{
    const auto parseSegments = [](const QByteArray &todayUsed) {
        const QByteArray payload = QByteArrayLiteral(R"({"success":true,"data":{"weeklyBudget":100,"weekUsed":50)")
            + todayUsed + QByteArrayLiteral("}}");
        return CodexZhResponseParser::parse(payload).snapshot.plan.usageSegments;
    };

    QCOMPARE(parseSegments(QByteArrayLiteral("")).size(), 0);
    QCOMPARE(parseSegments(QByteArrayLiteral(",\"todayUsed\":\"not-a-number\"")).size(), 0);
    QCOMPARE(parseSegments(QByteArrayLiteral(",\"todayUsed\":-1")).size(), 0);

    const QList<CodexZhUsageSegment> zeroToday = parseSegments(QByteArrayLiteral(",\"todayUsed\":0"));
    QCOMPARE(zeroToday.size(), 1);
    QCOMPARE(zeroToday.first().kind, QStringLiteral("previous"));
    QCOMPARE(zeroToday.first().used, 50.0);
}

void CodexZhResponseParserTest::clampsTodayUsageToWeeklyUsage()
{
    const QByteArray payload = R"({
        "success": true,
        "data": {
            "weeklyBudget": 100,
            "weekUsed": 50,
            "todayUsed": 80
        }
    })";

    const CodexZhParseResult result = CodexZhResponseParser::parse(payload);

    QVERIFY(result.ok);
    QCOMPARE(result.snapshot.plan.usageSegments.size(), 1);
    QCOMPARE(result.snapshot.plan.usageSegments.first().kind, QStringLiteral("today"));
    QCOMPARE(result.snapshot.plan.usageSegments.first().used, 50.0);
    QCOMPARE(result.snapshot.plan.usageSegments.first().usedPercent, 50.0);
}

void CodexZhResponseParserTest::errorFieldFallbackWhenMessageMissing()
{
    // 服务端新版错误字段为 error（无 key → "API Key is required"）
    const QByteArray payload = R"({"success":false,"error":"API Key is required"})";
    const CodexZhParseResult result = CodexZhResponseParser::parse(payload);

    QVERIFY(!result.ok);
    QCOMPARE(result.errorMessage, QStringLiteral("API Key is required"));
}

void CodexZhResponseParserTest::messageFieldStillWins()
{
    // 旧版 message 字段仍优先
    const QByteArray payload = R"({"success":false,"message":"旧错误","error":"新错误"})";
    const CodexZhParseResult result = CodexZhResponseParser::parse(payload);

    QVERIFY(!result.ok);
    QCOMPARE(result.errorMessage, QStringLiteral("旧错误"));
}

void CodexZhResponseParserTest::missingWeekCallsRendersAsDash()
{
    const QByteArray payload = R"({
        "success": true,
        "data": {
            "weekUsed": 50,
            "weeklyBudget": 100,
            "totalCalls": 9
        }
    })";

    const CodexZhParseResult result = CodexZhResponseParser::parse(payload);
    QVERIFY(result.ok);
    const QStringList items =
        result.snapshot.plan.extraText.split(QStringLiteral(" | "));
    // 本周调用缺失时显示占位符，不用总调用次数冒充
    QCOMPARE(items.at(5), QStringLiteral("本周调用：-"));
}

void CodexZhResponseParserTest::hugeTokenCountRendersWithoutOverflow()
{
    const QByteArray payload = R"({
        "success": true,
        "data": {
            "weekUsed": 50,
            "weeklyBudget": 100,
            "todayTokens": 2e19
        }
    })";

    const CodexZhParseResult result = CodexZhResponseParser::parse(payload);
    QVERIFY(result.ok);
    const QStringList items =
        result.snapshot.plan.extraText.split(QStringLiteral(" | "));
    // 超出 quint64 上限时显示原始值而非转换 UB 崩溃
    QCOMPARE(items.at(2),
             QStringLiteral("今日 Token：%1").arg(QString::number(2e19, 'f', 0)));
}

QTEST_GUILESS_MAIN(CodexZhResponseParserTest)

#include "tst_codexzhresponseparser.moc"
