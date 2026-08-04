// SPDX-License-Identifier: GPL-2.0-or-later

#include "minimaxclient.h"

#include <QIODevice>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

namespace
{
// 最小 QNetworkReply 桩：按构造参数返回指定 HTTP 状态码并立即结束。
class FakeReply : public QNetworkReply
{
    Q_OBJECT
public:
    FakeReply(const QNetworkRequest &request, int httpStatus, QObject *parent)
        : QNetworkReply(parent)
    {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, httpStatus);
        QTimer::singleShot(0, this, &FakeReply::finishNow);
    }

    void abort() override {}

protected:
    qint64 readData(char *data, qint64 maxlen) override
    {
        Q_UNUSED(data);
        Q_UNUSED(maxlen);
        return -1;
    }

private:
    void finishNow()
    {
        emit finished();
    }
};

// 每次请求计数；第 1 个请求返回 401，后续返回 200（验证 401 短路不再跨区重试）。
class FakeNetwork : public QNetworkAccessManager
{
    Q_OBJECT
public:
    int requestCount = 0;

    QNetworkReply *createRequest(Operation op, const QNetworkRequest &operation,
                                 QIODevice *outgoingData) override
    {
        Q_UNUSED(op);
        Q_UNUSED(outgoingData);
        ++requestCount;
        return new FakeReply(operation, requestCount == 1 ? 401 : 200, this);
    }
};
} // namespace

class MiniMaxClientTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void createsRestrictedAuthenticatedRequest();
    void exposesCredentialManagementContract();
    void unconfiguredStateIsNotAnError();
    void endpointCandidatesAreCodingPlanOnly();
    void doesNotFallbackAcrossRegionsOnAuthenticationFailure();
};

void MiniMaxClientTest::createsRestrictedAuthenticatedRequest()
{
    const QList<QUrl> endpoints = MiniMaxClient::endpointCandidates();
    QCOMPARE(endpoints.first(),
             QUrl(QStringLiteral("https://api.minimaxi.com/v1/api/openplatform/coding_plan/remains")));

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

void MiniMaxClientTest::endpointCandidatesAreCodingPlanOnly()
{
    const QList<QUrl> endpoints = MiniMaxClient::endpointCandidates();
    QCOMPARE(endpoints.size(), 2);
    QCOMPARE(endpoints.first(),
             QUrl(QStringLiteral("https://api.minimaxi.com/v1/api/openplatform/coding_plan/remains")));
    QCOMPARE(endpoints.last(),
             QUrl(QStringLiteral("https://api.minimax.io/v1/api/openplatform/coding_plan/remains")));
}

void MiniMaxClientTest::doesNotFallbackAcrossRegionsOnAuthenticationFailure()
{
    qputenv("MINIMAX_API_KEY", "test-key");
    MiniMaxClient client;
    FakeNetwork network;
    client.setNetworkAccessManager(&network);

    QSignalSpy snapshotSpy(&client, &MiniMaxClient::snapshotChanged);
    client.refresh();
    QVERIFY(snapshotSpy.wait(2000));

    // 401 直接终止，不再尝试第二个（国际区）端点
    QCOMPARE(network.requestCount, 1);
    QCOMPARE(client.snapshot().value(QStringLiteral("statusLabel")).toString(),
             QStringLiteral("请求失败"));
    QVERIFY2(client.snapshot().value(QStringLiteral("errorText")).toString()
                 .contains(QStringLiteral("Key 无效或已过期")),
             qPrintable(QStringLiteral("unexpected errorText: ")
                            + client.snapshot().value(QStringLiteral("errorText")).toString()));
    qunsetenv("MINIMAX_API_KEY");
}

QTEST_GUILESS_MAIN(MiniMaxClientTest)

#include "tst_minimaxclient.moc"
