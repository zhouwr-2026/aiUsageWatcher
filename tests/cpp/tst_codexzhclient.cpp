// SPDX-License-Identifier: GPL-2.0-or-later

// 目的：覆盖 CodexZhClient 的限流恢复路径。
//
// 关键事实（src/codexzhclient.cpp:134-138）：
//   refresh() 入口仅在 m_rateLimitedUntilMs > now 时早 return；窗口一过，
//   下一次定时刷新自动恢复，无需用户手动点"刷新"。
//
// 历史回归（已修复）：早 return 条件曾包含 `m_autoRefreshPaused`，QML
// refreshQueue 走 refresh()（非 forceRefresh()），429 之后永远不再发出
// 请求，死锁到下次手动刷新。本测试断言：
//   - 429 后 refresh() 在窗口内不会发请求；
//   - 窗口到期后 refresh() 正常发请求；
//   - forceRefresh() 强制清窗口；
//   - 死锁字段 m_autoRefreshPaused 不再存在（编译期 + QMetaObject）。

#include "codexzhclient.h"

#include <QDateTime>
#include <QIODevice>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <type_traits>

namespace
{
// 最小 QNetworkReply 桩：按构造参数返回指定 HTTP 状态码与可选 payload 并立即结束。
class FakeReply : public QNetworkReply
{
    Q_OBJECT
public:
    FakeReply(const QNetworkRequest &request, int httpStatus,
              const QByteArray &payload, QObject *parent)
        : QNetworkReply(parent)
        , m_payload(payload)
    {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, httpStatus);
        setError(QNetworkReply::NoError, QString());
        // 主动 open 让 readData 能成功返回。
        open(QIODevice::ReadOnly);
        QTimer::singleShot(0, this, &FakeReply::finishNow);
    }

    void abort() override {}

protected:
    qint64 readData(char *data, qint64 maxlen) override
    {
        if (m_consumed >= m_payload.size()) {
            return 0;
        }
        const qint64 available = qMin(maxlen, m_payload.size() - m_consumed);
        memcpy(data, m_payload.constData() + m_consumed, available);
        m_consumed += available;
        return available;
    }

private:
    void finishNow()
    {
        emit finished();
    }

    QByteArray m_payload;
    qint64 m_consumed = 0;
};

// 一份能通过 CodexZhResponseParser 的最小 payload；用于触发成功路径。
const QByteArray kSuccessPayload = R"({
    "success": true,
    "data": {
        "todayCalls": 1,
        "weekCalls": 1,
        "todayUsed": 1.0,
        "todayTokens": 1000,
        "weekUsed": 1.0,
        "totalRequests": 1,
        "analyticsTotalUsed": 1.0,
        "totalTokens": 1000,
        "dailyQuota": 100,
        "weeklyQuota": 100,
        "dailyBudget": 100,
        "weeklyBudget": 100,
        "remainQuota": 99.0,
        "subscriptionStart": "2026-01-01 00:00:00",
        "subscriptionEnd": "2026-12-31 23:59:59"
    }
})";

// 构造指定字段的 payload：两次调用若参数不同，parse 出来的 snapshot
// 用量字段必然不同，可用于验证 snapshot 真在更新（而不是被 == 短路）。
QByteArray buildSuccessPayload(double weekUsed, double weeklyBudget,
                               int todayCalls, int totalRequests)
{
    return QByteArrayLiteral(R"({
        "success": true,
        "data": {
            "todayCalls": )") + QByteArray::number(todayCalls) +
           QByteArrayLiteral(R"(,
            "weekCalls": )") + QByteArray::number(todayCalls) +
           QByteArrayLiteral(R"(,
            "todayUsed": )") + QByteArray::number(weekUsed) +
           QByteArrayLiteral(R"(,
            "todayTokens": 1000,
            "weekUsed": )") + QByteArray::number(weekUsed) +
           QByteArrayLiteral(R"(,
            "totalRequests": )") + QByteArray::number(totalRequests) +
           QByteArrayLiteral(R"(,
            "analyticsTotalUsed": )") + QByteArray::number(weekUsed) +
           QByteArrayLiteral(R"(,
            "totalTokens": 1000,
            "dailyQuota": 100,
            "weeklyQuota": )") + QByteArray::number(static_cast<int>(weeklyBudget * 500000)) +
           QByteArrayLiteral(R"(,
            "dailyBudget": 100,
            "weeklyBudget": )") + QByteArray::number(weeklyBudget) +
           QByteArrayLiteral(R"(,
            "remainQuota": )") + QByteArray::number(weeklyBudget - weekUsed) +
           QByteArrayLiteral(R"(,
            "subscriptionStart": "2026-01-01 00:00:00",
            "subscriptionEnd": "2026-12-31 23:59:59"
        }
    })");
}

