// SPDX-License-Identifier: GPL-2.0-or-later

#include "codexzhresponseparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cmath>
#include <limits>

namespace
{
QString toNumberOrNull(const QJsonObject &obj, const QString &key)
{
    const QJsonValue val = obj.value(key);
    if (val.isNull() || !val.isDouble()) {
        return QString();
    }
    const double d = val.toDouble();
    if (!std::isfinite(d)) {
        return QString();
    }
    return QString::number(d, 'f', 6);
}

QString toNumber(const QJsonObject &obj, const QString &key)
{
    const QJsonValue val = obj.value(key);
    if (!val.isDouble()) {
        return QStringLiteral("0");
    }
    const double d = val.toDouble();
    if (!std::isfinite(d)) {
        return QStringLiteral("0");
    }
    return QString::number(d, 'f', 6);
}

QString quotaPointsToUsd(const QJsonObject &obj, const QString &key)
{
    const QString val = toNumber(obj, key);
    if (val.isEmpty()) {
        return QStringLiteral("0");
    }
    bool ok = false;
    double points = val.toDouble(&ok);
    if (!ok) {
        return QStringLiteral("0");
    }
    return QString::number(points * 0.002, 'f', 6);
}

QString formatUsd(const QString &usd)
{
    bool ok = false;
    double val = usd.toDouble(&ok);
    if (!ok) {
        return QStringLiteral("$0.00");
    }
    return QStringLiteral("$%1").arg(val, 0, 'f', 2);
}

QString formatCount(const QString &count)
{
    bool ok = false;
    double val = count.toDouble(&ok);
    if (!ok) {
        return QStringLiteral("0");
    }
    if (val >= 1000000) {
        return QString::number(val / 1000000, 'f', 2) + QStringLiteral("M");
    }
    if (val >= 1000) {
        return QString::number(val / 1000, 'f', 2) + QStringLiteral("K");
    }
    return QString::number(val, 'f', 0);
}

QString buildExtraText(const QJsonObject &data)
{
    QStringList parts;

    const QString todayCalls = toNumber(data, QStringLiteral("todayCalls"));
    const QString todayUsed = quotaPointsToUsd(data, QStringLiteral("todayPoints"));
    const QString todayTokens = formatCount(toNumber(data, QStringLiteral("todayTokens")));
    const QString todayLimit = formatUsd(toNumber(data, QStringLiteral("dailyBudgetUsd")));
    const QString todayRemain = formatUsd(toNumberOrNull(data, QStringLiteral("todayRemain")));

    const QString weekCalls = toNumber(data, QStringLiteral("weekCalls"));
    const QString weekUsed = quotaPointsToUsd(data, QStringLiteral("weekPoints"));
    const QString weekLimit = formatUsd(toNumber(data, QStringLiteral("weeklyBudgetUsd")));
    const QString weekRemain = formatUsd(toNumberOrNull(data, QStringLiteral("weekRemain")));

    const QString totalCalls = formatCount(toNumber(data, QStringLiteral("totalCalls")));
    const QString totalUsed = quotaPointsToUsd(data, QStringLiteral("totalPoints"));
    const QString totalTokens = formatCount(toNumber(data, QStringLiteral("totalTokens")));
    const QString rpm = toNumber(data, QStringLiteral("rpm"));
    const QString tpm = toNumber(data, QStringLiteral("tpm"));
    const QString subStart = toNumber(data, QStringLiteral("subscriptionStartTs"));
    const QString subEnd = toNumber(data, QStringLiteral("subscriptionEndTs"));

    parts.append(QStringLiteral("今日 %1次/%2 / %3token").arg(todayCalls, todayUsed, todayTokens));
    parts.append(QStringLiteral("日限 %1 / 剩 %2").arg(todayLimit, todayRemain));
    parts.append(QStringLiteral("本周 %1次/%2").arg(weekCalls, weekUsed));
    parts.append(QStringLiteral("周限 %1 / 剩 %2").arg(weekLimit, weekRemain));
    parts.append(QStringLiteral("累计 %1次/%2 / %3token").arg(totalCalls, totalUsed, totalTokens));
    if (!rpm.isEmpty()) {
        parts.append(QStringLiteral("RPM: %1").arg(rpm));
    }
    if (!tpm.isEmpty()) {
        parts.append(QStringLiteral("TPM: %1").arg(tpm));
    }
    if (!subStart.isEmpty() && !subEnd.isEmpty()) {
        parts.append(QStringLiteral("订阅 %1 ~ %2").arg(subStart, subEnd));
    }

    return parts.join(QStringLiteral(" | "));
}
}

CodexZhParseResult CodexZhResponseParser::parse(QByteArrayView payload)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload.toByteArray(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {
            false,
            QStringLiteral("CodexZH 返回了无法识别的数据"),
            {},
        };
    }

    const QJsonObject root = document.object();
    const bool success = root.value(QStringLiteral("success")).toBool();
    if (!success) {
        const QString message = root.value(QStringLiteral("message")).toString();
        return {
            false,
            message.isEmpty() ? QStringLiteral("查询失败") : message,
            {},
        };
    }

    const QJsonValue dataValue = root.value(QStringLiteral("data"));
    if (!dataValue.isObject()) {
        return {
            false,
            QStringLiteral("CodexZH 返回了无效的数据结构"),
            {},
        };
    }

    const QJsonObject data = dataValue.toObject();

    const QString weeklyBudgetStr = toNumber(data, QStringLiteral("weeklyBudgetUsd"));
    const QString weekPointsStr = quotaPointsToUsd(data, QStringLiteral("weeklyQuota"));
    const QString weekUsedStr = toNumber(data, QStringLiteral("weekUsed"));
    const QString weekRemainStr = toNumberOrNull(data, QStringLiteral("remainQuota"));

    bool ok1 = false, ok2 = false, ok3 = false, ok4 = false;
    const double weeklyBudget = weeklyBudgetStr.toDouble(&ok1);
    const double weekPoints = weekPointsStr.toDouble(&ok2);
    const double weekUsed = weekUsedStr.toDouble(&ok3);
    const double weekRemain = weekRemainStr.isEmpty() ? 0.0 : weekRemainStr.toDouble(&ok4);

    if (!ok1 || !ok2 || !ok3) {
        return {
            false,
            QStringLiteral("CodexZH 数据解析失败"),
            {},
        };
    }

    const double weeklyLimit = ok1 ? weeklyBudget : (ok2 ? weekPoints : 0.0);
    const double remaining = ok4 ? weekRemain : qMax(weeklyLimit - weekUsed, 0.0);
    const double usedPercent = weeklyLimit > 0 ? qMin((weekUsed / weeklyLimit) * 100.0, 100.0) : 0.0;

    CodexZhPlan plan;
    plan.planId = QStringLiteral("weekly");
    plan.planName = QStringLiteral("Usage Stats");
    plan.used = usedPercent;
    plan.total = 100;
    plan.extraText = buildExtraText(data);

    CodexZhSnapshot snapshot;
    snapshot.statusLabel = QStringLiteral("可用");
    snapshot.errorText = QString();
    snapshot.plan = plan;

    return {true, {}, snapshot};
}
