// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArrayView>
#include <QList>
#include <QNetworkRequest>
#include <QUrl>
#include <QVariantMap>

#include "credentialclientbase.h"

/**
 * MiniMax 用量查询客户端（Coding Plan HTTP 余额）。
 *
 * 凭据处理：构建期不再自动打开 KDE 钱包；首次读取由 KWalletDispatcher 异步调度，
 * 避免阻塞 plasmashell 启动。环境变量 MINIMAX_API_KEY 直接生效，与 KWallet 互斥。
 */
class MiniMaxClient : public CredentialClientBase
{
    Q_OBJECT

public:
    explicit MiniMaxClient(QObject *parent = nullptr);
    ~MiniMaxClient() override;

    Q_INVOKABLE void refresh() override;

    static QList<QUrl> endpointCandidates();
    static QNetworkRequest createRequest(const QUrl &url, QByteArrayView apiKey);

    bool credentialConfigured() const override;

protected:
    QString walletEntryKey() const override;
    QVariantMap emptySnapshot(const QString &status, const QString &error = {}) const override;
    // 清除成功时若环境变量仍在生效则保留"已移除"文案且不清空快照。
    void onCredentialCleared() override;
    QString credentialClearedText() const override;
    // 额外清空端点轮询缓存（基类只清 active 凭据与 loading）。
    void finishRefresh() override;

private:
    void requestNextEndpoint();

    QList<QUrl> m_endpoints;
    qsizetype m_endpointIndex = 0;
};
