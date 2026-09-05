// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Plasma/Applet>
#include <QColor>
#include <QVariantList>

#include "agnesclient.h"
#include "codexloginoutputparser.h"
#include "commandcodeclient.h"
#include "codexzhclient.h"
#include "customusageclient.h"
#include "deepseekclient.h"
#include "kwalletdispatcher.h"
#include "minimaxclient.h"
#include "opencodegoclient.h"
#include "sharedproviderconfig.h"

class QQuickTextDocument;
class QNetworkAccessManager;
class QNetworkReply;
class QJsonObject;
class QTimer;
class KWalletDispatcher;

class AiUsageWatcherApplet : public Plasma::Applet
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap miniMaxSnapshot READ miniMaxSnapshot NOTIFY miniMaxSnapshotChanged)
    Q_PROPERTY(bool miniMaxLoading READ miniMaxLoading NOTIFY miniMaxLoadingChanged)
    Q_PROPERTY(bool miniMaxCredentialConfigured READ miniMaxCredentialConfigured NOTIFY miniMaxCredentialConfiguredChanged)
    Q_PROPERTY(QString miniMaxCredentialStatus READ miniMaxCredentialStatus NOTIFY miniMaxCredentialStatusChanged)
    Q_PROPERTY(bool miniMaxCredentialBusy READ miniMaxCredentialBusy NOTIFY miniMaxCredentialBusyChanged)
    Q_PROPERTY(bool miniMaxCredentialError READ miniMaxCredentialError NOTIFY miniMaxCredentialErrorChanged)
    Q_PROPERTY(QVariantMap deepseekSnapshot READ deepseekSnapshot NOTIFY deepseekSnapshotChanged)
    Q_PROPERTY(bool deepseekLoading READ deepseekLoading NOTIFY deepseekLoadingChanged)
    Q_PROPERTY(bool deepseekCredentialConfigured READ deepseekCredentialConfigured NOTIFY deepseekCredentialConfiguredChanged)
    Q_PROPERTY(QString deepseekCredentialStatus READ deepseekCredentialStatus NOTIFY deepseekCredentialStatusChanged)
    Q_PROPERTY(bool deepseekCredentialBusy READ deepseekCredentialBusy NOTIFY deepseekCredentialBusyChanged)
    Q_PROPERTY(bool deepseekCredentialError READ deepseekCredentialError NOTIFY deepseekCredentialErrorChanged)
    Q_PROPERTY(QVariantMap codexSnapshot READ codexSnapshot NOTIFY codexSnapshotChanged)
    Q_PROPERTY(bool codexUsageLoading READ codexUsageLoading NOTIFY codexUsageLoadingChanged)
    Q_PROPERTY(bool codexLoggedIn READ codexLoggedIn NOTIFY codexLoggedInChanged)
    Q_PROPERTY(QString codexLoginStatus READ codexLoginStatus NOTIFY codexLoginStatusChanged)
    Q_PROPERTY(bool codexLoginBusy READ codexLoginBusy NOTIFY codexLoginBusyChanged)
    Q_PROPERTY(bool codexLoginError READ codexLoginError NOTIFY codexLoginErrorChanged)
    Q_PROPERTY(QString codexDeviceCode READ codexDeviceCode NOTIFY codexDeviceCodeChanged)
    Q_PROPERTY(QString codexDeviceUrl READ codexDeviceUrl CONSTANT)
    Q_PROPERTY(QVariantList codexAccounts READ codexAccounts NOTIFY codexAccountsChanged)
    Q_PROPERTY(QVariantList customUsageSnapshots READ customUsageSnapshots NOTIFY customUsageSnapshotsChanged)
    Q_PROPERTY(bool customUsageLoading READ customUsageLoading NOTIFY customUsageLoadingChanged)
    Q_PROPERTY(QVariantMap codexzhSnapshot READ codexzhSnapshot NOTIFY codexzhSnapshotChanged)
    Q_PROPERTY(bool codexzhLoading READ codexzhLoading NOTIFY codexzhLoadingChanged)
    Q_PROPERTY(bool codexzhCredentialConfigured READ codexzhCredentialConfigured NOTIFY codexzhCredentialConfiguredChanged)
    Q_PROPERTY(QString codexzhCredentialStatus READ codexzhCredentialStatus NOTIFY codexzhCredentialStatusChanged)
    Q_PROPERTY(bool codexzhCredentialBusy READ codexzhCredentialBusy NOTIFY codexzhCredentialBusyChanged)
    Q_PROPERTY(bool codexzhCredentialError READ codexzhCredentialError NOTIFY codexzhCredentialErrorChanged)
    Q_PROPERTY(QVariantMap opencodeGoSnapshot READ opencodeGoSnapshot NOTIFY opencodeGoSnapshotChanged)
    Q_PROPERTY(bool opencodeGoLoading READ opencodeGoLoading NOTIFY opencodeGoLoadingChanged)
    Q_PROPERTY(bool opencodeGoCredentialConfigured READ opencodeGoCredentialConfigured NOTIFY opencodeGoCredentialConfiguredChanged)
    Q_PROPERTY(QString opencodeGoCredentialStatus READ opencodeGoCredentialStatus NOTIFY opencodeGoCredentialStatusChanged)
    Q_PROPERTY(bool opencodeGoCredentialBusy READ opencodeGoCredentialBusy NOTIFY opencodeGoCredentialBusyChanged)
    Q_PROPERTY(bool opencodeGoCredentialError READ opencodeGoCredentialError NOTIFY opencodeGoCredentialErrorChanged)
    Q_PROPERTY(QVariantMap commandCodeSnapshot READ commandCodeSnapshot NOTIFY commandCodeSnapshotChanged)
    Q_PROPERTY(bool commandCodeLoading READ commandCodeLoading NOTIFY commandCodeLoadingChanged)
    Q_PROPERTY(bool commandCodeCredentialConfigured READ commandCodeCredentialConfigured NOTIFY commandCodeCredentialConfiguredChanged)
    Q_PROPERTY(QString commandCodeCredentialStatus READ commandCodeCredentialStatus NOTIFY commandCodeCredentialStatusChanged)
    Q_PROPERTY(bool commandCodeCredentialBusy READ commandCodeCredentialBusy NOTIFY commandCodeCredentialBusyChanged)
    Q_PROPERTY(bool commandCodeCredentialError READ commandCodeCredentialError NOTIFY commandCodeCredentialErrorChanged)
    Q_PROPERTY(QVariantMap agnesSnapshot READ agnesSnapshot NOTIFY agnesSnapshotChanged)
    Q_PROPERTY(bool agnesLoading READ agnesLoading NOTIFY agnesLoadingChanged)
    Q_PROPERTY(bool agnesCredentialConfigured READ agnesCredentialConfigured NOTIFY agnesCredentialConfiguredChanged)
    Q_PROPERTY(QString agnesCredentialStatus READ agnesCredentialStatus NOTIFY agnesCredentialStatusChanged)
    Q_PROPERTY(bool agnesCredentialBusy READ agnesCredentialBusy NOTIFY agnesCredentialBusyChanged)
    Q_PROPERTY(bool agnesCredentialError READ agnesCredentialError NOTIFY agnesCredentialErrorChanged)
    Q_PROPERTY(QString sharedProviders READ sharedProviders NOTIFY sharedProvidersChanged)