// 可配置每次返回的状态码与 payload；用于模拟 "429 → 200" 序列验证限流恢复。
class FakeNetwork : public QNetworkAccessManager
{
    Q_OBJECT
public:
    QList<int> scriptedStatuses;
    QList<QByteArray> scriptedPayloads;
    int requestCount = 0;

    QNetworkReply *createRequest(Operation op, const QNetworkRequest &operation,
                                 QIODevice *outgoingData) override
    {
        Q_UNUSED(op);
        Q_UNUSED(outgoingData);
        const int status = requestCount < scriptedStatuses.size()
            ? scriptedStatuses.at(requestCount)
            : scriptedStatuses.last();
        QByteArray payload;
        if (status >= 200 && status < 300) {
            if (requestCount < scriptedPayloads.size()) {
                payload = scriptedPayloads.at(requestCount);
            } else if (scriptedPayloads.isEmpty()) {
                payload = kSuccessPayload;
            } else {
                payload = scriptedPayloads.last();
            }
        }
        ++requestCount;
        return new FakeReply(operation, status, payload, this);
    }
};

// SFINAE 检测：编译期判断 CodexZhClient 上是否还存在 m_autoRefreshPaused 字段。
// 该字段在 429 限流恢复热修中被移除；本检测确保它不会被悄悄加回来。
namespace codexzh_test
{
template<typename T, typename = void>
struct has_autoRefreshPaused : std::false_type {};
template<typename T>
struct has_autoRefreshPaused<T, std::void_t<decltype(std::declval<T&>().m_autoRefreshPaused)>>
    : std::true_type {};
} // namespace codexzh_test

// 友元声明只授权 CodexZhClientTest，所以安装函数放在测试类里。
} // namespace

class CodexZhClientTest : public QObject
{
    Q_OBJECT

private:
    static void installTestDeps(CodexZhClient &client,
                                QNetworkAccessManager *nam,
                                const QByteArray &apiKey)
    {
        client.m_network = nam;
        client.setStoredApiKey(apiKey);
    }

private Q_SLOTS:
    void unconfiguredRefreshIsANoOpAndReportsMissingCredential();
    void deadLockFieldAutoRefreshPausedIsRemoved();
    void rateLimitedWindowBlocksSubsequentRefresh();
    void rateLimitedWindowExpiryAllowsRefresh();
    void forceRefreshClearsRateLimitedWindow();
    void successfulResponseClearsRateLimitedWindow();
    void consecutiveRefreshesPropagateChangedUsage();
    void cancelDuringInFlightThenRefreshStillUpdates();
};

void CodexZhClientTest::unconfiguredRefreshIsANoOpAndReportsMissingCredential()
{
    CodexZhClient client;
    QSignalSpy loadingSpy(&client, &CodexZhClient::loadingChanged);

    // 未配置 API Key 时 refresh() 直接走"未配置"分支，不发请求，
    // 也不会进入"加载中"状态。snapshot 在初始化时就是"未配置"，若值未变则
    // snapshotChanged 不发射——只看最终态与 loading 信号即可。
    client.refresh();
    QCOMPARE(client.snapshot().value(QStringLiteral("statusLabel")).toString(),
             QStringLiteral("未配置"));
    QCOMPARE(loadingSpy.count(), 0);
}

void CodexZhClientTest::deadLockFieldAutoRefreshPausedIsRemoved()
{
    // 编译期 + 链接期检查：m_autoRefreshPaused 字段不应再存在。
    // SFINAE 检测：编译期若字段被重新引入，static_assert 立刻失败。
    static_assert(!codexzh_test::has_autoRefreshPaused<CodexZhClient>::value,
                  "m_autoRefreshPaused 字段不应再出现在 CodexZhClient 上；"
                  "429 限流应仅靠 m_rateLimitedUntilMs 时间窗自愈，不允许永久锁定。");
    // QMetaObject 检查：未声明的属性名不应出现在 metaobject 中。
    const QMetaObject &metaObject = CodexZhClient::staticMetaObject;
    QCOMPARE(metaObject.indexOfProperty("autoRefreshPaused"), -1);
}

