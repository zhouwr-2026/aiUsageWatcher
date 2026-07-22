// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

struct CodexAccountIdentity {
    QString accountId;
    QString email;

    bool isValid() const { return !accountId.isEmpty(); }
};

struct CodexDeviceAuthorization {
    QString deviceAuthId;
    QString userCode;
    int intervalSeconds = 5;
    int expiresInSeconds = 900;

    bool isValid() const { return !deviceAuthId.isEmpty() && !userCode.isEmpty(); }
};

struct CodexAuthorizationResult {
    QString authorizationCode;
    QString codeVerifier;

    bool isValid() const { return !authorizationCode.isEmpty() && !codeVerifier.isEmpty(); }
};

struct CodexTokenExchange {
    QString idToken;
    QString accessToken;
    QString refreshToken;

    bool isValid() const
    {
        return !idToken.isEmpty() && !accessToken.isEmpty() && !refreshToken.isEmpty();
    }
};

struct CodexRefreshExchange {
    QString idToken;
    QString accessToken;
    QString refreshToken;

    bool isValid() const { return !accessToken.isEmpty(); }
};

struct CodexUsageWindow {
    QString planId;
    QString planName;
    double usedPercent = 0;
    qint64 resetAtSeconds = 0;
};

struct CodexUsageResult {
    QList<CodexUsageWindow> windows;
    QString errorMessage;

    bool isValid() const { return errorMessage.isEmpty() && !windows.isEmpty(); }
};

class CodexLoginOutputParser
{
public:
    static CodexDeviceAuthorization deviceAuthorization(const QByteArray &json);
    static CodexAuthorizationResult authorizationResult(const QByteArray &json);
    static CodexTokenExchange tokenExchange(const QByteArray &json);
    static CodexRefreshExchange refreshExchange(const QByteArray &json);
    static CodexAccountIdentity accountIdentity(const QByteArray &authJson);
    static qint64 tokenExpiresAt(const QString &jwt);
    static CodexUsageResult usageResponse(const QByteArray &json);
};
