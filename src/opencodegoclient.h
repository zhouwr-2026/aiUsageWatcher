// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArray>
#include <QString>
#include <QVariantMap>

#include "credentialclientbase.h"

/**
 * OpenCode Go 用量查询客户端（控制台页面抓取）。
 *
 * OpenCode Go 无公开用量 API（官方 /zen/go/v1/usage 至今未上线），社区可行方案
 * 即抓取控制台页面：GET https://opencode.ai/workspace/{workspaceId}/go 并携带
 * 浏览器登录后的 auth Cookie（参考 github.com/ridho9/opencode-go-usage）。
 * 响应 HTML 内含 SolidJS 序列化的服务端用量（rollingUsage/weeklyUsage/
 * monthlyUsage，键名无引号），正则提取后解析出 usagePercent 与 resetInSec。
 *
 * 凭据（workspaceId + auth Cookie）以 JSON 形式写入基类存储（KWallet 二进制
 * 条目），通过 KWalletDispatcher 异步调度；构建期不再打开钱包。
 */
class OpenCodeGoClient : public CredentialClientBase
{
    Q_OBJECT

public:
    explicit OpenCodeGoClient(QObject *parent = nullptr);
    ~OpenCodeGoClient() override;

    Q_INVOKABLE void refresh() override;
    // OpenCodeGo 的“已配置”是双字段都非空（基类默认只看单值 blob）。
    bool credentialConfigured() const override;
    // 双字段凭据（workspaceId + cookie），序列化为 JSON 后交给基类提交。
    // 这是对基类单参版本的隐藏（签名不同，无法 override）；QML 走两参版本。
    using CredentialClientBase::saveCredential;
    Q_INVOKABLE void saveCredential(const QString &workspaceId, const QString &cookie);

    /// 从控制台页面 HTML 解析三档用量（纯函数，测试入口）；解析不完整返回空列表
    static QVariantList parseUsageHtml(const QByteArray &html, qint64 nowMs);

protected:
    QString walletEntryKey() const override;
    QVariantMap emptySnapshot(const QString &status, const QString &error = {}) const override;
    // 基类存储 JSON 原文；这里额外解码出 workspaceId/cookie 缓存供 refresh 抓取。
    void setStoredSecret(const QByteArray &secret) override;
    // 读回的是 JSON 串：解码为 workspaceId + cookie 后按“已配置/未配置”呈现。
    void handleCredentialReadOk(const QString &rawValue) override;
    // OpenCodeGo 保存成功后不立即刷新（等下一个定时器），保持原行为。
    void onCredentialSaved() override;
    // 清除成功后不清空快照（保持原行为）。
    void onCredentialCleared() override;
    // setError 的“无历史数据”状态文案为“不可用”（其余 provider 为“请求失败”）。
    QString requestFailedStatus() const override;
    QString credentialMissingText() const override;
    QString credentialSavedText() const override;
    QString credentialSaveFailedText() const override;
    QString credentialClearedText() const override;
    QString credentialClearFailedText() const override;

private:
    // OpenCodeGo 无公开 API，凭据不是 key 而是页面抓取所需的两字段：
    // 基类存储 JSON {workspaceId, cookie}；这里缓存解码结果供 refresh 使用。
    QString m_storedWorkspaceId;
    QString m_storedCookie;
};