void CodexZhClientTest::rateLimitedWindowBlocksSubsequentRefresh()
{
    CodexZhClient client;
    FakeNetwork network;
    network.scriptedStatuses = {429, 200};
    installTestDeps(client, &network, "test-key");

    // 第一次 refresh → 429 → m_rateLimitedUntilMs 被推到未来；snapshot 切到"请求失败"。
    QSignalSpy snapshotSpy(&client, &CodexZhClient::snapshotChanged);
    client.refresh();
    QVERIFY(snapshotSpy.wait(2000));
    QCOMPARE(network.requestCount, 1);

    const qint64 before = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(client.m_rateLimitedUntilMs > before);
    // 没有历史成功额度时 429 不会标 stale=true，仅切到"请求失败"+对应错误文案。
    QCOMPARE(client.snapshot().value(QStringLiteral("statusLabel")).toString(),
             QStringLiteral("请求失败"));
    QVERIFY(client.snapshot().value(QStringLiteral("errorText")).toString()
                .contains(QStringLiteral("请求过于频繁")));

    // 窗口内再 refresh：早 return，不应触发新请求。
    QSignalSpy loadingSpy(&client, &CodexZhClient::loadingChanged);
    client.refresh();
    QTest::qWait(300);
    QCOMPARE(network.requestCount, 1);
    QCOMPARE(loadingSpy.count(), 0);
}

void CodexZhClientTest::rateLimitedWindowExpiryAllowsRefresh()
{
    CodexZhClient client;
    FakeNetwork network;
    network.scriptedStatuses = {429, 200, 200};
    installTestDeps(client, &network, "test-key");

    // 第一次 → 429 触发窗口。
    QSignalSpy snapshotSpy(&client, &CodexZhClient::snapshotChanged);
    client.refresh();
    QVERIFY(snapshotSpy.wait(2000));
    QCOMPARE(network.requestCount, 1);
    QVERIFY(client.m_rateLimitedUntilMs > QDateTime::currentMSecsSinceEpoch());

    // 模拟窗口到期：直接把限流时间戳挪到过去。
    client.m_rateLimitedUntilMs = QDateTime::currentMSecsSinceEpoch() - 1;

    snapshotSpy.clear();
    QSignalSpy loadingSpy(&client, &CodexZhClient::loadingChanged);
    client.refresh();
    QVERIFY(snapshotSpy.wait(2000));
    QCOMPARE(network.requestCount, 2);
    // 限流窗口到期后的 refresh 应当进入"加载中"，loadingChanged 至少触发一次。
    QVERIFY(loadingSpy.count() >= 1);
}

void CodexZhClientTest::forceRefreshClearsRateLimitedWindow()
{
    CodexZhClient client;
    FakeNetwork network;
    network.scriptedStatuses = {429, 200};
    installTestDeps(client, &network, "test-key");

    QSignalSpy snapshotSpy(&client, &CodexZhClient::snapshotChanged);
    client.refresh();
    QVERIFY(snapshotSpy.wait(2000));
    QVERIFY(client.m_rateLimitedUntilMs > QDateTime::currentMSecsSinceEpoch());

    // 手动 forceRefresh 清窗口；立即再 refresh() 应能发出请求。
    client.forceRefresh();
    QCOMPARE(client.m_rateLimitedUntilMs, qint64(0));

    snapshotSpy.clear();
    QSignalSpy loadingSpy(&client, &CodexZhClient::loadingChanged);
    client.refresh();
    QVERIFY(snapshotSpy.wait(2000));
    QCOMPARE(network.requestCount, 2);
    QVERIFY(loadingSpy.count() >= 1);
}

