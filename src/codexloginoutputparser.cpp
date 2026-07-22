// SPDX-License-Identifier: GPL-2.0-or-later

#include "codexloginoutputparser.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>
#include <limits>

namespace
{
int integerValue(const QJsonValue &value, int fallback)
{
    if (value.isDouble()) {
        return value.toInt(fallback);
    }
    bool ok = false;
    const int parsed = value.toString().toInt(&ok);
    return ok ? parsed : fallback;
}


QJsonObject jwtClaims(const QString &jwt)
{
    const QList<QByteArray> parts = jwt.toLatin1().split('.');
    if (parts.size() != 3) {
        return {};
    }

    QByteArray payload = parts.at(1);
    while (payload.size() % 4 != 0) {
        payload.append('=');
    }
    return QJsonDocument::fromJson(
        QByteArray::fromBase64(payload, QByteArray::Base64UrlEncoding)).object();
}

QString windowLabel(qint64 seconds)
{
    if (seconds == 5 * 60 * 60) {
        return QStringLiteral("5 小时");
    }
    if (seconds == 7 * 24 * 60 * 60) {
        return QStringLiteral("7 天");
    }
    if (seconds == 30 * 24 * 60 * 60) {
        return QStringLiteral("30 天");
    }
    if (seconds % (24 * 60 * 60) == 0) {
        return QStringLiteral("%1 天").arg(seconds / (24 * 60 * 60));
    }
    if (seconds % (60 * 60) == 0) {
        return QStringLiteral("%1 小时").arg(seconds / (60 * 60));
    }
    return QStringLiteral("当前窗口");
}

QString windowId(qint64 seconds)
{
    if (seconds % (24 * 60 * 60) == 0) {
        return QStringLiteral("%1-days").arg(seconds / (24 * 60 * 60));
    }
    if (seconds % (60 * 60) == 0) {
        return QStringLiteral("%1-hours").arg(seconds / (60 * 60));
    }
    return QStringLiteral("%1-seconds").arg(seconds);
}

bool appendUsageWindow(const QJsonObject &rateLimit,
                       const QString &key,
                       QList<CodexUsageWindow> &windows)
{
    const QJsonValue value = rateLimit.value(key);
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    if (!value.isObject()) {
        return false;
    }

    const QJsonObject object = value.toObject();
    const double usedPercent = object.value(QStringLiteral("used_percent")).toDouble(-1);
    const double windowSecondsValue = object.value(
        QStringLiteral("limit_window_seconds")).toDouble(-1);
    const double resetAtValue = object.value(QStringLiteral("reset_at")).toDouble(0);
    if (!std::isfinite(usedPercent) || usedPercent < 0 || usedPercent > 100
        || !std::isfinite(windowSecondsValue) || windowSecondsValue <= 0
        || std::floor(windowSecondsValue) != windowSecondsValue
        || windowSecondsValue >= -static_cast<double>(std::numeric_limits<qint64>::min())
        || !std::isfinite(resetAtValue) || resetAtValue < 0
        || std::floor(resetAtValue) != resetAtValue
        || resetAtValue >= -static_cast<double>(std::numeric_limits<qint64>::min())) {
        return false;
    }

    const qint64 seconds = static_cast<qint64>(windowSecondsValue);
    windows.push_back({windowId(seconds),
                       windowLabel(seconds),
                       usedPercent,
                       static_cast<qint64>(resetAtValue)});
    return true;
}
}

CodexDeviceAuthorization CodexLoginOutputParser::deviceAuthorization(const QByteArray &json)
{
    const QJsonObject object = QJsonDocument::fromJson(json).object();
    CodexDeviceAuthorization authorization;
    authorization.deviceAuthId = object.value(QStringLiteral("device_auth_id")).toString();
    authorization.userCode = object.value(QStringLiteral("user_code")).toString();
    authorization.intervalSeconds = integerValue(object.value(QStringLiteral("interval")), 5);
    authorization.expiresInSeconds = integerValue(object.value(QStringLiteral("expires_in")), 900);
    return authorization;
}

CodexAuthorizationResult CodexLoginOutputParser::authorizationResult(const QByteArray &json)
{
    const QJsonObject object = QJsonDocument::fromJson(json).object();
    return {
        object.value(QStringLiteral("authorization_code")).toString(),
        object.value(QStringLiteral("code_verifier")).toString(),
    };
}

CodexTokenExchange CodexLoginOutputParser::tokenExchange(const QByteArray &json)
{
    const QJsonObject object = QJsonDocument::fromJson(json).object();
    return {
        object.value(QStringLiteral("id_token")).toString(),
        object.value(QStringLiteral("access_token")).toString(),
        object.value(QStringLiteral("refresh_token")).toString(),
    };
}

CodexRefreshExchange CodexLoginOutputParser::refreshExchange(const QByteArray &json)
{
    const QJsonObject object = QJsonDocument::fromJson(json).object();
    return {
        object.value(QStringLiteral("id_token")).toString(),
        object.value(QStringLiteral("access_token")).toString(),
        object.value(QStringLiteral("refresh_token")).toString(),
    };
}

CodexAccountIdentity CodexLoginOutputParser::accountIdentity(const QByteArray &authJson)
{
    const QJsonObject root = QJsonDocument::fromJson(authJson).object();
    const QJsonObject tokens = root.value(QStringLiteral("tokens")).toObject();
    const QString idToken = tokens.value(QStringLiteral("id_token")).toString();
    const QJsonObject claims = jwtClaims(idToken);
    const QJsonObject profile = claims.value(
        QStringLiteral("https://api.openai.com/profile")).toObject();
    const QJsonObject auth = claims.value(
        QStringLiteral("https://api.openai.com/auth")).toObject();
    CodexAccountIdentity identity;
    identity.accountId = tokens.value(QStringLiteral("account_id")).toString();
    if (identity.accountId.isEmpty()) {
        identity.accountId = auth.value(QStringLiteral("chatgpt_account_id")).toString();
    }
    identity.email = claims.value(QStringLiteral("email")).toString();
    if (identity.email.isEmpty()) {
        identity.email = profile.value(QStringLiteral("email")).toString();
    }
    return identity;
}

qint64 CodexLoginOutputParser::tokenExpiresAt(const QString &jwt)
{
    const QJsonValue value = jwtClaims(jwt).value(QStringLiteral("exp"));
    const double expiresAt = value.toDouble(0);
    if (!std::isfinite(expiresAt) || expiresAt <= 0 || std::floor(expiresAt) != expiresAt
        || expiresAt >= -static_cast<double>(std::numeric_limits<qint64>::min())) {
        return 0;
    }
    return static_cast<qint64>(expiresAt);
}

CodexUsageResult CodexLoginOutputParser::usageResponse(const QByteArray &json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isObject()) {
        return {{}, QStringLiteral("Codex 返回了无法识别的数据")};
    }
    const QJsonValue rateLimitValue = document.object().value(QStringLiteral("rate_limit"));
    if (!rateLimitValue.isObject()) {
        return {{}, QStringLiteral("Codex 响应缺少额度数据")};
    }

    QList<CodexUsageWindow> windows;
    const QJsonObject rateLimit = rateLimitValue.toObject();
    if (!appendUsageWindow(rateLimit, QStringLiteral("primary_window"), windows)
        || !appendUsageWindow(rateLimit, QStringLiteral("secondary_window"), windows)) {
        return {{}, QStringLiteral("Codex 返回了无效的额度数据")};
    }
    if (windows.isEmpty()) {
        return {{}, QStringLiteral("Codex 暂未返回可用额度窗口")};
    }
    return {windows, {}};
}
