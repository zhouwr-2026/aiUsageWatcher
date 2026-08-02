#include "sharedproviderconfig.h"

#include <KConfigGroup>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

namespace
{
constexpr auto configGroup = "Providers";
constexpr auto configKey = "definitions";
constexpr qsizetype maxConfigBytes = 4 * 1024 * 1024;
}

SharedProviderConfig::SharedProviderConfig(const QString &configFile, QObject *parent)
    : QObject(parent)
    , m_config(KSharedConfig::openConfig(configFile))
    , m_watcher(KConfigWatcher::create(m_config))
{
    reload();
    connect(m_watcher.data(),
            &KConfigWatcher::configChanged,
            this,
            [this](const KConfigGroup &group, const QByteArrayList &names) {
                if (group.name() == QLatin1String(configGroup)
                    && names.contains(QByteArray(configKey))) {
                    reload();
                }
            });
}

QString SharedProviderConfig::providers() const
{
    return m_providers;
}

bool SharedProviderConfig::ensure(const QString &providers)
{
    return !m_providers.isEmpty() || save(providers);
}

bool SharedProviderConfig::save(const QString &providers)
{
    if (!isValid(providers)) {
        qWarning() << "Rejected invalid shared provider configuration";
        return false;
    }
    if (providers == m_providers) {
        return true;
    }

    KConfigGroup group(m_config, QLatin1String(configGroup));
    group.writeEntry(configKey, providers, KConfigBase::Notify);
    if (!m_config->sync()) {
        qWarning() << "Failed to save shared provider configuration";
        return false;
    }

    m_providers = providers;
    Q_EMIT providersChanged();
    return true;
}

bool SharedProviderConfig::isValid(const QString &providers)
{
    const QByteArray data = providers.toUtf8();
    if (data.isEmpty() || data.size() > maxConfigBytes) {
        return false;
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()) {
        return false;
    }

    const QJsonArray definitions = document.array();
    for (const QJsonValue &definition : definitions) {
        if (!definition.isObject() || !definition.toObject().value(QStringLiteral("plans")).isArray()) {
            return false;
        }
    }
    return true;
}

void SharedProviderConfig::reload()
{
    const KConfigGroup group(m_config, QLatin1String(configGroup));
    const QString providers = group.readEntry(configKey, QString());
    if (providers.isEmpty()) {
        return;
    }
    if (!isValid(providers)) {
        qWarning() << "Ignored invalid shared provider configuration";
        return;
    }
    if (providers == m_providers) {
        return;
    }

    m_providers = providers;
    Q_EMIT providersChanged();
}
