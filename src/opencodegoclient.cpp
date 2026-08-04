// SPDX-License-Identifier: GPL-2.0-or-later

#include "opencodegoclient.h"

#include <KWallet>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QVariantList>

namespace
{
constexpr qsizetype maximumResponseBytes = 4 * 1024 * 1024;
const QString walletFolder = QStringLiteral("AIQuotaPilot");
const QString openCodeGoWalletEntry = QStringLiteral("opencode-go-credential");
constexpr int walletRetryLimit = 5;

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
}

OpenCodeGoClient::OpenCodeGoClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_snapshot(emptySnapshot(QStringLiteral("待刷新")))
    , m_credentialStatus(QStringLiteral("正在读取 KDE 钱包…"))
{
    m_credentialBusy = true;
    QTimer::singleShot(0, this, &OpenCodeGoClient::openWallet);
}

OpenCodeGoClient::~OpenCodeGoClient()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
    }
}

QVariantMap OpenCodeGoClient::snapshot() const
{
    return m_snapshot;
}

bool OpenCodeGoClient::loading() const
{
    return m_loading;
}

bool OpenCodeGoClient::credentialConfigured() const
{
    return !m_storedWorkspaceId.isEmpty() && !m_storedCookie.isEmpty();
}

QString OpenCodeGoClient::credentialStatus() const
{
    return m_credentialStatus;
}

bool OpenCodeGoClient::credentialBusy() const
{
    return m_credentialBusy;
}

bool OpenCodeGoClient::credentialError() const
{
    return m_credentialError;
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
    setLoading(true);
    if (!credentialConfigured()) {
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
    QNetworkReply *reply = m_network->get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 并发刷新时各 lambda 绑定自己的 reply；仅当自己仍是当前请求时才清 m_reply
        if (!reply)
            return;
        if (m_reply == reply)
            m_reply = nullptr;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 200) {
            const QByteArray body = reply->readAll();
            if (body.size() > maximumResponseBytes) {
                setSnapshot(emptySnapshot(QStringLiteral("不可用"), QStringLiteral("页面响应过大")));
            } else {
                const QVariantList plans = parseUsageHtml(body, QDateTime::currentMSecsSinceEpoch());
                if (plans.isEmpty()) {
                    setSnapshot(emptySnapshot(QStringLiteral("不可用"),
                                              QStringLiteral("未能从页面解析出用量数据，可能页面结构已变化")));
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
            setSnapshot(emptySnapshot(QStringLiteral("不可用"),
                                      QStringLiteral("无法访问 OpenCode 服务")));
        }
        reply->deleteLater();
        setLoading(false);
    });
}

void OpenCodeGoClient::saveCredential(const QString &workspaceId, const QString &cookie)
{
    const QString trimmedWorkspaceId = workspaceId.trimmed();
    const QString trimmedCookie = cookie.trimmed();
    if (trimmedWorkspaceId.isEmpty() || trimmedCookie.isEmpty()) {
        setCredentialState(QStringLiteral("工作区 ID 与 Cookie 不能为空"), false, true);
        return;
    }
    m_pendingWorkspaceId = trimmedWorkspaceId;
    m_pendingCookie = trimmedCookie;
    m_pendingCredentialOperation = PendingCredentialOperation::Save;
    setCredentialState(QStringLiteral("正在保存到 KDE 钱包…"), true, false);
    if (m_wallet && m_wallet->isOpen()) {
        performPendingCredentialOperation();
    } else {
        openWallet();
    }
}

void OpenCodeGoClient::clearCredential()
{
    m_pendingWorkspaceId.clear();
    m_pendingCookie.clear();
    m_pendingCredentialOperation = PendingCredentialOperation::Clear;
    setCredentialState(QStringLiteral("正在从 KDE 钱包移除…"), true, false);
    if (m_wallet && m_wallet->isOpen()) {
        performPendingCredentialOperation();
    } else {
        openWallet();
    }
}

