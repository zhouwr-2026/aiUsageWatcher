#pragma once

#include <KConfigWatcher>
#include <KSharedConfig>

#include <QObject>
#include <QString>

class SharedProviderConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString providers READ providers NOTIFY providersChanged)

public:
    explicit SharedProviderConfig(
        const QString &configFile = QStringLiteral("aiquotapilotrc"),
        QObject *parent = nullptr);

    QString providers() const;
    bool ensure(const QString &providers);
    bool save(const QString &providers);

Q_SIGNALS:
    void providersChanged();

private:
    static bool isValid(const QString &providers);
    void reload();

    KSharedConfig::Ptr m_config;
    KConfigWatcher::Ptr m_watcher;
    QString m_providers;
};
