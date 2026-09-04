// SPDX-License-Identifier: GPL-2.0-or-later

#include "codexzhclient.h"

#include "codexzhresponseparser.h"
#include "kwalletdispatcher.h"
#include "resilientnetworkrequest.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <QUrlQuery>
#include <QVariantList>
#include <QDateTime>

namespace
{
QVariantMap emptySnapshot(const QString &status, const QString &error = {})
{
    return {
        {QStringLiteral("providerId"), QStringLiteral("codexzh")},
        {QStringLiteral("statusLabel"), status},
        {QStringLiteral("errorText"), error},
        {QStringLiteral("plans"), QVariantList{}},
    };
}

QVariantMap toVariantMap(const CodexZhSnapshot &snapshot)
{
    QVariantList plans;
    QVariantMap plan{
        {QStringLiteral("planId"), snapshot.plan.planId},
        {QStringLiteral("planName"), snapshot.plan.planName},
        {QStringLiteral("used"), snapshot.plan.used},
        {QStringLiteral("total"), snapshot.plan.total},
        {QStringLiteral("unit"), QStringLiteral("USD")},
        {QStringLiteral("resetText"), snapshot.plan.resetText},
        {QStringLiteral("resetAt"), snapshot.plan.resetAtMs},
        {QStringLiteral("extraText"), snapshot.plan.extraText},
        {QStringLiteral("isValid"), true},
        {QStringLiteral("invalidReason"), QString()},
    };
    QVariantList usageSegments;
    for (const CodexZhUsageSegment &segment : snapshot.plan.usageSegments) {
        usageSegments.push_back(QVariantMap{
            {QStringLiteral("kind"), segment.kind},
            {QStringLiteral("used"), segment.used},
            {QStringLiteral("usedPercent"), segment.usedPercent},
            {QStringLiteral("formattedUsed"), segment.formattedUsed},
        });
    }
    if (!usageSegments.isEmpty()) {
        plan.insert(QStringLiteral("usageSegments"), usageSegments);
    }
    plans.push_back(plan);

    return {
        {QStringLiteral("providerId"), QStringLiteral("codexzh")},
        {QStringLiteral("statusLabel"), snapshot.statusLabel},
        {QStringLiteral("errorText"), snapshot.errorText},
        {QStringLiteral("plans"), plans},
    };
}

} // namespace

CodexZhClient::CodexZhClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_snapshot(emptySnapshot(QStringLiteral("未配置")))
{
    m_credentialStatus = QStringLiteral("待连接 KDE 钱包");
}

CodexZhClient::~CodexZhClient()
{
    m_storedApiKey.fill('\0');
    m_activeApiKey.fill('\0');
    m_pendingApiKey.fill(QChar(u'\0'));
}

QVariantMap CodexZhClient::snapshot() const
{
    return m_snapshot;
}

bool CodexZhClient::loading() const
{
    return m_loading;
}

bool CodexZhClient::credentialConfigured() const
{
    return !m_storedApiKey.isEmpty();
}

QString CodexZhClient::credentialStatus() const
{
    return m_credentialStatus;
}

bool CodexZhClient::credentialBusy() const
{
    return m_credentialBusy;
}

bool CodexZhClient::credentialError() const
{
    return m_credentialError;
}

QList<QUrl> CodexZhClient::endpointCandidates()
{
    return {
        QUrl(QStringLiteral("https://codexzh.com/api/v1/usage/stats")),
    };
}

QNetworkRequest CodexZhClient::createRequest(const QUrl &url, QByteArrayView apiKey)
{
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + apiKey.toByteArray());
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "AIUsageWatcher/0.1");
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);
    return request;
}

