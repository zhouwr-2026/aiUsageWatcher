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

public:
    AiUsageWatcherApplet(QObject *parent,
                         const KPluginMetaData &data,
                         const QVariantList &args);

    QVariantMap miniMaxSnapshot() const;
    bool miniMaxLoading() const;
    bool miniMaxCredentialConfigured() const;

    Q_INVOKABLE void refreshMiniMax();

Q_SIGNALS:
    void miniMaxSnapshotChanged();
    void miniMaxLoadingChanged();
    void miniMaxCredentialConfiguredChanged();

private:
    MiniMaxClient m_miniMaxClient;
};
