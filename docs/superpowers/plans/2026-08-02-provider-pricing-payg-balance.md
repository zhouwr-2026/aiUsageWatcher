# 供应商价格与按量计费余额 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 厂商配置页支持「套餐/订阅价格」与按量计费（DeepSeek）的充值金额/充值时间配置；悬浮面板显示厂商价格、底部总价、DeepSeek 余额/已用/充值信息，兼容柱状图与饼图。

**Architecture:** DeepSeek 查询走 C++ 客户端（GET `https://api.deepseek.com/user/balance`，Bearer API Key 存 KDE 钱包，快照带 `remaining` 字段）；JS 数据层（displayProvider）负责 payg 推算（已用 = 充值金额 − 余额）与总价累计；QML 配置页与面板消费 display 数据，紧凑视图零改动（纯数据驱动）。

**Tech Stack:** Qt6 / KDE Plasma 6 / QML / KConfig XT / KWallet / QtTest / qmltestrunner

## 全局约束

- 快照 plan 新增可选字段 `remaining`（number），其余 snapshot 生产者不传即不受影响
- DeepSeek 端点固定 `https://api.deepseek.com/user/balance`，无多端点候选
- 错误文案中文：「鉴权失败 (HTTP xxx)」「接口错误 (HTTP xxx)」「余额不足」；分类逻辑照抄 cc-switch balance.rs（401/403 确定性立即透出，其余非 2xx 确定性，网络/超时瞬时）
- `currency` 解析：`CNY` → `"元"`，其余币种保留 API 原值
- payg 推算：`used = clamp(topUpAmount − remaining, 0, topUpAmount)`；`remaining ≥ topUpAmount` 时进度条 0% 并标注「本次充值未消耗」
- 余额字段兼容字符串/数字两种 JSON 类型
- 价格/充值金额输入控件用 `TextField` + 校验器（两位小数），非必填
- 固定厂商严格相等校验需剔除 `price`/`topUpAmount`/`topUpDate` 三个用户字段
- deepseek 预设 template 为 `"%1 限额  %2/%3"`（无「重置于」段）
- 测试：C++ 用 QtTest（QTEST_GUILESS_MAIN），JS 用 qmltestrunner（`tests/tst_displayProvider.qml`，`-import package/contents/ui`）
- 每个任务结束后必须跑对应测试并提交；提交只含本任务文件（用 `git add <文件列表>` 精确暂存，勿 `git add -A`，工作区有他人 staged 的 minimax 改动）

---

### Task 1: DeepSeek 响应解析器（TDD）

**Files:**
- Create: `src/deepseekresponseparser.h`
- Create: `src/deepseekresponseparser.cpp`
- Create: `tests/cpp/tst_deepseekresponseparser.cpp`
- Modify: `CMakeLists.txt`（BUILD_TESTING 块，参照 minimaxresponseparser 注册，约 77-84 行）

**Interfaces:**
- Produces:
  - `struct DeepSeekBalance { QString currency; double totalBalance = 0; double grantedBalance = 0; double toppedUpBalance = 0; bool isAvailable = true; }`
  - `struct DeepSeekParseResult { bool ok = false; QString errorMessage; QList<DeepSeekBalance> balances; }`
  - `class DeepSeekResponseParser { static DeepSeekParseResult parse(QByteArrayView payload, int httpStatus); }`

- [ ] **Step 1: 写头文件**

```cpp
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArrayView>
#include <QList>
#include <QString>

struct DeepSeekBalance
{
    QString currency;
    double totalBalance = 0;
    double grantedBalance = 0;
    double toppedUpBalance = 0;
    bool isAvailable = true;
};

struct DeepSeekParseResult
{
    bool ok = false;
    QString errorMessage;
    QList<DeepSeekBalance> balances;
};

class DeepSeekResponseParser
{
public:
    // payload: HTTP 响应体；httpStatus: HTTP 状态码
    // 401/403 → ok=false, "鉴权失败 (HTTP xxx)"
    // 其他非 2xx → ok=false, "接口错误 (HTTP xxx)"
    // 2xx 且非法 JSON → ok=false, "DeepSeek 返回了无法识别的数据"
    static DeepSeekParseResult parse(QByteArrayView payload, int httpStatus);
};
```

- [ ] **Step 2: 写失败测试**

```cpp
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
```

- [ ] **Step 3: 注册测试到 CMakeLists.txt**

在 `tests/cpp/tst_minimaxresponseparser.cpp` 注册块后追加：

```cmake
    add_executable(tst_deepseekresponseparser
        tests/cpp/tst_deepseekresponseparser.cpp
        src/deepseekresponseparser.cpp
        src/deepseekresponseparser.h
    )
    target_include_directories(tst_deepseekresponseparser PRIVATE src)
    target_link_libraries(tst_deepseekresponseparser PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME deepseek-response-parser COMMAND tst_deepseekresponseparser)
```

- [ ] **Step 4: 跑测试确认失败**

Run: `cmake -S . -B build-test -DBUILD_TESTING=ON && cmake --build build-test -j2 --target tst_deepseekresponseparser && ./build-test/tst_deepseekresponseparser`
Expected: 编译失败（缺 `deepseekresponseparser.h`）或链接失败

- [ ] **Step 5: 写最小实现**

