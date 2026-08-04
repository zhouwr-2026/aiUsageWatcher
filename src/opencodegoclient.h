// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

class QNetworkAccessManager;
class QNetworkReply;
namespace KWallet
{
class Wallet;
}

/**
 * OpenCode Go 用量查询客户端（控制台页面抓取）。
 *
 * OpenCode Go 无公开用量 API（官方 /zen/go/v1/usage 至今未上线），社区可行方案
 * 即抓取控制台页面：GET https://opencode.ai/workspace/{workspaceId}/go 并携带
 * 浏览器登录后的 auth Cookie（参考 github.com/ridho9/opencode-go-usage）。
 * 响应 HTML 内含 SolidJS 序列化的服务端用量（rollingUsage/weeklyUsage/
 * monthlyUsage，键名无引号），正则提取后解析出 usagePercent 与 resetInSec。
 *
 * 凭据（workspaceId + auth Cookie）保存在 KDE Wallet（同 DeepSeekClient 模式）。
 * Cookie 会周期性失效，失效时快照提示用户在配置页更新。
 */
class OpenCodeGoClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool credentialConfigured READ credentialConfigured NOTIFY credentialConfiguredChanged)
    Q_PROPERTY(QString credentialStatus READ credentialStatus NOTIFY credentialStatusChanged)
    Q_PROPERTY(bool credentialBusy READ credentialBusy NOTIFY credentialBusyChanged)
    Q_PROPERTY(bool credentialError READ credentialError NOTIFY credentialErrorChanged)

public:
    explicit OpenCodeGoClient(QObject *parent = nullptr);
    ~OpenCodeGoClient() override;

    QVariantMap snapshot() const;
    bool loading() const;
    bool credentialConfigured() const;
    QString credentialStatus() const;
    bool credentialBusy() const;
    bool credentialError() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void saveCredential(const QString &workspaceId, const QString &cookie);
    Q_INVOKABLE void clearCredential();

    /// 从控制台页面 HTML 解析三档用量（纯函数，测试入口）；解析不完整返回空列表
    static QVariantList parseUsageHtml(const QByteArray &html, qint64 nowMs);

Q_SIGNALS:
    void snapshotChanged();
    void loadingChanged();
    void credentialConfiguredChanged();
    void credentialStatusChanged();
    void credentialBusyChanged();
    void credentialErrorChanged();

private:
    enum class PendingCredentialOperation {
        None,
        Save,
        Clear,
    };

    void openWallet();
    bool prepareWalletFolder();
    void loadCredential();
    void performPendingCredentialOperation();
    void setStoredCredential(const QString &workspaceId, const QString &cookie);
    void setCredentialState(const QString &status, bool busy, bool error);
    void setLoading(bool loading);
    void setSnapshot(const QVariantMap &snapshot);

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    KWallet::Wallet *m_wallet = nullptr;
    QString m_storedWorkspaceId;
    QString m_storedCookie;
    QString m_pendingWorkspaceId;
    QString m_pendingCookie;
    QString m_credentialStatus;
    QVariantMap m_snapshot;
    PendingCredentialOperation m_pendingCredentialOperation = PendingCredentialOperation::None;
    bool m_loading = false;
    bool m_walletOpening = false;
    int m_walletRetryCount = 0;
    bool m_credentialBusy = false;
    bool m_credentialError = false;
};
