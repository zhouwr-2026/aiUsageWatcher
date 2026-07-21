// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArrayView>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QVariantMap>

class QNetworkAccessManager;
namespace KWallet
{
class Wallet;
}

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
    Q_INVOKABLE void saveCredential(const QString &apiKey);
    Q_INVOKABLE void clearCredential();

    static QNetworkRequest createRequest(QByteArrayView apiKey);

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
    void setStoredApiKey(const QByteArray &apiKey);
    void setCredentialState(const QString &status, bool busy, bool error);
    void setLoading(bool loading);
    void setError(const QString &message);
    void setSnapshot(const QVariantMap &snapshot);

    QNetworkAccessManager *m_network = nullptr;
    KWallet::Wallet *m_wallet = nullptr;
    QByteArray m_storedApiKey;
    QString m_pendingApiKey;
    QString m_credentialStatus;
    QVariantMap m_snapshot;
    PendingCredentialOperation m_pendingCredentialOperation = PendingCredentialOperation::None;
    bool m_loading = false;
    bool m_walletOpening = false;
    bool m_credentialBusy = false;
    bool m_credentialError = false;
};