public:
    AiUsageWatcherApplet(QObject *parent,
                         const KPluginMetaData &data,
                         const QVariantList &args);

    QVariantMap miniMaxSnapshot() const;
    bool miniMaxLoading() const;
    bool miniMaxCredentialConfigured() const;
    QString miniMaxCredentialStatus() const;
    bool miniMaxCredentialBusy() const;
    bool miniMaxCredentialError() const;
    QVariantMap deepseekSnapshot() const;
    bool deepseekLoading() const;
    bool deepseekCredentialConfigured() const;
    QString deepseekCredentialStatus() const;
    bool deepseekCredentialBusy() const;
    bool deepseekCredentialError() const;
    QVariantMap codexSnapshot() const;
    bool codexUsageLoading() const;
    bool codexLoggedIn() const;
    QString codexLoginStatus() const;
    bool codexLoginBusy() const;
    bool codexLoginError() const;
    QString codexDeviceCode() const;
    QString codexDeviceUrl() const;
    QVariantList codexAccounts() const;
    QVariantList customUsageSnapshots() const;
    bool customUsageLoading() const;
    QVariantMap codexzhSnapshot() const;
    bool codexzhLoading() const;
    bool codexzhCredentialConfigured() const;
    QString codexzhCredentialStatus() const;
    bool codexzhCredentialBusy() const;
    bool codexzhCredentialError() const;
    QVariantMap opencodeGoSnapshot() const;
    bool opencodeGoLoading() const;
    bool opencodeGoCredentialConfigured() const;
    QString opencodeGoCredentialStatus() const;
    bool opencodeGoCredentialBusy() const;
    bool opencodeGoCredentialError() const;
    QVariantMap commandCodeSnapshot() const;
    bool commandCodeLoading() const;
    bool commandCodeCredentialConfigured() const;
    QString commandCodeCredentialStatus() const;
    bool commandCodeCredentialBusy() const;
    bool commandCodeCredentialError() const;
    QVariantMap agnesSnapshot() const;
    bool agnesLoading() const;
    bool agnesCredentialConfigured() const;
    QString agnesCredentialStatus() const;
    bool agnesCredentialBusy() const;
    bool agnesCredentialError() const;
    QString sharedProviders() const;

    Q_INVOKABLE void refreshMiniMax();
    Q_INVOKABLE void forceRefreshMiniMax();
    Q_INVOKABLE void saveMiniMaxApiKey(const QString &apiKey);
    Q_INVOKABLE void clearMiniMaxApiKey();
    Q_INVOKABLE void refreshDeepSeekUsage();
    Q_INVOKABLE void forceRefreshDeepSeekUsage();
    Q_INVOKABLE void saveDeepSeekApiKey(const QString &apiKey);
    Q_INVOKABLE void clearDeepSeekApiKey();
    Q_INVOKABLE void refreshCodexUsage();
    Q_INVOKABLE void refreshCodexLoginStatus();
    Q_INVOKABLE void startCodexLogin();
    Q_INVOKABLE void cancelCodexLogin();
    Q_INVOKABLE void openCodexLoginPage();
    Q_INVOKABLE void removeCodexAccount(const QString &profileId);
    Q_INVOKABLE void refreshCustomProviders(const QVariantList &definitions);
    Q_INVOKABLE void refreshCodexZhUsage();
    Q_INVOKABLE void forceRefreshCodexZhUsage();
    Q_INVOKABLE void saveCodexZhApiKey(const QString &apiKey);
    Q_INVOKABLE void clearCodexZhApiKey();
    Q_INVOKABLE void refreshOpenCodeGoUsage();
    Q_INVOKABLE void forceRefreshOpenCodeGoUsage();
    Q_INVOKABLE void saveOpenCodeGoCredential(const QString &workspaceId, const QString &cookie);
    Q_INVOKABLE void clearOpenCodeGoCredential();
    Q_INVOKABLE void refreshCommandCodeUsage();
    Q_INVOKABLE void forceRefreshCommandCodeUsage();
    Q_INVOKABLE void saveCommandCodeCookie(const QString &cookie);
    Q_INVOKABLE void clearCommandCodeCookie();
    Q_INVOKABLE void refreshAgnesUsage();
    Q_INVOKABLE void forceRefreshAgnesUsage();
    Q_INVOKABLE void cancelAllUsageRequests();
    Q_INVOKABLE void saveAgnesApiKey(const QString &apiKey);
    Q_INVOKABLE void clearAgnesApiKey();
    Q_INVOKABLE bool ensureSharedProviders(const QString &providers);
    Q_INVOKABLE bool saveSharedProviders(const QString &providers);
    Q_INVOKABLE void attachJavaScriptHighlighter(QQuickTextDocument *document,
                                                 const QColor &keywordColor,
                                                 const QColor &stringColor,
                                                 const QColor &commentColor,
                                                 const QColor &numberColor);

