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
    connect(&m_miniMaxClient,
            &MiniMaxClient::credentialStatusChanged,
            this,
            &AiUsageWatcherApplet::miniMaxCredentialStatusChanged);
    connect(&m_miniMaxClient,
            &MiniMaxClient::credentialBusyChanged,
            this,
            &AiUsageWatcherApplet::miniMaxCredentialBusyChanged);
    connect(&m_miniMaxClient,
            &MiniMaxClient::credentialErrorChanged,
            this,
            &AiUsageWatcherApplet::miniMaxCredentialErrorChanged);
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

QString AiUsageWatcherApplet::miniMaxCredentialStatus() const
{
    return m_miniMaxClient.credentialStatus();
}

bool AiUsageWatcherApplet::miniMaxCredentialBusy() const
{
    return m_miniMaxClient.credentialBusy();
}

bool AiUsageWatcherApplet::miniMaxCredentialError() const
{
    return m_miniMaxClient.credentialError();
}

void AiUsageWatcherApplet::refreshMiniMax()
{
    m_miniMaxClient.refresh();
}

void AiUsageWatcherApplet::saveMiniMaxApiKey(const QString &apiKey)
{
    m_miniMaxClient.saveCredential(apiKey);
}

void AiUsageWatcherApplet::clearMiniMaxApiKey()
{
    m_miniMaxClient.clearCredential();
}

K_PLUGIN_CLASS(AiUsageWatcherApplet)

#include "aiusagewatcherapplet.moc"
