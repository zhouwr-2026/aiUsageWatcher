// SPDX-License-Identifier: GPL-2.0-or-later

#include "codexloginoutputparser.h"

#include <QJsonDocument>
#include <QTest>

class CodexLoginOutputParserTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesDeviceAuthorizationResponse();
    void rejectsIncompleteDeviceAuthorizationResponse();
    void parsesAuthorizationAndTokenResponses();
    void extractsAccountIdentityWithoutExposingTokens();
    void parsesRefreshResponseWithRotatedFields();
    void readsTokenExpiry();
    void parsesUsageWindows();
    void rejectsInvalidUsagePercentage();
    void rejectsUsageIntegerOutsideQint64Range();
};

void CodexLoginOutputParserTest::parsesDeviceAuthorizationResponse()
{
    const CodexDeviceAuthorization authorization =
        CodexLoginOutputParser::deviceAuthorization(
            R"({"device_auth_id":"device-123","user_code":"K58J-YY9PL","interval":"7","expires_in":600})");
    QVERIFY(authorization.isValid());
    QCOMPARE(authorization.deviceAuthId, QStringLiteral("device-123"));
    QCOMPARE(authorization.userCode, QStringLiteral("K58J-YY9PL"));
    QCOMPARE(authorization.intervalSeconds, 7);
    QCOMPARE(authorization.expiresInSeconds, 600);
}

void CodexLoginOutputParserTest::parsesRefreshResponseWithRotatedFields()
{
    const CodexRefreshExchange tokens = CodexLoginOutputParser::refreshExchange(
        R"({"access_token":"new-access"})");
    QVERIFY(tokens.isValid());
    QCOMPARE(tokens.accessToken, QStringLiteral("new-access"));
    QVERIFY(tokens.refreshToken.isEmpty());
}

void CodexLoginOutputParserTest::readsTokenExpiry()
{
    const QByteArray payload = R"({"exp":1900000000})";
    const QString jwt = QStringLiteral("e30.%1.sig").arg(QString::fromLatin1(
        payload.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)));
    QCOMPARE(CodexLoginOutputParser::tokenExpiresAt(jwt), 1900000000LL);
    QCOMPARE(CodexLoginOutputParser::tokenExpiresAt(QStringLiteral("invalid")), 0LL);
}

void CodexLoginOutputParserTest::parsesUsageWindows()
{
    const CodexUsageResult result = CodexLoginOutputParser::usageResponse(R"json({
        "rate_limit": {
            "primary_window": {
                "used_percent": 42.5,
                "limit_window_seconds": 18000,
                "reset_at": 1900000000
            },
            "secondary_window": {
                "used_percent": 12,
                "limit_window_seconds": 604800,
                "reset_at": 1900604800
            }
        }
    })json");

    QVERIFY2(result.isValid(), qPrintable(result.errorMessage));
    QCOMPARE(result.windows.size(), 2);
    QCOMPARE(result.windows.at(0).planId, QStringLiteral("5-hours"));
    QCOMPARE(result.windows.at(0).planName, QStringLiteral("5 小时"));
    QCOMPARE(result.windows.at(0).usedPercent, 42.5);
    QCOMPARE(result.windows.at(1).planId, QStringLiteral("7-days"));
}

void CodexLoginOutputParserTest::rejectsInvalidUsagePercentage()
{
    const CodexUsageResult result = CodexLoginOutputParser::usageResponse(R"json({
        "rate_limit": {
            "primary_window": {
                "used_percent": 101,
                "limit_window_seconds": 18000,
                "reset_at": 1900000000
            }
        }
    })json");
    QVERIFY(!result.isValid());
    QVERIFY(!result.errorMessage.isEmpty());
}

void CodexLoginOutputParserTest::rejectsUsageIntegerOutsideQint64Range()
{
    const CodexUsageResult result = CodexLoginOutputParser::usageResponse(R"json({
        "rate_limit": {
            "primary_window": {
                "used_percent": 10,
                "limit_window_seconds": 1e100,
                "reset_at": 1900000000
            }
        }
    })json");
    QVERIFY(!result.isValid());
}

void CodexLoginOutputParserTest::rejectsIncompleteDeviceAuthorizationResponse()
{
    QVERIFY(!CodexLoginOutputParser::deviceAuthorization(R"({"user_code":"K58J-YY9PL"})")
                 .isValid());
}

void CodexLoginOutputParserTest::parsesAuthorizationAndTokenResponses()
{
    const CodexAuthorizationResult result = CodexLoginOutputParser::authorizationResult(
        R"({"authorization_code":"auth-code","code_verifier":"verifier"})");
    QVERIFY(result.isValid());

    const CodexTokenExchange tokens = CodexLoginOutputParser::tokenExchange(
        R"({"id_token":"id","access_token":"access","refresh_token":"refresh"})");
    QVERIFY(tokens.isValid());
}

void CodexLoginOutputParserTest::extractsAccountIdentityWithoutExposingTokens()
{
    const QByteArray payload = R"({"email":"user@example.com","https://api.openai.com/auth":{"chatgpt_account_id":"account-123"}})";
    const QByteArray jwt = QByteArrayLiteral("e30.")
        + payload.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)
        + QByteArrayLiteral(".sig");
    const QByteArray auth = QByteArrayLiteral("{\"tokens\":{\"id_token\":\"")
        + jwt + QByteArrayLiteral("\",\"refresh_token\":\"never-return-this\"}}");

    QVERIFY(QJsonDocument::fromJson(auth).isObject());
    QByteArray encodedPayload = jwt.split('.').at(1);
    while (encodedPayload.size() % 4 != 0)
        encodedPayload.append('=');
    QCOMPARE(QByteArray::fromBase64(encodedPayload, QByteArray::Base64UrlEncoding), payload);

    const CodexAccountIdentity identity = CodexLoginOutputParser::accountIdentity(auth);
    QCOMPARE(identity.accountId, QStringLiteral("account-123"));
    QCOMPARE(identity.email, QStringLiteral("user@example.com"));
}

QTEST_GUILESS_MAIN(CodexLoginOutputParserTest)

#include "tst_codexloginoutputparser.moc"