```cpp
// SPDX-License-Identifier: GPL-2.0-or-later

#include "deepseekresponseparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include <cmath>
#include <limits>

namespace
{
DeepSeekParseResult failure(const QString &message)
{
    return {false, message, {}};
}

bool readAmount(const QJsonValue &value, double &result)
{
    if (value.isDouble()) {
        const double number = value.toDouble();
        if (!std::isfinite(number) || number < 0) {
            return false;
        }
        result = number;
        return true;
    }
    if (value.isString()) {
        bool ok = false;
        const double number = value.toString().toDouble(&ok);
        if (!ok || !std::isfinite(number) || number < 0) {
            return false;
        }
        result = number;
        return true;
    }
    return false;
}

bool readBalance(const QJsonObject &object, DeepSeekBalance &balance)
{
    double total = 0;
    if (!readAmount(object.value(QStringLiteral("total_balance")), total)) {
        return false;
    }
    balance.currency = object.value(QStringLiteral("currency"))
                           .toString(QStringLiteral("CNY"));
    balance.totalBalance = total;
    balance.grantedBalance = object.value(QStringLiteral("granted_balance"))
                                .toDouble(0);
    balance.toppedUpBalance = object.value(QStringLiteral("topped_up_balance"))
                                  .toDouble(0);
    return true;
}
} // namespace

DeepSeekParseResult DeepSeekResponseParser::parse(QByteArrayView payload, int httpStatus)
{
    if (httpStatus == 401 || httpStatus == 403) {
        return failure(QStringLiteral("鉴权失败 (HTTP %1)").arg(httpStatus));
    }
    if (httpStatus < 200 || httpStatus >= 300) {
        return failure(QStringLiteral("接口错误 (HTTP %1)").arg(httpStatus));
    }

    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
        return failure(QStringLiteral("DeepSeek 返回了无法识别的数据"));
    }
    const QJsonObject body = document.object();
    const bool isAvailable = body.value(QStringLiteral("is_available"))
                                 .toBool(true);

    DeepSeekParseResult result;
    result.ok = true;
    const QJsonArray infos = body.value(QStringLiteral("balance_infos"))
                                  .toArray();
    for (const QJsonValue &value : infos) {
        if (!value.isObject()) {
            return failure(QStringLiteral("DeepSeek 返回了无法识别的数据"));
        }
        DeepSeekBalance balance;
        balance.isAvailable = isAvailable;
        if (!readBalance(value.toObject(), balance)) {
            return failure(QStringLiteral("DeepSeek 返回了无法识别的数据"));
        }
        result.balances.append(balance);
    }
    return result;
}
```

- [ ] **Step 6: 跑测试确认通过**

Run: `cmake --build build-test -j2 --target tst_deepseekresponseparser && ./build-test/tst_deepseekresponseparser`
Expected: 8/8 PASS

- [ ] **Step 7: 提交**

```bash
git add src/deepseekresponseparser.h src/deepseekresponseparser.cpp tests/cpp/tst_deepseekresponseparser.cpp CMakeLists.txt
git commit -m "feat: add DeepSeek balance response parser"
```

---

### Task 2: DeepSeek 客户端（契约 + 请求构造）

**Files:**
- Create: `src/deepseekclient.h`
- Create: `src/deepseekclient.cpp`
- Create: `tests/cpp/tst_deepseekclient.cpp`
- Modify: `CMakeLists.txt`（追加 tst_deepseekclient 注册，参照 tst_minimaxclient 块约 86-95 行）

**Interfaces:**
- Consumes: `DeepSeekResponseParser::parse(QByteArrayView, int)`（Task 1）；`DeepSeekBalance`
- Produces:
  - `class DeepSeekClient : public QObject`，Q_PROPERTY `snapshot`(QVariantMap) / `loading` / `credentialConfigured` / `credentialStatus` / `credentialBusy` / `credentialError`，NOTIFY 同名 `*Changed`
  - Q_INVOKABLE `refresh()` / `saveCredential(const QString &apiKey)` / `clearCredential()`
  - `static QNetworkRequest createRequest(const QUrl &url, QByteArrayView apiKey)`（Authorization: Bearer、Accept: application/json、SameOriginRedirectPolicy）
  - snapshot 结构：`{providerId: "deepseek", statusLabel, errorText, plans: [{planId: "balance", planName: "账户余额", used: -1, total: -1, remaining: <totalBalance>, unit: <currency 或 "元">, resetText: "", resetAt: 0, extraText: "", isValid: <isAvailable>, invalidReason: isValid ? "" : "余额不足"}]}`
  - `unit` 映射：`currency == "CNY"` → `"元"`，否则保留原值
  - 空 balances → statusLabel 沿用「暂无数据」语义（参照 minimaxclient 空快照模式）
  - 钱包 folder：`quota-pilot`（与 minimax 共享根 folder，key 名 `deepseek-api-key`）

- [ ] **Step 1: 写头文件**

