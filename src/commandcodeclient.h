// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QNetworkRequest>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include "credentialclientbase.h"

class ResilientNetworkRequest;

/**
 * Command Code 用量客户端：使用 KWallet Cookie 会话查询汇总与额度窗口。
 *
 * 请求模型：summary + credits 两个接口并行抓取，双请求都结束后合并为快照。
 * Cookie 在保存时抽取 __Secure-commandcode_prod_ 会话项，读回时校验后再使用。
 */
class CommandCodeClient : public CredentialClientBase
{
    Q_OBJECT

public:
    explicit CommandCodeClient(QObject *parent = nullptr);
    ~CommandCodeClient() override;

    Q_INVOKABLE void refresh() override;
    // Cookie 保存前抽取会话项并校验（与读回共用规则），通过后交给基类提交。
    Q_INVOKABLE void saveCredential(const QString &value) override;

    static QNetworkRequest createRequest(const QUrl &url, const QString &cookie);

protected:
    QString walletEntryKey() const override;
    QVariantMap emptySnapshot(const QString &status, const QString &error = {}) const override;
    // 读回时抽取会话项并校验格式（与保存路径共用同一套规则）。
    void handleCredentialReadOk(const QString &rawValue) override;
    QString credentialMissingText() const override;
    QString credentialSavedText() const override;
    QString credentialSaveFailedText() const override;
    QString credentialClearedText() const override;
    QString credentialClearFailedText() const override;
    QString walletAccessFailedText() const override;
    // 双请求并行：forceRefresh/cancelRefresh 需要同时中止两个请求。
    void abortActiveRequest() override;

private:
    void buildSnapshot();
    // 两个请求都结束后收尾（loading + pending 补发）。
    void finishRequests();

    ResilientNetworkRequest *m_summaryRequest = nullptr;
    ResilientNetworkRequest *m_creditsRequest = nullptr;
    QVariantMap m_summary;
    QVariantMap m_credits;
    QString m_summaryError;
    QString m_creditsError;
};
