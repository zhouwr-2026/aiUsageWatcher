// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArrayView>
#include <QNetworkRequest>
#include <QUrl>
#include <QVariantMap>

#include "credentialclientbase.h"

/**
 * Agnes AI 用量查询客户端（Coding Plan 余额）。
 *
 * 与 MiniMaxClient / DeepSeekClient 同样由 KWalletDispatcher 异步注入凭据。
 * Agnes 控制台 Reveal 出的 API Key 直接作为 Bearer 凭据；如果用量接口因
 * 鉴权策略变化拒绝 API Key，UI 会通过错误信息引导用户粘贴 localStorage
 * 里的 user session token（同样作为 Bearer）。
 */
class AgnesClient : public CredentialClientBase
{
    Q_OBJECT

public:
    explicit AgnesClient(QObject *parent = nullptr);
    ~AgnesClient() override;

    Q_INVOKABLE void refresh() override;
    // 重新保存凭据时解除 401 导致的暂停（m_authInvalid）。
    Q_INVOKABLE void saveCredential(const QString &value) override;

    static QUrl usageEndpoint();
    static QNetworkRequest createRequest(const QUrl &url, QByteArrayView apiKey);

protected:
    QString walletEntryKey() const override;
    QVariantMap emptySnapshot(const QString &status, const QString &error = {}) const override;

private:
    // 401/403 后暂停自动刷新，直到用户重新保存凭据（避免用坏 key 反复打 API）。
    bool m_authInvalid = false;
};