```cpp
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArrayView>
#include <QObject>
#include <QNetworkRequest>
#include <QString>
#include <QUrl>
#include <QVariantMap>

class QNetworkAccessManager;
class QNetworkReply;
namespace KWallet
{
class Wallet;
}

class DeepSeekClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool credentialConfigured READ credentialConfigured NOTIFY credentialConfiguredChanged)
    Q_PROPERTY(QString credentialStatus READ credentialStatus NOTIFY credentialStatusChanged)
    Q_PROPERTY(bool credentialBusy READ credentialBusy NOTIFY credentialBusyChanged)
    Q_PROPERTY(bool credentialError READ credentialError NOTIFY credentialErrorChanged)

public:
    explicit DeepSeekClient(QObject *parent = nullptr);
    ~DeepSeekClient() override;

    QVariantMap snapshot() const;
    bool loading() const;
    bool credentialConfigured() const;
    QString credentialStatus() const;
    bool credentialBusy() const;
    bool credentialError() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void saveCredential(const QString &apiKey);
    Q_INVOKABLE void clearCredential();

    static QUrl balanceEndpoint();
    static QNetworkRequest createRequest(const QUrl &url, QByteArrayView apiKey);

Q_SIGNALS:
    void snapshotChanged();
    void loadingChanged();
    void credentialConfiguredChanged();
    void credentialStatusChanged();
    void credentialBusyChanged();
    void credentialErrorChanged();

private:
    enum class PendingCredentialOperation {
        None,
        Save,
        Clear,
    };

    void openWallet();
    bool prepareWalletFolder();
    void loadCredential();
    void performPendingCredentialOperation();
    void setStoredApiKey(const QByteArray &apiKey);
    void setCredentialState(const QString &status, bool busy, bool error);
    void setLoading(bool loading);
    void setError(const QString &message);
    void setSnapshot(const QVariantMap &snapshot);
    void finishRefresh();

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    KWallet::Wallet *m_wallet = nullptr;
    QByteArray m_storedApiKey;
    QByteArray m_activeApiKey;
    QString m_lastRequestError;
    QString m_pendingApiKey;
    QString m_credentialStatus;
    QVariantMap m_snapshot;
    PendingCredentialOperation m_pendingCredentialOperation = PendingCredentialOperation::None;
    bool m_loading = false;
    bool m_walletOpening = false;
    bool m_credentialBusy = false;
    bool m_credentialError = false;
};
```

- [ ] **Step 2: 写失败测试**

```cpp
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
```

- [ ] **Step 3: 注册测试到 CMakeLists.txt**

在 tst_minimaxclient 注册块后追加：

```cmake
    add_executable(tst_deepseekclient
        tests/cpp/tst_deepseekclient.cpp
        src/deepseekclient.cpp
        src/deepseekclient.h
        src/deepseekresponseparser.cpp
        src/deepseekresponseparser.h
    )
    target_include_directories(tst_deepseekclient PRIVATE src)
    target_link_libraries(tst_deepseekclient PRIVATE Qt6::Core Qt6::Network Qt6::Test KF6::Wallet)
    add_test(NAME deepseek-client-contract COMMAND tst_deepseekclient)
```

- [ ] **Step 4: 跑测试确认失败**

Run: `cmake -S . -B build-test -DBUILD_TESTING=ON && cmake --build build-test -j2 --target tst_deepseekclient && ./build-test/tst_deepseekclient`
Expected: 编译失败（缺头文件）

- [ ] **Step 5: 写客户端实现**

参照 `src/minimaxclient.cpp` 完整模式实现（钱包打开/加载/保存/清除、refresh 状态机、错误透出、snapshot 组装），关键差异：

```cpp
// deepseekclient.cpp 关键片段（其余照 minimaxclient.cpp 同构实现）

QUrl DeepSeekClient::balanceEndpoint()
{
    return QUrl(QStringLiteral("https://api.deepseek.com/user/balance"));
}

QNetworkRequest DeepSeekClient::createRequest(const QUrl &url, QByteArrayView apiKey)
{
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey);
    request.setRawHeader("Accept", QByteArray("application/json"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);
    return request;
}

// refresh() 回调里组装快照（语义对齐设计文档）：
//  - DeepSeekResponseParser::parse(reply->readAll(), status) → result
//  - result.ok=false → setError(result.errorMessage)
//  - result.balances 取第一项组装 plan：
//      remaining = balance.totalBalance
//      unit = balance.currency == "CNY" ? "元" : balance.currency
//      isValid = balance.isAvailable
//      invalidReason = balance.isAvailable ? "" : "余额不足"
//      used = -1; total = -1  （display 层按充值金额推算）
//  - balances 为空 → snapshot 带空 plans + statusLabel "暂无数据"
//  - 钱包 folder "quota-pilot"，key "deepseek-api-key"
```

> 说明：实现者必须打开 `src/minimaxclient.cpp` 逐段对照抄写骨架（网络管理器、钱包、状态机、finishRefresh 的 HTTP 状态读取），仅替换端点/解析/快照组装三处。minimaxclient.cpp 中 `networkErrorMessage()` 的 429/5xx 文案逻辑不适用（DeepSeek 错误统一走解析器中文映射）。

- [ ] **Step 6: 跑测试确认通过**

Run: `cmake --build build-test -j2 --target tst_deepseekclient && ./build-test/tst_deepseekclient`
Expected: 4/4 PASS

- [ ] **Step 7: 提交**

```bash
git add src/deepseekclient.h src/deepseekclient.cpp tests/cpp/tst_deepseekclient.cpp CMakeLists.txt
git commit -m "feat: add DeepSeek balance client"
```

---

### Task 3: applet 注册与 main.qml 接入

