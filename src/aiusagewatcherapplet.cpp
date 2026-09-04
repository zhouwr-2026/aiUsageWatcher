// SPDX-License-Identifier: GPL-2.0-or-later

#include "aiusagewatcherapplet.h"
#include "codexloginoutputparser.h"
#include "javascripthighlighter.h"
#include "sharedproviderconfig.h"

#include <KPluginFactory>
#include <QDBusConnection>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQuickTextDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

#include <utility>

namespace
{
const QString codexDeviceLoginUrl = QStringLiteral("https://auth.openai.com/codex/device");
const QUrl codexDeviceCodeUrl(QStringLiteral("https://auth.openai.com/api/accounts/deviceauth/usercode"));
const QUrl codexDevicePollUrl(QStringLiteral("https://auth.openai.com/api/accounts/deviceauth/token"));
const QUrl codexTokenUrl(QStringLiteral("https://auth.openai.com/oauth/token"));
const QUrl codexUsageUrl(QStringLiteral("https://chatgpt.com/backend-api/wham/usage"));
const QString codexClientId = QStringLiteral("app_EMoamEEZ73f0CkXaXp7hrann");
const QString codexRedirectUrl = QStringLiteral("https://auth.openai.com/deviceauth/callback");
constexpr qsizetype maximumCodexResponseBytes = 1024 * 1024;

QNetworkRequest codexRequest(const QUrl &url, const QByteArray &contentType)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    request.setRawHeader("User-Agent", "QuotaPilot/0.2");
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);
    return request;
}

void guardCodexReply(QNetworkReply *reply)
{
    QObject::connect(reply,
                     &QNetworkReply::downloadProgress,
                     reply,
                     [reply](qint64 received, qint64) {
                         if (received > maximumCodexResponseBytes) {
                             reply->setProperty("aiUsageWatcherResponseTooLarge", true);
                             reply->abort();
                         }
                     });
}

bool isValidCodexProfileId(const QString &profileId)
{
    static const QRegularExpression validProfileId(
        QStringLiteral(R"(^profile-[0-9a-f]{32}$)"));
    return validProfileId.match(profileId).hasMatch();
}

QVariantMap emptyCodexSnapshot(const QString &status, const QString &error = {})
{
    return {
        {QStringLiteral("providerId"), QStringLiteral("codex")},
        {QStringLiteral("statusLabel"), status},
        {QStringLiteral("errorText"), error},
        {QStringLiteral("plans"), QVariantList{}},
    };
}

QVariantMap toCodexVariantMap(const CodexUsageResult &result)
{
    QVariantList plans;
    plans.reserve(result.windows.size());
    for (const CodexUsageWindow &window : result.windows) {
        const QString resetText = window.resetAtSeconds > 0
            ? QDateTime::fromSecsSinceEpoch(window.resetAtSeconds)
                  .toLocalTime()
                  .toString(QStringLiteral("MM-dd HH:mm"))
            : QString{};
        plans.push_back(QVariantMap{
            {QStringLiteral("planId"), window.planId},
            {QStringLiteral("planName"), window.planName},
            {QStringLiteral("used"), window.usedPercent},
            {QStringLiteral("total"), 100},
            {QStringLiteral("unit"), QStringLiteral("%")},
            {QStringLiteral("resetText"), resetText},
            {QStringLiteral("extraText"), QString()},
            {QStringLiteral("isValid"), true},
            {QStringLiteral("invalidReason"), QString()},
        });
    }
    return {
        {QStringLiteral("providerId"), QStringLiteral("codex")},
        {QStringLiteral("statusLabel"), QStringLiteral("可用")},
        {QStringLiteral("errorText"), QString()},
        {QStringLiteral("plans"), plans},
    };
}

QString codexNetworkError(int status, QNetworkReply::NetworkError error)
{
    if (status == 401 || status == 403) {
        return QStringLiteral("Codex 登录已过期，请重新登录");
    }
    if (status == 429) {
        return QStringLiteral("Codex 请求过于频繁，请稍后重试");
    }
    if (status >= 500) {
        return QStringLiteral("Codex 服务暂时不可用");
    }
    if (error == QNetworkReply::TimeoutError) {
        return QStringLiteral("Codex 请求超时");
    }
    return QStringLiteral("无法连接 Codex 额度服务");
}
}

