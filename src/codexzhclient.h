// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArrayView>
#include <QList>
#include <QNetworkRequest>
#include <QUrl>
#include <QVariantMap>
#include <QtGlobal>

#include "credentialclientbase.h"

/**
 * CodexZH 用量查询客户端（Coding Plan 余额）。
 *
 * 凭据处理与 MiniMaxClient 对齐：KWallet 异步注入，不阻塞 plasmashell。
 *
 * 限流自愈（历史回归 2026-09-04）：
 * 服务端 429 时设置 m_rateLimitedUntilMs，refresh() 在该窗口内直接早退，
 * 窗口到期后的下一次定时刷新自动恢复，无需用户手动干预。窗口不是永久锁：
 * forceRefresh() 强制清零窗口；成功响应也会清零。
 */
class CodexZhClient : public CredentialClientBase
{
    Q_OBJECT

public:
    explicit CodexZhClient(QObject *parent = nullptr);
    ~CodexZhClient() override;

    Q_INVOKABLE void refresh() override;
    // 手动刷新必须先清零限流窗口，否则窗口内手动刷新请求会被早退吞掉。
    Q_INVOKABLE void forceRefresh() override;

    static QList<QUrl> endpointCandidates();
    static QNetworkRequest createRequest(const QUrl &url, QByteArrayView apiKey);

protected:
    QString walletEntryKey() const override;
    QVariantMap emptySnapshot(const QString &status, const QString &error = {}) const override;
    // 钱包读回后凭据未变时不主动刷新：kwalld 频繁 walletOpened 会让
    // reloadCredential 被反复调用；key 没变就没必要立刻打 API，省一次限流窗口。
    void onCredentialLoaded(const QByteArray &secret) override;

    // 测试访问：限流恢复路径需要直接操作 m_network / m_storedSecret /
    // m_rateLimitedUntilMs，且不引入仅测试用的公有 setter。
    friend class CodexZhClientTest;

private:
    qint64 m_rateLimitedUntilMs = 0;
};