**Files:**
- Modify: `src/aiusagewatcherapplet.h`（参照 miniMax 属性/方法/信号块 24-29、54-59、79-81、102-106 行）
- Modify: `src/aiusagewatcherapplet.cpp`（成员创建、转发方法）
- Modify: `package/contents/ui/main.qml`（applyDeepSeekSnapshot / requestDeepSeekRefresh / Connections / refresh()）

**Interfaces:**
- Consumes: `DeepSeekClient`（Task 2）
- Produces: applet 对 QML 暴露 `deepseekSnapshot`(QVariantMap) / `deepseekLoading` / `deepseekCredentialConfigured` / `deepseekCredentialStatus` / `deepseekCredentialBusy` / `deepseekCredentialError` 属性 + 信号，`saveDeepSeekApiKey(QString)` / `clearDeepSeekApiKey()` / `refreshDeepSeekUsage()` 方法

- [ ] **Step 1: applet 头文件扩展**

参照 minimax 块逐项追加 deepseek 版（Q_PROPERTY ×6、getter ×6、Q_INVOKABLE ×3、signal ×6），getter 全部转发到 `m_deepSeekClient`，示例：

```cpp
    Q_PROPERTY(QVariantMap deepseekSnapshot READ deepseekSnapshot NOTIFY deepseekSnapshotChanged)
    Q_PROPERTY(bool deepseekCredentialConfigured READ deepseekCredentialConfigured NOTIFY deepseekCredentialConfiguredChanged)
    Q_INVOKABLE void saveDeepSeekApiKey(const QString &apiKey);
    Q_INVOKABLE void clearDeepSeekApiKey();
    Q_INVOKABLE void refreshDeepSeekUsage();
Q_SIGNALS:
    void deepseekSnapshotChanged();
    void deepseekCredentialConfiguredChanged();
    // ... 其余信号
```

- [ ] **Step 2: applet 实现**

参照 `aiusagewatcherapplet.cpp` 中 MiniMaxClient 的创建、信号连接（`connect(m_miniMaxClient, &MiniMaxClient::snapshotChanged, this, &...::miniMaxSnapshotChanged)`）、方法转发模式，完整追加 `m_deepSeekClient`（`std::unique_ptr<DeepSeekClient>`）对应代码。包含 `"deepseekclient.h"`。

- [ ] **Step 3: main.qml 接入**

参照 MiniMax 全套（main.qml 82-99 行），追加：

```qml
    function applyDeepSeekSnapshot() {
        const snapshot = usageBackend["deepseekSnapshot"]
        if (!snapshot || snapshot.providerId !== "deepseek"
                || !snapshot.plans || typeof snapshot.plans.length !== "number")
            return false
        runtimeSnapshots = ProviderNormalize.replaceSnapshot(runtimeSnapshots, snapshot)
        lastRefreshTime = new Date()
        return true
    }

    function requestDeepSeekRefresh() {
        const refreshFunction = usageBackend["refreshDeepSeekUsage"]
        if (typeof refreshFunction !== "function")
            return false
        refreshFunction.call(usageBackend)
        return true
    }
```

并在 `Component.onCompleted`、`onProviderDefinitionsChanged`、`refresh()` 三处调用 `applyDeepSeekSnapshot()` + `requestDeepSeekRefresh()`；`Connections` 块加 `function onDeepseekSnapshotChanged() { root.applyDeepSeekSnapshot() }`。

- [ ] **Step 4: 构建验证**

Run: `cmake -S . -B build-test -DBUILD_TESTING=ON && cmake --build build-test -j2`
Expected: 编译通过（含 6 个 C++ 测试 target）

Run: `./build-test/tst_deepseekclient && ./build-test/tst_deepseekresponseparser`
Expected: 全部 PASS

- [ ] **Step 5: 提交**

```bash
git add src/aiusagewatcherapplet.h src/aiusagewatcherapplet.cpp package/contents/ui/main.qml
git commit -m "feat: wire DeepSeek client into applet and panel"
```

---

### Task 4: JS 数据层（预设 + 归一化 + 校验 + payg 推算 + 总价）

**Files:**
- Modify: `package/contents/js/providerCatalog.js`（PRESETS 追加 deepseek 预设；`_LOGO_ASSETS` 暂不加，走 logoChar fallback）
- Modify: `package/contents/js/providerNormalize.js`（固定厂商分支保留 price/topUp 字段；自定义分支透传）
- Modify: `package/contents/js/providerConfig.js`（三字段校验；固定厂商比较剔除用户字段）
- Modify: `package/contents/js/displayProvider.js`（buildDisplay 透传；_displayPlan payg；totalPrice()）
- Create: `tests/tst_displayProvider.qml`
- Modify: `tests/run-plasma-smoke.sh`（追加 tst_displayProvider 执行）

**Interfaces:**
- Consumes: 快照契约（Task 2）：`plans[].remaining`(number, 可选)
- Produces:
  - `displayProvider.totalPrice(displayProviders) → number`（Σ price + Σ topUpAmount，仅启用厂商）
  - display provider 扩展字段：`price`(number|undefined)、`topUpAmount`(number|undefined)、`topUpDate`(string|undefined)
  - `_displayPlan` 对 `catalogId === "deepseek"` 且 planId `"balance"` 的 plan：有 `remaining` 与 `definition.topUpAmount > 0` 时 `total = topUpAmount`、`used = clamp(...)`；`remaining >= topUpAmount` 时 extraText 含「本次充值未消耗」；`topUpDate` 为今天时 extraText 前缀「今日已用 ¥x」否则「自充值以来已用 ¥x」；无充值金额时 `extraText = "余额 ¥x"`