void CodexZhClient::refresh()
{
    if (QDateTime::currentMSecsSinceEpoch() < m_rateLimitedUntilMs) {
        return;
    }
    if (m_loading) {
        // 普通轮询不排队：否则刷新队列会在慢请求结束后继续补发，制造额外 429。
        return;
    }

    if (m_storedApiKey.isEmpty()) {
        m_refreshPending = false;
        m_refreshInterrupted = false;
        setSnapshot(emptySnapshot(QStringLiteral("未配置")));
        return;
    }

    m_activeApiKey.fill('\0');
    m_activeApiKey = m_storedApiKey;
    setLoading(true);
    m_lastRequestError.clear();

    QUrl url = endpointCandidates().first();
    // 证据：CodexZH 当前接口按 query key 返回数据；仅 Authorization 会导致无数据回归。
    QUrlQuery query(url.query());
    query.addQueryItem(QStringLiteral("key"), QString::fromLatin1(m_activeApiKey));
    url.setQuery(query);

    auto *req = new ResilientNetworkRequest(m_network, this);
    // 服务端返回 429 时不再在同一轮立即重试，避免把限流放大成更多请求。
    req->setMaxAttempts(1);
    m_request = req;
    QPointer<CodexZhClient> self = this;

    auto finalize = [self, req](const QString &errorMessage) {
        if (req->parent() == self.data()) {
            req->deleteLater();
        }
        if (!self) {
            return;
        }
        if (self->m_request == req) {
            self->m_request = nullptr;
        }
        if (self->m_refreshInterrupted) {
            self->m_activeApiKey.fill('\0');
            self->m_activeApiKey.clear();
            self->setLoading(false);
            self->m_refreshPending = false;
            self->m_refreshInterrupted = false;
            QTimer::singleShot(0, self.data(), &CodexZhClient::refresh);
            return;
        }
        if (!errorMessage.isEmpty()) {
            self->setError(errorMessage);
        }
        self->m_activeApiKey.fill('\0');
        self->m_activeApiKey.clear();
        self->setLoading(false);
        if (self->m_refreshPending) {
            self->m_refreshPending = false;
            QTimer::singleShot(0, self.data(), &CodexZhClient::refresh);
        }
    };

    req->get(createRequest(url, m_activeApiKey), [self, req, finalize](ResilientNetworkRequest::Result result) {
        if (!self) {
            req->deleteLater();
            return;
        }
        switch (result.outcome) {
        case ResilientNetworkRequest::Outcome::Success: {
            const CodexZhParseResult parsed = CodexZhResponseParser::parse(result.payload);
            if (parsed.ok) {
                self->m_rateLimitedUntilMs = 0;
                self->setSnapshot(toVariantMap(parsed.snapshot));
                finalize({});
            } else {
                // 业务层解析失败（视为非可重试）
                finalize(QStringLiteral("CodexZH %1").arg(parsed.errorMessage));
            }
            return;
        }
        case ResilientNetworkRequest::Outcome::Aborted:
            self->m_refreshInterrupted = true;
            finalize({});
            return;
        case ResilientNetworkRequest::Outcome::NonRetryableFailure:
        case ResilientNetworkRequest::Outcome::RetryableFailure: {
            QString message;
            if (result.responseTooLarge) {
                message = QStringLiteral("CodexZH 响应过大，已拒绝处理");
            } else if (result.httpStatus == 401 || result.httpStatus == 403) {
                message = QStringLiteral("CodexZH 认证失败，请检查 API Key");
            } else if (result.httpStatus == 429) {
                self->m_rateLimitedUntilMs = QDateTime::currentMSecsSinceEpoch()
                    + qMax(60000, result.retryAfterMs);
                if (!self->m_snapshot.value(QStringLiteral("plans")).toList().isEmpty()) {
                    QVariantMap availableSnapshot = self->m_snapshot;
                    // 保留最近一次成功额度；限流只暂停后续请求，不把可用数据标成异常。
                    availableSnapshot.insert(QStringLiteral("statusLabel"), QStringLiteral("可用"));
                    availableSnapshot.insert(QStringLiteral("errorText"), QString());
                    availableSnapshot.insert(QStringLiteral("stale"), true);
                    self->setSnapshot(availableSnapshot);
                    finalize({});
                    return;
                }
                message = QStringLiteral("CodexZH 请求过于频繁，请稍后重试");
            } else if (result.httpStatus >= 500) {
                message = QStringLiteral("CodexZH 服务暂时不可用");
            } else if (!result.errorMessage.isEmpty()) {
                message = result.errorMessage;
                if (!message.startsWith(QStringLiteral("CodexZH"))) {
                    message = QStringLiteral("CodexZH %1").arg(message);
                }
            } else {
                message = QStringLiteral("CodexZH 无法连接服务");
            }
            finalize(message);
            return;
        }
        }
    });
}

