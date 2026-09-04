// SPDX-License-Identifier: GPL-2.0-or-later

#include "kwalletdispatcher.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QStandardPaths>
#include <QTimer>

#ifndef QUOTA_PILOT_KWALLET_WORKER_PATH
#define QUOTA_PILOT_KWALLET_WORKER_PATH "quota-pilot-kwallet-worker"
#endif

namespace
{
const QString watchedService = QStringLiteral("org.kde.kwalletd6");

QString resolveWorkerExecutable()
{
    // 优先用注入的路径（测试或父进程控制），其次编译期宏，最后 PATH 兜底。
    const QString fromEnv = qEnvironmentVariable("QUOTA_PILOT_KWALLET_WORKER_PATH");
    if (!fromEnv.isEmpty()) {
        return fromEnv;
    }
    const QString fromDefine = QString::fromLocal8Bit(QUOTA_PILOT_KWALLET_WORKER_PATH);
    if (!fromDefine.isEmpty()) {
        return fromDefine;
    }
    return QStringLiteral("quota-pilot-kwallet-worker");
}

QString opName(KWalletDispatcher::Op op)
{
    switch (op) {
    case KWalletDispatcher::Op::Read:
        return QStringLiteral("read");
    case KWalletDispatcher::Op::Save:
        return QStringLiteral("save");
    case KWalletDispatcher::Op::Clear:
        return QStringLiteral("clear");
    }
    return QStringLiteral("read");
}
}

KWalletDispatcher::KWalletDispatcher(QObject *parent)
    : QObject(parent)
    , m_serviceWatcher(new QDBusServiceWatcher(this))
    , m_watchdog(new QTimer(this))
{
    m_serviceWatcher->setConnection(QDBusConnection::sessionBus());
    m_serviceWatcher->addWatchedService(watchedService);
    m_serviceWatcher->setWatchMode(QDBusServiceWatcher::WatchForRegistration
                                   | QDBusServiceWatcher::WatchForUnregistration);
    connect(m_serviceWatcher,
            &QDBusServiceWatcher::serviceRegistered,
            this,
            &KWalletDispatcher::handleServiceRegistered);
    connect(m_serviceWatcher,
            &QDBusServiceWatcher::serviceUnregistered,
            this,
            &KWalletDispatcher::handleServiceUnregistered);

    // kwalletd6 保持常驻时不会重新发 serviceRegistered；监听钱包打开事件才能覆盖锁屏解锁。
    QDBusConnection::sessionBus().connect(watchedService,
                                          QStringLiteral("/modules/kwalletd6"),
                                          QStringLiteral("org.kde.KWallet"),
                                          QStringLiteral("walletOpened"),
                                          this,
                                          SLOT(handleWalletOpened(QString)));

    m_watchdog->setSingleShot(true);
    connect(m_watchdog, &QTimer::timeout, this, &KWalletDispatcher::handleWatchdogTimeout);

    refreshServiceRegistration();
}

void KWalletDispatcher::handleWalletOpened(const QString &walletName)
{
    Q_UNUSED(walletName);
    Q_EMIT walletOpened();
}

KWalletDispatcher::~KWalletDispatcher()
{
    // 先断开并丢弃所有回调，避免 worker 结束时访问已析构的客户端。
    m_pending.clear();
    m_current.callback = {};
    if (m_process) {
        m_process->disconnect(this);
    }
    killCurrentWorker();
}

void KWalletDispatcher::setWorkerExecutablePath(const QString &path)
{
    m_explicitExecutablePath = path;
}

QString KWalletDispatcher::workerExecutablePath() const
{
    return m_explicitExecutablePath.isEmpty() ? resolveWorkerExecutable() : m_explicitExecutablePath;
}

void KWalletDispatcher::setRequestTimeoutMs(int ms)
{
    if (ms <= 0) {
        return;
    }
    m_requestTimeoutMs = ms;
}

void KWalletDispatcher::submit(KWalletDispatcher::Op op,
                                const QString &provider,
                                const QString &value,
                                KWalletDispatcher::Callback callback)
{
    PendingRequest request;
    request.op = op;
    request.provider = provider;
    request.value = value;
    request.callback = std::move(callback);
    m_pending.enqueue(std::move(request));
    if (!m_process) {
        dispatchNext();
    }
}

void KWalletDispatcher::handleServiceRegistered(const QString &service)
{
    if (service != watchedService) {
        return;
    }
    if (!m_serviceRegistered) {
        m_serviceRegistered = true;
        Q_EMIT walletServiceAvailabilityChanged(true);
    }
    // 服务上线：尝试把队列里因为"wallet_open_failed"积压的请求重新派发。
    // 简单策略：唤醒空闲即派发；当前在跑的请求不会被中断。
    if (!m_process && !m_pending.isEmpty()) {
        dispatchNext();
    }
}

void KWalletDispatcher::handleServiceUnregistered(const QString &service)
{
    if (service != watchedService) {
        return;
    }
    if (m_serviceRegistered) {
        m_serviceRegistered = false;
        Q_EMIT walletServiceAvailabilityChanged(false);
    }
}

void KWalletDispatcher::handleProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_process) {
        return;
    }
    const QByteArray stdoutBytes = m_process->readAllStandardOutput();
    finishCurrent(stdoutBytes, exitCode, status);
    dispatchNext();
}