- [ ] **Step 1: providerCatalog 预设**

在 `PRESETS` 数组末尾（codexzh 后）追加：

```js
, {
    "catalogId": "deepseek",
    "label": "DeepSeek",
    "id": "deepseek",
    "providerName": "DeepSeek",
    "vendor": "DeepSeek",
    "website": "https://platform.deepseek.com/",
    "sourceLabel": "余额",
    "template": "%1 限额  %2/%3",
    "plans": [{ "id": "balance", "planName": "账户余额", "unit": "元" }]
}
```

- [ ] **Step 2: providerNormalize 保留/透传字段**

固定厂商分支（约 47-52 行）改为：

```js
            // 内置 catalog 用预设，但保留用户的 enabled / logoPath / 价格与充值字段
            return Object.assign({}, fixedDefinition, {
                "enabled": definition.enabled !== false,
                "logoPath": (typeof definition.logoPath === "string" && definition.logoPath.length > 0)
                    ? definition.logoPath : fixedDefinition.logoPath || "",
                "price": _isFiniteNumber(definition.price) ? definition.price : 0,
                "topUpAmount": _isFiniteNumber(definition.topUpAmount) ? definition.topUpAmount : 0,
                "topUpDate": typeof definition.topUpDate === "string" ? definition.topUpDate : ""
            });
```

自定义分支（约 67-107 行）返回值同样追加三字段（`_isFiniteNumber` 归一化 + `topUpDate` 字符串透传）。

- [ ] **Step 3: providerConfig 校验**

`validateProvider` 固定厂商分支（约 34-40 行）改为剔除用户字段再比较：

```js
    if (catalogId !== ProviderCatalog.CUSTOM_ID) {
        var preset = ProviderCatalog.definitionFor(catalogId)
        var candidateForCompare = JSON.parse(JSON.stringify(candidate))
        delete candidateForCompare.price
        delete candidateForCompare.topUpAmount
        delete candidateForCompare.topUpDate
        if (!preset || JSON.stringify(candidateForCompare) !== JSON.stringify(preset))
            return { valid: false, message: "固定厂商信息必须使用内置预设" }
        return { valid: true, message: "" }
    }
```

并在 providerName 校验后追加三字段校验：

```js
    var price = typeof candidate.price === "number" ? candidate.price : NaN
    if (candidate.price !== undefined && candidate.price !== null
            && (!isFinite(price) || price < 0))
        return { valid: false, message: "套餐价格必须为非负数字或留空" }
    var topUpAmount = typeof candidate.topUpAmount === "number" ? candidate.topUpAmount : NaN
    if (candidate.topUpAmount !== undefined && candidate.topUpAmount !== null
            && (!isFinite(topUpAmount) || topUpAmount < 0))
        return { valid: false, message: "充值金额必须为非负数字或留空" }
    var topUpDate = typeof candidate.topUpDate === "string" ? candidate.topUpDate.trim() : ""
    if (topUpDate && !/^\d{4}-\d{2}-\d{2}$/.test(topUpDate))
        return { valid: false, message: "充值时间必须使用 YYYY-MM-DD 格式" }
```

- [ ] **Step 4: displayProvider payg 推算与总价**

`_displayPlan`（约 187-221 行）在 `var displayPlan = {...}` 后追加 deepseek 特判：

```js
    if (definition.catalogId === "deepseek" && snapshotPlan.planId === "balance") {
        var remaining = _isFiniteNumber(snapshotPlan.remaining) ? snapshotPlan.remaining : -1
        var topUp = _isFiniteNumber(definition.topUpAmount) ? definition.topUpAmount : 0
        if (topUp > 0 && remaining >= 0) {
            var used = Math.max(0, Math.min(topUp, topUp - remaining))
            var percent = Math.round(used / topUp * 100)
            displayPlan.usedPercent = percent
            displayPlan.usedPercentLabel = percent + "%"
            displayPlan.usedText = "¥" + used.toFixed(2)
            displayPlan.totalText = "¥" + topUp.toFixed(2)
            displayPlan.isInvalid = false
            displayPlan.barClass = _usageClass(percent)
            var paygSegments = []
            if (used > 0) {
                var dateText = typeof definition.topUpDate === "string" ? definition.topUpDate : ""
                var today = new Date()
                var todayMonth = today.getMonth() + 1
                var todayDay = today.getDate()
                var todayText = today.getFullYear() + "-"
                    + (todayMonth < 10 ? "0" + todayMonth : todayMonth) + "-"
                    + (todayDay < 10 ? "0" + todayDay : todayDay)
                var usedLabel = dateText === todayText
                    ? "今日已用 ¥" + used.toFixed(2)
                    : "自充值以来已用 ¥" + used.toFixed(2)
                paygSegments.push("剩余 ¥" + remaining.toFixed(2))
                if (dateText)
                    paygSegments.push("充值 " + dateText.substring(5))
                paygSegments.push(usedLabel)
            } else {
                paygSegments.push("剩余 ¥" + remaining.toFixed(2))
                if (typeof definition.topUpDate === "string" && definition.topUpDate)
                    paygSegments.push("充值 " + definition.topUpDate.substring(5))
                paygSegments.push("本次充值未消耗")
            }
            displayPlan.extraText = paygSegments.join(" | ")
        } else if (remaining >= 0) {
            displayPlan.extraText = "余额 ¥" + remaining.toFixed(2)
        }
    }
```

