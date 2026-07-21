// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Plasma/Applet>

#include "minimaxclient.h"

class AiUsageWatcherApplet : public Plasma::Applet
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap miniMaxSnapshot READ miniMaxSnapshot NOTIFY miniMaxSnapshotChanged)
    Q_PROPERTY(bool miniMaxLoading READ miniMaxLoading NOTIFY miniMaxLoadingChanged)
    Q_PROPERTY(bool miniMaxCredentialConfigured READ miniMaxCredentialConfigured NOTIFY miniMaxCredentialConfiguredChanged)
    Q_PROPERTY(QString miniMaxCredentialStatus READ miniMaxCredentialStatus NOTIFY miniMaxCredentialStatusChanged)
    Q_PROPERTY(bool miniMaxCredentialBusy READ miniMaxCredentialBusy NOTIFY miniMaxCredentialBusyChanged)
    Q_PROPERTY(bool miniMaxCredentialError READ miniMaxCredentialError NOTIFY miniMaxCredentialErrorChanged)

public:
    AiUsageWatcherApplet(QObject *parent,
                         const KPluginMetaData &data,
                         const QVariantList &args);

    QVariantMap miniMaxSnapshot() const;
    bool miniMaxLoading() const;
    bool miniMaxCredentialConfigured() const;
    QString miniMaxCredentialStatus() const;
    bool miniMaxCredentialBusy() const;
    bool miniMaxCredentialError() const;

    Q_INVOKABLE void refreshMiniMax();
    Q_INVOKABLE void saveMiniMaxApiKey(const QString &apiKey);
    Q_INVOKABLE void clearMiniMaxApiKey();

Q_SIGNALS:
    void miniMaxSnapshotChanged();
    void miniMaxLoadingChanged();
    void miniMaxCredentialConfiguredChanged();
    void miniMaxCredentialStatusChanged();
    void miniMaxCredentialBusyChanged();
    void miniMaxCredentialErrorChanged();

private:
    MiniMaxClient m_miniMaxClient;
};