AiUsageWatcherApplet::AiUsageWatcherApplet(QObject *parent,
                                           const KPluginMetaData &data,
                                           const QVariantList &args)
    : Plasma::Applet(parent, data, args)
    , m_walletDispatcher(this)
    , m_agnesClient(this)
    , m_miniMaxClient(this)
    , m_deepSeekClient(this)
    , m_sharedProviderConfig(QStringLiteral("aiquotapilotrc"), this)
    , m_codexzhClient(this)
    , m_opencodeGoClient(this)
    , m_commandCodeClient(this)
    , m_customUsageClient(this)
    , m_codexNetwork(new QNetworkAccessManager(this))
    , m_codexPollTimer(new QTimer(this))
    , m_codexSnapshot(emptyCodexSnapshot(QStringLiteral("未登录")))
{
    qInfo() << "aiUsageWatcher: native backend loaded";

    // 把所有 wallet 事件的 reloadCredential 收口到一个 debounce 计时器上。
    // kwalletd 在某些环境（频繁解锁/锁屏）会在 1 秒内连续 emit `walletOpened`
    // 多次，每次都让 applet reloadCredential → handleCredentialRead → refresh()，
    // 把 CodexZH 的 60s 限流窗口瞬间打穿。debounce 让多次事件只触发一次重读。
    m_walletReloadDebounce = new QTimer(this);
    m_walletReloadDebounce->setSingleShot(true);
    m_walletReloadDebounce->setInterval(2000);
    connect(m_walletReloadDebounce, &QTimer::timeout, this, [this] {
        m_agnesClient.reloadCredential();
        m_miniMaxClient.reloadCredential();
        m_deepSeekClient.reloadCredential();
        m_codexzhClient.reloadCredential();
        m_opencodeGoClient.reloadCredential();
        m_commandCodeClient.reloadCredential();
    });

    // KWallet 调度器：钱包服务 watcher 仅用于状态通知；首次请求不等服务就绪，
    // 由 worker 的 D-Bus 调用按需激活 kwalletd。
    connect(&m_walletDispatcher,
            &KWalletDispatcher::walletServiceAvailabilityChanged,
            this,
            [this](bool available) {
                Q_EMIT walletServiceAvailabilityChanged();
                if (!available) {
                    return;
                }
                // KWallet 服务恢复时重新读取；客户端会保留已有内存凭据直到读到明确 not_found。
                m_walletReloadDebounce->start();
            });
    connect(&m_walletDispatcher,
            &KWalletDispatcher::walletOpened,
            this,
            [this] {
                // 解锁事件早于钱包内部切换完成，短暂错峰后统一重读凭据。
                m_walletReloadDebounce->start();
            });
    // 把同一个调度器注入所有客户端，避免并发打开钱包。
    m_agnesClient.setWalletDispatcher(&m_walletDispatcher);
    m_miniMaxClient.setWalletDispatcher(&m_walletDispatcher);
    m_deepSeekClient.setWalletDispatcher(&m_walletDispatcher);
    m_codexzhClient.setWalletDispatcher(&m_walletDispatcher);
    m_opencodeGoClient.setWalletDispatcher(&m_walletDispatcher);
    m_commandCodeClient.setWalletDispatcher(&m_walletDispatcher);

    connect(&m_sharedProviderConfig,
            &SharedProviderConfig::providersChanged,
            this,
            &AiUsageWatcherApplet::sharedProvidersChanged);
    const bool eventConnected = QDBusConnection::sessionBus().connect(
        QString(),
        QStringLiteral("/QuotaPilot"),
        QStringLiteral("org.kde.quotaPilot"),
        QStringLiteral("ModelActivated"),
        this,
        SLOT(handleModelActivated(QString)));
    if (!eventConnected) {
        qWarning() << "QuotaPilot: failed to subscribe to D-Bus model activation events";
    }
    QDBusConnection::systemBus().connect(QStringLiteral("org.freedesktop.login1"),
                                         QStringLiteral("/org/freedesktop/login1"),
                                         QStringLiteral("org.freedesktop.login1.Manager"),
                                         QStringLiteral("PrepareForSleep"),
                                         this,
                                         SLOT(handlePrepareForSleep(bool)));
    connect(&m_miniMaxClient,
            &MiniMaxClient::snapshotChanged,
            this,
            &AiUsageWatcherApplet::miniMaxSnapshotChanged);
    connect(&m_miniMaxClient,
            &MiniMaxClient::loadingChanged,
            this,
            &AiUsageWatcherApplet::miniMaxLoadingChanged);
    connect(&m_miniMaxClient,
            &MiniMaxClient::credentialConfiguredChanged,
            this,
            &AiUsageWatcherApplet::miniMaxCredentialConfiguredChanged);
    connect(&m_miniMaxClient,
            &MiniMaxClient::credentialStatusChanged,
            this,
            &AiUsageWatcherApplet::miniMaxCredentialStatusChanged);
    connect(&m_miniMaxClient,
            &MiniMaxClient::credentialBusyChanged,
            this,
            &AiUsageWatcherApplet::miniMaxCredentialBusyChanged);
    connect(&m_miniMaxClient,
            &MiniMaxClient::credentialErrorChanged,
            this,
            &AiUsageWatcherApplet::miniMaxCredentialErrorChanged);
    connect(&m_deepSeekClient,
            &DeepSeekClient::snapshotChanged,
            this,
            &AiUsageWatcherApplet::deepseekSnapshotChanged);
    connect(&m_deepSeekClient,
            &DeepSeekClient::loadingChanged,
            this,
            &AiUsageWatcherApplet::deepseekLoadingChanged);
    connect(&m_deepSeekClient,
            &DeepSeekClient::credentialConfiguredChanged,
            this,
            &AiUsageWatcherApplet::deepseekCredentialConfiguredChanged);
    connect(&m_deepSeekClient,
            &DeepSeekClient::credentialStatusChanged,
            this,
            &AiUsageWatcherApplet::deepseekCredentialStatusChanged);
    connect(&m_deepSeekClient,
            &DeepSeekClient::credentialBusyChanged,
            this,
            &AiUsageWatcherApplet::deepseekCredentialBusyChanged);
    connect(&m_deepSeekClient,
            &DeepSeekClient::credentialErrorChanged,
            this,
            &AiUsageWatcherApplet::deepseekCredentialErrorChanged);
    connect(&m_codexzhClient,
            &CodexZhClient::snapshotChanged,
            this,
            &AiUsageWatcherApplet::codexzhSnapshotChanged);
    connect(&m_codexzhClient,
            &CodexZhClient::loadingChanged,
            this,
            &AiUsageWatcherApplet::codexzhLoadingChanged);
    connect(&m_codexzhClient,
            &CodexZhClient::credentialConfiguredChanged,
            this,
            &AiUsageWatcherApplet::codexzhCredentialConfiguredChanged);
    connect(&m_codexzhClient,
            &CodexZhClient::credentialStatusChanged,
            this,
            &AiUsageWatcherApplet::codexzhCredentialStatusChanged);
    connect(&m_codexzhClient,
            &CodexZhClient::credentialBusyChanged,
            this,
            &AiUsageWatcherApplet::codexzhCredentialBusyChanged);
    connect(&m_codexzhClient,
            &CodexZhClient::credentialErrorChanged,
            this,
            &AiUsageWatcherApplet::codexzhCredentialErrorChanged);
    connect(&m_opencodeGoClient,
            &OpenCodeGoClient::snapshotChanged,
            this,
            &AiUsageWatcherApplet::opencodeGoSnapshotChanged);
    connect(&m_opencodeGoClient,
            &OpenCodeGoClient::loadingChanged,
            this,
            &AiUsageWatcherApplet::opencodeGoLoadingChanged);
    connect(&m_opencodeGoClient,
            &OpenCodeGoClient::credentialConfiguredChanged,
            this,
            &AiUsageWatcherApplet::opencodeGoCredentialConfiguredChanged);
    connect(&m_opencodeGoClient,
            &OpenCodeGoClient::credentialStatusChanged,
            this,
            &AiUsageWatcherApplet::opencodeGoCredentialStatusChanged);
    connect(&m_opencodeGoClient,
            &OpenCodeGoClient::credentialBusyChanged,
            this,
            &AiUsageWatcherApplet::opencodeGoCredentialBusyChanged);
    connect(&m_opencodeGoClient,
            &OpenCodeGoClient::credentialErrorChanged,
            this,
            &AiUsageWatcherApplet::opencodeGoCredentialErrorChanged);
    connect(&m_commandCodeClient, &CommandCodeClient::snapshotChanged, this, &AiUsageWatcherApplet::commandCodeSnapshotChanged);
    connect(&m_commandCodeClient, &CommandCodeClient::loadingChanged, this, &AiUsageWatcherApplet::commandCodeLoadingChanged);
    connect(&m_commandCodeClient, &CommandCodeClient::credentialConfiguredChanged, this, &AiUsageWatcherApplet::commandCodeCredentialConfiguredChanged);
    connect(&m_commandCodeClient, &CommandCodeClient::credentialStatusChanged, this, &AiUsageWatcherApplet::commandCodeCredentialStatusChanged);
    connect(&m_commandCodeClient, &CommandCodeClient::credentialBusyChanged, this, &AiUsageWatcherApplet::commandCodeCredentialBusyChanged);
    connect(&m_commandCodeClient, &CommandCodeClient::credentialErrorChanged, this, &AiUsageWatcherApplet::commandCodeCredentialErrorChanged);
    connect(&m_agnesClient, &AgnesClient::snapshotChanged, this, &AiUsageWatcherApplet::agnesSnapshotChanged);
    connect(&m_agnesClient, &AgnesClient::loadingChanged, this, &AiUsageWatcherApplet::agnesLoadingChanged);
    connect(&m_agnesClient, &AgnesClient::credentialConfiguredChanged, this, &AiUsageWatcherApplet::agnesCredentialConfiguredChanged);
    connect(&m_agnesClient, &AgnesClient::credentialStatusChanged, this, &AiUsageWatcherApplet::agnesCredentialStatusChanged);
    connect(&m_agnesClient, &AgnesClient::credentialBusyChanged, this, &AiUsageWatcherApplet::agnesCredentialBusyChanged);
    connect(&m_agnesClient, &AgnesClient::credentialErrorChanged, this, &AiUsageWatcherApplet::agnesCredentialErrorChanged);
    connect(&m_customUsageClient,
            &CustomUsageClient::snapshotsChanged,
            this,
            &AiUsageWatcherApplet::customUsageSnapshotsChanged);
    connect(&m_customUsageClient,
            &CustomUsageClient::loadingChanged,
            this,
            &AiUsageWatcherApplet::customUsageLoadingChanged);
    m_codexPollTimer->setSingleShot(true);
    connect(m_codexPollTimer,
            &QTimer::timeout,
            this,
            &AiUsageWatcherApplet::pollCodexAuthorization);
    QTimer::singleShot(0, this, &AiUsageWatcherApplet::refreshCodexLoginStatus);
}

