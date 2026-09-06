// SPDX-License-Identifier: GPL-2.0-or-later

#include "commandcodeclient.h"

#include "resilientnetworkrequest.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <QVariantList>

namespace
{
const QUrl summaryUrl(QStringLiteral("https://api.commandcode.ai/internal/usage/summary"));
const QUrl creditsUrl(QStringLiteral("https://api.commandcode.ai/internal/billing/credits"));

QVariantMap emptySnapshot(const QString &status, const QString &error = {})
{
    return {{QStringLiteral("providerId"), QStringLiteral("command-code")},
            {QStringLiteral("statusLabel"), status},
            {QStringLiteral("errorText"), error},
            {QStringLiteral("plans"), QVariantList{}}};
}

QVariantMap plan(const QString &id, const QString &name, double used,
                 double total, const QString &reset)
{
    return {{QStringLiteral("planId"), id}, {QStringLiteral("planName"), name},
            {QStringLiteral("used"), used}, {QStringLiteral("total"), total},
            {QStringLiteral("unit"), QStringLiteral("%")},
            {QStringLiteral("resetText"), reset}, {QStringLiteral("extraText"), QString{}},
            {QStringLiteral("isValid"), total > 0 && used >= 0},
            {QStringLiteral("invalidReason"), total > 0 && used >= 0 ? QString{} : QStringLiteral("额度字段无效")}};
}

QString resetText(const QJsonValue &value)
{
    if (value.isString())
    {
        const QString text = value.toString().trimmed();
        const QDateTime parsed = QDateTime::fromString(text, Qt::ISODate);
        return parsed.isValid() ? parsed.toLocalTime().toString(QStringLiteral("MM-dd HH:mm")) : text;
    }
    const qint64 raw = qint64(value.toDouble(0));
    if (raw <= 0)
        return {};
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (raw < 1000000000LL)
        return QDateTime::fromMSecsSinceEpoch(nowMs + raw * 1000)
            .toLocalTime().toString(QStringLiteral("MM-dd HH:mm"));
    return (raw > 100000000000LL
                ? QDateTime::fromMSecsSinceEpoch(raw)
                : QDateTime::fromSecsSinceEpoch(raw))
        .toLocalTime().toString(QStringLiteral("MM-dd HH:mm"));
}

bool validCookie(const QString &cookie)
{
    return !cookie.isEmpty() && !cookie.contains(QChar(u'\n'))
        && !cookie.contains(QChar(u'\r'));
}

QString commandCodeSessionCookie(const QString &cookie)
{
    QStringList sessionParts;
    for (const QString &part : cookie.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        const QString trimmed = part.trimmed();
        if (trimmed.startsWith(QStringLiteral("__Secure-commandcode_prod_.")))
            sessionParts.push_back(trimmed);
    }
    return sessionParts.join(QStringLiteral("; "));
}
}

CommandCodeClient::CommandCodeClient(QObject *parent)
    : CredentialClientBase(parent)
{
    setSnapshot(emptySnapshot(QStringLiteral("未配置")));
}

CommandCodeClient::~CommandCodeClient()
{
    if (m_summaryRequest) m_summaryRequest->abort();
    if (m_creditsRequest) m_creditsRequest->abort();
}

QVariantMap CommandCodeClient::emptySnapshot(const QString &status, const QString &error) const
{
    return ::emptySnapshot(status, error);
}

QString CommandCodeClient::walletEntryKey() const
{
    return QStringLiteral("commandcode");
}

QString CommandCodeClient::credentialMissingText() const
{
    return QStringLiteral("未配置");
}

QString CommandCodeClient::credentialSavedText() const
{
    return QStringLiteral("已保存");
}

QString CommandCodeClient::credentialSaveFailedText() const
{
    return QStringLiteral("保存到 KDE 钱包失败");
}

QString CommandCodeClient::credentialClearedText() const
{
    return QStringLiteral("已清除");
}

QString CommandCodeClient::credentialClearFailedText() const
{
    return QStringLiteral("清除 KDE 钱包失败");
}

QString CommandCodeClient::walletAccessFailedText() const
{
    return QStringLiteral("凭据服务暂不可用");
}

void CommandCodeClient::handleCredentialReadOk(const QString &rawValue)
{
    const QString cookie = commandCodeSessionCookie(rawValue.trimmed());
    setStoredSecret(validCookie(cookie) ? cookie.toUtf8() : QByteArray{});
    // 与原实现一致：空值也报“格式无效”，但 error 标志只在“非空却抽不出会话项”时为真。
    const bool invalid = m_storedSecret.isEmpty() && !rawValue.trimmed().isEmpty();
    setCredentialState(m_storedSecret.isEmpty()
                           ? QStringLiteral("凭据格式无效，请重新保存")
                           : QStringLiteral("已配置"),
                       false,
                       invalid);
    if (!m_storedSecret.isEmpty()) {
        refresh();
    }
}

