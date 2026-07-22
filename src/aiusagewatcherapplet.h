// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Plasma/Applet>
#include <QColor>
#include <QVariantList>

#include "customusageclient.h"
#include "minimaxclient.h"

class QQuickTextDocument;
class QNetworkAccessManager;
class QNetworkReply;
class QJsonObject;
class QTimer;

class AiUsageWatcherApplet : public Plasma::Applet
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap miniMaxSnapshot READ miniMaxSnapshot NOTIFY miniMaxSnapshotChanged)
    Q_PROPERTY(bool miniMaxLoading READ miniMaxLoading NOTIFY miniMaxLoadingChanged)
    Q_PROPERTY(bool miniMaxCredentialConfigured READ miniMaxCredentialConfigured NOTIFY miniMaxCredentialConfiguredChanged)
    Q_PROPERTY(QString miniMaxCredentialStatus READ miniMaxCredentialStatus NOTIFY miniMaxCredentialStatusChanged)
    Q_PROPERTY(bool miniMaxCredentialBusy READ miniMaxCredentialBusy NOTIFY miniMaxCredentialBusyChanged)
    Q_PROPERTY(bool miniMaxCredentialError READ miniMaxCredentialError NOTIFY miniMaxCredentialErrorChanged)
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

    Q_INVOKABLE void refreshMiniMax();
    Q_INVOKABLE void saveMiniMaxApiKey(const QString &apiKey);
    Q_INVOKABLE void clearMiniMaxApiKey();
    Q_INVOKABLE void refreshCodexUsage();
    Q_INVOKABLE void refreshCodexLoginStatus();
    Q_INVOKABLE void startCodexLogin();
    Q_INVOKABLE void cancelCodexLogin();
    Q_INVOKABLE void openCodexLoginPage();
    Q_INVOKABLE void removeCodexAccount(const QString &profileId);
    Q_INVOKABLE void refreshCustomProviders(const QVariantList &definitions);
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

private Q_SLOTS:
    void handleModelActivated(const QString &modelName);

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

    MiniMaxClient m_miniMaxClient;
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
};