void AiUsageWatcherApplet::handleModelActivated(const QString &modelName)
{
    const QString normalizedName = modelName.trimmed();
    if (normalizedName.isEmpty()) {
        qWarning() << "QuotaPilot: ignored an empty D-Bus model activation event";
        return;
    }
    Q_EMIT modelActivated(normalizedName);
}

void AiUsageWatcherApplet::handlePrepareForSleep(bool sleeping)
{
    if (sleeping) {
        return;
    }
    // 事件丢失时提供一次低频兜底；不循环轰炸钱包，避免密码对话框期间重复启动 worker。
    QTimer::singleShot(3000, this, [this] {
        m_walletReloadDebounce->start();
        Q_EMIT refreshRecoveryRequested();
    });
}

QVariantMap AiUsageWatcherApplet::miniMaxSnapshot() const
{
    return m_miniMaxClient.snapshot();
}

bool AiUsageWatcherApplet::miniMaxLoading() const
{
    return m_miniMaxClient.loading();
}

bool AiUsageWatcherApplet::miniMaxCredentialConfigured() const
{
    return m_miniMaxClient.credentialConfigured();
}

QString AiUsageWatcherApplet::miniMaxCredentialStatus() const
{
    return m_miniMaxClient.credentialStatus();
}

bool AiUsageWatcherApplet::miniMaxCredentialBusy() const
{
    return m_miniMaxClient.credentialBusy();
}

bool AiUsageWatcherApplet::miniMaxCredentialError() const
{
    return m_miniMaxClient.credentialError();
}

QString AiUsageWatcherApplet::sharedProviders() const
{
    return m_sharedProviderConfig.providers();
}

bool AiUsageWatcherApplet::walletServiceAvailable() const
{
    return m_walletDispatcher.walletServiceRegistered();
}

QString AiUsageWatcherApplet::documentationUrl() const
{
    return QStringLiteral("https://gitee.com/eruditeLoong/aiUsageWatcher/blob/master/docs/requirements.md");
}

QVariantMap AiUsageWatcherApplet::deepseekSnapshot() const
{
    return m_deepSeekClient.snapshot();
}

bool AiUsageWatcherApplet::deepseekLoading() const
{
    return m_deepSeekClient.loading();
}

bool AiUsageWatcherApplet::deepseekCredentialConfigured() const
{
    return m_deepSeekClient.credentialConfigured();
}

QString AiUsageWatcherApplet::deepseekCredentialStatus() const
{
    return m_deepSeekClient.credentialStatus();
}

bool AiUsageWatcherApplet::deepseekCredentialBusy() const
{
    return m_deepSeekClient.credentialBusy();
}

bool AiUsageWatcherApplet::deepseekCredentialError() const
{
    return m_deepSeekClient.credentialError();
}

QVariantMap AiUsageWatcherApplet::codexzhSnapshot() const
{
    return m_codexzhClient.snapshot();
}

bool AiUsageWatcherApplet::codexzhLoading() const
{
    return m_codexzhClient.loading();
}

bool AiUsageWatcherApplet::codexzhCredentialConfigured() const
{
    return m_codexzhClient.credentialConfigured();
}

QString AiUsageWatcherApplet::codexzhCredentialStatus() const
{
    return m_codexzhClient.credentialStatus();
}

bool AiUsageWatcherApplet::codexzhCredentialBusy() const
{
    return m_codexzhClient.credentialBusy();
}

bool AiUsageWatcherApplet::codexzhCredentialError() const
{
    return m_codexzhClient.credentialError();
}

QVariantMap AiUsageWatcherApplet::opencodeGoSnapshot() const
{
    return m_opencodeGoClient.snapshot();
}

QVariantMap AiUsageWatcherApplet::commandCodeSnapshot() const { return m_commandCodeClient.snapshot(); }
bool AiUsageWatcherApplet::commandCodeLoading() const { return m_commandCodeClient.loading(); }
bool AiUsageWatcherApplet::commandCodeCredentialConfigured() const { return m_commandCodeClient.credentialConfigured(); }
QString AiUsageWatcherApplet::commandCodeCredentialStatus() const { return m_commandCodeClient.credentialStatus(); }
bool AiUsageWatcherApplet::commandCodeCredentialBusy() const { return m_commandCodeClient.credentialBusy(); }
bool AiUsageWatcherApplet::commandCodeCredentialError() const { return m_commandCodeClient.credentialError(); }