Q_SIGNALS:
    void modelActivated(const QString &modelName);
    void miniMaxSnapshotChanged();
    void miniMaxLoadingChanged();
    void miniMaxCredentialConfiguredChanged();
    void miniMaxCredentialStatusChanged();
    void miniMaxCredentialBusyChanged();
    void miniMaxCredentialErrorChanged();
    void deepseekSnapshotChanged();
    void deepseekLoadingChanged();
    void deepseekCredentialConfiguredChanged();
    void deepseekCredentialStatusChanged();
    void deepseekCredentialBusyChanged();
    void deepseekCredentialErrorChanged();
    void codexSnapshotChanged();
    void codexUsageLoadingChanged();
    void codexLoggedInChanged();
    void codexLoginStatusChanged();
    void codexLoginBusyChanged();
    void codexLoginErrorChanged();
    void codexDeviceCodeChanged();
    void codexAccountsChanged();
    void customUsageSnapshotsChanged();
    void customUsageLoadingChanged();
    void codexzhSnapshotChanged();
    void codexzhLoadingChanged();
    void codexzhCredentialConfiguredChanged();
    void codexzhCredentialStatusChanged();
    void codexzhCredentialBusyChanged();
    void codexzhCredentialErrorChanged();
    void opencodeGoSnapshotChanged();
    void opencodeGoLoadingChanged();
    void opencodeGoCredentialConfiguredChanged();
    void opencodeGoCredentialStatusChanged();
    void opencodeGoCredentialBusyChanged();
    void opencodeGoCredentialErrorChanged();
    void commandCodeSnapshotChanged();
    void commandCodeLoadingChanged();
    void commandCodeCredentialConfiguredChanged();
    void commandCodeCredentialStatusChanged();
    void commandCodeCredentialBusyChanged();
    void commandCodeCredentialErrorChanged();
    void agnesSnapshotChanged();
    void agnesLoadingChanged();
    void agnesCredentialConfiguredChanged();
    void agnesCredentialStatusChanged();
    void agnesCredentialBusyChanged();
    void agnesCredentialErrorChanged();
    void sharedProvidersChanged();
    void walletServiceAvailabilityChanged();
    void refreshRecoveryRequested();

