// SPDX-License-Identifier: GPL-2.0-or-later

#include "codexzhresponseparser.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QTime>

#include <cmath>
#include <limits>
#include <optional>

namespace
{
std::optional<double> toNumberOrNull(const QJsonObject &obj, const QString &key)
{
    const QJsonValue val = obj.value(key);
    if (val.isDouble()) {
        const double value = val.toDouble();
        return std::isfinite(value) ? std::optional<double>(value) : std::nullopt;
    }
    if (!val.isString()) {
        return std::nullopt;
    }

    QString normalized = val.toString();
    normalized.remove(QRegularExpression(QStringLiteral("[$,\\s]")));
    bool ok = false;
    const double value = normalized.toDouble(&ok);
    return ok && std::isfinite(value) ? std::optional<double>(value) : std::nullopt;
}

std::optional<double> quotaPointsToUsd(const QJsonObject &obj, const QString &key)
{
    const std::optional<double> points = toNumberOrNull(obj, key);
    return points ? std::optional<double>(*points / 500000.0) : std::nullopt;
}

QString formatUsd(double usd)
{
    return QStringLiteral("$%1").arg(usd, 0, 'f', 2);
}

// 完整整数 + 千分位逗号,官网原样展示调用次数/Token;不用 K/M。
QString formatCountInt(double value)
{
    if (!std::isfinite(value) || value < 0) {
        return QStringLiteral("0");
    }
    // 超出 quint64 上限的直接显示原值（畸形响应防御，避免转换 UB）
    if (value >= 1.8446744073709552e19) {
        return QString::number(value, 'f', 0);
    }
    const quint64 intVal = static_cast<quint64>(std::round(value));
    const QString digits = QString::number(intVal);
    QString out;
    out.reserve(digits.size() + digits.size() / 3);
    for (int i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0) {
            out.append(QLatin1Char(','));
        }
        out.append(digits.at(i));
    }
    return out;
}

QString formattedOrUsd(const QJsonObject &obj, const QString &formattedKey, const QString &rawKey)
{
    const QJsonValue formatted = obj.value(formattedKey);
    if (formatted.isString()) {
        const QString text = formatted.toString().trimmed();
        if (!text.isEmpty()) {
            return text.startsWith(QLatin1Char('$')) ? text : QStringLiteral("$") + text;
        }
    }
    return formatUsd(toNumberOrNull(obj, rawKey).value_or(0.0));
}

// 官网优先级:主 formatted → 次 formatted → 主 raw → 次 raw。
QString chainedFormattedOrUsd(const QJsonObject &obj,
                              const QString &primaryFormatted,
                              const QString &primaryRaw,
                              const QString &secondaryFormatted,
                              const QString &secondaryRaw)
{
    auto firstString = [&](const QString &key) -> QString {
        const QJsonValue v = obj.value(key);
        if (v.isString()) {
            const QString t = v.toString().trimmed();
            if (!t.isEmpty()) {
                return t;
            }
        }
        return {};
    };
    auto firstNumber = [&](const QString &key, bool *found) -> double {
        // 与 formattedOrUsd 一致：兼容字符串数字（"$1,234.5" 等官网格式）
        const std::optional<double> value = toNumberOrNull(obj, key);
        if (value) {
            *found = true;
            return *value;
        }
        *found = false;
        return 0.0;
    };
    if (!primaryFormatted.isEmpty()) {
        const QString t = firstString(primaryFormatted);
        if (!t.isEmpty()) {
            return t.startsWith(QLatin1Char('$')) ? t : QStringLiteral("$") + t;
        }
    }
    if (!secondaryFormatted.isEmpty()) {
        const QString t = firstString(secondaryFormatted);
        if (!t.isEmpty()) {
            return t.startsWith(QLatin1Char('$')) ? t : QStringLiteral("$") + t;
        }
    }
    if (!primaryRaw.isEmpty()) {
        bool ok = false;
        const double d = firstNumber(primaryRaw, &ok);
        if (ok) {
            return QStringLiteral("$%1").arg(d, 0, 'f', 2);
        }
    }
    if (!secondaryRaw.isEmpty()) {
        bool ok = false;
        const double d = firstNumber(secondaryRaw, &ok);
        if (ok) {
            return QStringLiteral("$%1").arg(d, 0, 'f', 2);
        }
    }
    return QStringLiteral("$0.00");
}