bool AiUsageWatcherApplet::opencodeGoLoading() const
{
    return m_opencodeGoClient.loading();
}

bool AiUsageWatcherApplet::opencodeGoCredentialConfigured() const
{
    return m_opencodeGoClient.credentialConfigured();
}

QString AiUsageWatcherApplet::opencodeGoCredentialStatus() const
{
    return m_opencodeGoClient.credentialStatus();
}

bool AiUsageWatcherApplet::opencodeGoCredentialBusy() const
{
    return m_opencodeGoClient.credentialBusy();
}

bool AiUsageWatcherApplet::opencodeGoCredentialError() const
{
    return m_opencodeGoClient.credentialError();
}

QVariantMap AiUsageWatcherApplet::codexSnapshot() const
{
    return m_codexSnapshot;
}

bool AiUsageWatcherApplet::codexUsageLoading() const
{
    return m_codexUsageLoading;
}

bool AiUsageWatcherApplet::codexLoggedIn() const
{
    return m_codexLoggedIn;
}

QString AiUsageWatcherApplet::codexLoginStatus() const
{
    return m_codexLoginStatus;
}

bool AiUsageWatcherApplet::codexLoginBusy() const
{
    return m_codexLoginBusy;
}

bool AiUsageWatcherApplet::codexLoginError() const
{
    return m_codexLoginError;
}

QString AiUsageWatcherApplet::codexDeviceCode() const
{
    return m_codexDeviceCode;
}

QString AiUsageWatcherApplet::codexDeviceUrl() const
{
    return codexDeviceLoginUrl;
}

QVariantList AiUsageWatcherApplet::codexAccounts() const
{
    return m_codexAccounts;
}

QVariantList AiUsageWatcherApplet::customUsageSnapshots() const
{
    return m_customUsageClient.snapshots();
}

bool AiUsageWatcherApplet::customUsageLoading() const
{
    return m_customUsageClient.loading();
}

void AiUsageWatcherApplet::refreshMiniMax()
{
    m_miniMaxClient.refresh();
}

void AiUsageWatcherApplet::forceRefreshMiniMax()
{
    m_miniMaxClient.forceRefresh();
}

void AiUsageWatcherApplet::saveMiniMaxApiKey(const QString &apiKey)
{
    m_miniMaxClient.saveCredential(apiKey);
}

void AiUsageWatcherApplet::clearMiniMaxApiKey()
{
    m_miniMaxClient.clearCredential();
}

bool AiUsageWatcherApplet::ensureSharedProviders(const QString &providers)
{
    return m_sharedProviderConfig.ensure(providers);
}

bool AiUsageWatcherApplet::saveSharedProviders(const QString &providers)
{
    return m_sharedProviderConfig.save(providers);
}

void AiUsageWatcherApplet::refreshDeepSeekUsage()
{
    m_deepSeekClient.refresh();
}

void AiUsageWatcherApplet::forceRefreshDeepSeekUsage()
{
    m_deepSeekClient.forceRefresh();
}

void AiUsageWatcherApplet::saveDeepSeekApiKey(const QString &apiKey)
{
    m_deepSeekClient.saveCredential(apiKey);
}

void AiUsageWatcherApplet::clearDeepSeekApiKey()
{
    m_deepSeekClient.clearCredential();
}

void AiUsageWatcherApplet::refreshCodexZhUsage()
{
    m_codexzhClient.refresh();
}

void AiUsageWatcherApplet::forceRefreshCodexZhUsage()
{
    m_codexzhClient.forceRefresh();
}

void AiUsageWatcherApplet::refreshOpenCodeGoUsage()
{
    m_opencodeGoClient.refresh();
}

void AiUsageWatcherApplet::forceRefreshOpenCodeGoUsage()
{
    m_opencodeGoClient.forceRefresh();
}

void AiUsageWatcherApplet::saveOpenCodeGoCredential(const QString &workspaceId, const QString &cookie)
{
    m_opencodeGoClient.saveCredential(workspaceId, cookie);
}

void AiUsageWatcherApplet::clearOpenCodeGoCredential()
{
    m_opencodeGoClient.clearCredential();
}

void AiUsageWatcherApplet::refreshCommandCodeUsage() { m_commandCodeClient.refresh(); }
void AiUsageWatcherApplet::forceRefreshCommandCodeUsage() { m_commandCodeClient.forceRefresh(); }
void AiUsageWatcherApplet::saveCommandCodeCookie(const QString &cookie) { m_commandCodeClient.saveCredential(cookie); }
void AiUsageWatcherApplet::clearCommandCodeCookie() { m_commandCodeClient.clearCredential(); }

QVariantMap AiUsageWatcherApplet::agnesSnapshot() const { return m_agnesClient.snapshot(); }
bool AiUsageWatcherApplet::agnesLoading() const { return m_agnesClient.loading(); }
bool AiUsageWatcherApplet::agnesCredentialConfigured() const { return m_agnesClient.credentialConfigured(); }
QString AiUsageWatcherApplet::agnesCredentialStatus() const { return m_agnesClient.credentialStatus(); }
bool AiUsageWatcherApplet::agnesCredentialBusy() const { return m_agnesClient.credentialBusy(); }
bool AiUsageWatcherApplet::agnesCredentialError() const { return m_agnesClient.credentialError(); }

void AiUsageWatcherApplet::refreshAgnesUsage() { m_agnesClient.refresh(); }
void AiUsageWatcherApplet::forceRefreshAgnesUsage() { m_agnesClient.forceRefresh(); }
void AiUsageWatcherApplet::cancelAllUsageRequests()
{
    m_agnesClient.cancelRefresh();
    m_miniMaxClient.cancelRefresh();
    m_deepSeekClient.cancelRefresh();
    m_codexzhClient.cancelRefresh();
    m_opencodeGoClient.cancelRefresh();
    m_commandCodeClient.cancelRefresh();
}
void AiUsageWatcherApplet::saveAgnesApiKey(const QString &apiKey) { m_agnesClient.saveCredential(apiKey); }
void AiUsageWatcherApplet::clearAgnesApiKey() { m_agnesClient.clearCredential(); }

void AiUsageWatcherApplet::saveCodexZhApiKey(const QString &apiKey)
{
    m_codexzhClient.saveCredential(apiKey);
}

void AiUsageWatcherApplet::clearCodexZhApiKey()
{
    m_codexzhClient.clearCredential();
}

