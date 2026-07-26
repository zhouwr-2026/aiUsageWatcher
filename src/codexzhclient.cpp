// SPDX-License-Identifier: GPL-2.0-or-later

#include "codexzhclient.h"

#include "codexzhresponseparser.h"

#include <KWallet>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QVariantList>

namespace
{
constexpr qsizetype maximumResponseBytes = 1024 * 1024;
const QString walletFolder = QStringLiteral("AI Usage Watcher");
const QString codexZhWalletEntry = QStringLiteral("CodexZH API Key");

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
    plans.push_back(QVariantMap{
        {QStringLiteral("planId"), snapshot.plan.planId},
        {QStringLiteral("planName"), snapshot.plan.planName},
        {QStringLiteral("used"), snapshot.plan.used},
        {QStringLiteral("total"), snapshot.plan.total},
        {QStringLiteral("unit"), QStringLiteral("USD")},
        {QStringLiteral("resetText"), QString()},
        {QStringLiteral("extraText"), snapshot.plan.extraText},
        {QStringLiteral("isValid"), true},
        {QStringLiteral("invalidReason"), QString()},
    });

    return {
        {QStringLiteral("providerId"), QStringLiteral("codexzh")},
        {QStringLiteral("statusLabel"), snapshot.statusLabel},
        {QStringLiteral("errorText"), snapshot.errorText},
        {QStringLiteral("plans"), plans},
    };
}

QString networkErrorMessage(int httpStatus, QNetworkReply::NetworkError error)
{
    if (httpStatus == 401 || httpStatus == 403) {
        return QStringLiteral("CodexZH 认证失败，请检查 API Key");
    }
    if (httpStatus == 429) {
        return QStringLiteral("CodexZH 请求过于频繁，请稍后重试");
    }
    if (httpStatus >= 500) {
        return QStringLiteral("CodexZH 服务暂时不可用");
    }
    if (error == QNetworkReply::TimeoutError) {
        return QStringLiteral("CodexZH 请求超时");
    }
    return QStringLiteral("无法连接 CodexZH 服务");
}
}

CodexZhClient::CodexZhClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_snapshot(emptySnapshot(QStringLiteral("未配置")))
{
    m_credentialStatus = QStringLiteral("正在读取 KDE 钱包…");
    m_credentialBusy = true;
    QTimer::singleShot(0, this, &CodexZhClient::openWallet);
}

CodexZhClient::~CodexZhClient()
{
    m_storedApiKey.fill('\0');
    m_activeApiKey.fill('\0');
    m_pendingApiKey.fill(QChar(u'\0'));
}

QVariantMap CodexZhClient::snapshot() const
{
    return m_snapshot;
}

bool CodexZhClient::loading() const
{
    return m_loading;
}

bool CodexZhClient::credentialConfigured() const
{
    return !m_storedApiKey.isEmpty();
}

QString CodexZhClient::credentialStatus() const
{
    return m_credentialStatus;
}

bool CodexZhClient::credentialBusy() const
{
    return m_credentialBusy;
}

bool CodexZhClient::credentialError() const
{
    return m_credentialError;
}

QList<QUrl> CodexZhClient::endpointCandidates()
{
    return {
        QUrl(QStringLiteral("https://codexzh.com/api/v1/usage/stats")),
    };
}

QNetworkRequest CodexZhClient::createRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "AIUsageWatcher/0.1");
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);
    return request;
}

