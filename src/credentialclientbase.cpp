// SPDX-License-Identifier: GPL-2.0-or-later

#include "credentialclientbase.h"

#include "resilientnetworkrequest.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QVariantList>

namespace
{
// 钱包 worker 不可用时的错误码集合（KWalletDispatcher 定义之外，客户端只判断字符串）。
bool isWalletUnavailableCode(const QString &errorCode)
{
    return errorCode == QLatin1String("wallet_disabled")
        || errorCode == QLatin1String("wallet_open_failed")
        || errorCode == QLatin1String("timeout")
        || errorCode == QLatin1String("worker_failed_to_start")
        || errorCode == QLatin1String("worker_crashed");
}
}

CredentialClientBase::CredentialClientBase(QObject *parent)
    : QObject(parent)
{
    m_network = new QNetworkAccessManager(this);
    m_credentialStatus = QStringLiteral("待连接 KDE 钱包");
}

CredentialClientBase::~CredentialClientBase()
{
    // 尽力擦除内存中的凭据副本（不留明文副本在堆上）。
    m_storedSecret.fill('\0');
    m_storedSecret.clear();
    m_activeSecret.fill('\0');
    m_activeSecret.clear();
    m_pendingSecret.fill('\0');
    m_pendingSecret.clear();
}

QVariantMap CredentialClientBase::snapshot() const
{
    return m_snapshot;
}

bool CredentialClientBase::loading() const
{
    return m_loading;
}

bool CredentialClientBase::credentialConfigured() const
{
    return !storedSecret().isEmpty();
}

QString CredentialClientBase::credentialStatus() const
{
    return m_credentialStatus;
}

bool CredentialClientBase::credentialBusy() const
{
    return m_credentialBusy;
}

bool CredentialClientBase::credentialError() const
{
    return m_credentialError;
}

void CredentialClientBase::forceRefresh()
{
    if (!m_loading) {
        refresh();
        return;
    }
    m_refreshPending = true;
    m_refreshInterrupted = true;
    abortActiveRequest();
}

void CredentialClientBase::cancelRefresh()
{
    m_refreshPending = false;
    m_refreshInterrupted = false;
    abortActiveRequest();
}

void CredentialClientBase::saveCredential(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        setCredentialState(credentialEmptyText(), false, true);
        return;
    }
    submitCredentialSave(trimmed);
}

void CredentialClientBase::clearCredential()
{
    if (!m_dispatcher) {
        setCredentialState(QStringLiteral("凭据服务暂不可用"), false, true);
        return;
    }
    clearPendingSecret();
    setCredentialState(QStringLiteral("正在从 KDE 钱包移除…"), true, false);

    m_dispatcher->submit(KWalletDispatcher::Op::Clear,
                         walletEntryKey(),
                         QString(),
                         [this](const KWalletDispatcher::Result &result) {
                             handleCredentialClear(result);
                         });
}

void CredentialClientBase::setWalletDispatcher(KWalletDispatcher *dispatcher)
{
    m_dispatcher = dispatcher;
    if (m_dispatcher && !m_initialLoadDispatched) {
        m_initialLoadDispatched = true;
        // 给 Plasma 完成首轮 UI 初始化留出窗口；worker 仍不阻塞 plasmashell。
        QTimer::singleShot(1500, this, [this] { requestCredentialLoad(); });
    }
}

void CredentialClientBase::reloadCredential()
{
    requestCredentialLoad();
}

void CredentialClientBase::setNetworkAccessManager(QNetworkAccessManager *network)
{
    if (!network || m_loading) {
        return;
    }
    if (m_network && m_network != network) {
        m_network->deleteLater();
    }
    m_network = network;
}

QByteArray CredentialClientBase::storedSecret() const
{
    return m_storedSecret;
}

void CredentialClientBase::setStoredSecret(const QByteArray &secret)
{
    const bool wasConfigured = credentialConfigured();
    m_storedSecret.fill('\0');
    m_storedSecret = secret;
    if (wasConfigured != credentialConfigured()) {
        Q_EMIT credentialConfiguredChanged();
    }
}

QByteArray CredentialClientBase::pendingSecret() const
{
    return m_pendingSecret;
}

void CredentialClientBase::setPendingSecret(const QByteArray &secret)
{
    m_pendingSecret.fill('\0');
    m_pendingSecret = secret;
}

void CredentialClientBase::clearPendingSecret()
{
    m_pendingSecret.fill('\0');
    m_pendingSecret.clear();
}

void CredentialClientBase::abortActiveRequest()
{
    // 默认实现覆盖单请求型 client（m_request / m_reply 至多一个非空）。
    if (m_request) {
        m_request->abort();
    }
    if (m_reply && m_reply->isRunning()) {
        m_reply->abort();
    }
}

QString CredentialClientBase::credentialEmptyText() const
{
    return QStringLiteral("API Key 不能为空");
}

QString CredentialClientBase::credentialMissingText() const
{
    return QStringLiteral("尚未保存 API Key");
}

QString CredentialClientBase::credentialLoadedText() const
{
    return QStringLiteral("已保存在 KDE 钱包");
}

QString CredentialClientBase::credentialSavedText() const
{
    return QStringLiteral("API Key 已保存到 KDE 钱包");
}

QString CredentialClientBase::credentialSaveFailedText() const
{
    return QStringLiteral("API Key 保存失败，请检查 KDE 钱包");
}

