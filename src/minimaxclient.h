// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArrayView>
#include <QNetworkRequest>
#include <QObject>
#include <QVariantMap>

class QNetworkAccessManager;

class MiniMaxClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool credentialConfigured READ credentialConfigured NOTIFY credentialConfiguredChanged)

public:
    explicit MiniMaxClient(QObject *parent = nullptr);

    QVariantMap snapshot() const;
    bool loading() const;
    bool credentialConfigured() const;

    Q_INVOKABLE void refresh();

    static QNetworkRequest createRequest(QByteArrayView apiKey);

Q_SIGNALS:
    void snapshotChanged();
    void loadingChanged();
    void credentialConfiguredChanged();

private:
    void setLoading(bool loading);
    void setError(const QString &message);
    void setSnapshot(const QVariantMap &snapshot);

    QNetworkAccessManager *m_network = nullptr;
    QVariantMap m_snapshot;
    bool m_loading = false;
};
