// SPDX-License-Identifier: GPL-2.0-or-later

#include "deepseekclient.h"

#include <QNetworkRequest>
#include <QTest>

class DeepSeekClientTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void balanceEndpointIsDeepSeekUserBalance();
    void createsAuthenticatedRequest();
    void exposesCredentialManagementContract();
    void unconfiguredStateIsNotAnError();
};

void DeepSeekClientTest::balanceEndpointIsDeepSeekUserBalance()
{
    QCOMPARE(DeepSeekClient::balanceEndpoint(),
             QUrl(QStringLiteral("https://api.deepseek.com/user/balance")));
}

void DeepSeekClientTest::createsAuthenticatedRequest()
{
    const QNetworkRequest request = DeepSeekClient::createRequest(
        DeepSeekClient::balanceEndpoint(), "test-key");

    QCOMPARE(request.url(), DeepSeekClient::balanceEndpoint());
    QCOMPARE(request.rawHeader("Authorization"), QByteArray("Bearer test-key"));
    QCOMPARE(request.rawHeader("Accept"), QByteArray("application/json"));
    QCOMPARE(request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(),
             static_cast<int>(QNetworkRequest::SameOriginRedirectPolicy));
}

void DeepSeekClientTest::exposesCredentialManagementContract()
{
    const QMetaObject &metaObject = DeepSeekClient::staticMetaObject;

    QVERIFY(metaObject.indexOfProperty("credentialConfigured") >= 0);
    QVERIFY(metaObject.indexOfProperty("credentialStatus") >= 0);
    QVERIFY(metaObject.indexOfProperty("credentialBusy") >= 0);
    QVERIFY(metaObject.indexOfProperty("credentialError") >= 0);
    QVERIFY(metaObject.indexOfMethod("saveCredential(QString)") >= 0);
    QVERIFY(metaObject.indexOfMethod("clearCredential()") >= 0);
}

void DeepSeekClientTest::unconfiguredStateIsNotAnError()
{
    qunsetenv("DEEPSEEK_API_KEY");
    DeepSeekClient client;

    QCOMPARE(client.snapshot().value(QStringLiteral("statusLabel")).toString(),
             QStringLiteral("未配置"));
    QCOMPARE(client.snapshot().value(QStringLiteral("errorText")).toString(), QString{});
}

QTEST_GUILESS_MAIN(DeepSeekClientTest)

#include "tst_deepseekclient.moc"
