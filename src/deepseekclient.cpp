// SPDX-License-Identifier: GPL-2.0-or-later

#include "deepseekclient.h"

#include "deepseekresponseparser.h"

#include <KWallet>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QVariantList>

namespace
{
constexpr qsizetype maximumResponseBytes = 1024 * 1024;
const QString walletFolder = QStringLiteral("AIQuotaPilot");
const QString deepSeekWalletEntry = QStringLiteral("deepseek-api-key");
constexpr int walletRetryLimit = 5;

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
}

DeepSeekClient::DeepSeekClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_snapshot(emptySnapshot(QStringLiteral("未配置")))
{
    const bool configuredByEnvironment = !qgetenv("DEEPSEEK_API_KEY").trimmed().isEmpty();
    m_credentialStatus = configuredByEnvironment
        ? QStringLiteral("已通过环境变量配置")
        : QStringLiteral("正在读取 KDE 钱包…");
    if (!configuredByEnvironment) {
        m_credentialBusy = true;
        QTimer::singleShot(0, this, &DeepSeekClient::openWallet);
    }
}

DeepSeekClient::~DeepSeekClient()
{
    m_storedApiKey.fill('\0');
    m_activeApiKey.fill('\0');
    m_pendingApiKey.fill(QChar(u'\0'));
}

QVariantMap DeepSeekClient::snapshot() const
{
    return m_snapshot;
}

bool DeepSeekClient::loading() const
{
    return m_loading;
}

bool DeepSeekClient::credentialConfigured() const
{
    return !qgetenv("DEEPSEEK_API_KEY").trimmed().isEmpty() || !m_storedApiKey.isEmpty();
}

QString DeepSeekClient::credentialStatus() const
{
    return m_credentialStatus;
}

bool DeepSeekClient::credentialBusy() const
{
    return m_credentialBusy;
}

bool DeepSeekClient::credentialError() const
{
    return m_credentialError;
}

QUrl DeepSeekClient::balanceEndpoint()
{
    return QUrl(QStringLiteral("https://api.deepseek.com/user/balance"));
}

QNetworkRequest DeepSeekClient::createRequest(const QUrl &url, QByteArrayView apiKey)
{
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + apiKey.toByteArray());
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);
    return request;
}

void DeepSeekClient::refresh()
{
    if (m_loading) {
        return;
    }

    QByteArray apiKey = qgetenv("DEEPSEEK_API_KEY").trimmed();
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
    m_lastRequestError.clear();
    setLoading(true);

    QNetworkReply *reply = m_network->get(
        createRequest(balanceEndpoint(), m_activeApiKey));
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

void DeepSeekClient::finishRefresh()
{
    m_activeApiKey.fill('\0');
    m_activeApiKey.clear();
    setLoading(false);
}

void DeepSeekClient::saveCredential(const QString &apiKey)
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

void DeepSeekClient::clearCredential()
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

void DeepSeekClient::openWallet()
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
            QTimer::singleShot(5000, this, &DeepSeekClient::openWallet);
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
        setStoredApiKey({});
        m_wallet = nullptr;
        if (qgetenv("DEEPSEEK_API_KEY").trimmed().isEmpty()) {
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

bool DeepSeekClient::prepareWalletFolder()
{
    if (!m_wallet || !m_wallet->isOpen()) {
        return false;
    }
    if (!m_wallet->hasFolder(walletFolder) && !m_wallet->createFolder(walletFolder)) {
        return false;
    }
    return m_wallet->setFolder(walletFolder);
}

void DeepSeekClient::loadCredential()
{
    QString apiKey;
    const int result = m_wallet->readPassword(deepSeekWalletEntry, apiKey);
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

void DeepSeekClient::performPendingCredentialOperation()
{
    if (!prepareWalletFolder()) {
        setCredentialState(QStringLiteral("无法访问 KDE 钱包"), false, true);
        return;
    }

    if (m_pendingCredentialOperation == PendingCredentialOperation::Save) {
        QString apiKey = m_pendingApiKey;
        const int result = m_wallet->writePassword(deepSeekWalletEntry, apiKey);
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
        const int result = m_wallet->hasEntry(deepSeekWalletEntry)
            ? m_wallet->removeEntry(deepSeekWalletEntry) : 0;
        if (result == 0) {
            setStoredApiKey({});
            const bool environmentConfigured = !qgetenv("DEEPSEEK_API_KEY").trimmed().isEmpty();
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

void DeepSeekClient::setStoredApiKey(const QByteArray &apiKey)
{
    const bool wasConfigured = credentialConfigured();
    m_storedApiKey.fill('\0');
    m_storedApiKey = apiKey;
    if (wasConfigured != credentialConfigured()) {
        Q_EMIT credentialConfiguredChanged();
    }
}

void DeepSeekClient::setCredentialState(const QString &status, bool busy, bool error)
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

void DeepSeekClient::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    Q_EMIT loadingChanged();
}

void DeepSeekClient::setError(const QString &message)
{
    setSnapshot(emptySnapshot(QStringLiteral("请求失败"), message));
}

void DeepSeekClient::setSnapshot(const QVariantMap &snapshot)
{
    if (m_snapshot == snapshot) {
        return;
    }
    m_snapshot = snapshot;
    Q_EMIT snapshotChanged();
}