void CommandCodeClient::saveCredential(const QString &value)
{
    const QString cookie = commandCodeSessionCookie(value.trimmed());
    if (!validCookie(cookie)) {
        setCredentialState(QStringLiteral("请粘贴包含 Command Code 会话项的完整 Cookie，且不能包含换行"), false, true);
        return;
    }
    submitCredentialSave(cookie);
}

void CommandCodeClient::abortActiveRequest()
{
    if (m_summaryRequest) m_summaryRequest->abort();
    if (m_creditsRequest) m_creditsRequest->abort();
}

QNetworkRequest CommandCodeClient::createRequest(const QUrl &url, const QString &cookie)
{
    QNetworkRequest request(url);
    request.setRawHeader("Cookie", cookie.toUtf8());
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "AIQuotaPilot/0.2");
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);
    return request;
}

void CommandCodeClient::refresh()
{
    if (m_loading) {
        m_refreshPending = true;
        return;
    }
    if (m_storedSecret.isEmpty()) {
        m_refreshPending = false;
        m_refreshInterrupted = false;
        setSnapshot(emptySnapshot(QStringLiteral("未配置")));
        return;
    }
    m_summary.clear(); m_credits.clear(); m_summaryError.clear(); m_creditsError.clear();
    setLoading(true);

    const QByteArray cookie = m_storedSecret;

    QPointer<CommandCodeClient> self = this;
    auto completeIfReady = [self]() {
        if (!self) return;
        if (self->m_summaryRequest || self->m_creditsRequest) return;
        if (self->m_refreshInterrupted) {
            self->finishRequests();
            return;
        }
        self->buildSnapshot();
        self->finishRequests();
    };

    auto wireOne = [self, completeIfReady, cookie](bool summary) {
        ResilientNetworkRequest **slot = summary ? &self->m_summaryRequest : &self->m_creditsRequest;
        QString *errorSlot = summary ? &self->m_summaryError : &self->m_creditsError;
        QVariantMap *target = summary ? &self->m_summary : &self->m_credits;
        const QUrl url = summary ? summaryUrl : creditsUrl;

        auto *req = new ResilientNetworkRequest(self->m_network, self.data());
        *slot = req;

        req->get(createRequest(url, QString::fromUtf8(cookie)),
                 [self, req, completeIfReady, errorSlot, target, summary](ResilientNetworkRequest::Result result) {
            req->deleteLater();
            if (!self) return;
            switch (result.outcome) {
            case ResilientNetworkRequest::Outcome::Success: {
                QJsonParseError parseError;
                const QJsonDocument doc = QJsonDocument::fromJson(result.payload, &parseError);
                if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
                    *errorSlot = QStringLiteral("服务返回格式无效");
                } else {
                    *target = doc.object().toVariantMap();
                }
                break;
            }
            case ResilientNetworkRequest::Outcome::Aborted:
                self->m_refreshInterrupted = true;
                break;
            case ResilientNetworkRequest::Outcome::NonRetryableFailure:
            case ResilientNetworkRequest::Outcome::RetryableFailure: {
                QString message;
                if (result.responseTooLarge) {
                    message = QStringLiteral("服务响应过大");
                } else if (result.httpStatus == 401 || result.httpStatus == 403) {
                    message = QStringLiteral("Cookie 已失效，请在配置页更新");
                } else if (!result.errorMessage.isEmpty()) {
                    message = QStringLiteral("Command Code 请求失败：%1").arg(result.errorMessage);
                } else if (result.httpStatus > 0) {
                    message = QStringLiteral("Command Code 请求失败（HTTP %1）").arg(result.httpStatus);
                } else {
                    message = QStringLiteral("Command Code 请求失败");
                }
                *errorSlot = message;
                break;
            }
            }
            if (self) {
                if ((summary ? self->m_summaryRequest : self->m_creditsRequest) == req) {
                    if (summary) self->m_summaryRequest = nullptr; else self->m_creditsRequest = nullptr;
                }
                completeIfReady();
            }
        });
    };

    wireOne(true);
    wireOne(false);
}

void CommandCodeClient::finishRequests()
{
    m_loading = false; Q_EMIT loadingChanged();
    if (m_refreshPending) {
        m_refreshPending = false;
        m_refreshInterrupted = false;
        QTimer::singleShot(0, this, &CommandCodeClient::refresh);
    }
}

