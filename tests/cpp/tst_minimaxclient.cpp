// SPDX-License-Identifier: GPL-2.0-or-later

#include "minimaxclient.h"

#include <QNetworkRequest>
#include <QTest>

class MiniMaxClientTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void createsRestrictedAuthenticatedRequest();
    void exposesCredentialManagementContract();
    void unconfiguredStateIsNotAnError();
};

void MiniMaxClientTest::createsRestrictedAuthenticatedRequest()
{
    const QList<QUrl> endpoints = MiniMaxClient::endpointCandidates();
    QCOMPARE(endpoints.size(), 4);
    QCOMPARE(endpoints.first(),
             QUrl(QStringLiteral("https://api.minimaxi.com/v1/api/openplatform/coding_plan/remains")));
    QCOMPARE(endpoints.last(),
             QUrl(QStringLiteral("https://api.minimax.io/v1/token_plan/remains")));

    const QNetworkRequest request = MiniMaxClient::createRequest(endpoints.first(), "test-key");

    QCOMPARE(request.url(), endpoints.first());
    QCOMPARE(request.rawHeader("Authorization"), QByteArray("Bearer test-key"));
    QCOMPARE(request.rawHeader("Content-Type"), QByteArray("application/json"));
    QCOMPARE(request.rawHeader("Accept"), QByteArray("application/json"));
    QCOMPARE(request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(),
             static_cast<int>(QNetworkRequest::SameOriginRedirectPolicy));
}

void MiniMaxClientTest::exposesCredentialManagementContract()
{
    const QMetaObject &metaObject = MiniMaxClient::staticMetaObject;

    QVERIFY(metaObject.indexOfProperty("credentialConfigured") >= 0);
    QVERIFY(metaObject.indexOfProperty("credentialStatus") >= 0);
    QVERIFY(metaObject.indexOfProperty("credentialBusy") >= 0);
    QVERIFY(metaObject.indexOfProperty("credentialError") >= 0);
    QVERIFY(metaObject.indexOfMethod("saveCredential(QString)") >= 0);
    QVERIFY(metaObject.indexOfMethod("clearCredential()") >= 0);
}

void MiniMaxClientTest::unconfiguredStateIsNotAnError()
{
    qunsetenv("MINIMAX_API_KEY");
    MiniMaxClient client;

    QCOMPARE(client.snapshot().value(QStringLiteral("statusLabel")).toString(),
             QStringLiteral("未配置"));
    QCOMPARE(client.snapshot().value(QStringLiteral("errorText")).toString(), QString{});
}

QTEST_GUILESS_MAIN(MiniMaxClientTest)

#include "tst_minimaxclient.moc"
