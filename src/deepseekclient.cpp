// SPDX-License-Identifier: GPL-2.0-or-later

#include "deepseekclient.h"

#include "deepseekresponseparser.h"
#include "kwalletdispatcher.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QVariantList>

namespace
{
constexpr qsizetype maximumResponseBytes = 1024 * 1024;

QVariantMap emptySnapshot(const QString &status, const QString &error = {})
{
    return {
        {QStringLiteral("providerId"), QStringLiteral("deepseek")},
        {QStringLiteral("statusLabel"), status},
        {QStringLiteral("errorText"), error},
        {QStringLiteral("plans"), QVariantList{}},
    };
}

QVariantMap balanceToPlan(const DeepSeekBalance &balance)
{
    const QString unit = balance.currency == QStringLiteral("CNY")
        ? QStringLiteral("元")
        : balance.currency;
    return QVariantMap{
        {QStringLiteral("planId"), QStringLiteral("balance")},
        {QStringLiteral("planName"), QStringLiteral("账户余额")},
        {QStringLiteral("used"), -1},
        {QStringLiteral("total"), -1},
        {QStringLiteral("remaining"), balance.totalBalance},
        {QStringLiteral("unit"), unit},
        {QStringLiteral("resetText"), QString()},
        {QStringLiteral("resetAt"), 0},
        {QStringLiteral("extraText"), QString()},
        {QStringLiteral("isValid"), balance.isAvailable},
        {QStringLiteral("invalidReason"), balance.isAvailable ? QString() : QStringLiteral("余额不足")},
    };
}

QVariantMap toVariantMap(const QList<DeepSeekBalance> &balances)
{
    QVariantList plans;
    if (!balances.isEmpty()) {
        plans.push_back(balanceToPlan(balances.first()));
    }
    const bool available = !balances.isEmpty() && balances.first().isAvailable;
    return {
        {QStringLiteral("providerId"), QStringLiteral("deepseek")},
        {QStringLiteral("statusLabel"), available
             ? QStringLiteral("可用") : QStringLiteral("余额不足")},
        {QStringLiteral("errorText"), QString()},
        {QStringLiteral("plans"), plans},
    };
}

QString transportErrorMessage(QNetworkReply::NetworkError error)
{
    if (error == QNetworkReply::TimeoutError) {
        return QStringLiteral("DeepSeek 请求超时");
    }
    return QStringLiteral("无法连接 DeepSeek 服务");
}

bool isEnvironmentConfigured()
{
    return !qgetenv("DEEPSEEK_API_KEY").trimmed().isEmpty();
}
}

DeepSeekClient::DeepSeekClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_snapshot(emptySnapshot(QStringLiteral("未配置")))
{
    m_credentialStatus = isEnvironmentConfigured()
        ? QStringLiteral("已通过环境变量配置")
        : QStringLiteral("待连接 KDE 钱包");
}

DeepSeekClient::~DeepSeekClient()
{
    m_storedApiKey.fill('\0');
    m_activeApiKey.fill('\0');
    m_pendingApiKey.fill(QChar(u'\0'));
}

QVariantMap DeepSeekClient::snapshot() const
{
    return m_snapshot;
}

bool DeepSeekClient::loading() const
{
    return m_loading;
}

bool DeepSeekClient::credentialConfigured() const
{
    return isEnvironmentConfigured() || !m_storedApiKey.isEmpty();
}

QString DeepSeekClient::credentialStatus() const
{
    return m_credentialStatus;
}

bool DeepSeekClient::credentialBusy() const
{
    return m_credentialBusy;
}

bool DeepSeekClient::credentialError() const
{
    return m_credentialError;
}

QUrl DeepSeekClient::balanceEndpoint()
{
    return QUrl(QStringLiteral("https://api.deepseek.com/user/balance"));
}

QNetworkRequest DeepSeekClient::createRequest(const QUrl &url, QByteArrayView apiKey)
{
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + apiKey.toByteArray());
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);
    return request;
}