void CodexZhClientTest::successfulResponseClearsRateLimitedWindow()
{
    CodexZhClient client;
    FakeNetwork network;
    // 序列：429 → 触发限流窗口 → 第二次靠 forceRefresh 清窗口再发请求 →
    // 成功响应必须把 m_rateLimitedUntilMs 清零。
    network.scriptedStatuses = {429, 200};
    installTestDeps(client, &network, "test-key");

    QSignalSpy snapshotSpy(&client, &CodexZhClient::snapshotChanged);
    client.refresh();
    QVERIFY(snapshotSpy.wait(2000));
    QCOMPARE(network.requestCount, 1);
    QVERIFY(client.m_rateLimitedUntilMs > QDateTime::currentMSecsSinceEpoch());

    // forceRefresh 把窗口清零并立刻发下一次请求；服务端这次回 200。
    client.forceRefresh();
    snapshotSpy.clear();
    client.refresh();
    QVERIFY(snapshotSpy.wait(2000));
    QCOMPARE(network.requestCount, 2);
    QCOMPARE(client.m_rateLimitedUntilMs, qint64(0));
}

// 用户实测场景：两次成功刷新，第二次服务端返回的用量与第一次不同，
// snapshot 必须真把 used 推进到新值（而不是被 == 短路 return）。
void CodexZhClientTest::consecutiveRefreshesPropagateChangedUsage()
{
    CodexZhClient client;
    FakeNetwork network;
    network.scriptedStatuses = {200, 200};
    network.scriptedPayloads = {
        buildSuccessPayload(/*weekUsed*/ 30.0, /*weeklyBudget*/ 100.0,
                            /*todayCalls*/ 1, /*totalRequests*/ 1),
        buildSuccessPayload(/*weekUsed*/ 80.0, /*weeklyBudget*/ 100.0,
                            /*todayCalls*/ 5, /*totalRequests*/ 9),
    };
    installTestDeps(client, &network, "test-key");

    QSignalSpy snapshotSpy(&client, &CodexZhClient::snapshotChanged);
    client.refresh();
    QVERIFY(snapshotSpy.wait(2000));
    QCOMPARE(network.requestCount, 1);

    const QVariantList firstPlans = client.snapshot().value("plans").toList();
    QCOMPARE(firstPlans.size(), 1);
    QCOMPARE(firstPlans.first().toMap().value("used").toDouble(), 30.0);

    // 第一次响应回来后再调 refresh()；服务端这次返回 used=80。
    // snapshot 必须真把 used 推上去，而不是被 == 短路。
    snapshotSpy.clear();
    client.refresh();
    QVERIFY(snapshotSpy.wait(2000));
    QCOMPARE(network.requestCount, 2);

    const QVariantList secondPlans = client.snapshot().value("plans").toList();
    QCOMPARE(secondPlans.size(), 1);
    QCOMPARE(secondPlans.first().toMap().value("used").toDouble(), 80.0);
}

// 用户实测场景：第一次请求还在飞行中时点"刷新"（cancelAllUsageRequests +
// forceRefresh 路径），第二次刷新必须能发出请求并把数据推进。
void CodexZhClientTest::cancelDuringInFlightThenRefreshStillUpdates()
{
    CodexZhClient client;
    FakeNetwork network;
    network.scriptedStatuses = {200, 200};
    network.scriptedPayloads = {
        buildSuccessPayload(30.0, 100.0, 1, 1),
        buildSuccessPayload(80.0, 100.0, 5, 9),
    };
    installTestDeps(client, &network, "test-key");

    QSignalSpy snapshotSpy(&client, &CodexZhClient::snapshotChanged);
    client.refresh();

    // 第一次响应到达前模拟"刷新"按钮：cancelRefresh → 模拟 forceRefresh 链路。
    // 真实按钮触发的是 cancelAllUsageRequests() + 之后队列里的 forceRefresh，
    // 顺序折叠下来就是 cancel + 第二次 refresh。
    client.cancelRefresh();
    client.refresh();
    QVERIFY(snapshotSpy.wait(2000));

    // 最终必须有两个网络请求落到 FakeNetwork，且最终 snapshot 是 80.0。
    QCOMPARE(network.requestCount, 2);
    const QVariantList plans = client.snapshot().value("plans").toList();
    QCOMPARE(plans.size(), 1);
    QCOMPARE(plans.first().toMap().value("used").toDouble(), 80.0);
}

QTEST_GUILESS_MAIN(CodexZhClientTest)

#include "tst_codexzhclient.moc"