void AiUsageWatcherApplet::refreshCustomProviders(const QVariantList &definitions)
{
    m_customUsageClient.refresh(definitions);
}

void AiUsageWatcherApplet::refreshCodexUsage()
{
    if (m_codexUsageLoading) {
        return;
    }

    const QString profileId = defaultCodexProfileId();
    if (profileId.isEmpty()) {
        setCodexSnapshot(emptyCodexSnapshot(QStringLiteral("未登录")));
        return;
    }

    QJsonObject auth;
    if (!loadCodexAuth(profileId, auth)) {
        setCodexSnapshot(emptyCodexSnapshot(
            QStringLiteral("登录失效"), QStringLiteral("无法读取 Codex 登录信息")));
        return;
    }

    const QJsonObject tokens = auth.value(QStringLiteral("tokens")).toObject();
    const QString accessToken = tokens.value(QStringLiteral("access_token")).toString();
    if (accessToken.isEmpty()) {
        setCodexSnapshot(emptyCodexSnapshot(
            QStringLiteral("登录失效"), QStringLiteral("Codex 登录信息缺少访问令牌")));
        return;
    }

    setCodexUsageLoading(true);
    const qint64 expiresAt = CodexLoginOutputParser::tokenExpiresAt(accessToken);
    if (expiresAt > 0 && expiresAt <= QDateTime::currentSecsSinceEpoch() + 60) {
        refreshCodexAccessToken(profileId, auth);
    } else {
        requestCodexUsage(profileId, auth, true);
    }
}

bool AiUsageWatcherApplet::loadCodexAuth(const QString &profileId, QJsonObject &auth) const
{
    if (!isValidCodexProfileId(profileId)) {
        return false;
    }
    QFile file(QDir(codexAccountsRoot()).filePath(
        profileId + QStringLiteral("/auth.json")));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024) {
        return false;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject() || !document.object().value(QStringLiteral("tokens")).isObject()) {
        return false;
    }
    auth = document.object();
    return true;
}

bool AiUsageWatcherApplet::saveCodexAuth(const QString &profileId, const QJsonObject &auth) const
{
    if (!isValidCodexProfileId(profileId)) {
        return false;
    }
    const QString path = QDir(codexAccountsRoot()).filePath(
        profileId + QStringLiteral("/auth.json"));
    const QByteArray data = QJsonDocument(auth).toJson(QJsonDocument::Indented);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(data) != data.size()
        || !file.commit()) {
        return false;
    }
    return QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

void AiUsageWatcherApplet::requestCodexUsage(const QString &profileId,
                                              const QJsonObject &auth,
                                              bool canRefresh)
{
    const QJsonObject tokens = auth.value(QStringLiteral("tokens")).toObject();
    const QString accessToken = tokens.value(QStringLiteral("access_token")).toString();
    const QString accountId = tokens.value(QStringLiteral("account_id")).toString();
    if (accessToken.isEmpty()) {
        setCodexSnapshot(emptyCodexSnapshot(
            QStringLiteral("登录失效"), QStringLiteral("Codex 登录信息缺少访问令牌")));
        setCodexUsageLoading(false);
        return;
    }

    QNetworkRequest request = codexRequest(codexUsageUrl, QByteArrayLiteral("application/json"));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toUtf8());
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "codex-cli");
    if (!accountId.isEmpty()) {
        request.setRawHeader("ChatGPT-Account-Id", accountId.toUtf8());
    }

    QNetworkReply *reply = m_codexNetwork->get(request);
    guardCodexReply(reply);
    m_codexUsageReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, profileId, auth, canRefresh]() {
        if (m_codexUsageReply == reply) {
            m_codexUsageReply = nullptr;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QNetworkReply::NetworkError error = reply->error();
        const QByteArray response = reply->read(maximumCodexResponseBytes + 1);
        const bool responseTooLarge = reply->property(
            "aiUsageWatcherResponseTooLarge").toBool()
            || response.size() > maximumCodexResponseBytes;
        reply->deleteLater();

        if ((status == 401 || status == 403) && canRefresh) {
            refreshCodexAccessToken(profileId, auth);
            return;
        }
        if (responseTooLarge) {
            setCodexSnapshot(emptyCodexSnapshot(
                QStringLiteral("请求失败"), QStringLiteral("Codex 响应过大，已拒绝处理")));
            setCodexUsageLoading(false);
            return;
        }
        if (error != QNetworkReply::NoError || status < 200 || status >= 300) {
            setCodexSnapshot(emptyCodexSnapshot(
                QStringLiteral("请求失败"), codexNetworkError(status, error)));
            setCodexUsageLoading(false);
            return;
        }

        const CodexUsageResult result = CodexLoginOutputParser::usageResponse(response);
        if (!result.isValid()) {
            setCodexSnapshot(emptyCodexSnapshot(QStringLiteral("请求失败"),
                                                result.errorMessage));
        } else {
            setCodexSnapshot(toCodexVariantMap(result));
        }
        setCodexUsageLoading(false);
    });
}

void AiUsageWatcherApplet::refreshCodexAccessToken(const QString &profileId,
                                                    const QJsonObject &auth)
{
    const QJsonObject oldTokens = auth.value(QStringLiteral("tokens")).toObject();
    const QString refreshToken = oldTokens.value(QStringLiteral("refresh_token")).toString();
    if (refreshToken.isEmpty()) {
        setCodexSnapshot(emptyCodexSnapshot(
            QStringLiteral("登录失效"), QStringLiteral("Codex 登录已过期，请重新登录")));
        setCodexUsageLoading(false);
        return;
    }

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    form.addQueryItem(QStringLiteral("refresh_token"), refreshToken);
    form.addQueryItem(QStringLiteral("client_id"), codexClientId);
    form.addQueryItem(QStringLiteral("scope"), QStringLiteral("openid profile email offline_access"));
    QNetworkReply *reply = m_codexNetwork->post(
        codexRequest(codexTokenUrl, QByteArrayLiteral("application/x-www-form-urlencoded")),
        form.query(QUrl::FullyEncoded).toUtf8());
    guardCodexReply(reply);
    m_codexUsageReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, profileId, auth]() {
        if (m_codexUsageReply == reply) {
            m_codexUsageReply = nullptr;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray response = reply->read(maximumCodexResponseBytes + 1);
        const QNetworkReply::NetworkError error = reply->error();
        reply->deleteLater();
        if (error != QNetworkReply::NoError || status < 200 || status >= 300) {
            setCodexSnapshot(emptyCodexSnapshot(
                QStringLiteral("登录失效"), QStringLiteral("Codex 登录已过期，请重新登录")));
            setCodexUsageLoading(false);
            return;
        }

        const CodexRefreshExchange exchange = CodexLoginOutputParser::refreshExchange(response);
        if (!exchange.isValid()) {
            setCodexSnapshot(emptyCodexSnapshot(
                QStringLiteral("登录失效"), QStringLiteral("Codex 刷新令牌响应无效")));
            setCodexUsageLoading(false);
            return;
        }

        QJsonObject updatedAuth = auth;
        QJsonObject tokens = updatedAuth.value(QStringLiteral("tokens")).toObject();
        tokens.insert(QStringLiteral("access_token"), exchange.accessToken);
        if (!exchange.idToken.isEmpty()) {
            tokens.insert(QStringLiteral("id_token"), exchange.idToken);
        }
        if (!exchange.refreshToken.isEmpty()) {
            tokens.insert(QStringLiteral("refresh_token"), exchange.refreshToken);
        }
        updatedAuth.insert(QStringLiteral("tokens"), tokens);
        updatedAuth.insert(QStringLiteral("last_refresh"),
                           QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        if (!saveCodexAuth(profileId, updatedAuth)) {
            setCodexSnapshot(emptyCodexSnapshot(
                QStringLiteral("请求失败"), QStringLiteral("无法安全更新 Codex 登录信息")));
            setCodexUsageLoading(false);
            return;
        }
        requestCodexUsage(profileId, updatedAuth, false);
    });
}

