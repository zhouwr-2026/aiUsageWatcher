// SPDX-License-Identifier: GPL-2.0-or-later

#include "minimaxclient.h"

#include <QNetworkRequest>
#include <QTest>

class MiniMaxClientTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void createsRestrictedAuthenticatedRequest();
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

QTEST_GUILESS_MAIN(MiniMaxClientTest)

#include "tst_minimaxclient.moc"
