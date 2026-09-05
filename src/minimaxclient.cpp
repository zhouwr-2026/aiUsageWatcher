// SPDX-License-Identifier: GPL-2.0-or-later

#include "minimaxclient.h"

#include "minimaxresponseparser.h"
#include "resilientnetworkrequest.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <QVariantList>

namespace
{
constexpr qsizetype maximumResponseBytes = 1024 * 1024;

QVariantMap emptySnapshot(const QString &status, const QString &error = {})
{
    return {
        {QStringLiteral("providerId"), QStringLiteral("minimax")},
        {QStringLiteral("statusLabel"), status},
        {QStringLiteral("errorText"), error},
        {QStringLiteral("plans"), QVariantList{}},
    };
}

QVariantMap toVariantMap(const MiniMaxSnapshot &snapshot)
{
    QVariantList plans;
    plans.reserve(snapshot.plans.size());
    for (const MiniMaxPlan &plan : snapshot.plans) {
        const QString resetText = plan.resetAtMs > 0
            ? QDateTime::fromMSecsSinceEpoch(plan.resetAtMs)
                  .toLocalTime()
                  .toString(QStringLiteral("MM-dd HH:mm"))
            : QString{};
        plans.push_back(QVariantMap{
            {QStringLiteral("planId"), plan.planId},
            {QStringLiteral("planName"), plan.planName},
            {QStringLiteral("used"), plan.used},
            {QStringLiteral("total"), plan.total},
            {QStringLiteral("unit"), QStringLiteral("%")},
            {QStringLiteral("resetText"), resetText},
            {QStringLiteral("resetAt"), plan.resetAtMs},
            {QStringLiteral("extraText"), QString()},
            {QStringLiteral("isValid"), true},
            {QStringLiteral("invalidReason"), QString()},
        });
    }

    return {
        {QStringLiteral("providerId"), QStringLiteral("minimax")},
        {QStringLiteral("statusLabel"), snapshot.statusLabel},
        {QStringLiteral("errorText"), QString()},
        {QStringLiteral("plans"), plans},
    };
}

bool isEnvironmentConfigured()
{
    return !qgetenv("MINIMAX_API_KEY").trimmed().isEmpty();
}
}

MiniMaxClient::MiniMaxClient(QObject *parent)
    : CredentialClientBase(parent)
{
    setSnapshot(emptySnapshot(QStringLiteral("未配置")));
    // 状态仍报告"未配置"，等读取完成后再切换到"已保存"/"凭据服务暂不可用"。
    setCredentialState(isEnvironmentConfigured()
                           ? QStringLiteral("已通过环境变量配置")
                           : QStringLiteral("待连接 KDE 钱包"),
                       false,
                       false);
}

MiniMaxClient::~MiniMaxClient() = default;

bool MiniMaxClient::credentialConfigured() const
{
    return isEnvironmentConfigured() || !storedSecret().isEmpty();
}

QVariantMap MiniMaxClient::emptySnapshot(const QString &status, const QString &error) const
{
    return ::emptySnapshot(status, error);
}

QString MiniMaxClient::walletEntryKey() const
{
    return QStringLiteral("minimax");
}

QString MiniMaxClient::credentialClearedText() const
{
    return isEnvironmentConfigured()
        ? QStringLiteral("钱包凭据已移除；环境变量仍在生效")
        : QStringLiteral("API Key 已移除");
}

void MiniMaxClient::onCredentialCleared()
{
    if (!isEnvironmentConfigured()) {
        setSnapshot(emptySnapshot(QStringLiteral("未配置")));
    }
}

void MiniMaxClient::finishRefresh()
{
    CredentialClientBase::finishRefresh();
    m_endpoints.clear();
    m_endpointIndex = 0;
}

QList<QUrl> MiniMaxClient::endpointCandidates()
{
    return {
        QUrl(QStringLiteral("https://api.minimaxi.com/v1/api/openplatform/coding_plan/remains")),
        QUrl(QStringLiteral("https://api.minimax.io/v1/api/openplatform/coding_plan/remains")),
    };
}

QNetworkRequest MiniMaxClient::createRequest(const QUrl &url, QByteArrayView apiKey)
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