void AiUsageWatcherApplet::setCodexSnapshot(const QVariantMap &snapshot)
{
    if (m_codexSnapshot == snapshot) {
        return;
    }
    m_codexSnapshot = snapshot;
    Q_EMIT codexSnapshotChanged();
}

void AiUsageWatcherApplet::setCodexUsageLoading(bool loading)
{
    if (m_codexUsageLoading == loading) {
        return;
    }
    m_codexUsageLoading = loading;
    Q_EMIT codexUsageLoadingChanged();
}

void AiUsageWatcherApplet::refreshCodexLoginStatus()
{
    loadCodexAccounts();
    refreshCodexUsage();
}

QString AiUsageWatcherApplet::codexAccountsRoot() const
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(QStringLiteral("quota-pilot/codex-accounts"));
}

QString AiUsageWatcherApplet::defaultCodexProfileId() const
{
    QFile file(QDir(codexAccountsRoot()).filePath(QStringLiteral("default-profile")));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 128) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

bool AiUsageWatcherApplet::saveDefaultCodexProfileId(const QString &profileId)
{
    QDir().mkpath(codexAccountsRoot());
    const QString path = QDir(codexAccountsRoot()).filePath(QStringLiteral("default-profile"));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(profileId.toUtf8()) != profileId.toUtf8().size()
        || !file.commit()) {
        return false;
    }
    return QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

