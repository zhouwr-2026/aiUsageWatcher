// SPDX-License-Identifier: GPL-2.0-or-later

#include "minimaxclient.h"

#include "minimaxresponseparser.h"

#include <QDateTime>
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
        const QString resetText = QDateTime::fromMSecsSinceEpoch(plan.resetAtMs)
                                      .toLocalTime()
                                      .toString(QStringLiteral("MM-dd HH:mm"));
        plans.push_back(QVariantMap{
            {QStringLiteral("planId"), plan.planId},
            {QStringLiteral("planName"), plan.planName},
            {QStringLiteral("used"), plan.used},
            {QStringLiteral("total"), plan.total},
            {QStringLiteral("unit"), QStringLiteral("%")},
            {QStringLiteral("resetText"), resetText},
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

QString networkErrorMessage(int httpStatus, QNetworkReply::NetworkError error)
{
    if (httpStatus == 401 || httpStatus == 403) {
        return QStringLiteral("MiniMax 认证失败，请检查 API Key");
    }
    if (httpStatus == 429) {
        return QStringLiteral("MiniMax 请求过于频繁，请稍后重试");
    }
    if (httpStatus >= 500) {
        return QStringLiteral("MiniMax 服务暂时不可用");
    }
    if (error == QNetworkReply::TimeoutError) {
        return QStringLiteral("MiniMax 请求超时");
    }
    return QStringLiteral("无法连接 MiniMax 服务");
}
}

MiniMaxClient::MiniMaxClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_snapshot(emptySnapshot(QStringLiteral("未配置"),
                               QStringLiteral("请设置 MINIMAX_API_KEY 环境变量")))
{
}

QVariantMap MiniMaxClient::snapshot() const
{
    return m_snapshot;
}

bool MiniMaxClient::loading() const
{
    return m_loading;
}

bool MiniMaxClient::credentialConfigured() const
{
    return !qgetenv("MINIMAX_API_KEY").trimmed().isEmpty();
}

QNetworkRequest MiniMaxClient::createRequest(QByteArrayView apiKey)
{
    QNetworkRequest request(QUrl(QStringLiteral("https://www.minimaxi.com/v1/token_plan/remains")));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + apiKey.toByteArray());
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "AIUsageWatcher/0.1");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);
    return request;
}

void MiniMaxClient::refresh()
{
    if (m_loading) {
        return;
    }

    QByteArray apiKey = qgetenv("MINIMAX_API_KEY").trimmed();
    if (apiKey.isEmpty()) {
        setSnapshot(emptySnapshot(QStringLiteral("未配置"),
                                  QStringLiteral("请设置 MINIMAX_API_KEY 环境变量")));
        return;
    }

    setLoading(true);
    QNetworkReply *reply = m_network->get(createRequest(apiKey));
    apiKey.fill('\0');

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
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool timedOut = reply->property("aiUsageWatcherTimedOut").toBool();
        const bool responseTooLarge = reply->property("aiUsageWatcherResponseTooLarge").toBool();

        if (timedOut) {
            setError(QStringLiteral("MiniMax 请求超时"));
        } else if (responseTooLarge) {
            setError(QStringLiteral("MiniMax 响应过大，已拒绝处理"));
        } else if (reply->error() != QNetworkReply::NoError || httpStatus < 200 || httpStatus >= 300) {
            setError(networkErrorMessage(httpStatus, reply->error()));
        } else {
            const QByteArray payload = reply->read(maximumResponseBytes + 1);
            if (payload.size() > maximumResponseBytes) {
                setError(QStringLiteral("MiniMax 响应过大，已拒绝处理"));
            } else {
                const MiniMaxParseResult result = MiniMaxResponseParser::parse(payload);
                if (result.ok) {
                    setSnapshot(toVariantMap(result.snapshot));
                } else {
                    setError(result.errorMessage);
                }
            }
        }

        setLoading(false);
        reply->deleteLater();
    });
}

void MiniMaxClient::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    Q_EMIT loadingChanged();
}

void MiniMaxClient::setError(const QString &message)
{
    QVariantMap snapshot = m_snapshot;
    snapshot.insert(QStringLiteral("statusLabel"), QStringLiteral("请求失败"));
    snapshot.insert(QStringLiteral("errorText"), message);
    setSnapshot(snapshot);
}

void MiniMaxClient::setSnapshot(const QVariantMap &snapshot)
{
    if (m_snapshot == snapshot) {
        return;
    }
    m_snapshot = snapshot;
    Q_EMIT snapshotChanged();
}