void OpenCodeGoClient::openWallet()
{
    if ((m_wallet && m_wallet->isOpen()) || m_walletOpening) {
        if (m_wallet && m_wallet->isOpen()) {
            performPendingCredentialOperation();
        }
        return;
    }
    if (!KWallet::Wallet::isEnabled()) {
        setCredentialState(QStringLiteral("KDE 钱包未启用，无法安全保存凭据"), false, true);
        return;
    }
    if (m_wallet) {
        m_wallet->deleteLater();
        m_wallet = nullptr;
    }
    m_walletOpening = true;
    m_wallet = KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(),
                                           0,
                                           KWallet::Wallet::Asynchronous);
    if (!m_wallet) {
        m_walletOpening = false;
        setCredentialState(QStringLiteral("无法打开 KDE 钱包"), false, true);
        if (++m_walletRetryCount < walletRetryLimit) {
            QTimer::singleShot(5000, this, &OpenCodeGoClient::openWallet);
        }
        return;
    }
    m_wallet->setParent(this);
    KWallet::Wallet *openedWallet = m_wallet;
    connect(openedWallet, &KWallet::Wallet::walletOpened, this, [this, openedWallet](bool success) {
        if (m_wallet != openedWallet) {
            return;
        }
        m_walletOpening = false;
        m_walletRetryCount = 0;
        if (!success || !prepareWalletFolder()) {
            setCredentialState(QStringLiteral("无法访问 KDE 钱包"), false, true);
            return;
        }
        if (m_pendingCredentialOperation == PendingCredentialOperation::None) {
            loadCredential();
        } else {
            performPendingCredentialOperation();
        }
    });
    connect(openedWallet, &KWallet::Wallet::walletClosed, this, [this, openedWallet]() {
        if (m_wallet != openedWallet) {
            openedWallet->deleteLater();
            return;
        }
        setStoredCredential({}, {});
        m_wallet = nullptr;
        openedWallet->deleteLater();
    });
}

bool OpenCodeGoClient::prepareWalletFolder()
{
    return m_wallet && m_wallet->hasFolder(walletFolder)
        ? m_wallet->setFolder(walletFolder)
        : m_wallet && m_wallet->createFolder(walletFolder);
}

void OpenCodeGoClient::loadCredential()
{
    if (!m_wallet || !m_wallet->isOpen()) {
        setCredentialState(QStringLiteral("无法读取 KDE 钱包"), false, true);
        return;
    }
    QByteArray storedJson;
    if (m_wallet->readEntry(openCodeGoWalletEntry, storedJson) != 0 || storedJson.isEmpty()) {
        setStoredCredential({}, {});
        setCredentialState(QStringLiteral("未配置"), false, false);
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(storedJson);
    if (!doc.isObject()) {
        setStoredCredential({}, {});
        setCredentialState(QStringLiteral("凭据格式无效，请重新保存"), false, true);
        return;
    }
    const QJsonObject obj = doc.object();
    setStoredCredential(obj.value(QStringLiteral("workspaceId")).toString(),
                        obj.value(QStringLiteral("cookie")).toString());
    setCredentialState(credentialConfigured() ? QStringLiteral("已配置") : QStringLiteral("未配置"),
                       false, false);
    // 启动时钱包异步打开，凭据加载完成后自动拉取一次用量
    if (credentialConfigured())
        QTimer::singleShot(0, this, &OpenCodeGoClient::refresh);
}

void OpenCodeGoClient::performPendingCredentialOperation()
{
    if (!m_wallet || !m_wallet->isOpen()) {
        setCredentialState(QStringLiteral("无法访问 KDE 钱包"), false, true);
        return;
    }
    if (m_pendingCredentialOperation == PendingCredentialOperation::Save) {
        const QJsonObject obj{
            {QStringLiteral("workspaceId"), m_pendingWorkspaceId},
            {QStringLiteral("cookie"), m_pendingCookie},
        };
        const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
        const int rc = m_wallet->writeEntry(openCodeGoWalletEntry, payload);
        if (rc != 0) {
            setCredentialState(QStringLiteral("保存到 KDE 钱包失败"), false, true);
            return;
        }
        setStoredCredential(m_pendingWorkspaceId, m_pendingCookie);
        setCredentialState(QStringLiteral("已保存"), false, false);
    } else if (m_pendingCredentialOperation == PendingCredentialOperation::Clear) {
        m_wallet->removeEntry(openCodeGoWalletEntry);
        setStoredCredential({}, {});
        setCredentialState(QStringLiteral("已清除"), false, false);
    }
    m_pendingCredentialOperation = PendingCredentialOperation::None;
}

void OpenCodeGoClient::setStoredCredential(const QString &workspaceId, const QString &cookie)
{
    const bool changed = m_storedWorkspaceId != workspaceId || m_storedCookie != cookie;
    m_storedWorkspaceId = workspaceId;
    m_storedCookie = cookie;
    if (changed) {
        Q_EMIT credentialConfiguredChanged();
    }
}

void OpenCodeGoClient::setCredentialState(const QString &status, bool busy, bool error)
{
    m_credentialStatus = status;
    m_credentialBusy = busy;
    m_credentialError = error;
    Q_EMIT credentialStatusChanged();
    Q_EMIT credentialBusyChanged();
    Q_EMIT credentialErrorChanged();
}

void OpenCodeGoClient::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    Q_EMIT loadingChanged();
}

void OpenCodeGoClient::setSnapshot(const QVariantMap &snapshot)
{
    m_snapshot = snapshot;
    Q_EMIT snapshotChanged();
}
