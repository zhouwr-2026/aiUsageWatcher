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
};

void MiniMaxClientTest::createsRestrictedAuthenticatedRequest()
{
    const QNetworkRequest request = MiniMaxClient::createRequest("test-key");

    QCOMPARE(request.url(), QUrl(QStringLiteral("https://www.minimaxi.com/v1/token_plan/remains")));
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

QTEST_GUILESS_MAIN(MiniMaxClientTest)

#include "tst_minimaxclient.moc"
