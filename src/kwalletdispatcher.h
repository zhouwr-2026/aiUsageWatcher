// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QString>

#include <functional>

class QDBusServiceWatcher;
class QTimer;

// KWalletDispatcher — 在 plasmashell 进程内统一调度 KWallet worker。
//
// 设计要点（与 docs/specs/kwallet-readiness-gated-init.spec.md 对齐）：
// - 不持有 KWallet::Wallet 实例；所有钱包读写都通过 QProcess 派发到
//   quota-pilot-kwallet-worker 独立可执行程序；
// - 同一时刻最多一个 worker 在跑；新请求进入忙状态会排队，避免四个客户端并发打开钱包；
// - QDBusServiceWatcher 仅观察 org.kde.kwalletd6 的注册/注销，用于：
//     1. 对 UI 暴露"钱包服务是否已就绪"的状态；
//     2. 服务上线时尝试唤醒因服务缺席而失败的请求（首次请求不强制等服务）；
// - QProcess 看门狗：单请求超时 3 秒后 kill worker；worker 自身另有 SIGALRM 硬超时。
class KWalletDispatcher : public QObject
{
    Q_OBJECT
public:
    // 操作类型：与 worker JSON 协议中的 "op" 字段一一对应。
    enum class Op {
        Read,
        Save,
        Clear,
    };
    Q_ENUM(Op)

    // 调度结果。Read 成功时 value 是读取到的原始字符串（密码或 JSON 字节流）；
    // Save/Clear 成功时 value 为空。errorCode 见 worker 协议（wallet_disabled、
    // wallet_open_failed、folder_create_failed、folder_select_failed、not_found、
    // io_error、timeout、invalid_request 等）。migrated 仅当 Read 命中旧文件夹迁移时为 true。
    struct Result
    {
        bool ok = false;
        QString value;
        QString errorCode;
        bool migrated = false;
    };

    // 回调签名。回调在 dispatcher's 线程（同线程）上被同步触发，因此调用方可以
    // 直接访问 QObject 成员而不必再 queued-connection。
    using Callback = std::function<void(Result)>;

    explicit KWalletDispatcher(QObject *parent = nullptr);
    ~KWalletDispatcher() override;

    // 注入 worker 可执行路径；不调用则走 QUOTA_PILOT_KWALLET_WORKER_PATH 编译宏或 PATH。
    void setWorkerExecutablePath(const QString &path);
    QString workerExecutablePath() const;

    // 设置/获取单请求超时（毫秒）。覆盖默认 3 秒；用于测试时缩短等待。
    void setRequestTimeoutMs(int ms);
    int requestTimeoutMs() const { return m_requestTimeoutMs; }

    // 提交一个请求。worker 不可用或上一次请求还在飞，新请求进入排队；空队列里直接派发。
    void submit(Op op,
                const QString &provider,
                const QString &value,
                Callback callback);

    // 空查询接口，给 UI 用。
    bool busy() const { return m_process != nullptr; }
    int pendingCount() const { return m_pending.size(); }
    bool walletServiceRegistered() const { return m_serviceRegistered; }

Q_SIGNALS:
    // 钱包服务上线/下线通知；纯状态信号，调用方不要据此阻塞首次请求。
    void walletServiceAvailabilityChanged(bool registered);
    // 钱包完成解锁后触发；服务注册状态在锁屏前后通常不变，不能替代此事件。
    void walletOpened();
    // 有请求入队 → 派发中、派发中 → 完成，调用方可以借此刷新"凭据服务暂不可用"提示。
    void busyChanged();
    // 单次请求结束（成功或失败都触发），携带 provider 便于调用方做映射。
    void requestFinished(const QString &provider, bool ok, const QString &errorCode);

private Q_SLOTS:
    void handleWalletOpened(const QString &walletName);
    void handleServiceRegistered(const QString &service);
    void handleServiceUnregistered(const QString &service);
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);
    void handleProcessErrored(QProcess::ProcessError error);
    void handleWatchdogTimeout();

private:
    struct PendingRequest
    {
        Op op;
        QString provider;
        QString value;
        Callback callback;
    };

    void startWorker(PendingRequest request);
    void finishCurrent(const QByteArray &stdoutBytes, int exitCode, QProcess::ExitStatus status);
    void killCurrentWorker();
    void dispatchNext();
    void refreshServiceRegistration();

    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    QProcess *m_process = nullptr;
    QTimer *m_watchdog = nullptr;
    QQueue<PendingRequest> m_pending;
    PendingRequest m_current;
    QString m_explicitExecutablePath;
    bool m_serviceRegistered = false;
    int m_requestTimeoutMs = 3000;
};
