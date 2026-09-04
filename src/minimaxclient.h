// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArrayView>
#include <QList>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include "kwalletdispatcher.h"

class QNetworkAccessManager;
class QNetworkReply;
class ResilientNetworkRequest;

/**
 * MiniMax 用量查询客户端（Coding Plan HTTP 余额）。
 *
 * 凭据处理：构建期不再自动打开 KDE 钱包；首次读取由 KWalletDispatcher 异步调度，
 * 避免阻塞 plasmashell 启动。环境变量 MINIMAX_API_KEY 直接生效，与 KWallet 互斥。
 */
class MiniMaxClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool credentialConfigured READ credentialConfigured NOTIFY credentialConfiguredChanged)
    Q_PROPERTY(QString credentialStatus READ credentialStatus NOTIFY credentialStatusChanged)
    Q_PROPERTY(bool credentialBusy READ credentialBusy NOTIFY credentialBusyChanged)
    Q_PROPERTY(bool credentialError READ credentialError NOTIFY credentialErrorChanged)

public:
    explicit MiniMaxClient(QObject *parent = nullptr);
    ~MiniMaxClient() override;

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

    static QList<QUrl> endpointCandidates();
    static QNetworkRequest createRequest(const QUrl &url, QByteArrayView apiKey);

    // 注入钱包调度器；必须由所有者（AiUsageWatcherApplet）注入。
    void setWalletDispatcher(KWalletDispatcher *dispatcher);
    void reloadCredential();

    // 测试注入点：替换网络访问管理器（ponytail: 仅测试使用，生命周期由调用方管理）
    void setNetworkAccessManager(QNetworkAccessManager *network);

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
    void requestNextEndpoint();
    void finishRefresh();

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    ResilientNetworkRequest *m_request = nullptr;
    KWalletDispatcher *m_dispatcher = nullptr;
    QByteArray m_storedApiKey;
    QByteArray m_activeApiKey;
    QList<QUrl> m_endpoints;
    QString m_lastRequestError;
    qsizetype m_endpointIndex = 0;
    QString m_pendingApiKey;
    QString m_credentialStatus;
    QVariantMap m_snapshot;
    bool m_loading = false;
    bool m_refreshPending = false;
    bool m_refreshInterrupted = false;
    bool m_initialLoadDispatched = false;
    bool m_credentialBusy = false;
    bool m_credentialError = false;
};