void AiUsageWatcherApplet::loadCodexAccounts()
{
    QDir rootDirectory(codexAccountsRoot());
    rootDirectory.mkpath(QStringLiteral("."));
    const QStringList profileIds = rootDirectory.entryList(
        {QStringLiteral("profile-*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    QVariantList accounts;
    for (const QString &profileId : profileIds) {
        QFile authFile(rootDirectory.filePath(profileId + QStringLiteral("/auth.json")));
        if (!authFile.open(QIODevice::ReadOnly) || authFile.size() > 2 * 1024 * 1024) {
            continue;
        }
        const CodexAccountIdentity identity = CodexLoginOutputParser::accountIdentity(
            authFile.readAll());
        if (!identity.isValid()) {
            continue;
        }
        accounts.push_back(QVariantMap{
            {QStringLiteral("profileId"), profileId},
            {QStringLiteral("accountId"), identity.accountId},
            {QStringLiteral("login"), identity.email.isEmpty()
                 ? QStringLiteral("ChatGPT (%1)").arg(identity.accountId.left(8))
                 : identity.email},
        });
    }

    QString defaultProfileId = defaultCodexProfileId();
    bool defaultExists = false;
    for (const QVariant &value : std::as_const(accounts)) {
        if (value.toMap().value(QStringLiteral("profileId")).toString() == defaultProfileId) {
            defaultExists = true;
            break;
        }
    }
    if (!defaultExists) {
        defaultProfileId = accounts.isEmpty()
            ? QString{} : accounts.first().toMap().value(QStringLiteral("profileId")).toString();
        saveDefaultCodexProfileId(defaultProfileId);
    }
    for (QVariant &value : accounts) {
        QVariantMap account = value.toMap();
        account.insert(QStringLiteral("isDefault"),
                       account.value(QStringLiteral("profileId")).toString() == defaultProfileId);
        value = account;
    }

    if (m_codexAccounts != accounts) {
        m_codexAccounts = accounts;
        Q_EMIT codexAccountsChanged();
    }
    setCodexLoginState(!accounts.isEmpty(),
                       accounts.isEmpty()
                           ? QStringLiteral("尚未添加 Codex 账号")
                           : QStringLiteral("已登录 %1 个 Codex 账号").arg(accounts.size()),
                       false,
                       false);
}

void AiUsageWatcherApplet::startCodexLogin()
{
    if (m_codexLoginBusy) {
        return;
    }

    m_codexDeviceCode.clear();
    Q_EMIT codexDeviceCodeChanged();
    m_codexDeviceAuthId.clear();
    m_codexLoginCancelled = false;
    m_pendingCodexProfileId = QStringLiteral("profile-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QChar(u'-'));
    m_pendingCodexProfilePath = QDir(codexAccountsRoot()).filePath(m_pendingCodexProfileId);
    if (!QDir().mkpath(m_pendingCodexProfilePath)
        || !QFile::setPermissions(m_pendingCodexProfilePath,
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner)) {
        removeCodexProfileDirectory(m_pendingCodexProfileId);
        m_pendingCodexProfileId.clear();
        m_pendingCodexProfilePath.clear();
        setCodexLoginState(m_codexLoggedIn,
                           QStringLiteral("无法创建 Codex 账号目录"),
                           false,
                           true);
        return;
    }
    setCodexLoginState(m_codexLoggedIn, QStringLiteral("正在生成设备验证码…"), true, false);
    requestCodexDeviceCode();
}

void AiUsageWatcherApplet::requestCodexDeviceCode()
{
    const QJsonObject body{{QStringLiteral("client_id"), codexClientId}};
    QNetworkReply *reply = m_codexNetwork->post(
        codexRequest(codexDeviceCodeUrl, QByteArrayLiteral("application/json")),
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    guardCodexReply(reply);
    m_codexReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_codexReply == reply) {
            m_codexReply = nullptr;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray response = reply->read(maximumCodexResponseBytes + 1);
        const bool failed = reply->error() != QNetworkReply::NoError || status < 200 || status >= 300;
        reply->deleteLater();
        if (m_codexLoginCancelled) {
            return;
        }
        if (failed) {
            failCodexLogin(QStringLiteral("无法获取 Codex 设备验证码"));
            return;
        }

        const CodexDeviceAuthorization authorization =
            CodexLoginOutputParser::deviceAuthorization(response);
        if (!authorization.isValid()) {
            failCodexLogin(QStringLiteral("Codex 设备验证码响应无效"));
            return;
        }
        m_codexDeviceAuthId = authorization.deviceAuthId;
        m_codexDeviceCode = authorization.userCode;
        m_codexPollIntervalMs = qMax((qBound(1, authorization.intervalSeconds, 60) + 3) * 1000,
                                    8000);
        m_codexDeviceExpiresAtMs = QDateTime::currentMSecsSinceEpoch()
            + qBound(60, authorization.expiresInSeconds, 3600) * 1000LL;
        Q_EMIT codexDeviceCodeChanged();
        setCodexLoginState(m_codexLoggedIn,
                           QStringLiteral("请在浏览器中输入设备验证码"),
                           true,
                           false);
        openCodexLoginPage();
        m_codexPollTimer->start(m_codexPollIntervalMs);
    });
}

void AiUsageWatcherApplet::pollCodexAuthorization()
{
    if (!m_codexLoginBusy || m_codexDeviceAuthId.isEmpty()) {
        return;
    }
    if (QDateTime::currentMSecsSinceEpoch() >= m_codexDeviceExpiresAtMs) {
        failCodexLogin(QStringLiteral("Codex 设备验证码已过期，请重新登录"));
        return;
    }

    const QJsonObject body{
        {QStringLiteral("device_auth_id"), m_codexDeviceAuthId},
        {QStringLiteral("user_code"), m_codexDeviceCode},
    };
    QNetworkReply *reply = m_codexNetwork->post(
        codexRequest(codexDevicePollUrl, QByteArrayLiteral("application/json")),
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    guardCodexReply(reply);
    m_codexReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_codexReply == reply) {
            m_codexReply = nullptr;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray response = reply->read(maximumCodexResponseBytes + 1);
        const QNetworkReply::NetworkError error = reply->error();
        reply->deleteLater();
        if (m_codexLoginCancelled) {
            return;
        }
        if (status == 403 || status == 404) {
            m_codexPollTimer->start(m_codexPollIntervalMs);
            return;
        }
        if (status == 410) {
            failCodexLogin(QStringLiteral("Codex 设备验证码已过期，请重新登录"));
            return;
        }
        if (error != QNetworkReply::NoError || status < 200 || status >= 300) {
            failCodexLogin(QStringLiteral("检查 Codex 授权状态失败"));
            return;
        }
        const CodexAuthorizationResult result =
            CodexLoginOutputParser::authorizationResult(response);
        if (!result.isValid()) {
            failCodexLogin(QStringLiteral("Codex 授权响应无效"));
            return;
        }
        exchangeCodexAuthorization(result.authorizationCode, result.codeVerifier);
    });
}

void AiUsageWatcherApplet::exchangeCodexAuthorization(const QString &authorizationCode,
                                                       const QString &codeVerifier)
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    form.addQueryItem(QStringLiteral("code"), authorizationCode);
    form.addQueryItem(QStringLiteral("redirect_uri"), codexRedirectUrl);
    form.addQueryItem(QStringLiteral("client_id"), codexClientId);
    form.addQueryItem(QStringLiteral("code_verifier"), codeVerifier);
    QNetworkReply *reply = m_codexNetwork->post(
        codexRequest(codexTokenUrl, QByteArrayLiteral("application/x-www-form-urlencoded")),
        form.query(QUrl::FullyEncoded).toUtf8());
    guardCodexReply(reply);
    m_codexReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_codexReply == reply) {
            m_codexReply = nullptr;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray response = reply->read(maximumCodexResponseBytes + 1);
        const bool failed = reply->error() != QNetworkReply::NoError || status < 200 || status >= 300;
        reply->deleteLater();
        if (m_codexLoginCancelled) {
            return;
        }
        if (failed) {
            failCodexLogin(QStringLiteral("Codex 授权令牌交换失败"));
            return;
        }
        const CodexTokenExchange tokens = CodexLoginOutputParser::tokenExchange(response);
        if (!tokens.isValid()) {
            failCodexLogin(QStringLiteral("Codex 授权令牌响应无效"));
            return;
        }
        if (!saveCodexTokens(tokens.idToken, tokens.accessToken, tokens.refreshToken)) {
            failCodexLogin(QStringLiteral("无法安全保存 Codex 登录信息"));
            return;
        }
        m_codexDeviceCode.clear();
        m_codexDeviceAuthId.clear();
        Q_EMIT codexDeviceCodeChanged();
        finishCodexLoginProfile(true);
    });
}

void AiUsageWatcherApplet::finishCodexLoginProfile(bool succeeded)
{
    if (!succeeded) {
        removeCodexProfileDirectory(m_pendingCodexProfileId);
        const QString status = m_codexLoginCancelled
            ? QStringLiteral("Codex 登录已取消") : QStringLiteral("Codex 登录失败");
        m_pendingCodexProfileId.clear();
        m_pendingCodexProfilePath.clear();
        setCodexLoginState(!m_codexAccounts.isEmpty(),
                           status,
                           false,
                           !m_codexLoginCancelled);
        return;
    }

    QFile authFile(QDir(m_pendingCodexProfilePath).filePath(QStringLiteral("auth.json")));
    if (!authFile.open(QIODevice::ReadOnly) || authFile.size() > 2 * 1024 * 1024) {
        removeCodexProfileDirectory(m_pendingCodexProfileId);
        m_pendingCodexProfileId.clear();
        m_pendingCodexProfilePath.clear();
        setCodexLoginState(!m_codexAccounts.isEmpty(),
                           QStringLiteral("Codex 登录完成，但无法读取账号信息"),
                           false,
                           true);
        return;
    }
    const CodexAccountIdentity identity = CodexLoginOutputParser::accountIdentity(
        authFile.readAll());
    if (!identity.isValid()) {
        removeCodexProfileDirectory(m_pendingCodexProfileId);
        m_pendingCodexProfileId.clear();
        m_pendingCodexProfilePath.clear();
        setCodexLoginState(!m_codexAccounts.isEmpty(),
                           QStringLiteral("Codex 登录信息格式无效"),
                           false,
                           true);
        return;
    }

    for (const QVariant &value : std::as_const(m_codexAccounts)) {
        const QVariantMap account = value.toMap();
        if (account.value(QStringLiteral("accountId")).toString() == identity.accountId) {
            removeCodexProfileDirectory(
                account.value(QStringLiteral("profileId")).toString());
        }
    }
    if (m_codexAccounts.isEmpty()) {
        saveDefaultCodexProfileId(m_pendingCodexProfileId);
    }
    m_pendingCodexProfileId.clear();
    m_pendingCodexProfilePath.clear();
    loadCodexAccounts();
    refreshCodexUsage();
}

