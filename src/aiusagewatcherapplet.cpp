// SPDX-License-Identifier: GPL-2.0-or-later

#include "aiusagewatcherapplet.h"

#include <KPluginFactory>
#include <QDebug>

AiUsageWatcherApplet::AiUsageWatcherApplet(QObject *parent,
                                           const KPluginMetaData &data,
                                           const QVariantList &args)
    : Plasma::Applet(parent, data, args)
    , m_miniMaxClient(this)
{
    qInfo() << "aiUsageWatcher: native backend loaded";
    connect(&m_miniMaxClient,
            &MiniMaxClient::snapshotChanged,
            this,
            &AiUsageWatcherApplet::miniMaxSnapshotChanged);
    connect(&m_miniMaxClient,
            &MiniMaxClient::loadingChanged,
            this,
            &AiUsageWatcherApplet::miniMaxLoadingChanged);
    connect(&m_miniMaxClient,
            &MiniMaxClient::credentialConfiguredChanged,
            this,
            &AiUsageWatcherApplet::miniMaxCredentialConfiguredChanged);
}

QVariantMap AiUsageWatcherApplet::miniMaxSnapshot() const
{
    return m_miniMaxClient.snapshot();
}

bool AiUsageWatcherApplet::miniMaxLoading() const
{
    return m_miniMaxClient.loading();
}

bool AiUsageWatcherApplet::miniMaxCredentialConfigured() const
{
    return m_miniMaxClient.credentialConfigured();
}

void AiUsageWatcherApplet::refreshMiniMax()
{
    m_miniMaxClient.refresh();
}

K_PLUGIN_CLASS(AiUsageWatcherApplet)

#include "aiusagewatcherapplet.moc"