QString CredentialClientBase::credentialClearedText() const
{
    return QStringLiteral("API Key 已移除");
}

QString CredentialClientBase::credentialClearFailedText() const
{
    return QStringLiteral("API Key 移除失败，请检查 KDE 钱包");
}

QString CredentialClientBase::walletAccessFailedText() const
{
    return QStringLiteral("无法访问 KDE 钱包");
}

QString CredentialClientBase::requestFailedStatus() const
{
    return QStringLiteral("请求失败");
}

void CredentialClientBase::onCredentialLoaded(const QByteArray &secret)
{
    setStoredSecret(secret);
    setCredentialState(credentialLoadedText(), false, false);
    refresh();
}

void CredentialClientBase::onCredentialSaved()
{
    refresh();
}

void CredentialClientBase::onCredentialCleared()
{
    setSnapshot(emptySnapshot(QStringLiteral("未配置")));
}

void CredentialClientBase::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    Q_EMIT loadingChanged();
}

void CredentialClientBase::setError(const QString &message)
{
    if (!m_snapshot.value(QStringLiteral("plans")).toList().isEmpty()) {
        QVariantMap staleSnapshot = m_snapshot;
        staleSnapshot.insert(QStringLiteral("statusLabel"), QStringLiteral("数据暂时不可更新"));
        staleSnapshot.insert(QStringLiteral("errorText"), message);
        staleSnapshot.insert(QStringLiteral("stale"), true);
        setSnapshot(staleSnapshot);
        return;
    }
    setSnapshot(emptySnapshot(requestFailedStatus(), message));
}

void CredentialClientBase::setSnapshot(const QVariantMap &snapshot)
{
    if (m_snapshot == snapshot) {
        return;
    }
    m_snapshot = snapshot;
    Q_EMIT snapshotChanged();
}

void CredentialClientBase::setCredentialState(const QString &status, bool busy, bool error)
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

void CredentialClientBase::finishRefresh()
{
    m_activeSecret.fill('\0');
    m_activeSecret.clear();
    setLoading(false);
    if (m_refreshPending) {
        m_refreshPending = false;
        m_refreshInterrupted = false;
        QTimer::singleShot(0, this, [this] { refresh(); });
    }
}

void CredentialClientBase::submitCredentialSave(const QString &trimmedValue)
{
    if (!m_dispatcher) {
        setCredentialState(QStringLiteral("凭据服务暂不可用"), false, true);
        return;
    }
    setPendingSecret(trimmedValue.toUtf8());
    setCredentialState(QStringLiteral("正在保存到 KDE 钱包…"), true, false);

    const QString snapshotValue = QString::fromUtf8(pendingSecret());
    m_dispatcher->submit(KWalletDispatcher::Op::Save,
                         walletEntryKey(),
                         snapshotValue,
                         [this](const KWalletDispatcher::Result &result) {
                             handleCredentialSave(result);
                         });
}

void CredentialClientBase::credentialReadFailure(const QString &errorCode)
{
    if (errorCode == QLatin1String("not_found")) {
        setStoredSecret({});
        setCredentialState(credentialMissingText(), false, false);
        return;
    }
    if (isWalletUnavailableCode(errorCode)) {
        setCredentialState(QStringLiteral("凭据服务暂不可用"), false, true);
        return;
    }
    setCredentialState(walletAccessFailedText(), false, true);
}

void CredentialClientBase::requestCredentialLoad()
{
    if (!m_dispatcher) {
        return;
    }
    m_credentialBusy = true;
    Q_EMIT credentialBusyChanged();
    m_dispatcher->submit(KWalletDispatcher::Op::Read,
                         walletEntryKey(),
                         QString(),
                         [this](const KWalletDispatcher::Result &result) {
                             handleCredentialRead(result);
                         });
}

void CredentialClientBase::handleCredentialRead(const KWalletDispatcher::Result &result)
{
    if (result.ok) {
        // 解码动作整体交给子类（含空值分支）：API Key 型默认实现负责 trim，
        // Command Code / OpenCodeGo 覆写时按各自格式给出空值提示。
        handleCredentialReadOk(result.value);
        return;
    }
    credentialReadFailure(result.errorCode);
}

void CredentialClientBase::handleCredentialReadOk(const QString &rawValue)
{
    const QString trimmed = rawValue.trimmed();
    if (!trimmed.isEmpty()) {
        onCredentialLoaded(trimmed.toUtf8());
        return;
    }
    setStoredSecret({});
    setCredentialState(credentialMissingText(), false, false);
}

void CredentialClientBase::handleCredentialSave(const KWalletDispatcher::Result &result)
{
    if (result.ok) {
        QByteArray stored = pendingSecret();
        setStoredSecret(stored);
        stored.fill('\0');
        setCredentialState(credentialSavedText(), false, false);
        clearPendingSecret();
        onCredentialSaved();
        return;
    }
    setCredentialState(credentialSaveFailedText(), false, true);
    clearPendingSecret();
}

void CredentialClientBase::handleCredentialClear(const KWalletDispatcher::Result &result)
{
    if (result.ok) {
        setStoredSecret({});
        setCredentialState(credentialClearedText(), false, false);
        onCredentialCleared();
        clearPendingSecret();
        return;
    }
    setCredentialState(credentialClearFailedText(), false, true);
    clearPendingSecret();
}
