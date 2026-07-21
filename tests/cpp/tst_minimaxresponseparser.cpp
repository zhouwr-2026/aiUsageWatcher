// SPDX-License-Identifier: GPL-2.0-or-later

#include "minimaxresponseparser.h"

#include <QTest>

class MiniMaxResponseParserTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesPercentageResponse();
    void treatsExhaustedQuotaAsFullyUsed();
    void acceptsNotSubscribedResponseWithoutPlans();
    void rejectsApiError();
    void rejectsMalformedPayload();
    void rejectsInvalidPercentage();
};

void MiniMaxResponseParserTest::parsesPercentageResponse()
{
    const QByteArray payload = R"json({
        "model_remains": [
            {
                "start_time": 1784617200000,
                "end_time": 1784635200000,
                "model_name": "general",
                "weekly_start_time": 1784476800000,
                "weekly_end_time": 1785081600000,
                "current_interval_status": 1,
                "current_interval_remaining_percent": 100,
                "current_weekly_status": 1,
                "current_weekly_remaining_percent": 72
            },
            {
                "start_time": 1784563200000,
                "end_time": 1784649600000,
                "model_name": "video",
                "weekly_start_time": 1784476800000,
                "weekly_end_time": 1785081600000,
                "current_interval_status": 3,
                "current_interval_remaining_percent": 100,
                "current_weekly_status": 3,
                "current_weekly_remaining_percent": 100
            }
        ],
        "base_resp": {"status_code": 0, "status_msg": "success"}
    })json";

    const auto result = MiniMaxResponseParser::parse(payload);

    QVERIFY2(result.ok, qPrintable(result.errorMessage));
    QCOMPARE(result.snapshot.statusLabel, QStringLiteral("可用"));
    QCOMPARE(result.snapshot.plans.size(), 2);
    QCOMPARE(result.snapshot.plans.at(0).planId, QStringLiteral("general-interval"));
    QCOMPARE(result.snapshot.plans.at(0).planName, QStringLiteral("通用模型 · 5 小时"));
    QCOMPARE(result.snapshot.plans.at(0).used, 0);
    QCOMPARE(result.snapshot.plans.at(0).resetAtMs, 1784635200000LL);
    QCOMPARE(result.snapshot.plans.at(1).planId, QStringLiteral("general-weekly"));
    QCOMPARE(result.snapshot.plans.at(1).planName, QStringLiteral("通用模型 · 每周"));
    QCOMPARE(result.snapshot.plans.at(1).used, 28);
    QCOMPARE(result.snapshot.plans.at(1).resetAtMs, 1785081600000LL);
}

void MiniMaxResponseParserTest::treatsExhaustedQuotaAsFullyUsed()
{
    const QByteArray payload = R"json({
        "model_remains": [{
            "start_time": 1,
            "end_time": 2,
            "model_name": "general",
            "current_interval_status": 2,
            "current_interval_remaining_percent": 0,
            "current_weekly_status": 3,
            "current_weekly_remaining_percent": 100
        }],
        "base_resp": {"status_code": 0, "status_msg": "success"}
    })json";

    const auto result = MiniMaxResponseParser::parse(payload);

    QVERIFY(result.ok);
    QCOMPARE(result.snapshot.plans.size(), 1);
    QCOMPARE(result.snapshot.plans.first().used, 100);
}

void MiniMaxResponseParserTest::acceptsNotSubscribedResponseWithoutPlans()
{
    const QByteArray payload = R"json({
        "model_remains": [{
            "model_name": "video",
            "current_interval_status": 3,
            "current_interval_remaining_percent": 100,
            "current_weekly_status": 3,
            "current_weekly_remaining_percent": 100
        }],
        "base_resp": {"status_code": 0, "status_msg": "success"}
    })json";

    const auto result = MiniMaxResponseParser::parse(payload);

    QVERIFY(result.ok);
    QCOMPARE(result.snapshot.statusLabel, QStringLiteral("未订阅"));
    QVERIFY(result.snapshot.plans.isEmpty());
}

void MiniMaxResponseParserTest::rejectsApiError()
{
    const auto result = MiniMaxResponseParser::parse(R"json({
        "model_remains": [],
        "base_resp": {"status_code": 1004, "status_msg": "invalid token"}
    })json");

    QVERIFY(!result.ok);
    QCOMPARE(result.errorCode, QStringLiteral("api_error"));
}

void MiniMaxResponseParserTest::rejectsMalformedPayload()
{
    const auto result = MiniMaxResponseParser::parse("not-json");

    QVERIFY(!result.ok);
    QCOMPARE(result.errorCode, QStringLiteral("invalid_response"));
}

void MiniMaxResponseParserTest::rejectsInvalidPercentage()
{
    const auto result = MiniMaxResponseParser::parse(R"json({
        "model_remains": [{
            "model_name": "general",
            "end_time": 2,
            "current_interval_status": 1,
            "current_interval_remaining_percent": 101,
            "current_weekly_status": 3,
            "current_weekly_remaining_percent": 100
        }],
        "base_resp": {"status_code": 0, "status_msg": "success"}
    })json");

    QVERIFY(!result.ok);
    QCOMPARE(result.errorCode, QStringLiteral("invalid_response"));
}

QTEST_GUILESS_MAIN(MiniMaxResponseParserTest)

#include "tst_minimaxresponseparser.moc"
