// SPDX-License-Identifier: GPL-2.0-or-later

#include "agnesclient.h"

#include "agnesresponseparser.h"
#include "resilientnetworkrequest.h"

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
        {QStringLiteral("providerId"), QStringLiteral("agnes-ai")},
        {QStringLiteral("statusLabel"), status},
        {QStringLiteral("errorText"), error},
        {QStringLiteral("plans"), QVariantList{}},
    };
}

QVariantMap toVariantMap(const AgnesSnapshot &snapshot)
{
    QVariantList plans;
    plans.reserve(snapshot.plans.size());
    for (const AgnesPlan &plan : snapshot.plans) {
        plans.push_back(QVariantMap{
            {QStringLiteral("planId"), plan.planId},
            {QStringLiteral("planName"), plan.planName},
            {QStringLiteral("used"), plan.used},
            {QStringLiteral("total"), plan.total},
            {QStringLiteral("unit"), plan.unit},
            {QStringLiteral("resetText"), plan.resetText},
            {QStringLiteral("resetAt"), plan.resetAtMs},
            {QStringLiteral("extraText"), plan.extraText},
            {QStringLiteral("isValid"), true},
            {QStringLiteral("invalidReason"), QString()},
        });
    }
    return {
        {QStringLiteral("providerId"), QStringLiteral("agnes-ai")},
        {QStringLiteral("statusLabel"), snapshot.statusLabel},
        {QStringLiteral("errorText"), snapshot.errorText},
        {QStringLiteral("plans"), plans},
    };
}
}

AgnesClient::AgnesClient(QObject *parent)
    : CredentialClientBase(parent)
{
    setSnapshot(emptySnapshot(QStringLiteral("未配置")));
}

AgnesClient::~AgnesClient() = default;

QVariantMap AgnesClient::emptySnapshot(const QString &status, const QString &error) const
{
    return ::emptySnapshot(status, error);
}

QString AgnesClient::walletEntryKey() const
{
    return QStringLiteral("agnes-ai");
}

QUrl AgnesClient::usageEndpoint()
{
    return QUrl(QStringLiteral("https://platform-backend.agnes-ai.com/api/user/subscription"));
}

QNetworkRequest AgnesClient::createRequest(const QUrl &url, QByteArrayView apiKey)
{
    QNetworkRequest request(url);
    request.setRawHeader("Authorization",
                          QByteArrayLiteral("Bearer ") + apiKey.toByteArray());
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "AIUsageWatcher/0.2");
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);
    return request;
}

void AgnesClient::saveCredential(const QString &value)
{
    // 重新保存代表用户提供了新凭据，解除 401 暂停（否则 refresh 会一直早退）。
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        setCredentialState(credentialEmptyText(), false, true);
        return;
    }
    m_authInvalid = false;
    submitCredentialSave(trimmed);
}

void AgnesClient::refresh()
{
    if (m_authInvalid) {
        return;
    }
    if (m_loading) {
        m_refreshPending = true;
        return;
    }
    if (m_storedSecret.isEmpty()) {
        setSnapshot(emptySnapshot(QStringLiteral("未配置")));
        return;
    }

    m_activeSecret.fill('\0');
    m_activeSecret = m_storedSecret;
    setLoading(true);
    m_lastRequestError.clear();

    auto *req = new ResilientNetworkRequest(m_network, this);
    m_request = req;
    QPointer<AgnesClient> self = this;

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
            self->m_refreshInterrupted = false;
            self->m_refreshPending = false;
            self->finishRefresh();
            QTimer::singleShot(0, self.data(), &AgnesClient::refresh);
            return;
        }
        if (!errorMessage.isEmpty()) {
            self->setError(errorMessage);
        }
        self->finishRefresh();
        if (self->m_refreshPending) {
            self->m_refreshPending = false;
            QTimer::singleShot(0, self.data(), &AgnesClient::refresh);
        }
    };

    req->get(createRequest(usageEndpoint(), m_activeSecret),
             [self, req, finalize](ResilientNetworkRequest::Result result) {
        if (!self) {
            req->deleteLater();
            return;
        }
        switch (result.outcome) {
        case ResilientNetworkRequest::Outcome::Success: {
            const AgnesParseResult parsed = AgnesResponseParser::parse(result.payload);
            if (parsed.ok) {
                self->setSnapshot(toVariantMap(parsed.snapshot));
                finalize({});
            } else {
                finalize(QStringLiteral("Agnes %1").arg(parsed.errorMessage));
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
                message = QStringLiteral("Agnes 响应过大，已拒绝处理");
            } else if (result.httpStatus == 401 || result.httpStatus == 403) {
                self->m_authInvalid = true;
                self->setCredentialState(QStringLiteral("Agnes Bearer Token 无效或已过期，请更新浏览器登录令牌"), false, true);
                message = QStringLiteral("Agnes 凭据无效或已过期；若使用 API Key 仍被拒绝，请在浏览器登录后粘贴 localStorage.token 作为 Bearer");
            } else if (result.httpStatus == 429) {
                message = QStringLiteral("Agnes 请求过于频繁，请稍后重试");
            } else if (result.httpStatus >= 500) {
                message = QStringLiteral("Agnes 服务暂时不可用");
            } else if (!result.errorMessage.isEmpty()) {
                message = result.errorMessage;
                if (!message.startsWith(QStringLiteral("Agnes"))) {
                    message = QStringLiteral("Agnes %1").arg(message);
                }
            } else {
                message = QStringLiteral("Agnes 无法连接服务");
            }
            finalize(message);
            return;
        }
        }
    });
}