`buildDisplay` 的返回对象（约 278-297 行）追加三字段：

```js
            price: _isFiniteNumber(definition.price) ? definition.price : undefined,
            topUpAmount: _isFiniteNumber(definition.topUpAmount) ? definition.topUpAmount : undefined,
            topUpDate: typeof definition.topUpDate === "string" ? definition.topUpDate : undefined,
```

文件末尾新增：

```js
function totalPrice(displayProviders) {
    if (!Array.isArray(displayProviders))
        return 0
    var total = 0
    for (var i = 0; i < displayProviders.length; ++i) {
        var provider = displayProviders[i]
        if (!provider || provider.enabled === false)
            continue
        if (_isFiniteNumber(provider.price) && provider.price > 0)
            total += provider.price
        else if (_isFiniteNumber(provider.topUpAmount) && provider.topUpAmount > 0)
            total += provider.topUpAmount
    }
    return Math.round(total * 100) / 100
}
```

> 说明：`provider.enabled` 在 buildDisplay 过滤后恒为 true，totalPrice 的 enabled 判断为防御性冗余，保留无妨（`filterEnabled` 已过滤）。

- [ ] **Step 5: 写 QML 测试 tst_displayProvider.qml**

```qml
import QtQuick
import QtTest
import "../package/contents/js/providerCatalog.js" as ProviderCatalog
import "../package/contents/js/providerNormalize.js" as ProviderNormalize
import "../package/contents/js/displayProvider.js" as DisplayProvider

Item {
    id: host

    width: 320
    height: 520

    function deepSeekDefinition() {
        const def = ProviderCatalog.definitionFor("deepseek")
        def.topUpAmount = 100
        def.topUpDate = "2026-08-01"
        return def
    }

    function buildWith(topUpAmount, remaining, topUpDate) {
        const def = deepSeekDefinition()
        def.topUpAmount = topUpAmount
        def.topUpDate = topUpDate || ""
        const snapshots = [{
            providerId: "deepseek",
            statusLabel: "可用",
            errorText: "",
            plans: [{
                planId: "balance",
                planName: "账户余额",
                used: -1,
                total: -1,
                remaining: remaining,
                unit: "元",
                resetText: "",
                resetAt: 0,
                extraText: "",
                isValid: true,
                invalidReason: ""
            }]
        }]
        return DisplayProvider.buildDisplay([def], snapshots, { sortMode: "default" })[0]
    }

    function planOf(display) {
        return display.plans[0]
    }

    TestCase {
        name: "PaygInference"
        when: windowShown

        function test_usedIsTopUpMinusRemaining() {
            const display = buildWith(100, 87.5, "2026-08-01")
            const plan = planOf(display)
            compare(plan.usedPercent, 13)
            compare(plan.usedText, "¥12.50")
            compare(plan.totalText, "¥100.00")
            verify(plan.extraText.indexOf("剩余 ¥87.50") >= 0)
            verify(plan.extraText.indexOf("充值 08-01") >= 0)
            verify(plan.extraText.indexOf("自充值以来已用 ¥12.50") >= 0)
        }

        function test_remainingAboveTopUpIsUnconsumed() {
            const display = buildWith(100, 120, "2026-08-01")
            const plan = planOf(display)
            compare(plan.usedPercent, 0)
            compare(plan.usedText, "¥0.00")
            verify(plan.extraText.indexOf("本次充值未消耗") >= 0)
        }

        function test_noTopUpShowsBalanceOnly() {
            const display = buildWith(0, 87.5, "")
            const plan = planOf(display)
            compare(plan.usedPercent, -1)
            compare(plan.extraText, "余额 ¥87.50")
        }

        function test_remainingInvalidKeepsGray() {
            const display = buildWith(100, -1, "")
            const plan = planOf(display)
            compare(plan.usedPercent, -1)
            compare(plan.barClass, "bar-gray")
        }
    }

    TestCase {
        name: "TotalPrice"
        when: windowShown

        function test_sumsPriceAndTopUp() {
            const priced = ProviderCatalog.definitionFor("minimax")
            priced.price = 30
            const payg = deepSeekDefinition()          // topUpAmount = 100
            const displayed = DisplayProvider.buildDisplay([priced, payg], [], { sortMode: "default" })
            compare(DisplayProvider.totalPrice(displayed), 130)
        }

        function test_ignoresZeroAndMissing() {
            const priced = ProviderCatalog.definitionFor("codex")   // 无 price
            const zero = ProviderCatalog.definitionFor("minimax")
            zero.price = 0
            const displayed = DisplayProvider.buildDisplay([priced, zero], [], { sortMode: "default" })
            compare(DisplayProvider.totalPrice(displayed), 0)
        }
    }
}
```

- [ ] **Step 6: 跑测试确认失败/通过（TDD：先实现已写，直接验证）**

Run: `qmltestrunner -input tests/tst_displayProvider.qml -import package/contents/ui`
Expected: 6/6 PASS（若在 Step 4 前先跑则 FAIL —— 本计划实现先于测试写入，以最终 PASS 为准；TDD 语义由步骤顺序保证）

- [ ] **Step 7: smoke 并入**

在 `tests/run-plasma-smoke.sh` 的 tst_fullView 执行块后追加：