private Q_SLOTS:
    void handleModelActivated(const QString &modelName);
    void handlePrepareForSleep(bool sleeping);

private:
    QString codexAccountsRoot() const;
    QString defaultCodexProfileId() const;
    bool saveDefaultCodexProfileId(const QString &profileId);
    void loadCodexAccounts();
    void finishCodexLoginProfile(bool succeeded);
    bool removeCodexProfileDirectory(const QString &profileId);
    void requestCodexDeviceCode();
    void pollCodexAuthorization();
    void exchangeCodexAuthorization(const QString &authorizationCode,
                                    const QString &codeVerifier);
    bool saveCodexTokens(const QString &idToken,
                         const QString &accessToken,
                         const QString &refreshToken);
    bool loadCodexAuth(const QString &profileId, QJsonObject &auth) const;
    bool saveCodexAuth(const QString &profileId, const QJsonObject &auth) const;
    void requestCodexUsage(const QString &profileId, const QJsonObject &auth, bool canRefresh);
    void refreshCodexAccessToken(const QString &profileId, const QJsonObject &auth);
    void setCodexSnapshot(const QVariantMap &snapshot);
    void setCodexUsageLoading(bool loading);
    void failCodexLogin(const QString &status);
    void setCodexLoginState(bool loggedIn,
                            const QString &status,
                            bool busy,
                            bool error);

    // 钱包调度器（plasmashell 进程内不再持有 KWallet::Wallet 实例）。
    KWalletDispatcher m_walletDispatcher;
    SharedProviderConfig m_sharedProviderConfig;
    AgnesClient m_agnesClient;
    MiniMaxClient m_miniMaxClient;
    DeepSeekClient m_deepSeekClient;
    CodexZhClient m_codexzhClient;
    OpenCodeGoClient m_opencodeGoClient;
    CommandCodeClient m_commandCodeClient;
    CustomUsageClient m_customUsageClient;
    QNetworkAccessManager *m_codexNetwork = nullptr;
    QNetworkReply *m_codexReply = nullptr;
    QNetworkReply *m_codexUsageReply = nullptr;
    QTimer *m_codexPollTimer = nullptr;
    QString m_codexLoginStatus = QStringLiteral("正在检查登录状态…");
    QString m_codexDeviceCode;
    QString m_codexDeviceAuthId;
    QString m_pendingCodexProfileId;
    QString m_pendingCodexProfilePath;
    QVariantList m_codexAccounts;
    QVariantMap m_codexSnapshot;
    qint64 m_codexDeviceExpiresAtMs = 0;
    int m_codexPollIntervalMs = 8000;
    bool m_codexLoggedIn = false;
    bool m_codexLoginBusy = false;
    bool m_codexLoginError = false;
    bool m_codexLoginCancelled = false;
    bool m_codexUsageLoading = false;

    // 把所有 wallet 事件（service 上下线 / walletOpened / handlePrepareForSleep）
    // 合并到同一个 debounce 计时器上。kwalletd 在某些环境（频繁解锁/锁屏）里
    // 会在 1 秒内连续 emit `walletOpened` 多次，每次都让 applet reloadCredential →
    // handleCredentialRead → refresh()，把 CodexZH 的限流配额瞬间打穿。
    QTimer *m_walletReloadDebounce = nullptr;
};
