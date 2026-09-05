// SPDX-License-Identifier: GPL-2.0-or-later

#include "deepseekclient.h"

#include "deepseekresponseparser.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QVariantList>

#include <QByteArrayView>

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
    : CredentialClientBase(parent)
{
    setSnapshot(emptySnapshot(QStringLiteral("未配置")));
    // 环境变量优先于 KDE 钱包（互斥）；构造期已能确定时直接标注状态。
    setCredentialState(isEnvironmentConfigured()
                           ? QStringLiteral("已通过环境变量配置")
                           : QStringLiteral("待连接 KDE 钱包"),
                       false,
                       false);
}

DeepSeekClient::~DeepSeekClient() = default;

QVariantMap DeepSeekClient::emptySnapshot(const QString &status, const QString &error) const
{
    return ::emptySnapshot(status, error);
}

QString DeepSeekClient::walletEntryKey() const
{
    return QStringLiteral("deepseek");
}

QString DeepSeekClient::credentialClearedText() const
{
    return isEnvironmentConfigured()
        ? QStringLiteral("钱包凭据已移除；环境变量仍在生效")
        : QStringLiteral("API Key 已移除");
}

void DeepSeekClient::onCredentialCleared()
{
    if (!isEnvironmentConfigured()) {
        setSnapshot(emptySnapshot(QStringLiteral("未配置")));
    }
}

QUrl DeepSeekClient::balanceEndpoint()
{
    return QUrl(QStringLiteral("https://api.deepseek.com/user/balance"));
}

QNetworkRequest DeepSeekClient::createRequest(const QUrl &url, QByteArrayView apiKey)
{
    QNetworkRequest request(url);
    request.setRawHeader("Authorization",
                          QByteArrayLiteral("Bearer ") + apiKey.toByteArray());
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "AIQuotaPilot/0.2");
    request.setTransferTimeout(15000);
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

    // 环境变量优先，其次 KDE 钱包；都为空则按"未配置"处理（与旧行为一致）。
    QByteArray apiKey = qgetenv("DEEPSEEK_API_KEY").trimmed();
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
    m_lastRequestError.clear();
    setLoading(true);

    QNetworkReply *reply = m_network->get(
        createRequest(balanceEndpoint(), m_activeSecret));
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