QString buildExtraText(const QJsonObject &data)
{
    std::optional<double> dailyBudget = toNumberOrNull(data, QStringLiteral("dailyBudget"));
    if (!dailyBudget) {
        dailyBudget = quotaPointsToUsd(data, QStringLiteral("dailyQuota"));
    }
    std::optional<double> weeklyBudget = toNumberOrNull(data, QStringLiteral("weeklyBudget"));
    if (!weeklyBudget) {
        weeklyBudget = quotaPointsToUsd(data, QStringLiteral("weeklyQuota"));
    }
    const double todayUsed = toNumberOrNull(data, QStringLiteral("todayUsed")).value_or(0.0);
    const double weekUsed = toNumberOrNull(data, QStringLiteral("weekUsed")).value_or(0.0);
    const double todayRemainValue = qMax(dailyBudget.value_or(0.0) - todayUsed, 0.0);
    std::optional<double> weekRemain = toNumberOrNull(data, QStringLiteral("remainQuota"));
    if (!weekRemain) {
        weekRemain = qMax(weeklyBudget.value_or(0.0) - weekUsed, 0.0);
    }

    std::optional<double> totalCalls = toNumberOrNull(data, QStringLiteral("totalRequests"));
    if (!totalCalls) {
        totalCalls = toNumberOrNull(data, QStringLiteral("totalCalls"));
    }

    QString subStart = data.value(QStringLiteral("subscriptionStart")).toString().trimmed();
    QString subEnd = data.value(QStringLiteral("subscriptionEnd")).toString().trimmed();
    if (subStart.isEmpty()) {
        subStart = QStringLiteral("-");
    }
    if (subEnd.isEmpty()) {
        subEnd = QStringLiteral("-");
    }

    QStringList parts;
    parts.reserve(16);
    // 本周调用缺失时显示占位符，不用总调用次数冒充（语义不同，避免误导）
    const std::optional<double> weekCalls = toNumberOrNull(data, QStringLiteral("weekCalls"));

    parts.append(QStringLiteral("今日调用：%1").arg(formatCountInt(toNumberOrNull(data, QStringLiteral("todayCalls")).value_or(0.0))));
    parts.append(QStringLiteral("今日消费：%1")
                     .arg(formattedOrUsd(data, QStringLiteral("todayUsedFormatted"), QStringLiteral("todayUsed"))));
    parts.append(QStringLiteral("今日 Token：%1")
                     .arg(formatCountInt(toNumberOrNull(data, QStringLiteral("todayTokens")).value_or(0.0))));
    parts.append(QStringLiteral("日限额度：%1").arg(formatUsd(dailyBudget.value_or(0.0))));
    parts.append(QStringLiteral("今日剩余：%1").arg(formatUsd(todayRemainValue)));
    parts.append(QStringLiteral("本周调用：%1").arg(weekCalls ? formatCountInt(*weekCalls) : QStringLiteral("-")));
    parts.append(QStringLiteral("本周消费：%1")
                     .arg(formattedOrUsd(data, QStringLiteral("weekUsedFormatted"), QStringLiteral("weekUsed"))));
    parts.append(QStringLiteral("周限额度：%1").arg(formatUsd(weeklyBudget.value_or(0.0))));
    parts.append(QStringLiteral("实时剩余：%1").arg(formatUsd(weekRemain.value_or(0.0))));
    parts.append(QStringLiteral("总请求次数：%1").arg(formatCountInt(totalCalls.value_or(0.0))));
    parts.append(QStringLiteral("总使用额度：%1")
                     .arg(chainedFormattedOrUsd(data,
                                                QStringLiteral("analyticsTotalUsedFormatted"),
                                                QStringLiteral("analyticsTotalUsed"),
                                                QStringLiteral("totalUsedFormatted"),
                                                QStringLiteral("totalUsed"))));
    parts.append(QStringLiteral("总使用 Token：%1")
                     .arg(formatCountInt(toNumberOrNull(data, QStringLiteral("totalTokens")).value_or(0.0))));
    parts.append(QStringLiteral("RPM：%1").arg(formatCountInt(toNumberOrNull(data, QStringLiteral("rpm")).value_or(0.0))));
    parts.append(QStringLiteral("TPM：%1").arg(formatCountInt(toNumberOrNull(data, QStringLiteral("tpm")).value_or(0.0))));
    parts.append(QStringLiteral("订阅开始：%1").arg(subStart));
    parts.append(QStringLiteral("订阅到期：%1").arg(subEnd));

    return parts.join(QStringLiteral(" | "));
}