void CodexZhClient::refresh()
{
    if (m_loading) {
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

    QUrl url = endpointCandidates().first();
    url.setQuery(QStringLiteral("key=%1").arg(QString::fromLatin1(m_activeApiKey)));

    QNetworkReply *reply = m_network->get(createRequest(url));
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

        if (timedOut) {
            setSnapshot(emptySnapshot(QStringLiteral("请求失败"), QStringLiteral("CodexZH 请求超时")));
        } else if (responseTooLarge) {
            setSnapshot(emptySnapshot(QStringLiteral("请求失败"), QStringLiteral("CodexZH 响应过大，已拒绝处理")));
        } else if (reply->error() != QNetworkReply::NoError || httpStatus < 200 || httpStatus >= 300) {
            setSnapshot(emptySnapshot(QStringLiteral("请求失败"), networkErrorMessage(httpStatus, reply->error())));
        } else {
            const QByteArray payload = reply->read(maximumResponseBytes + 1);
            if (payload.size() > maximumResponseBytes) {
                setSnapshot(emptySnapshot(QStringLiteral("请求失败"), QStringLiteral("CodexZH 响应过大，已拒绝处理")));
            } else {
                const CodexZhParseResult result = CodexZhResponseParser::parse(payload);
                if (result.ok) {
                    setSnapshot(toVariantMap(result.snapshot));
                } else {
                    setSnapshot(emptySnapshot(QStringLiteral("请求失败"), result.errorMessage));
                }
            }
        }

        reply->deleteLater();
        m_activeApiKey.fill('\0');
        m_activeApiKey.clear();
        setLoading(false);
    });
}

void CodexZhClient::saveCredential(const QString &apiKey)
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

void CodexZhClient::clearCredential()
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

void CodexZhClient::openWallet()
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
        return;
    }
    m_wallet->setParent(this);
    KWallet::Wallet *openedWallet = m_wallet;

    connect(openedWallet, &KWallet::Wallet::walletOpened, this, [this, openedWallet](bool success) {
        if (m_wallet != openedWallet) {
            return;
        }
        m_walletOpening = false;
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
        setStoredApiKey({});
        m_wallet = nullptr;
        setCredentialState(QStringLiteral("KDE 钱包已锁定"), false, true);
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

bool CodexZhClient::prepareWalletFolder()
{
    if (!m_wallet || !m_wallet->isOpen()) {
        return false;
    }
    if (!m_wallet->hasFolder(walletFolder) && !m_wallet->createFolder(walletFolder)) {
        return false;
    }
    return m_wallet->setFolder(walletFolder);
}

void CodexZhClient::loadCredential()
{
    QString apiKey;
    const int result = m_wallet->readPassword(codexZhWalletEntry, apiKey);
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

void CodexZhClient::performPendingCredentialOperation()
{
    if (!prepareWalletFolder()) {
        setCredentialState(QStringLiteral("无法访问 KDE 钱包"), false, true);
        return;
    }

    if (m_pendingCredentialOperation == PendingCredentialOperation::Save) {
        QString apiKey = m_pendingApiKey;
        const int result = m_wallet->writePassword(codexZhWalletEntry, apiKey);
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
        const int result = m_wallet->hasEntry(codexZhWalletEntry)
            ? m_wallet->removeEntry(codexZhWalletEntry) : 0;
        if (result == 0) {
            setStoredApiKey({});
            setCredentialState(QStringLiteral("API Key 已移除"), false, false);
            setSnapshot(emptySnapshot(QStringLiteral("未配置")));
        } else {
            setCredentialState(QStringLiteral("API Key 移除失败，请检查 KDE 钱包"), false, true);
        }
    }

    m_pendingApiKey.fill(QChar(u'\0'));
    m_pendingApiKey.clear();
    m_pendingCredentialOperation = PendingCredentialOperation::None;
}

void CodexZhClient::setStoredApiKey(const QByteArray &apiKey)
{
    const bool wasConfigured = credentialConfigured();
    m_storedApiKey.fill('\0');
    m_storedApiKey = apiKey;
    if (wasConfigured != credentialConfigured()) {
        Q_EMIT credentialConfiguredChanged();
    }
}

void CodexZhClient::setCredentialState(const QString &status, bool busy, bool error)
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

void CodexZhClient::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    Q_EMIT loadingChanged();
}

void CodexZhClient::setSnapshot(const QVariantMap &snapshot)
{
    if (m_snapshot == snapshot) {
        return;
    }
    m_snapshot = snapshot;
    Q_EMIT snapshotChanged();
}