```bash
QT_QPA_PLATFORM=offscreen "$qmltestrunner" \
    -input tests/tst_displayProvider.qml \
    -import package/contents/ui
```

- [ ] **Step 8: 提交**

```bash
git add package/contents/js/providerCatalog.js package/contents/js/providerNormalize.js package/contents/js/providerConfig.js package/contents/js/displayProvider.js tests/tst_displayProvider.qml tests/run-plasma-smoke.sh
git commit -m "feat: add pricing and payg inference to data layer"
```

---

### Task 5: 配置页（价格 + 充值设置 + DeepSeek 凭据）

**Files:**
- Modify: `package/contents/ui/config/ProviderEditor.qml`
- Modify: `package/contents/ui/config/ProvidersConfig.qml`

**Interfaces:**
- Consumes: definition 三字段（Task 4 校验/归一化）
- Produces: `ProviderEditor.candidate` 携带 `price` / `topUpAmount` / `topUpDate`；`isDeepSeek` 只读属性；信号复用现有 `saveApiKeyRequested` / `clearApiKeyRequested` / `refreshDeepSeekRequested`（新增）；ProvidersConfig 新增 `syncDeepSeekState()` 与 `credentialBackendMethod` 的 deepseek 分支

- [ ] **Step 1: ProviderEditor 基础字段**

在 `isCodexZh` 后追加 `readonly property bool isDeepSeek: (candidate.catalogId || "") === "deepseek"`。

在「基本信息」SectionHeading 内（logo 区块后、其余字段附近）追加价格输入行（自定义与固定厂商均可用）：

```qml
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.formWidth
            Layout.maximumWidth: root.formWidth

            FieldLabel { text: qsTr("套餐/订阅价格：") }

            QQC2.TextField {
                objectName: "priceField"
                Layout.preferredWidth: root.fieldWidth
                Layout.maximumWidth: root.fieldWidth
                placeholderText: qsTr("可选，单位 ¥，如 30 或 19.9")
                text: (typeof root.candidate.price === "number" && root.candidate.price > 0)
                    ? String(root.candidate.price) : ""
                validator: QQC2.DoubleValidator {
                    bottom: 0
                    decimals: 2
                }
                onTextChanged: {
                    const parsed = text.trim() === "" ? 0 : Number(text)
                    root.updateField("price", isFinite(parsed) ? parsed : 0)
                }
            }
        }
```

- [ ] **Step 2: ProviderEditor 充值设置小节**

在凭据区块（SectionHeading `%1 API 凭据`）之前插入充值小节（仅 deepseek 可见）：

```qml
        SectionHeading {
            visible: root.isDeepSeek
            text: qsTr("充值设置（可选）")
        }

        GridLayout {
            visible: root.isDeepSeek
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.formWidth
            Layout.maximumWidth: root.formWidth
            columns: 2
            columnSpacing: Kirigami.Units.largeSpacing

            FieldLabel { text: qsTr("充值金额：") }

            QQC2.TextField {
                objectName: "topUpAmountField"
                Layout.preferredWidth: root.fieldWidth
                Layout.maximumWidth: root.fieldWidth
                placeholderText: qsTr("可选，最近一次充值金额，单位 ¥")
                text: (typeof root.candidate.topUpAmount === "number" && root.candidate.topUpAmount > 0)
                    ? String(root.candidate.topUpAmount) : ""
                validator: QQC2.DoubleValidator {
                    bottom: 0
                    decimals: 2
                }
                onTextChanged: {
                    const parsed = text.trim() === "" ? 0 : Number(text)
                    root.updateField("topUpAmount", isFinite(parsed) ? parsed : 0)
                }
            }

            FieldLabel { text: qsTr("充值时间：") }

            QQC2.TextField {
                objectName: "topUpDateField"
                Layout.preferredWidth: root.fieldWidth
                Layout.maximumWidth: root.fieldWidth
                placeholderText: qsTr("YYYY-MM-DD")
                text: (typeof root.candidate.topUpDate === "string") ? root.candidate.topUpDate : ""
                onTextChanged: root.updateField("topUpDate", text.trim())
            }
        }
```

- [ ] **Step 3: ProviderEditor 凭据区块扩展 deepseek**

凭据区块所有 `visible: root.isMiniMax || root.isCodexZh` 改为 `visible: root.isMiniMax || root.isCodexZh || root.isDeepSeek`；SectionHeading 文案三元扩展为三支（`root.isCodexZh ? "CodexZH" : (root.isDeepSeek ? "DeepSeek" : "MiniMax")`）；apiKeyField placeholder/objectName 同理三支；刷新按钮分支加 `refreshDeepSeekRequested()`；新增信号 `signal refreshDeepSeekRequested()`。

- [ ] **Step 4: ProvidersConfig 扩展**

`credentialBackendMethod`（279-290 行）加分支：

```js
        } else if (catalogId === "deepseek") {
            if (action === "save") return "saveDeepSeekApiKey"
            if (action === "clear") return "clearDeepSeekApiKey"
            if (action === "refresh") return "refreshDeepSeekUsage"
        }
```

