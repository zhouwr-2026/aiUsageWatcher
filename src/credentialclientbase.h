// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include "kwalletdispatcher.h"

class QNetworkAccessManager;
class QNetworkReply;
class ResilientNetworkRequest;

/**
 * 用量查询客户端的公共骨架：KWallet 凭据生命周期 + snapshot/loading 状态机。
 *
 * 历史：六个 provider 客户端（Agnes / MiniMax / DeepSeek / CodexZH /
 * Command Code / OpenCode Go）分别复制了同一套 ~250 行/文件的凭据读写、
 * 状态机与错误文案，任何一处行为修复（如 CodexZH 限流）都要改六遍。
 * 本类把这套通用骨架收敛为模板方法；各 provider 真正的差异（钱包条目名、
 * 凭据编码/校验、网络请求主体、快照解析）全部保留在子类。
 *
 * 行为约束（修改前必读）：
 *   - onCredentialLoaded 默认“读回即刷新”；CodexZH 覆写为“凭据未变不刷新”，
 *     用于绕开 kwalletd 频繁 walletOpened 打穿 60s 限流窗口。
 *   - onCredentialSaved 默认保存成功后立即刷新；OpenCodeGo 覆写为空。
 *   - onCredentialCleared 默认清空快照；MiniMax/DeepSeek 按环境变量是否生效
 *     决定是否保留快照，OpenCodeGo 覆写为空。
 */
class CredentialClientBase : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool credentialConfigured READ credentialConfigured NOTIFY credentialConfiguredChanged)
    Q_PROPERTY(QString credentialStatus READ credentialStatus NOTIFY credentialStatusChanged)
    Q_PROPERTY(bool credentialBusy READ credentialBusy NOTIFY credentialBusyChanged)
    Q_PROPERTY(bool credentialError READ credentialError NOTIFY credentialErrorChanged)

public:
    explicit CredentialClientBase(QObject *parent = nullptr);
    ~CredentialClientBase() override;

    QVariantMap snapshot() const;
    bool loading() const;
    virtual bool credentialConfigured() const;
    QString credentialStatus() const;
    bool credentialBusy() const;
    bool credentialError() const;

    // 请求生命周期：refresh 是纯虚（各 provider 网络主体差异大），
    // forceRefresh/cancelRefresh 走公共调度骨架 + abortActiveRequest 差异点。
    // CodexZH 覆写 forceRefresh 以清零限流窗口。
    Q_INVOKABLE virtual void refresh() = 0;
    Q_INVOKABLE virtual void forceRefresh();
    Q_INVOKABLE void cancelRefresh();

    // 单值凭据（API Key / Cookie）保存入口；子类可覆写自定义校验
    //（Command Code 需抽取会话项、OpenCodeGo 是两参数覆写）。
    Q_INVOKABLE virtual void saveCredential(const QString &value);
    Q_INVOKABLE void clearCredential();

    // 注入钱包调度器；必须由所有者（AiUsageWatcherApplet）注入。
    void setWalletDispatcher(KWalletDispatcher *dispatcher);
    void reloadCredential();

    // 测试注入点：替换网络访问管理器（生命周期由调用方管理）。
    void setNetworkAccessManager(QNetworkAccessManager *network);

    // 凭据存储访问器。apiKey 型（Agnes/MiniMax/DeepSeek/CodexZH）使用基类默认实现；
    // Command Code 覆写为 m_cookie 映射；OpenCodeGo 覆写为 JSON 编解码。
    virtual QByteArray storedSecret() const;
    virtual void setStoredSecret(const QByteArray &secret);
    virtual QByteArray pendingSecret() const;
    virtual void setPendingSecret(const QByteArray &secret);
    virtual void clearPendingSecret();

Q_SIGNALS:
    void snapshotChanged();
    void loadingChanged();
    void credentialConfiguredChanged();
    void credentialStatusChanged();
    void credentialBusyChanged();
    void credentialErrorChanged();

protected:
    // ---- 子类必须实现的差异点 ----
    virtual QString walletEntryKey() const = 0;
    // 各 provider 的“空快照”（providerId 不同）；由 setError 与清空路径使用。
    virtual QVariantMap emptySnapshot(const QString &status,
                                      const QString &error = {}) const = 0;
    // 请求主体结束后回收在途请求（abort/deleteLater）；默认什么都不做。
    virtual void abortActiveRequest();

    // ---- 文案差异点（默认值取自 API Key 型语义）----
    virtual QString credentialEmptyText() const;      // "API Key 不能为空"
    virtual QString credentialMissingText() const;    // "尚未保存 API Key"
    virtual QString credentialLoadedText() const;     // "已保存在 KDE 钱包"
    virtual QString credentialSavedText() const;      // "API Key 已保存到 KDE 钱包"
    virtual QString credentialSaveFailedText() const; // "API Key 保存失败，请检查 KDE 钱包"
    virtual QString credentialClearedText() const;    // "API Key 已移除"
    virtual QString credentialClearFailedText() const;// "API Key 移除失败，请检查 KDE 钱包"
    virtual QString walletAccessFailedText() const;   // "无法访问 KDE 钱包"
    // setError 在“无历史数据可保留”时使用的空快照状态；OpenCodeGo 为“不可用”。
    virtual QString requestFailedStatus() const;      // "请求失败"

    // ---- 行为差异点 ----
    // 钱包读回（result.ok）后的解码动作；默认 trim 后写入存储并立即刷新。
    // Command Code 覆写为“抽取会话项后校验”；OpenCodeGo 覆写为 JSON 解码。
    virtual void handleCredentialReadOk(const QString &rawValue);
    // 钱包读回有效凭据后的动作；默认写入存储并立即 refresh。
    virtual void onCredentialLoaded(const QByteArray &secret);
    // 保存成功后的动作；默认立即 refresh。
    virtual void onCredentialSaved();
    // 清除成功后的动作；默认把快照重置为“未配置”。
    virtual void onCredentialCleared();

    // 公共状态机工具（供子类 refresh 主体使用）。
    void setLoading(bool loading);
    void setError(const QString &message);
    void setSnapshot(const QVariantMap &snapshot);
    void setCredentialState(const QString &status, bool busy, bool error);
    // 刷新收尾：清 active 凭据、结束 loading、按 pending 队列补发。
    // MiniMax 覆写为“额外清端点到期缓存”；其它 client 使用基类版本。
    virtual void finishRefresh();
    // 凭据保存提交骨架（校验 dispatcher + 置 busy + submit Save）。
    void submitCredentialSave(const QString &trimmedValue);
    // 凭据读取失败的错误码分类（not_found / wallet 不可用 / 其它）。
    void credentialReadFailure(const QString &errorCode);

    // 通用骨架状态（子类 refresh 主体直接使用）。
    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    ResilientNetworkRequest *m_request = nullptr;
    QString m_lastRequestError;
    bool m_loading = false;
    bool m_refreshPending = false;
    bool m_refreshInterrupted = false;

protected:
    QByteArray m_storedSecret;
    QByteArray m_activeSecret;
    QByteArray m_pendingSecret;

private:
    void requestCredentialLoad();
    void handleCredentialRead(const KWalletDispatcher::Result &result);
    void handleCredentialSave(const KWalletDispatcher::Result &result);
    void handleCredentialClear(const KWalletDispatcher::Result &result);

    KWalletDispatcher *m_dispatcher = nullptr;
    QString m_credentialStatus;
    QVariantMap m_snapshot;
    bool m_initialLoadDispatched = false;
    bool m_credentialBusy = false;
    bool m_credentialError = false;
};