void MiniMaxClient::refresh()
{
    if (m_loading) {
        m_refreshPending = true;
        return;
    }

    QByteArray apiKey = qgetenv("MINIMAX_API_KEY").trimmed();
    if (apiKey.isEmpty()) {
        apiKey = m_storedSecret;
    }
    if (apiKey.isEmpty()) {
        m_refreshPending = false;
        m_refreshInterrupted = false;
        setSnapshot(emptySnapshot(QStringLiteral("未配置")));
        return;
    }

    m_activeSecret.fill('\0');
    m_activeSecret = apiKey;
    apiKey.fill('\0');
    m_endpoints = endpointCandidates();
    m_endpointIndex = 0;
    m_lastRequestError.clear();
    setLoading(true);
    requestNextEndpoint();
}

void MiniMaxClient::requestNextEndpoint()
{
    if (m_endpointIndex >= m_endpoints.size()) {
        setError(m_lastRequestError.isEmpty()
                     ? QStringLiteral("无法连接 MiniMax 服务") : m_lastRequestError);
        finishRefresh();
        return;
    }

    const QUrl endpoint = m_endpoints.at(m_endpointIndex++);

    auto *req = new ResilientNetworkRequest(m_network, this);
    m_request = req;
    QPointer<MiniMaxClient> self = this;

    auto tryNextOrFinish = [self, req](const QString &errorMessage, bool goNextEndpoint) {
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
            self->m_refreshInterrupted = false;
            self->m_refreshPending = false;
            self->finishRefresh();
            QTimer::singleShot(0, self.data(), &MiniMaxClient::refresh);
            return;
        }
        if (goNextEndpoint) {
            // 当前 endpoint 已重试耗尽，转下一个 endpoint。
            self->m_lastRequestError = errorMessage;
            self->requestNextEndpoint();
            return;
        }
        if (!errorMessage.isEmpty()) {
            self->setError(errorMessage);
        }
        self->finishRefresh();
        if (self->m_refreshPending) {
            self->m_refreshPending = false;
            QTimer::singleShot(0, self.data(), &MiniMaxClient::refresh);
        }
    };

    req->get(createRequest(endpoint, m_activeSecret),
             [self, req, tryNextOrFinish](ResilientNetworkRequest::Result result) {
        if (!self) {
            req->deleteLater();
            return;
        }
        switch (result.outcome) {
        case ResilientNetworkRequest::Outcome::Success: {
            const MiniMaxParseResult parsed = MiniMaxResponseParser::parse(result.payload);
            if (parsed.ok) {
                self->setSnapshot(toVariantMap(parsed.snapshot));
                tryNextOrFinish({}, false);
                return;
            }
            // 1004 等业务层拒绝 —— 跨区域重试只会放大无效请求，立刻短路。
            if (parsed.errorCode == QLatin1String("api_error")) {
                tryNextOrFinish(parsed.errorMessage, false);
                return;
            }
            // 其它解析错误：当前 endpoint 失败，转下一个。
            tryNextOrFinish(parsed.errorMessage, true);
            return;
        }
        case ResilientNetworkRequest::Outcome::Aborted:
            self->m_refreshInterrupted = true;
            tryNextOrFinish({}, false);
            return;
        case ResilientNetworkRequest::Outcome::NonRetryableFailure:
        case ResilientNetworkRequest::Outcome::RetryableFailure: {
            QString message;
            if (result.responseTooLarge) {
                message = QStringLiteral("MiniMax 响应过大，已拒绝处理");
            } else if (result.httpStatus == 401 || result.httpStatus == 403) {
                // 凭据问题：当前 endpoint 失败后再换 endpoint 也没意义；直接报失败。
                message = QStringLiteral("MiniMax Key 无效或已过期");
                tryNextOrFinish(message, false);
                return;
            } else if (!result.errorMessage.isEmpty()) {
                message = result.errorMessage;
                if (!message.startsWith(QStringLiteral("MiniMax"))) {
                    message = QStringLiteral("MiniMax %1").arg(message);
                }
            } else {
                message = QStringLiteral("MiniMax 无法连接服务");
            }
            // RetryableFailure 表示同一 endpoint 重试已耗尽，可尝试下一个 endpoint。
            // NonRetryableFailure 在当前 endpoint 已不可重试；同样尝试下一个。
            tryNextOrFinish(message,
                            result.outcome == ResilientNetworkRequest::Outcome::RetryableFailure
                                || result.outcome == ResilientNetworkRequest::Outcome::NonRetryableFailure);
            return;
        }
        }
    });
}
