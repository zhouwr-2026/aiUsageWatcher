// SPDX-License-Identifier: GPL-2.0-or-later

#include "opencodegoclient.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QVariantList>

namespace
{
constexpr qsizetype maximumResponseBytes = 1024 * 1024;

QVariantMap emptySnapshot(const QString &status, const QString &error = {})
{
    return {
        {QStringLiteral("providerId"), QStringLiteral("opencode-go")},
        {QStringLiteral("statusLabel"), status},
        {QStringLiteral("errorText"), error},
        {QStringLiteral("plans"), QVariantList{}},
    };
}

QVariantMap planMap(const QString &planId, const QString &planName, double percent,
                    qint64 resetMs)
{
    return {
        {QStringLiteral("planId"), planId},
        {QStringLiteral("planName"), planName},
        {QStringLiteral("used"), qMin(100.0, percent)},
        {QStringLiteral("total"), 100},
        {QStringLiteral("unit"), QStringLiteral("%")},
        {QStringLiteral("resetText"), resetMs > 0
             ? QDateTime::fromMSecsSinceEpoch(resetMs).toLocalTime().toString(QStringLiteral("MM-dd HH:mm"))
             : QString{}},
        {QStringLiteral("extraText"), QString{}},
        {QStringLiteral("isValid"), true},
        {QStringLiteral("invalidReason"), QString{}},
    };
}

// 把 wallet 中 JSON 解码出来；不合法返回 false 并清空两字段。
bool decodeCredential(const QByteArray &json, QString &workspaceId, QString &cookie)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        workspaceId = obj.value(QStringLiteral("workspaceId")).toString();
        cookie = obj.value(QStringLiteral("cookie")).toString();
        return true;
    }
    workspaceId.clear();
    cookie.clear();
    return false;
}
}

OpenCodeGoClient::OpenCodeGoClient(QObject *parent)
    : CredentialClientBase(parent)
{
    setSnapshot(emptySnapshot(QStringLiteral("待刷新")));
}

OpenCodeGoClient::~OpenCodeGoClient()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
    }
}

QVariantMap OpenCodeGoClient::emptySnapshot(const QString &status, const QString &error) const
{
    return ::emptySnapshot(status, error);
}

QString OpenCodeGoClient::walletEntryKey() const
{
    return QStringLiteral("opencodego");
}

bool OpenCodeGoClient::credentialConfigured() const
{
    // 双字段都非空才算“已配置”；JSON 里没有 workspaceId/cookie 时为空。
    return !m_storedWorkspaceId.isEmpty() && !m_storedCookie.isEmpty();
}

void OpenCodeGoClient::setStoredSecret(const QByteArray &secret)
{
    // 解码 JSON 后缓存双字段，供 refresh 构造抓取 URL / Cookie 头使用。
    QString workspaceId;
    QString cookie;
    decodeCredential(secret, workspaceId, cookie);
    const bool contentChanged = m_storedWorkspaceId != workspaceId || m_storedCookie != cookie;
    m_storedWorkspaceId = workspaceId;
    m_storedCookie = cookie;
    // 基类按“配置状态是否翻转”emit；OpenCodeGo 原语义是“任一字段变化”即通知，
    // 保持原语义（内容变化但状态不变时 QML 绑定值不变，属无害多一次通知）。
    CredentialClientBase::setStoredSecret(secret);
    if (contentChanged) {
        Q_EMIT credentialConfiguredChanged();
    }
}

void OpenCodeGoClient::handleCredentialReadOk(const QString &rawValue)
{
    QString workspaceId;
    QString cookie;
    if (decodeCredential(rawValue.toUtf8(), workspaceId, cookie)) {
        setStoredSecret(rawValue.toUtf8());
        setCredentialState(credentialConfigured() ? QStringLiteral("已配置")
                                                  : QStringLiteral("未配置"),
                           false,
                           false);
        if (credentialConfigured()) {
            refresh();
        }
        return;
    }
    setStoredSecret({});
    setCredentialState(QStringLiteral("凭据格式无效，请重新保存"), false, true);
}

void OpenCodeGoClient::onCredentialSaved()
{
    // 原行为：保存成功后不立即刷新（等下一个定时器），避免页面抓取过频。
}

void OpenCodeGoClient::onCredentialCleared()
{
    // 原行为：清除成功后保留当前快照，等待下一次刷新自然更新。
}

void OpenCodeGoClient::saveCredential(const QString &workspaceId, const QString &cookie)
{
    const QString trimmedWorkspaceId = workspaceId.trimmed();
    const QString trimmedCookie = cookie.trimmed();
    if (trimmedWorkspaceId.isEmpty() || trimmedCookie.isEmpty()) {
        setCredentialState(QStringLiteral("工作区 ID 与 Cookie 不能为空"), false, true);
        return;
    }
    // 序列化为 JSON，复用基类“提交保存”骨架（pending + busy + submit Save）。
    const QJsonObject obj{
        {QStringLiteral("workspaceId"), trimmedWorkspaceId},
        {QStringLiteral("cookie"), trimmedCookie},
    };
    const QString payload = QString::fromUtf8(
        QJsonDocument(obj).toJson(QJsonDocument::Compact));
    submitCredentialSave(payload);
}

QString OpenCodeGoClient::credentialMissingText() const
{
    return QStringLiteral("未配置");
}

QString OpenCodeGoClient::credentialSavedText() const
{
    return QStringLiteral("已保存");
}

QString OpenCodeGoClient::credentialSaveFailedText() const
{
    return QStringLiteral("保存到 KDE 钱包失败");
}

QString OpenCodeGoClient::credentialClearedText() const
{
    return QStringLiteral("已清除");
}