void CommandCodeClient::buildSnapshot()
{
    QVariantList plans;
    QVariantMap windows = m_credits.value(QStringLiteral("windowLimits")).toMap();
    if (windows.isEmpty())
        windows = m_credits.value(QStringLiteral("limits")).toMap();
    for (const auto &entry : {QPair<QString, QString>{QStringLiteral("five-hour"), QStringLiteral("5 小时")}, QPair<QString, QString>{QStringLiteral("weekly"), QStringLiteral("每周")}, QPair<QString, QString>{QStringLiteral("monthly"), QStringLiteral("月度额度")}}) {
        QVariantMap value = windows.value(entry.first).toMap();
        if (value.isEmpty()) {
            const QString camelId = entry.first == QLatin1String("five-hour")
                ? QStringLiteral("fiveHour") : entry.first;
            value = windows.value(camelId).toMap();
        }
        if (value.isEmpty() && entry.first == QLatin1String("monthly")) {
            const QVariantMap creditValues = m_credits.value(QStringLiteral("credits")).toMap();
            const QVariant remainingValue = creditValues.value(QStringLiteral("monthlyCredits"));
            const QVariant consumedValue = m_summary.value(QStringLiteral("totalMonthlyCredits"));
            bool remainingOk = false;
            bool consumedOk = false;
            const double remaining = remainingValue.toDouble(&remainingOk);
            const double consumed = consumedValue.toDouble(&consumedOk);
            if (remainingOk && consumedOk && remaining >= 0 && consumed >= 0) {
                const double total = consumed + remaining;
                value.insert(QStringLiteral("usedPercent"), total > 0 ? consumed / total * 100.0 : -1.0);
                value.insert(QStringLiteral("limit"), 100.0);
            }
        }
        if (value.isEmpty()) {
            const QString stem = entry.first == QLatin1String("five-hour")
                ? QStringLiteral("fiveHour")
                : (entry.first == QLatin1String("weekly") ? QStringLiteral("weekly")
                                                            : QStringLiteral("monthly"));
            for (const QString &suffix : {QStringLiteral("Limit"), QStringLiteral("Usage"), QStringLiteral("" )}) {
                const QVariantMap candidate = m_credits.value(stem + suffix).toMap();
                if (!candidate.isEmpty()) { value = candidate; break; }
            }
        }
        if (value.isEmpty()) {
            const QString stem = entry.first == QLatin1String("five-hour")
                ? QStringLiteral("fiveHour")
                : (entry.first == QLatin1String("weekly") ? QStringLiteral("weekly")
                                                            : QStringLiteral("monthly"));
            for (const QString &suffix : {QStringLiteral("Percent"), QStringLiteral("Percentage")}) {
                const QVariant scalar = m_credits.value(stem + suffix);
                if (scalar.isValid()) { value.insert(QStringLiteral("usedPercent"), scalar); break; }
            }
        }
        const QStringList usedKeys{QStringLiteral("usedPercent"), QStringLiteral("usagePercent"),
                                   QStringLiteral("percentUsed"), QStringLiteral("usage_pct")};
        QVariant usedValue;
        for (const QString &key : usedKeys) {
            if (value.contains(key)) { usedValue = value.value(key); break; }
        }
        const QStringList totalKeys{QStringLiteral("limit"), QStringLiteral("total"),
                                    QStringLiteral("cap"), QStringLiteral("max"),
                                    QStringLiteral("credits")};
        QVariant totalValue;
        for (const QString &key : totalKeys) {
            if (value.contains(key)) { totalValue = value.value(key); break; }
        }
        double used = usedValue.isValid() ? usedValue.toDouble() : -1;
        double total = totalValue.isValid() ? totalValue.toDouble() : 100;
        if (!usedValue.isValid() && value.contains(QStringLiteral("used"))) {
            bool ok = false;
            const double absoluteUsed = value.value(QStringLiteral("used")).toDouble(&ok);
            used = ok ? absoluteUsed : -1;
        }
        if (!usedValue.isValid() && value.contains(QStringLiteral("remaining"))) {
            bool ok = false;
            const double remaining = value.value(QStringLiteral("remaining")).toDouble(&ok);
            used = ok ? total - remaining : -1;
        }
        QVariant resetValue = value.value(QStringLiteral("resetAt"));
        if (!resetValue.isValid()) resetValue = value.value(QStringLiteral("resetInSec"));
        if (!resetValue.isValid()) resetValue = value.value(QStringLiteral("resetsInSec"));
        plans.push_back(plan(entry.first, entry.second, used, total,
                             resetText(QJsonValue::fromVariant(resetValue))));
    }
    const bool summaryOk = !m_summary.isEmpty();
    const bool creditsOk = !m_credits.isEmpty();
    const QString error = !m_summaryError.isEmpty() ? m_summaryError : m_creditsError;
    if (!summaryOk && !creditsOk
        && !error.contains(QStringLiteral("Cookie 已失效"))
        && !snapshot().value(QStringLiteral("plans")).toList().isEmpty()) {
        QVariantMap staleSnapshot = snapshot();
        staleSnapshot.insert(QStringLiteral("statusLabel"), QStringLiteral("数据暂时不可更新"));
        staleSnapshot.insert(QStringLiteral("errorText"), error);
        staleSnapshot.insert(QStringLiteral("stale"), true);
        setSnapshot(staleSnapshot);
        return;
    }
    // 快照只含白名单字段；m_summary 仅作内部计算源，不整包透传给 QML。
    setSnapshot({{QStringLiteral("providerId"), QStringLiteral("command-code")}, {QStringLiteral("statusLabel"), summaryOk && creditsOk ? QStringLiteral("可用") : (summaryOk || creditsOk ? QStringLiteral("部分可用") : QStringLiteral("查询失败"))}, {QStringLiteral("errorText"), error}, {QStringLiteral("plans"), plans}});
}
