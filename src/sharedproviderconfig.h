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
    // 从磁盘重新加载（构造与 KConfigWatcher 变更时内部调用；
    // 公开供外部在等不到 watcher 通知时主动刷新，也便于测试验证）。
    void reload();

Q_SIGNALS:
    void providersChanged();

private:
    static bool isValid(const QString &providers);

    KSharedConfig::Ptr m_config;
    KConfigWatcher::Ptr m_watcher;
    QString m_providers;
};
