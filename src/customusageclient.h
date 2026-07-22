// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QQueue>
#include <QVariantList>
#include <QVariantMap>

class QJsonObject;
class QNetworkAccessManager;
class QNetworkReply;
class QProcess;

class CustomUsageClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList snapshots READ snapshots NOTIFY snapshotsChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

public:
    explicit CustomUsageClient(QObject *parent = nullptr);

    QVariantList snapshots() const;
    bool loading() const;

    Q_INVOKABLE void refresh(const QVariantList &definitions);

    static QVariantMap snapshotFromResult(const QVariantMap &definition,
                                          const QVariantMap &result);

Q_SIGNALS:
    void snapshotsChanged();
    void loadingChanged();

private:
    enum class WorkerStage {
        None,
        Request,
        Extract,
    };

    void beginRefresh(const QVariantList &definitions);
    void startNextProvider();
    void startWorker(WorkerStage stage, const QJsonObject &task);
    void handleWorkerFinished(QProcess *worker);
    void startNetworkRequest(const QVariantMap &requestConfiguration);
    void handleNetworkFinished(QNetworkReply *reply);
    void failCurrent(const QString &message);
    void finishCurrent(const QVariantMap &snapshot);
    void finishRefresh();
    static QVariantMap failedSnapshot(const QVariantMap &definition,
                                      const QString &message);

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    QProcess *m_worker = nullptr;
    QQueue<QVariantMap> m_jobs;
    QVariantMap m_currentDefinition;
    QVariantList m_snapshots;
    QVariantList m_pendingDefinitions;
    WorkerStage m_workerStage = WorkerStage::None;
    bool m_loading = false;
    bool m_refreshPending = false;
};
