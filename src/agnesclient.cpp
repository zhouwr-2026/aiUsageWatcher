// SPDX-License-Identifier: GPL-2.0-or-later

#include "agnesclient.h"

#include "agnesresponseparser.h"
#include "kwalletdispatcher.h"
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

// 高置信、简洁的失败分类：HTTP 状态优先，其次 Qt 传输层错误；
// 兜底文案带 enum 数值便于定位，且不包含任何凭据内容。
QString networkErrorMessage(int httpStatus, QNetworkReply::NetworkError error)
{
    if (httpStatus == 401 || httpStatus == 403) {
        return QStringLiteral("Agnes 凭据无效或已过期；若使用 API Key 仍被拒绝，请在浏览器登录后粘贴 localStorage.token 作为 Bearer");
    }
    if (httpStatus == 429) {
        return QStringLiteral("Agnes 请求过于频繁，请稍后重试");
    }
    if (httpStatus >= 500) {
        return QStringLiteral("Agnes 服务暂时不可用");
    }

    switch (error) {
    case QNetworkReply::TimeoutError:
        return QStringLiteral("Agnes 请求超时");
    case QNetworkReply::HostNotFoundError:
        return QStringLiteral("无法解析 Agnes 服务地址");
    case QNetworkReply::ConnectionRefusedError:
        return QStringLiteral("Agnes 服务拒绝连接");
    case QNetworkReply::SslHandshakeFailedError:
        return QStringLiteral("Agnes TLS 握手失败");
    case QNetworkReply::RemoteHostClosedError:
        return QStringLiteral("Agnes 服务关闭了连接");
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
        return QStringLiteral("Agnes 网络暂时不可用");
    default:
        return QStringLiteral("无法连接 Agnes 服务（网络错误 %1）").arg(static_cast<int>(error));
    }
}
}

AgnesClient::AgnesClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_snapshot(emptySnapshot(QStringLiteral("未配置")))
{
    m_credentialStatus = QStringLiteral("待连接 KDE 钱包");
}

AgnesClient::~AgnesClient()
{
    m_storedApiKey.fill('\0');
    m_activeApiKey.fill('\0');
    m_pendingApiKey.fill(QChar(u'\0'));
}

QVariantMap AgnesClient::snapshot() const
{
    return m_snapshot;
}

bool AgnesClient::loading() const
{
    return m_loading;
}

bool AgnesClient::credentialConfigured() const
{
    return !m_storedApiKey.isEmpty();
}

QString AgnesClient::credentialStatus() const
{
    return m_credentialStatus;
}

bool AgnesClient::credentialBusy() const
{
    return m_credentialBusy;
}

bool AgnesClient::credentialError() const
{
    return m_credentialError;
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

void AgnesClient::refresh()
{
    if (m_authInvalid) {
        return;
    }
    if (m_loading) {
        m_refreshPending = true;
        return;
    }
    if (m_storedApiKey.isEmpty()) {
        setSnapshot(emptySnapshot(QStringLiteral("未配置")));
        return;
    }

    m_activeApiKey.fill('\0');
    m_activeApiKey = m_storedApiKey;
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

    req->get(createRequest(usageEndpoint(), m_activeApiKey),
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

void AgnesClient::forceRefresh()
{
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

void AgnesClient::cancelRefresh()
{
    m_refreshPending = false;
    m_refreshInterrupted = false;
    if (m_request) m_request->abort();
}

void AgnesClient::saveCredential(const QString &apiKey)
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
    m_authInvalid = false;
    setCredentialState(QStringLiteral("正在保存到 KDE 钱包…"), true, false);

    const QString snapshotValue = m_pendingApiKey;
    m_dispatcher->submit(KWalletDispatcher::Op::Save,
                         QStringLiteral("agnes-ai"),
                         snapshotValue,
                         [this](const KWalletDispatcher::Result &result) {
                             handleCredentialSave(result);
                         });
}

void AgnesClient::clearCredential()
{
    if (!m_dispatcher) {
        setCredentialState(QStringLiteral("凭据服务暂不可用"), false, true);
        return;
    }
    m_pendingApiKey.fill(QChar(u'\0'));
    m_pendingApiKey.clear();
    setCredentialState(QStringLiteral("正在从 KDE 钱包移除…"), true, false);

    m_dispatcher->submit(KWalletDispatcher::Op::Clear,
                         QStringLiteral("agnes-ai"),
                         QString(),
                         [this](const KWalletDispatcher::Result &result) {
                             handleCredentialClear(result);
                         });
}

void AgnesClient::setWalletDispatcher(KWalletDispatcher *dispatcher)
{
    m_dispatcher = dispatcher;
    if (m_dispatcher && !m_initialLoadDispatched) {
        m_initialLoadDispatched = true;
        QTimer::singleShot(1500, this, [this] { requestCredentialLoad(); });
    }
}

void AgnesClient::reloadCredential()
{
    requestCredentialLoad();
}

void AgnesClient::requestCredentialLoad()
{
    if (!m_dispatcher) {
        return;
    }
    m_credentialBusy = true;
    Q_EMIT credentialBusyChanged();
    m_dispatcher->submit(KWalletDispatcher::Op::Read,
                         QStringLiteral("agnes-ai"),
                         QString(),
                         [this](const KWalletDispatcher::Result &result) {
                             handleCredentialRead(result);
                         });
}

void AgnesClient::handleCredentialRead(const KWalletDispatcher::Result &result)
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

void AgnesClient::handleCredentialSave(const KWalletDispatcher::Result &result)
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

void AgnesClient::handleCredentialClear(const KWalletDispatcher::Result &result)
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

void AgnesClient::setStoredApiKey(const QByteArray &apiKey)
{
    const bool wasConfigured = credentialConfigured();
    m_storedApiKey.fill('\0');
    m_storedApiKey = apiKey;
    if (wasConfigured != credentialConfigured()) {
        Q_EMIT credentialConfiguredChanged();
    }
}

void AgnesClient::setCredentialState(const QString &status, bool busy, bool error)
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

void AgnesClient::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    Q_EMIT loadingChanged();
}

void AgnesClient::setError(const QString &message)
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

void AgnesClient::setSnapshot(const QVariantMap &snapshot)
{
    if (m_snapshot == snapshot) {
        return;
    }
    m_snapshot = snapshot;
    Q_EMIT snapshotChanged();
}

void AgnesClient::finishRefresh()
{
    m_activeApiKey.fill('\0');
    m_activeApiKey.clear();
    setLoading(false);
    if (m_refreshPending) {
        m_refreshPending = false;
        m_refreshInterrupted = false;
        QTimer::singleShot(0, this, &AgnesClient::refresh);
    }
}
