// SPDX-License-Identifier: GPL-2.0-or-later

#include "codexzhclient.h"

#include "codexzhresponseparser.h"
#include "resilientnetworkrequest.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <QUrlQuery>
#include <QVariantList>

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
}

CodexZhClient::CodexZhClient(QObject *parent)
    : CredentialClientBase(parent)
{
    setSnapshot(emptySnapshot(QStringLiteral("未配置")));
}

CodexZhClient::~CodexZhClient() = default;

QVariantMap CodexZhClient::emptySnapshot(const QString &status, const QString &error) const
{
    return ::emptySnapshot(status, error);
}

QString CodexZhClient::walletEntryKey() const
{
    return QStringLiteral("codexzh");
}

void CodexZhClient::onCredentialLoaded(const QByteArray &secret)
{
    const QByteArray previousKey = m_storedSecret;
    setStoredSecret(secret);
    setCredentialState(credentialLoadedText(), false, false);
    // 凭据未变时不主动刷新：kwalld 频繁触发 walletOpened 会让 reloadCredential
    // 被反复调用；key 没变就没必要立刻打 API，省一次 60s 限流窗口。
    if (m_storedSecret != previousKey) {
        refresh();
    }
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
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "AIUsageWatcher/0.2");
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

    if (m_storedSecret.isEmpty()) {
        m_refreshPending = false;
        m_refreshInterrupted = false;
        setSnapshot(emptySnapshot(QStringLiteral("未配置")));
        return;
    }

    m_activeSecret.fill('\0');
    m_activeSecret = m_storedSecret;
    setLoading(true);
    m_lastRequestError.clear();

    QUrl url = endpointCandidates().first();
    // 证据：CodexZH 当前接口按 query key 返回数据；仅 Authorization 会导致无数据回归。
    QUrlQuery query(url.query());
    query.addQueryItem(QStringLiteral("key"), QString::fromLatin1(m_activeSecret));
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
            self->m_activeSecret.fill('\0');
            self->m_activeSecret.clear();
            self->setLoading(false);
            self->m_refreshPending = false;
            self->m_refreshInterrupted = false;
            QTimer::singleShot(0, self.data(), &CodexZhClient::refresh);
            return;
        }
        if (!errorMessage.isEmpty()) {
            self->setError(errorMessage);
        }
        self->m_activeSecret.fill('\0');
        self->m_activeSecret.clear();
        self->setLoading(false);
        if (self->m_refreshPending) {
            self->m_refreshPending = false;
            QTimer::singleShot(0, self.data(), &CodexZhClient::refresh);
        }
    };

    req->get(createRequest(url, m_activeSecret), [self, req, finalize](ResilientNetworkRequest::Result result) {
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
                if (!self->snapshot().value(QStringLiteral("plans")).toList().isEmpty()) {
                    QVariantMap availableSnapshot = self->snapshot();
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