void CodexZhClient::forceRefresh()
{
    m_rateLimitedUntilMs = 0;
    if (!m_loading) {
        refresh();
        return;
    }

    m_refreshPending = true;
    m_refreshInterrupted = true;
    if (m_request) {
        m_request->abort();
    }
}

void CodexZhClient::cancelRefresh()
{
    m_refreshPending = false;
    m_refreshInterrupted = false;
    if (m_request) m_request->abort();
}

void CodexZhClient::saveCredential(const QString &apiKey)
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
                         QStringLiteral("codexzh"),
                         snapshotValue,
                         [this](const KWalletDispatcher::Result &result) {
                             handleCredentialSave(result);
                         });
}

void CodexZhClient::clearCredential()
{
    if (!m_dispatcher) {
        setCredentialState(QStringLiteral("凭据服务暂不可用"), false, true);
        return;
    }
    m_pendingApiKey.fill(QChar(u'\0'));
    m_pendingApiKey.clear();
    setCredentialState(QStringLiteral("正在从 KDE 钱包移除…"), true, false);

    m_dispatcher->submit(KWalletDispatcher::Op::Clear,
                         QStringLiteral("codexzh"),
                         QString(),
                         [this](const KWalletDispatcher::Result &result) {
                             handleCredentialClear(result);
                         });
}

void CodexZhClient::setWalletDispatcher(KWalletDispatcher *dispatcher)
{
    m_dispatcher = dispatcher;
    if (m_dispatcher && !m_initialLoadDispatched) {
        m_initialLoadDispatched = true;
        QTimer::singleShot(1500, this, [this] { requestCredentialLoad(); });
    }
}

void CodexZhClient::reloadCredential()
{
    requestCredentialLoad();
}

void CodexZhClient::requestCredentialLoad()
{
    if (!m_dispatcher) {
        return;
    }
    m_credentialBusy = true;
    Q_EMIT credentialBusyChanged();
    m_dispatcher->submit(KWalletDispatcher::Op::Read,
                         QStringLiteral("codexzh"),
                         QString(),
                         [this](const KWalletDispatcher::Result &result) {
                             handleCredentialRead(result);
                         });
}

void CodexZhClient::handleCredentialRead(const KWalletDispatcher::Result &result)
{
    if (result.ok) {
        const QString trimmed = result.value.trimmed();
        if (!trimmed.isEmpty()) {
            const QByteArray previousKey = m_storedApiKey;
            setStoredApiKey(trimmed.toUtf8());
            setCredentialState(QStringLiteral("已保存在 KDE 钱包"), false, false);
            // 凭据未变时不主动刷新：kwalletd 频繁触发 walletOpened 会让 reloadCredential
            // 被反复调用；key 没变就没必要立刻打 API，省一次 60s 限流窗口。
            if (m_storedApiKey != previousKey) {
                refresh();
            }
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

void CodexZhClient::handleCredentialSave(const KWalletDispatcher::Result &result)
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

void CodexZhClient::handleCredentialClear(const KWalletDispatcher::Result &result)
{
    if (result.ok) {
        setStoredApiKey({});
        setCredentialState(QStringLiteral("API Key 已移除"), false, false);
        setSnapshot(emptySnapshot(QStringLiteral("未配置")));
        m_pendingApiKey.fill(QChar(u'\0'));
        m_pendingApiKey.clear();
        return;
    }
    setCredentialState(QStringLiteral("API Key 移除失败，请检查 KDE 钱包"), false, true);
    m_pendingApiKey.fill(QChar(u'\0'));
    m_pendingApiKey.clear();
}

void CodexZhClient::setStoredApiKey(const QByteArray &apiKey)
{
    const bool wasConfigured = credentialConfigured();
    m_storedApiKey.fill('\0');
    m_storedApiKey = apiKey;
    if (wasConfigured != credentialConfigured()) {
        Q_EMIT credentialConfiguredChanged();
    }
}

void CodexZhClient::setCredentialState(const QString &status, bool busy, bool error)
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

void CodexZhClient::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    Q_EMIT loadingChanged();
}

void CodexZhClient::setError(const QString &message)
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

void CodexZhClient::setSnapshot(const QVariantMap &snapshot)
{
    if (m_snapshot == snapshot) {
        return;
    }
    m_snapshot = snapshot;
    Q_EMIT snapshotChanged();
}