void KWalletDispatcher::handleProcessErrored(QProcess::ProcessError error)
{
    // FailedToStart：worker 二进制不存在；当作超时/失败处理，不阻塞 plasmashell。
    Q_UNUSED(error);
    if (!m_process) {
        return;
    }
    Result result;
    result.ok = false;
    result.errorCode = QStringLiteral("worker_failed_to_start");
    const QString provider = m_current.provider;
    if (m_current.callback) {
        m_current.callback(result);
    }
    Q_EMIT requestFinished(provider, false, result.errorCode);

    m_process->deleteLater();
    m_process = nullptr;
    m_watchdog->stop();
    m_current = {};
    Q_EMIT busyChanged();
    dispatchNext();
}

void KWalletDispatcher::handleWatchdogTimeout()
{
    // QProcess 超时：worker 没退出多半是同步钱包调用卡死，直接 kill。
    if (!m_process) {
        return;
    }
    const QString provider = m_current.provider;
    const auto callback = std::move(m_current.callback);
    m_process->disconnect(this);
    killCurrentWorker();
    m_process->deleteLater();
    m_process = nullptr;
    Result result;
    result.ok = false;
    result.errorCode = QStringLiteral("timeout");
    if (callback) {
        callback(result);
    }
    Q_EMIT requestFinished(provider, false, result.errorCode);

    m_current = {};
    Q_EMIT busyChanged();
    dispatchNext();
}

void KWalletDispatcher::startWorker(KWalletDispatcher::PendingRequest request)
{
    m_current = std::move(request);
    m_process = new QProcess(this);
    m_process->setProgram(workerExecutablePath());
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process,
            static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this,
            &KWalletDispatcher::handleProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &KWalletDispatcher::handleProcessErrored);

    QJsonObject payload{
        {QStringLiteral("op"), opName(m_current.op)},
        {QStringLiteral("provider"), m_current.provider},
        {QStringLiteral("timeoutMs"), m_requestTimeoutMs},
    };
    if (m_current.op == Op::Save) {
        payload.insert(QStringLiteral("value"), m_current.value);
    }
    const QByteArray stdinBytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    m_process->start(QIODevice::ReadWrite);
    if (!m_process->waitForStarted(1000)) {
        handleProcessErrored(QProcess::FailedToStart);
        return;
    }
    m_process->write(stdinBytes);
    m_process->closeWriteChannel();
    m_watchdog->start(m_requestTimeoutMs);
    Q_EMIT busyChanged();
}

void KWalletDispatcher::finishCurrent(const QByteArray &stdoutBytes,
                                       int exitCode,
                                       QProcess::ExitStatus status)
{
    m_watchdog->stop();
    Result result;
    const QString provider = m_current.provider;

    if (status == QProcess::CrashExit) {
        result.errorCode = QStringLiteral("worker_crashed");
    } else if (stdoutBytes.isEmpty()) {
        result.errorCode = QStringLiteral("empty_response");
    } else {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(stdoutBytes, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            result.errorCode = QStringLiteral("invalid_response");
        } else {
            const QJsonObject object = document.object();
            result.ok = object.value(QStringLiteral("ok")).toBool();
            result.value = object.value(QStringLiteral("value")).toString();
            result.errorCode = object.value(QStringLiteral("error")).toString();
            result.migrated = object.value(QStringLiteral("migrated")).toBool();
            if (!result.ok && result.errorCode.isEmpty()) {
                result.errorCode = QStringLiteral("unknown_error");
            }
            if (result.ok && exitCode != 0) {
                // worker 报告成功但退出码非 0；保守当作失败。
                result.ok = false;
                if (result.errorCode.isEmpty()) {
                    result.errorCode = QStringLiteral("non_zero_exit");
                }
            }
            if (!result.ok && status == QProcess::NormalExit && exitCode == 2) {
                result.errorCode = QStringLiteral("timeout");
            }
        }
    }

    if (m_current.callback) {
        m_current.callback(result);
    }
    Q_EMIT requestFinished(provider, result.ok, result.errorCode);

    m_process->deleteLater();
    m_process = nullptr;
    m_current = {};
    Q_EMIT busyChanged();
}

void KWalletDispatcher::killCurrentWorker()
{
    if (!m_process) {
        return;
    }
    if (m_process->state() != QProcess::NotRunning) {
        // 先 SIGKILL 兜底；QProcess::kill 强杀，避免 plasmashell 被同步钱包阻塞。
        m_process->kill();
        if (!m_process->waitForFinished(500)) {
            m_process->terminate();
            m_process->waitForFinished(200);
        }
    }
}

void KWalletDispatcher::dispatchNext()
{
    if (m_process) {
        return;
    }
    if (m_pending.isEmpty()) {
        return;
    }
    PendingRequest next = m_pending.dequeue();
    startWorker(std::move(next));
}

void KWalletDispatcher::refreshServiceRegistration()
{
    // 启动时主动询问一次 session bus：service 可能在 plasmashell 启动前就已就绪。
    QDBusConnectionInterface *interface = QDBusConnection::sessionBus().interface();
    if (!interface) {
        return;
    }
    QDBusReply<bool> reply = interface->isServiceRegistered(watchedService);
    if (reply.isValid() && reply.value() != m_serviceRegistered) {
        m_serviceRegistered = reply.value();
        Q_EMIT walletServiceAvailabilityChanged(m_serviceRegistered);
    }
}