QString OpenCodeGoClient::credentialClearFailedText() const
{
    return QStringLiteral("清除 KDE 钱包失败");
}

QString OpenCodeGoClient::requestFailedStatus() const
{
    return QStringLiteral("不可用");
}

QVariantList OpenCodeGoClient::parseUsageHtml(const QByteArray &html, qint64 nowMs)
{
    // SolidJS 序列化：rollingUsage:$R[N]={...}（键名无引号）
    struct WindowSlot {
        QLatin1String pattern;
        QString planId;
        QString planName;
    };
    const QList<WindowSlot> windowSlots = {
        {QLatin1String("rollingUsage:\\$R\\[\\d+\\]=(\\{[^}]+\\})"), QStringLiteral("five-hour"), QStringLiteral("5 小时")},
        {QLatin1String("weeklyUsage:\\$R\\[\\d+\\]=(\\{[^}]+\\})"), QStringLiteral("weekly"), QStringLiteral("每周")},
        {QLatin1String("monthlyUsage:\\$R\\[\\d+\\]=(\\{[^}]+\\})"), QStringLiteral("monthly"), QStringLiteral("月度额度")},
    };

    const QRegularExpression keyQuoteRe(QStringLiteral("([{,]\\s*)([a-zA-Z_][a-zA-Z0-9_]*)(\\s*:)"));

    QVariantList plans;
    for (const WindowSlot &slot : windowSlots) {
        const QRegularExpression re(slot.pattern,
                                    QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = re.match(QString::fromUtf8(html));
        if (!match.hasMatch())
            return {};
        // 修复 SolidJS 无引号键名 → 合法 JSON
        QString jsonStr = match.captured(1);
        jsonStr.replace(keyQuoteRe, QStringLiteral("\\1\"\\2\"\\3"));
        const QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (!doc.isObject())
            return {};
        const QJsonObject obj = doc.object();
        const QJsonValue percentV = obj.value(QStringLiteral("usagePercent"));
        const QJsonValue percentAlt = obj.value(QStringLiteral("usage_percent"));
        const double percent = (percentV.isDouble() ? percentV : percentAlt).toDouble(-1.0);
        if (percent < 0.0)
            return {};
        const QJsonValue resetV = obj.value(QStringLiteral("resetInSec"));
        const QJsonValue resetAlt = obj.value(QStringLiteral("resets_in_seconds"));
        const qint64 resetsInSec = qint64((resetV.isDouble() ? resetV : resetAlt).toDouble(-1.0));
        plans.push_back(planMap(slot.planId, slot.planName, percent,
                                resetsInSec > 0 ? nowMs + resetsInSec * 1000 : 0));
    }
    return plans;
}

void OpenCodeGoClient::refresh()
{
    if (m_loading) {
        m_refreshPending = true;
        return;
    }
    setLoading(true);
    if (!credentialConfigured()) {
        m_refreshPending = false;
        m_refreshInterrupted = false;
        setSnapshot(emptySnapshot(QStringLiteral("未配置"),
                                  QStringLiteral("请在配置页填写 OpenCode 工作区 ID 与 Cookie")));
        setLoading(false);
        return;
    }

    QUrl url(QStringLiteral("https://opencode.ai/workspace/%1/go")
                 .arg(QString::fromUtf8(QUrl::toPercentEncoding(m_storedWorkspaceId))));
    QNetworkRequest request(url);
    // 用户粘贴的是完整 Cookie 串（oc_locale=zh; auth=...）时原样使用；
    // 仅填纯 token（不含 =）时补 auth= 前缀
    QByteArray cookieHeader = m_storedCookie.toUtf8();
    if (!cookieHeader.contains('='))
        cookieHeader = "auth=" + cookieHeader;
    request.setRawHeader("Cookie", cookieHeader);
    request.setRawHeader("User-Agent",
                         QByteArrayLiteral("Mozilla/5.0 (X11; Linux x86_64) Gecko/20100101 Firefox/148.0"));
    request.setRawHeader("Accept", QByteArrayLiteral("text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"));
    request.setTransferTimeout(15000);
    QNetworkReply *reply = m_network->get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 并发刷新时各 lambda 绑定自己的 reply；仅当自己仍是当前请求时才清 m_reply
        if (!reply)
            return;
        if (m_reply == reply)
            m_reply = nullptr;
        if (m_refreshInterrupted) {
            reply->deleteLater();
            setLoading(false);
            m_refreshPending = false;
            m_refreshInterrupted = false;
            QTimer::singleShot(0, this, &OpenCodeGoClient::refresh);
            return;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 200) {
            const QByteArray body = reply->readAll();
            if (body.size() > maximumResponseBytes) {
                setError(QStringLiteral("页面响应过大"));
            } else {
                const QVariantList plans = parseUsageHtml(body, QDateTime::currentMSecsSinceEpoch());
                if (plans.isEmpty()) {
                    setError(QStringLiteral("未能从页面解析出用量数据，可能页面结构已变化"));
                } else {
                    setSnapshot({
                        {QStringLiteral("providerId"), QStringLiteral("opencode-go")},
                        {QStringLiteral("statusLabel"), QStringLiteral("可用")},
                        {QStringLiteral("errorText"), QString{}},
                        {QStringLiteral("plans"), plans},
                    });
                }
            }
        } else if (status == 401 || status == 403) {
            setSnapshot(emptySnapshot(QStringLiteral("凭据无效"),
                                      QStringLiteral("Cookie 已失效，请在配置页更新")));
        } else {
            setError(QStringLiteral("无法访问 OpenCode 服务"));
        }
        reply->deleteLater();
        setLoading(false);
        if (m_refreshPending) {
            m_refreshPending = false;
            QTimer::singleShot(0, this, &OpenCodeGoClient::refresh);
        }
    });
}
