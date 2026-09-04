// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QNetworkRequest>
#include <QUrl>
#include <QVariantMap>

#include "kwalletdispatcher.h"

class QNetworkAccessManager;
class QNetworkReply;
class ResilientNetworkRequest;

/**
 * Command Code 用量客户端：使用 KWallet Cookie 会话查询汇总与额度窗口。
 */
class CommandCodeClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool credentialConfigured READ credentialConfigured NOTIFY credentialConfiguredChanged)
    Q_PROPERTY(QString credentialStatus READ credentialStatus NOTIFY credentialStatusChanged)
    Q_PROPERTY(bool credentialBusy READ credentialBusy NOTIFY credentialBusyChanged)
    Q_PROPERTY(bool credentialError READ credentialError NOTIFY credentialErrorChanged)

public:
    explicit CommandCodeClient(QObject *parent = nullptr);
    ~CommandCodeClient() override;

    QVariantMap snapshot() const;
    bool loading() const;
    bool credentialConfigured() const;
    QString credentialStatus() const;
    bool credentialBusy() const;
    bool credentialError() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void forceRefresh();
    Q_INVOKABLE void cancelRefresh();
    Q_INVOKABLE void saveCredential(const QString &cookie);
    Q_INVOKABLE void clearCredential();

    static QNetworkRequest createRequest(const QUrl &url, const QString &cookie);

    void setWalletDispatcher(KWalletDispatcher *dispatcher);
    void reloadCredential();
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
    void setCredentialState(const QString &status, bool busy, bool error);
    void setStoredCookie(const QString &cookie);
    void setSnapshot(const QVariantMap &snapshot);
    void setLoading(bool loading);
    void finishRequests();
    void buildSnapshot();

    QNetworkAccessManager *m_network = nullptr;
    KWalletDispatcher *m_dispatcher = nullptr;
    QNetworkReply *m_summaryReply = nullptr;
    QNetworkReply *m_creditsReply = nullptr;
    ResilientNetworkRequest *m_summaryRequest = nullptr;
    ResilientNetworkRequest *m_creditsRequest = nullptr;
    QString m_cookie;
    QString m_pendingCookie;
    QString m_credentialStatus;
    QVariantMap m_snapshot;
    QVariantMap m_summary;
    QVariantMap m_credits;
    QString m_summaryError;
    QString m_creditsError;
    bool m_loading = false;
    bool m_refreshPending = false;
    bool m_refreshInterrupted = false;
    bool m_initialLoadDispatched = false;
    bool m_credentialBusy = false;
    bool m_credentialError = false;
};