void DeepSeekClient::refresh()
{
    if (m_loading) {
        m_refreshPending = true;
        return;
    }

    QByteArray apiKey = qgetenv("DEEPSEEK_API_KEY").trimmed();
    if (apiKey.isEmpty()) {
        apiKey = m_storedApiKey;
    }
    if (apiKey.isEmpty()) {
        m_refreshPending = false;
        m_refreshInterrupted = false;
        setSnapshot(emptySnapshot(QStringLiteral("未配置")));
        return;
    }

    m_activeApiKey.fill('\0');
    m_activeApiKey = apiKey;
    apiKey.fill('\0');
    m_lastRequestError.clear();
    setLoading(true);

    QNetworkReply *reply = m_network->get(
        createRequest(balanceEndpoint(), m_activeApiKey));
    m_reply = reply;

    QTimer::singleShot(15000, reply, [reply]() {
        if (reply->isRunning()) {
            reply->setProperty("aiUsageWatcherTimedOut", true);
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
        if (received > maximumResponseBytes) {
            reply->setProperty("aiUsageWatcherResponseTooLarge", true);
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_reply == reply) {
            m_reply = nullptr;
        }
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool hasHttpStatus = httpStatus >= 100 && httpStatus < 700;
        const QNetworkReply::NetworkError transportError = reply->error();
        const bool timedOut = reply->property("aiUsageWatcherTimedOut").toBool();
        const bool responseTooLarge = reply->property("aiUsageWatcherResponseTooLarge").toBool();

        if (m_refreshInterrupted) {
            reply->deleteLater();
            finishRefresh();
            return;
        }

        bool succeeded = false;
        if (timedOut) {
            m_lastRequestError = QStringLiteral("DeepSeek 请求超时");
        } else if (responseTooLarge) {
            m_lastRequestError = QStringLiteral("DeepSeek 响应过大，已拒绝处理");
        } else if (!hasHttpStatus) {
            m_lastRequestError = transportErrorMessage(transportError);
        } else {
            const QByteArray payload = reply->read(maximumResponseBytes + 1);
            if (payload.size() > maximumResponseBytes) {
                m_lastRequestError = QStringLiteral("DeepSeek 响应过大，已拒绝处理");
            } else {
                const DeepSeekParseResult result = DeepSeekResponseParser::parse(payload, httpStatus);
                if (result.ok) {
                    if (result.balances.isEmpty()) {
                        setSnapshot(emptySnapshot(QStringLiteral("暂无数据")));
                    } else {
                        setSnapshot(toVariantMap(result.balances));
                    }
                    succeeded = true;
                } else {
                    m_lastRequestError = result.errorMessage;
                }
            }
        }

        reply->deleteLater();
        if (succeeded) {
            finishRefresh();
        } else {
            setError(m_lastRequestError);
            finishRefresh();
        }
    });
}

void DeepSeekClient::forceRefresh()
{
    if (!m_loading) {
        refresh();
        return;
    }

    m_refreshPending = true;
    m_refreshInterrupted = true;
    // abort() 的 finished() 回调统一清理活动 Key，再串行触发这次手动刷新。
    if (m_reply && m_reply->isRunning()) {
        m_reply->abort();
    }
}

void DeepSeekClient::cancelRefresh()
{
    m_refreshPending = false;
    m_refreshInterrupted = false;
    if (m_reply) m_reply->abort();
}

void DeepSeekClient::finishRefresh()
{
    m_activeApiKey.fill('\0');
    m_activeApiKey.clear();
    setLoading(false);
    if (m_refreshPending) {
        m_refreshPending = false;
        m_refreshInterrupted = false;
        QTimer::singleShot(0, this, &DeepSeekClient::refresh);
    }
}

void DeepSeekClient::saveCredential(const QString &apiKey)
{
    const QString trimmedApiKey = apiKey.trimmed();
    if (trimmedApiKey.isEmpty()) {
        setCredentialState(QStringLiteral("API Key 不能为空"), false, true);
        return;
    }
    if (!m_dispatcher) {
        setCredentialState(QStringLiteral("凭据服务暂不可用"), false, true);
        return;
    }
    m_pendingApiKey.fill(QChar(u'\0'));
    m_pendingApiKey = trimmedApiKey;
    setCredentialState(QStringLiteral("正在保存到 KDE 钱包…"), true, false);

    const QString snapshotValue = m_pendingApiKey;
    m_dispatcher->submit(KWalletDispatcher::Op::Save,
                         QStringLiteral("deepseek"),
                         snapshotValue,
                         [this](const KWalletDispatcher::Result &result) {
                             handleCredentialSave(result);
                         });
}

void DeepSeekClient::clearCredential()
{
    if (!m_dispatcher) {
        setCredentialState(QStringLiteral("凭据服务暂不可用"), false, true);
        return;
    }
    m_pendingApiKey.fill(QChar(u'\0'));
    m_pendingApiKey.clear();
    setCredentialState(QStringLiteral("正在从 KDE 钱包移除…"), true, false);

    m_dispatcher->submit(KWalletDispatcher::Op::Clear,
                         QStringLiteral("deepseek"),
                         QString(),
                         [this](const KWalletDispatcher::Result &result) {
                             handleCredentialClear(result);
                         });
}

void DeepSeekClient::setWalletDispatcher(KWalletDispatcher *dispatcher)
{
    m_dispatcher = dispatcher;
    if (m_dispatcher && !m_initialLoadDispatched) {
        m_initialLoadDispatched = true;
        QTimer::singleShot(1500, this, [this] { requestCredentialLoad(); });
    }
}

void DeepSeekClient::reloadCredential()
{
    requestCredentialLoad();
}

void DeepSeekClient::requestCredentialLoad()
{
    if (!m_dispatcher) {
        return;
    }
    m_credentialBusy = true;
    Q_EMIT credentialBusyChanged();
    m_dispatcher->submit(KWalletDispatcher::Op::Read,
                         QStringLiteral("deepseek"),
                         QString(),
                         [this](const KWalletDispatcher::Result &result) {
                             handleCredentialRead(result);
                         });
}

void DeepSeekClient::handleCredentialRead(const KWalletDispatcher::Result &result)
{
    if (result.ok) {
        const QString trimmed = result.value.trimmed();
        if (!trimmed.isEmpty()) {
            setStoredApiKey(trimmed.toUtf8());
            setCredentialState(QStringLiteral("已保存在 KDE 钱包"), false, false);
            refresh();
            return;
        }
        setStoredApiKey({});
        setCredentialState(QStringLiteral("尚未保存 API Key"), false, false);
        return;
    }
    if (result.errorCode == QLatin1String("not_found")) {
        setStoredApiKey({});
        setCredentialState(QStringLiteral("尚未保存 API Key"), false, false);
        return;
    }
    if (result.errorCode == QLatin1String("wallet_disabled")
        || result.errorCode == QLatin1String("wallet_open_failed")
        || result.errorCode == QLatin1String("timeout")
        || result.errorCode == QLatin1String("worker_failed_to_start")
        || result.errorCode == QLatin1String("worker_crashed")) {
        setCredentialState(QStringLiteral("凭据服务暂不可用"), false, true);
        return;
    }
    setCredentialState(QStringLiteral("无法访问 KDE 钱包"), false, true);
}

void DeepSeekClient::handleCredentialSave(const KWalletDispatcher::Result &result)
{
    if (result.ok) {
        QByteArray storedApiKey = m_pendingApiKey.toUtf8();
        setStoredApiKey(storedApiKey);
        storedApiKey.fill('\0');
        setCredentialState(QStringLiteral("API Key 已保存到 KDE 钱包"), false, false);
        m_pendingApiKey.fill(QChar(u'\0'));
        m_pendingApiKey.clear();
        refresh();
        return;
    }
    setCredentialState(QStringLiteral("API Key 保存失败，请检查 KDE 钱包"), false, true);
    m_pendingApiKey.fill(QChar(u'\0'));
    m_pendingApiKey.clear();
}

void DeepSeekClient::handleCredentialClear(const KWalletDispatcher::Result &result)
{
    if (result.ok) {
        setStoredApiKey({});
        const bool environmentConfigured = isEnvironmentConfigured();
        setCredentialState(environmentConfigured
                               ? QStringLiteral("钱包凭据已移除；环境变量仍在生效")
                               : QStringLiteral("API Key 已移除"),
                           false,
                           false);
        if (!environmentConfigured) {
            setSnapshot(emptySnapshot(QStringLiteral("未配置")));
        }
        m_pendingApiKey.fill(QChar(u'\0'));
        m_pendingApiKey.clear();
        return;
    }
    setCredentialState(QStringLiteral("API Key 移除失败，请检查 KDE 钱包"), false, true);
    m_pendingApiKey.fill(QChar(u'\0'));
    m_pendingApiKey.clear();
}

void DeepSeekClient::setStoredApiKey(const QByteArray &apiKey)
{
    const bool wasConfigured = credentialConfigured();
    m_storedApiKey.fill('\0');
    m_storedApiKey = apiKey;
    if (wasConfigured != credentialConfigured()) {
        Q_EMIT credentialConfiguredChanged();
    }
}

void DeepSeekClient::setCredentialState(const QString &status, bool busy, bool error)
{
    if (m_credentialStatus != status) {
        m_credentialStatus = status;
        Q_EMIT credentialStatusChanged();
    }
    if (m_credentialBusy != busy) {
        m_credentialBusy = busy;
        Q_EMIT credentialBusyChanged();
    }
    if (m_credentialError != error) {
        m_credentialError = error;
        Q_EMIT credentialErrorChanged();
    }
}

void DeepSeekClient::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    Q_EMIT loadingChanged();
}

void DeepSeekClient::setError(const QString &message)
{
    if (!m_snapshot.value(QStringLiteral("plans")).toList().isEmpty()) {
        QVariantMap staleSnapshot = m_snapshot;
        staleSnapshot.insert(QStringLiteral("statusLabel"), QStringLiteral("数据暂时不可更新"));
        staleSnapshot.insert(QStringLiteral("errorText"), message);
        staleSnapshot.insert(QStringLiteral("stale"), true);
        setSnapshot(staleSnapshot);
        return;
    }
    setSnapshot(emptySnapshot(QStringLiteral("请求失败"), message));
}

void DeepSeekClient::setSnapshot(const QVariantMap &snapshot)
{
    if (m_snapshot == snapshot) {
        return;
    }
    m_snapshot = snapshot;
    Q_EMIT snapshotChanged();
}