bool AiUsageWatcherApplet::saveCodexTokens(const QString &idToken,
                                           const QString &accessToken,
                                           const QString &refreshToken)
{
    QJsonObject tokens{
        {QStringLiteral("id_token"), idToken},
        {QStringLiteral("access_token"), accessToken},
        {QStringLiteral("refresh_token"), refreshToken},
    };
    QJsonObject auth{
        {QStringLiteral("auth_mode"), QStringLiteral("chatgpt")},
        {QStringLiteral("OPENAI_API_KEY"), QJsonValue::Null},
        {QStringLiteral("tokens"), tokens},
        {QStringLiteral("last_refresh"),
         QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
    };
    const CodexAccountIdentity identity = CodexLoginOutputParser::accountIdentity(
        QJsonDocument(auth).toJson(QJsonDocument::Compact));
    if (!identity.isValid()) {
        return false;
    }
    tokens.insert(QStringLiteral("account_id"), identity.accountId);
    auth.insert(QStringLiteral("tokens"), tokens);

    const QString path = QDir(m_pendingCodexProfilePath).filePath(QStringLiteral("auth.json"));
    const QByteArray data = QJsonDocument(auth).toJson(QJsonDocument::Indented);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(data) != data.size()
        || !file.commit()) {
        return false;
    }
    return QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

void AiUsageWatcherApplet::failCodexLogin(const QString &status)
{
    m_codexPollTimer->stop();
    if (m_codexReply) {
        QNetworkReply *reply = m_codexReply;
        m_codexReply = nullptr;
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
    removeCodexProfileDirectory(m_pendingCodexProfileId);
    m_pendingCodexProfileId.clear();
    m_pendingCodexProfilePath.clear();
    m_codexDeviceAuthId.clear();
    m_codexDeviceExpiresAtMs = 0;
    if (!m_codexDeviceCode.isEmpty()) {
        m_codexDeviceCode.clear();
        Q_EMIT codexDeviceCodeChanged();
    }
    setCodexLoginState(!m_codexAccounts.isEmpty(), status, false, true);
}

bool AiUsageWatcherApplet::removeCodexProfileDirectory(const QString &profileId)
{
    if (!isValidCodexProfileId(profileId)) {
        return false;
    }
    const QString root = QDir::cleanPath(codexAccountsRoot());
    const QString path = QDir::cleanPath(QDir(root).filePath(profileId));
    return path.startsWith(root + QDir::separator()) && QDir(path).removeRecursively();
}

void AiUsageWatcherApplet::removeCodexAccount(const QString &profileId)
{
    if (!removeCodexProfileDirectory(profileId)) {
        setCodexLoginState(m_codexLoggedIn,
                           QStringLiteral("无法删除 Codex 账号"),
                           false,
                           true);
        return;
    }
    loadCodexAccounts();
    refreshCodexUsage();
}

void AiUsageWatcherApplet::cancelCodexLogin()
{
    if (!m_codexLoginBusy) {
        return;
    }
    m_codexLoginCancelled = true;
    m_codexPollTimer->stop();
    if (m_codexReply) {
        QNetworkReply *reply = m_codexReply;
        m_codexReply = nullptr;
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
    removeCodexProfileDirectory(m_pendingCodexProfileId);
    m_pendingCodexProfileId.clear();
    m_pendingCodexProfilePath.clear();
    m_codexDeviceAuthId.clear();
    if (!m_codexDeviceCode.isEmpty()) {
        m_codexDeviceCode.clear();
        Q_EMIT codexDeviceCodeChanged();
    }
    setCodexLoginState(!m_codexAccounts.isEmpty(),
                       QStringLiteral("Codex 登录已取消"),
                       false,
                       false);
}

void AiUsageWatcherApplet::openCodexLoginPage()
{
    if (!QDesktopServices::openUrl(QUrl(codexDeviceLoginUrl))) {
        setCodexLoginState(m_codexLoggedIn,
                           QStringLiteral("无法打开浏览器，请手动访问登录地址"),
                           m_codexLoginBusy,
                           true);
    }
}

void AiUsageWatcherApplet::setCodexLoginState(bool loggedIn,
                                               const QString &status,
                                               bool busy,
                                               bool error)
{
    if (m_codexLoggedIn != loggedIn) {
        m_codexLoggedIn = loggedIn;
        Q_EMIT codexLoggedInChanged();
    }
    if (m_codexLoginStatus != status) {
        m_codexLoginStatus = status;
        Q_EMIT codexLoginStatusChanged();
    }
    if (m_codexLoginBusy != busy) {
        m_codexLoginBusy = busy;
        Q_EMIT codexLoginBusyChanged();
    }
    if (m_codexLoginError != error) {
        m_codexLoginError = error;
        Q_EMIT codexLoginErrorChanged();
    }
}

void AiUsageWatcherApplet::attachJavaScriptHighlighter(QQuickTextDocument *document,
                                                       const QColor &keywordColor,
                                                       const QColor &stringColor,
                                                       const QColor &commentColor,
                                                       const QColor &numberColor)
{
    QTextDocument *textDocument = document ? document->textDocument() : nullptr;
    if (!textDocument)
        return;
    auto *highlighter = textDocument->findChild<JavaScriptHighlighter *>(
        QStringLiteral("quotaPilotJavaScriptHighlighter"), Qt::FindDirectChildrenOnly);
    if (highlighter) {
        highlighter->updateColors(keywordColor, stringColor, commentColor, numberColor);
        return;
    }
    new JavaScriptHighlighter(textDocument,
                              keywordColor,
                              stringColor,
                              commentColor,
                              numberColor);
}

K_PLUGIN_CLASS_WITH_JSON(AiUsageWatcherApplet, "../package/metadata.json")

#include "aiusagewatcherapplet.moc"
