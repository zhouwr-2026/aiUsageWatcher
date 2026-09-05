// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArrayView>
#include <QNetworkRequest>
#include <QUrl>
#include <QVariantMap>

#include "credentialclientbase.h"

/**
 * DeepSeek 余额查询客户端。
 *
 * 凭据处理与 MiniMaxClient 对齐：构建期不打开钱包，由 KWalletDispatcher 异步调度。
 * 环境变量 DEEPSEEK_API_KEY 直接生效，与 KWallet 互斥。
 */
class DeepSeekClient : public CredentialClientBase
{
    Q_OBJECT

public:
    explicit DeepSeekClient(QObject *parent = nullptr);
    ~DeepSeekClient() override;

    Q_INVOKABLE void refresh() override;

    static QUrl balanceEndpoint();
    static QNetworkRequest createRequest(const QUrl &url, QByteArrayView apiKey);

protected:
    QString walletEntryKey() const override;
    QVariantMap emptySnapshot(const QString &status, const QString &error = {}) const override;
    // 清除成功时若环境变量仍在生效则保留"已移除"文案且不清空快照。
    void onCredentialCleared() override;
    QString credentialClearedText() const override;
};
