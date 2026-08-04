// SPDX-License-Identifier: GPL-2.0-or-later

#include "minimaxclient.h"

#include "minimaxresponseparser.h"

#include <KWallet>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QVariantList>

namespace
{
constexpr qsizetype maximumResponseBytes = 1024 * 1024;
const QString walletFolder = QStringLiteral("AIQuotaPilot");
const QString miniMaxWalletEntry = QStringLiteral("MiniMax API Key");
constexpr int walletRetryLimit = 5;

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

QString networkErrorMessage(int httpStatus, QNetworkReply::NetworkError error)
{
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
    , m_snapshot(emptySnapshot(QStringLiteral("未配置")))
{
    const bool configuredByEnvironment = !qgetenv("MINIMAX_API_KEY").trimmed().isEmpty();
    m_credentialStatus = configuredByEnvironment
        ? QStringLiteral("已通过环境变量配置")
        : QStringLiteral("正在读取 KDE 钱包…");
    if (!configuredByEnvironment) {
        m_credentialBusy = true;
        QTimer::singleShot(0, this, &MiniMaxClient::openWallet);
    }
}

MiniMaxClient::~MiniMaxClient()
{
    m_storedApiKey.fill('\0');
    m_activeApiKey.fill('\0');
    m_pendingApiKey.fill(QChar(u'\0'));
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
    return !qgetenv("MINIMAX_API_KEY").trimmed().isEmpty() || !m_storedApiKey.isEmpty();
}

QString MiniMaxClient::credentialStatus() const
{
    return m_credentialStatus;
}

bool MiniMaxClient::credentialBusy() const
{
    return m_credentialBusy;
}

bool MiniMaxClient::credentialError() const
{
    return m_credentialError;
}

QList<QUrl> MiniMaxClient::endpointCandidates()
{
    return {
        QUrl(QStringLiteral("https://api.minimaxi.com/v1/api/openplatform/coding_plan/remains")),
        QUrl(QStringLiteral("https://api.minimax.io/v1/api/openplatform/coding_plan/remains")),
    };
}

void MiniMaxClient::setNetworkAccessManager(QNetworkAccessManager *network)
{
    if (m_network) {
        m_network->deleteLater();
    }
    m_network = network;
}

QNetworkRequest MiniMaxClient::createRequest(const QUrl &url, QByteArrayView apiKey)
{
    QNetworkRequest request(url);
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
        apiKey = m_storedApiKey;
    }
    if (apiKey.isEmpty()) {
        setSnapshot(emptySnapshot(QStringLiteral("未配置")));
        return;
    }

    m_activeApiKey.fill('\0');
    m_activeApiKey = apiKey;
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

    QNetworkReply *reply = m_network->get(
        createRequest(m_endpoints.at(m_endpointIndex++), m_activeApiKey));
    m_reply = reply;

    QTimer::singleShot(10000, reply, [reply]() {
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
        const bool timedOut = reply->property("aiUsageWatcherTimedOut").toBool();
        const bool responseTooLarge = reply->property("aiUsageWatcherResponseTooLarge").toBool();

        bool succeeded = false;
        if (timedOut) {
            m_lastRequestError = QStringLiteral("MiniMax 请求超时");
        } else if (responseTooLarge) {
            m_lastRequestError = QStringLiteral("MiniMax 响应过大，已拒绝处理");
        } else if (httpStatus == 401 || httpStatus == 403) {
            m_lastRequestError = QStringLiteral("MiniMax Key 无效或已过期");
            setSnapshot(emptySnapshot(QStringLiteral("请求失败"), m_lastRequestError));
            reply->deleteLater();
            finishRefresh();
            return;
        } else if (reply->error() != QNetworkReply::NoError || httpStatus < 200 || httpStatus >= 300) {
            m_lastRequestError = networkErrorMessage(httpStatus, reply->error());
        } else {
            const QByteArray payload = reply->read(maximumResponseBytes + 1);
            if (payload.size() > maximumResponseBytes) {
                m_lastRequestError = QStringLiteral("MiniMax 响应过大，已拒绝处理");
            } else {
                const MiniMaxParseResult result = MiniMaxResponseParser::parse(payload);
                if (result.ok) {
                    setSnapshot(toVariantMap(result.snapshot));
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
            requestNextEndpoint();
        }
    });
}

void MiniMaxClient::finishRefresh()
{
    m_activeApiKey.fill('\0');
    m_activeApiKey.clear();
    m_endpoints.clear();
    m_endpointIndex = 0;
    setLoading(false);
}

void MiniMaxClient::saveCredential(const QString &apiKey)
{
    const QString trimmedApiKey = apiKey.trimmed();
    if (trimmedApiKey.isEmpty()) {
        setCredentialState(QStringLiteral("API Key 不能为空"), false, true);
        return;
    }

    m_pendingApiKey.fill(QChar(u'\0'));
    m_pendingApiKey = trimmedApiKey;
    m_pendingCredentialOperation = PendingCredentialOperation::Save;
    setCredentialState(QStringLiteral("正在保存到 KDE 钱包…"), true, false);
    if (m_wallet && m_wallet->isOpen()) {
        performPendingCredentialOperation();
    } else {
        openWallet();
    }
}

void MiniMaxClient::clearCredential()
{
    m_pendingApiKey.fill(QChar(u'\0'));
    m_pendingApiKey.clear();
    m_pendingCredentialOperation = PendingCredentialOperation::Clear;
    setCredentialState(QStringLiteral("正在从 KDE 钱包移除…"), true, false);
    if (m_wallet && m_wallet->isOpen()) {
        performPendingCredentialOperation();
    } else {
        openWallet();
    }
}

void MiniMaxClient::openWallet()
{
    if ((m_wallet && m_wallet->isOpen()) || m_walletOpening) {
        if (m_wallet && m_wallet->isOpen()) {
            performPendingCredentialOperation();
        }
        return;
    }
    if (!KWallet::Wallet::isEnabled()) {
        setCredentialState(QStringLiteral("KDE 钱包未启用，无法安全保存 API Key"), false, true);
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
        // Plasma 启动竞态：kwalletd 可能晚于 plasmashell 就绪，失败后定时重试自愈
        // ponytail: 上限 5 次（约 25s），kwalletd 故障时避免永久重试；钱包被禁用时提前返回
        if (++m_walletRetryCount < walletRetryLimit) {
            QTimer::singleShot(5000, this, &MiniMaxClient::openWallet);
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
        // 钱包文件夹改名后一次性迁移旧 Key（幂等，见 migrateLegacyWalletEntry）
        migrateLegacyWalletEntry();
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
        setStoredApiKey({});
        m_wallet = nullptr;
        if (qgetenv("MINIMAX_API_KEY").trimmed().isEmpty()) {
            setCredentialState(QStringLiteral("KDE 钱包已锁定"), false, true);
        } else {
            setCredentialState(QStringLiteral("已通过环境变量配置"), false, false);
        }
        openedWallet->deleteLater();
    });
    connect(openedWallet,
            &KWallet::Wallet::folderUpdated,
            this,
            [this, openedWallet](const QString &folder) {
                if (m_wallet == openedWallet && folder == walletFolder
                    && m_pendingCredentialOperation == PendingCredentialOperation::None) {
                    loadCredential();
                }
            });
}

bool MiniMaxClient::prepareWalletFolder()
{
    if (!m_wallet || !m_wallet->isOpen()) {
        return false;
    }
    if (!m_wallet->hasFolder(walletFolder) && !m_wallet->createFolder(walletFolder)) {
        return false;
    }
    return m_wallet->setFolder(walletFolder);
}

bool MiniMaxClient::migrateLegacyWalletEntry()
{
    // 项目改名后钱包文件夹由 "AI Usage Watcher" 迁至 AIQuotaPilot。
    // 幂等：新文件夹已有 Key 则跳过；迁移后删除旧条目。
    const QString legacyFolder = QStringLiteral("AI Usage Watcher");
    if (!m_wallet || !m_wallet->isOpen() || !m_wallet->hasFolder(legacyFolder)) {
        return true;
    }
    m_wallet->setFolder(legacyFolder);
    QString legacyKey;
    const int result = m_wallet->readPassword(miniMaxWalletEntry, legacyKey);
    m_wallet->setFolder(walletFolder);
    if (result != 0 || legacyKey.trimmed().isEmpty()) {
        legacyKey.fill(QChar(u'\0'));
        return true;
    }
    QString currentKey;
    const bool alreadyPresent = m_wallet->readPassword(miniMaxWalletEntry, currentKey) == 0
        && !currentKey.isEmpty();
    currentKey.fill(QChar(u'\0'));
    if (alreadyPresent) {
        legacyKey.fill(QChar(u'\0'));
        return true;
    }
    m_wallet->writePassword(miniMaxWalletEntry, legacyKey);
    m_wallet->setFolder(legacyFolder);
    m_wallet->removeEntry(miniMaxWalletEntry);
    m_wallet->setFolder(walletFolder);
    legacyKey.fill(QChar(u'\0'));
    return true;
}

void MiniMaxClient::loadCredential()
{
    QString apiKey;
    const int result = m_wallet->readPassword(miniMaxWalletEntry, apiKey);
    if (result == 0 && !apiKey.trimmed().isEmpty()) {
        setStoredApiKey(apiKey.trimmed().toUtf8());
        apiKey.fill(QChar(u'\0'));
        setCredentialState(QStringLiteral("已保存在 KDE 钱包"), false, false);
        refresh();
        return;
    }
    apiKey.fill(QChar(u'\0'));
    setStoredApiKey({});
    setCredentialState(QStringLiteral("尚未保存 API Key"), false, false);
}

void MiniMaxClient::performPendingCredentialOperation()
{
    if (!prepareWalletFolder()) {
        setCredentialState(QStringLiteral("无法访问 KDE 钱包"), false, true);
        return;
    }

    if (m_pendingCredentialOperation == PendingCredentialOperation::Save) {
        QString apiKey = m_pendingApiKey;
        const int result = m_wallet->writePassword(miniMaxWalletEntry, apiKey);
        if (result == 0) {
            QByteArray storedApiKey = apiKey.toUtf8();
            setStoredApiKey(storedApiKey);
            storedApiKey.fill('\0');
            setCredentialState(QStringLiteral("API Key 已保存到 KDE 钱包"), false, false);
            refresh();
        } else {
            setCredentialState(QStringLiteral("API Key 保存失败，请检查 KDE 钱包"), false, true);
        }
        apiKey.fill(QChar(u'\0'));
    } else if (m_pendingCredentialOperation == PendingCredentialOperation::Clear) {
        const int result = m_wallet->hasEntry(miniMaxWalletEntry)
            ? m_wallet->removeEntry(miniMaxWalletEntry) : 0;
        if (result == 0) {
            setStoredApiKey({});
            const bool environmentConfigured = !qgetenv("MINIMAX_API_KEY").trimmed().isEmpty();
            setCredentialState(environmentConfigured
                                   ? QStringLiteral("钱包凭据已移除；环境变量仍在生效")
                                   : QStringLiteral("API Key 已移除"),
                               false,
                               false);
            if (!environmentConfigured) {
                setSnapshot(emptySnapshot(QStringLiteral("未配置")));
            }
        } else {
            setCredentialState(QStringLiteral("API Key 移除失败，请检查 KDE 钱包"), false, true);
        }
    }

    m_pendingApiKey.fill(QChar(u'\0'));
    m_pendingApiKey.clear();
    m_pendingCredentialOperation = PendingCredentialOperation::None;
}

void MiniMaxClient::setStoredApiKey(const QByteArray &apiKey)
{
    const bool wasConfigured = credentialConfigured();
    m_storedApiKey.fill('\0');
    m_storedApiKey = apiKey;
    if (wasConfigured != credentialConfigured()) {
        Q_EMIT credentialConfiguredChanged();
    }
}

void MiniMaxClient::setCredentialState(const QString &status, bool busy, bool error)
{
    if (error && !busy) {
        m_pendingApiKey.fill(QChar(u'\0'));
        m_pendingApiKey.clear();
        m_pendingCredentialOperation = PendingCredentialOperation::None;
    }
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
    setSnapshot(emptySnapshot(QStringLiteral("请求失败"), message));
}

void MiniMaxClient::setSnapshot(const QVariantMap &snapshot)
{
    if (m_snapshot == snapshot) {
        return;
    }
    m_snapshot = snapshot;
    Q_EMIT snapshotChanged();
}
