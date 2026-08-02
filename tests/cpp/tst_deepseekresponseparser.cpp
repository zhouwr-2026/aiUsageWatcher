// SPDX-License-Identifier: GPL-2.0-or-later

#include "deepseekresponseparser.h"

#include <QTest>

class DeepSeekResponseParserTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesBalanceInfosWithStringAmounts();
    void parsesMultipleCurrencies();
    void mapsUnauthorizedToChineseError();
    void mapsServerError();
    void rejectsMalformedJson();
    void marksUnavailableBalance();
    void acceptsNumericAmounts();
    void toleratesMissingBalanceInfos();
};

void DeepSeekResponseParserTest::parsesBalanceInfosWithStringAmounts()
{
    const QByteArray payload = R"({
        "is_available": true,
        "balance_infos": [{
            "currency": "CNY",
            "total_balance": "110.00",
            "granted_balance": "10.00",
            "topped_up_balance": "100.00"
        }]
    })";
    const auto result = DeepSeekResponseParser::parse(payload, 200);

    QVERIFY(result.ok);
    QCOMPARE(result.errorMessage, QString{});
    QCOMPARE(result.balances.size(), 1);
    QCOMPARE(result.balances[0].currency, QStringLiteral("CNY"));
    QCOMPARE(result.balances[0].totalBalance, 110.00);
    QCOMPARE(result.balances[0].grantedBalance, 10.00);
    QCOMPARE(result.balances[0].toppedUpBalance, 100.00);
    QVERIFY(result.balances[0].isAvailable);
}

void DeepSeekResponseParserTest::parsesMultipleCurrencies()
{
    const QByteArray payload = R"({
        "is_available": true,
        "balance_infos": [
            {"currency": "CNY", "total_balance": "110.00"},
            {"currency": "USD", "total_balance": "5.00"}
        ]
    })";
    const auto result = DeepSeekResponseParser::parse(payload, 200);

    QVERIFY(result.ok);
    QCOMPARE(result.balances.size(), 2);
    QCOMPARE(result.balances[1].currency, QStringLiteral("USD"));
    QCOMPARE(result.balances[1].totalBalance, 5.00);
}

void DeepSeekResponseParserTest::mapsUnauthorizedToChineseError()
{
    const auto result = DeepSeekResponseParser::parse("{}", 401);

    QVERIFY(!result.ok);
    QCOMPARE(result.errorMessage, QStringLiteral("鉴权失败 (HTTP 401)"));
}

void DeepSeekResponseParserTest::mapsServerError()
{
    const auto result = DeepSeekResponseParser::parse("oops", 500);

    QVERIFY(!result.ok);
    QCOMPARE(result.errorMessage, QStringLiteral("接口错误 (HTTP 500)"));
}

void DeepSeekResponseParserTest::rejectsMalformedJson()
{
    const auto result = DeepSeekResponseParser::parse("not json", 200);

    QVERIFY(!result.ok);
    QCOMPARE(result.errorMessage, QStringLiteral("DeepSeek 返回了无法识别的数据"));
}

void DeepSeekResponseParserTest::marksUnavailableBalance()
{
    const QByteArray payload = R"({
        "is_available": false,
        "balance_infos": [{"currency": "CNY", "total_balance": "0.00"}]
    })";
    const auto result = DeepSeekResponseParser::parse(payload, 200);

    QVERIFY(result.ok);
    QCOMPARE(result.balances.size(), 1);
    QVERIFY(!result.balances[0].isAvailable);
}

void DeepSeekResponseParserTest::acceptsNumericAmounts()
{
    const QByteArray payload = R"({
        "is_available": true,
        "balance_infos": [{"currency": "CNY", "total_balance": 88.5}]
    })";
    const auto result = DeepSeekResponseParser::parse(payload, 200);

    QVERIFY(result.ok);
    QCOMPARE(result.balances[0].totalBalance, 88.5);
}

void DeepSeekResponseParserTest::toleratesMissingBalanceInfos()
{
    const auto result = DeepSeekResponseParser::parse(R"({"is_available": true})", 200);

    QVERIFY(result.ok);
    QVERIFY(result.balances.isEmpty());
}

QTEST_GUILESS_MAIN(DeepSeekResponseParserTest)

#include "tst_deepseekresponseparser.moc"
