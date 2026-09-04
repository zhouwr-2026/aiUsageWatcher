// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArrayView>
#include <QObject>
#include <QNetworkRequest>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include "kwalletdispatcher.h"

class QNetworkAccessManager;
class QNetworkReply;
class ResilientNetworkRequest;

/**
 * Agnes AI 用量查询客户端（Coding Plan 余额）。
 *
 * 与 MiniMaxClient / DeepSeekClient 同样由 KWalletDispatcher 异步注入凭据。
 * Agnes 控制台 Reveal 出的 API Key 直接作为 Bearer 凭据；如果用量接口因
 * 鉴权策略变化拒绝 API Key，UI 会通过错误信息引导用户粘贴 localStorage
 * 里的 user session token（同样作为 Bearer）。
 */
class AgnesClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool credentialConfigured READ credentialConfigured NOTIFY credentialConfiguredChanged)
    Q_PROPERTY(QString credentialStatus READ credentialStatus NOTIFY credentialStatusChanged)
    Q_PROPERTY(bool credentialBusy READ credentialBusy NOTIFY credentialBusyChanged)
    Q_PROPERTY(bool credentialError READ credentialError NOTIFY credentialErrorChanged)

public:
    explicit AgnesClient(QObject *parent = nullptr);
    ~AgnesClient() override;

    QVariantMap snapshot() const;
    bool loading() const;
    bool credentialConfigured() const;
    QString credentialStatus() const;
    bool credentialBusy() const;
    bool credentialError() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void forceRefresh();
    Q_INVOKABLE void cancelRefresh();
    Q_INVOKABLE void saveCredential(const QString &apiKey);
    Q_INVOKABLE void clearCredential();

    static QUrl usageEndpoint();
    static QNetworkRequest createRequest(const QUrl &url, QByteArrayView apiKey);

    // 注入钱包调度器；必须由所有者（AiUsageWatcherApplet）注入。
    void setWalletDispatcher(KWalletDispatcher *dispatcher);
    void reloadCredential();

Q_SIGNALS:
    void snapshotChanged();
    void loadingChanged();
    void credentialConfiguredChanged();
    void credentialStatusChanged();
    void credentialBusyChanged();
    void credentialErrorChanged();

private:
    void requestCredentialLoad();
    void handleCredentialRead(const KWalletDispatcher::Result &result);
    void handleCredentialSave(const KWalletDispatcher::Result &result);
    void handleCredentialClear(const KWalletDispatcher::Result &result);
    void setStoredApiKey(const QByteArray &apiKey);
    void setCredentialState(const QString &status, bool busy, bool error);
    void setLoading(bool loading);
    void setError(const QString &message);
    void setSnapshot(const QVariantMap &snapshot);
    void finishRefresh();

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    ResilientNetworkRequest *m_request = nullptr;
    KWalletDispatcher *m_dispatcher = nullptr;
    QByteArray m_storedApiKey;
    QByteArray m_activeApiKey;
    QString m_lastRequestError;
    QString m_pendingApiKey;
    QString m_credentialStatus;
    QVariantMap m_snapshot;
    bool m_loading = false;
    bool m_refreshPending = false;
    bool m_refreshInterrupted = false;
    bool m_initialLoadDispatched = false;
    bool m_credentialBusy = false;
    bool m_credentialError = false;
    bool m_authInvalid = false;
};