QDateTime nextWeeklyResetAt(const QDateTime &now)
{
    if (!now.isValid()) {
        return {};
    }
    // CodexZH 周额度按自然周刷新，每周一 00:00 恢复满额。
    // 取「now 之后」最近的周一 00:00：今天若是周一 00:00 也已算过去，要跳到下一周。
    const QDateTime local = now.toLocalTime();
    const QDate today = local.date();
    const int daysToMonday = (Qt::Monday - today.dayOfWeek() + 7) % 7;
    QDate resetDate = today.addDays(daysToMonday);
    QDateTime reset(resetDate, QTime(0, 0));
    if (reset <= local) {
        resetDate = resetDate.addDays(7);
        reset = QDateTime(resetDate, QTime(0, 0));
    }
    return reset;
}
}

QString CodexZhResponseParser::nextWeeklyResetText(const QDateTime &now)
{
    return nextWeeklyResetAt(now).toString(QStringLiteral("MM-dd HH:mm"));
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

    const std::optional<double> weeklyBudget = toNumberOrNull(data, QStringLiteral("weeklyBudget"));
    const std::optional<double> weekPoints = quotaPointsToUsd(data, QStringLiteral("weeklyQuota"));
    const std::optional<double> weekUsed = toNumberOrNull(data, QStringLiteral("weekUsed"));

    if (!weekUsed || (!weeklyBudget && !weekPoints)) {
        return {
            false,
            QStringLiteral("CodexZH 数据解析失败"),
            {},
        };
    }

    const double weeklyLimit = weeklyBudget ? *weeklyBudget : *weekPoints;
    if (weeklyLimit <= 0 || *weekUsed < 0) {
        return {
            false,
            QStringLiteral("CodexZH 数据解析失败"),
            {},
        };
    }

    CodexZhPlan plan;
    plan.planId = QStringLiteral("weekly");
    plan.planName = QStringLiteral("周限额");
    plan.used = *weekUsed;
    plan.total = weeklyLimit;
    const QDateTime resetAt = nextWeeklyResetAt(QDateTime::currentDateTime());
    plan.resetText = resetAt.toString(QStringLiteral("MM-dd HH:mm"));
    plan.resetAtMs = resetAt.toMSecsSinceEpoch();
    plan.extraText = buildExtraText(data);

    const std::optional<double> todayUsed = toNumberOrNull(data, QStringLiteral("todayUsed"));
    if (todayUsed && *todayUsed >= 0) {
        const double clampedToday = qBound(0.0, *todayUsed, *weekUsed);
        const double previous = *weekUsed - clampedToday;
        if (previous > 0) {
            plan.usageSegments.append({QStringLiteral("previous"),
                                       previous,
                                       previous / weeklyLimit * 100.0,
                                       formatUsd(previous)});
        }
        if (clampedToday > 0) {
            plan.usageSegments.append({QStringLiteral("today"),
                                       clampedToday,
                                       clampedToday / weeklyLimit * 100.0,
                                       formattedOrUsd(data,
                                                      QStringLiteral("todayUsedFormatted"),
                                                      QStringLiteral("todayUsed"))});
        }
    }

    CodexZhSnapshot snapshot;
    snapshot.statusLabel = QStringLiteral("可用");
    snapshot.errorText = QString();
    snapshot.plan = plan;

    return {true, {}, snapshot};
}