新增 `syncDeepSeekState()`（参照 `syncCodexZhState` 311-325 行模式，前缀 `deepseek`）与对应属性（`deepseekCredentialConfigured` / `deepseekCredentialBusy` / `deepseekCredentialError` / `deepseekCredentialStatus` / `deepseekUsageLoading` / `deepseekUsageStatus` / `deepseekUsageError`），`connectDeepSeekSignals()`（参照 327-339 行），`Component.onCompleted` 调用，`Connections` 块追加 deepseek 信号（参照 403-421 行），ProviderEditor 绑定追加 deepseek 分支（`providerEditor.isDeepSeek ? root.deepseekCredential* : ...` 参照 614-627 行），信号接线加 `onRefreshDeepSeekRequested`。

- [ ] **Step 5: 构建 + 冒烟验证**

Run: `./tests/run-plasma-smoke.sh`
Expected: PASS（含 tst_fullView 与新增 tst_displayProvider）

- [ ] **Step 6: 提交**

```bash
git add package/contents/ui/config/ProviderEditor.qml package/contents/ui/config/ProvidersConfig.qml
git commit -m "feat: add price, topup and DeepSeek credential fields to config pages"
```

---

### Task 6: 面板显示（厂商价格 + 底部总价）

**Files:**
- Modify: `package/contents/ui/ProviderGroup.qml`
- Modify: `package/contents/ui/FullView.qml`
- Modify: `tests/tst_fullView.qml`

**Interfaces:**
- Consumes: display provider 的 `price` / `topUpAmount` / `topUpDate` 与 `DisplayProvider.totalPrice()`（Task 4）
- Produces: `ProviderGroup.priceText`（字符串，空则不显示）；`FullView` 底部总价标签（objectName `totalPriceLabel`，合计 > 0 才可见）

- [ ] **Step 1: ProviderGroup 价格标签**

新增属性 `property string priceText: ""`；在名称行（websiteMouseArea 之后、sourceLabel 之前，约 129-131 行）插入：

```qml
                PlasmaComponents.Label {
                    objectName: "providerPriceLabel"
                    visible: root.priceText.length > 0
                    text: root.priceText
                    color: Kirigami.Theme.positiveTextColor
                    font: Kirigami.Theme.smallFont
                    font.bold: true
                }
```

- [ ] **Step 2: FullView 绑定价格与总价**

在 `fullView` 的 delegate 绑定处（约 214-230 行）追加：

```qml
                    priceText: (typeof modelData.price === "number" && modelData.price > 0)
                        ? "¥" + modelData.price.toFixed(2) : ""
```

新增只读属性：

```qml
    readonly property string totalPriceText: {
        const total = DisplayProvider.totalPrice(root.providers)
        return total > 0 ? "总价 ¥" + total.toFixed(2) : ""
    }
```

（FullView 顶部 import 追加 `import "../js/displayProvider.js" as DisplayProvider`）

在状态栏（statusLabel 之前，约 260-272 行）插入总价标签：

```qml
        PlasmaComponents.Label {
            objectName: "totalPriceLabel"
            Layout.fillWidth: true
            visible: root.totalPriceText.length > 0
            text: root.totalPriceText
            color: Kirigami.Theme.positiveTextColor
            font: Kirigami.Theme.smallFont
            font.bold: true
        }
```

- [ ] **Step 3: tst_fullView 扩展**

在现有 TestCase 中追加（验证价格标签与总价标签可见性，构造带 price 的 definition）：

```qml
        function test_priceAndTotalVisible() {
            const defs = tokenHubDefinitions()
            defs[0].price = 30                       // minimax
            const providers = DisplayProvider.buildDisplay(
                defs, liveSnapshots(), { sortMode: "default" })
            host.providers = providers
            // 通过 FullView 组件断言：等待渲染后检查 providerPriceLabel / totalPriceLabel
            // 实现时对齐 tst_fullView.qml 现有 FullView 加载与 descendantsNamed 断言模式
        }
```

> 说明：本任务改 `tests/tst_fullView.qml` 时先读该文件现有结构（FullView 实例化方式与 `descendantsNamed` 断言），把 `price`/`topUp` 加入 `tokenHubDefinitions()` 或独立用例，断言 `providerPriceLabel` 文本含 "¥30"、`totalPriceLabel` 文本为 "总价 ¥30.00"。

- [ ] **Step 4: 冒烟验证**

Run: `./tests/run-plasma-smoke.sh`
Expected: PASS（三个 qmltestrunner 用例 + 一致性 diff）

- [ ] **Step 5: 提交**

```bash
git add package/contents/ui/ProviderGroup.qml package/contents/ui/FullView.qml tests/tst_fullView.qml
git commit -m "feat: show provider price and panel total price"
```

---

## 验证清单（全部完成后）

```bash
cmake -S . -B build-test -DBUILD_TESTING=ON && cmake --build build-test -j2 && ctest --test-dir build-test --output-on-failure
./tests/run-plasma-smoke.sh
```

Expected: 全部 C++ 测试 PASS + smoke PASS

手动验证（`plasmawindowed aiUsageWatcher`）：
1. 配置页添加 DeepSeek → API Key 保存（KDE 钱包）→ 充值金额 100 / 充值时间 今天 → 面板显示余额/已用/「今日已用」标注
2. 厂商价格填入 → 面板名称右侧 ¥xx、底部「总价 ¥xx」随 DeepSeek 充值金额累加
3. 紧凑视图两种样式（bar/pie）均正常轮播 DeepSeek（有充值显示百分比，无充值显示 —）
4. 删除 DeepSeek API Key → 面板「未配置」，无崩溃
