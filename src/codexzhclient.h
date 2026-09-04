// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArrayView>
#include <QList>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <QtGlobal>

#include "kwalletdispatcher.h"

class QNetworkAccessManager;
class QNetworkReply;
class ResilientNetworkRequest;

/**
 * CodexZH 用量查询客户端。
 *
 * 凭据处理与 MiniMaxClient 对齐。CodexZH 没有专用环境变量，必须依赖 KWallet。
 */
class CodexZhClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool credentialConfigured READ credentialConfigured NOTIFY credentialConfiguredChanged)
    Q_PROPERTY(QString credentialStatus READ credentialStatus NOTIFY credentialStatusChanged)
    Q_PROPERTY(bool credentialBusy READ credentialBusy NOTIFY credentialBusyChanged)
    Q_PROPERTY(bool credentialError READ credentialError NOTIFY credentialErrorChanged)
    // 测试访问：限流恢复路径需要直接操作 m_network / m_storedApiKey /
    // m_rateLimitedUntilMs，且不引入仅测试用的公有 setter。
    friend class CodexZhClientTest;

public:
    explicit CodexZhClient(QObject *parent = nullptr);
    ~CodexZhClient() override;

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
    qint64 m_rateLimitedUntilMs = 0;
};
