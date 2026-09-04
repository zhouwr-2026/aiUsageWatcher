// SPDX-License-Identifier: GPL-2.0-or-later

#include "agnesresponseparser.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>

#include <cmath>

namespace
{
AgnesParseResult invalidResponse(const QString &message)
{
    return {false, message.isEmpty()
                       ? QStringLiteral("Agnes 返回了无法识别的数据")
                       : message, {}};
}

QString clusterLabel(const QString &cluster)
{
    if (cluster == QLatin1String("text_generation")) {
        return QStringLiteral("文本生成");
    }
    if (cluster == QLatin1String("image_generation")) {
        return QStringLiteral("图像生成");
    }
    if (cluster == QLatin1String("video_generation")) {
        return QStringLiteral("视频生成");
    }
    return cluster;
}

QString timeframeLabel(const QString &timeframe)
{
    if (timeframe == QLatin1String("windowed")) {
        return QStringLiteral("滚动窗口");
    }
    if (timeframe == QLatin1String("weekly")) {
        return QStringLiteral("每周");
    }
    if (timeframe == QLatin1String("daily")) {
        return QStringLiteral("每日");
    }
    return timeframe;
}

// Agnes 后端会把 limit/used 用 Double 返回；为防止超大额度（百万级 token）造成
// 32-bit 浮点误差，这里强制读 64-bit 整数。无法读为整数的（如字符串）一律视为缺字段。
bool readInt64(const QJsonObject &object, const QString &key, qint64 &result)
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number)
        || std::floor(number) != number
        || number < static_cast<double>(std::numeric_limits<qint64>::min()) / 2.0
        || number >= static_cast<double>(std::numeric_limits<qint64>::max()) / 2.0) {
        return false;
    }
    result = static_cast<qint64>(number);
    return true;
}

// usage_pct 已经按 used / limit 算好（0..100 之外的值都视为脏数据）
bool readPercentage(const QJsonObject &object, const QString &key, double &result)
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < 0 || number > 100) {
        return false;
    }
    result = number;
    return true;
}

// reset_at 为 epoch 秒；不是数字就当成缺字段（前端没有 resetText）
bool readEpochSeconds(const QJsonObject &object, const QString &key, qint64 &result)
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || number <= 0
        || number >= 4'102'444'800.0) { // 2100-01-01
        return false;
    }
    result = static_cast<qint64>(number);
    return true;
}

// 把每条 (cluster, timeframe, window) 转成一个 AgnesPlan；空对象直接跳过。
// window 内必须至少存在 usage_pct 才记为有效 plan，避免把"还没订阅"误显示成 0%。
// limit/used 缺失时回退到 usage_pct（百分号 0..100 视为虚轴），但 JSON 明确给了非整数
// 或负值时按脏数据直接丢弃该窗口。
bool appendWindow(const QString &cluster,
                  const QString &timeframe,
                  const QJsonObject &window,
                  QList<AgnesPlan> &plans)
{
    double percent = 0;
    if (!readPercentage(window, QStringLiteral("usage_pct"), percent)) {
        return false;
    }
    qint64 limit = 0;
    qint64 used = 0;
    if (window.contains(QStringLiteral("limit"))) {
        if (!readInt64(window, QStringLiteral("limit"), limit) || limit < 0) {
            return false;
        }
    }
    if (window.contains(QStringLiteral("used"))) {
        if (!readInt64(window, QStringLiteral("used"), used) || used < 0) {
            return false;
        }
    }
    if (limit == 0) {
        // Agnes 在没订阅时会省略 limit 或返回 0；但网页 UI 即使无 limit 也展示 percent。
        // 这种情形我们用 100 当虚轴（与 MiniMax 的"通用"plan 一致）以让控件可绘制。
        limit = 100;
        used = std::llround(percent);
    }
    qint64 resetAtSecs = 0;
    readEpochSeconds(window, QStringLiteral("reset_at"), resetAtSecs);
    const QString resetText = resetAtSecs > 0
        ? QDateTime::fromSecsSinceEpoch(resetAtSecs)
              .toLocalTime()
              .toString(QStringLiteral("MM-dd HH:mm"))
        : QString();
    const QString startText = window.value(QStringLiteral("time_range_start")).toString();
    const QString endText = window.value(QStringLiteral("time_range_end")).toString();
    const QString range = startText.isEmpty() && endText.isEmpty()
        ? QString()
        : QStringLiteral("%1 - %2").arg(startText, endText);
    AgnesPlan plan;
    plan.planId = cluster + QLatin1Char('.') + timeframe;
    plan.planName = clusterLabel(cluster) + QStringLiteral(" · ") + timeframeLabel(timeframe);
    plan.unit = QStringLiteral("次");
    plan.used = static_cast<double>(used);
    plan.total = static_cast<double>(limit);
    plan.percent = percent;
    plan.resetText = resetText;
    plan.resetAtMs = resetAtSecs * 1000;
    QStringList extraParts;
    if (!range.isEmpty()) {
        extraParts << range;
    }
    if (!resetText.isEmpty()) {
        extraParts << QStringLiteral("重置 ") + resetText;
    }
    plan.extraText = extraParts.join(QStringLiteral(" · "));
    plans.push_back(plan);
    return true;
}
}

AgnesParseResult AgnesResponseParser::parse(QByteArrayView payload)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload.toByteArray(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return invalidResponse({});
    }

    const QJsonObject root = document.object();
    const qint64 code = static_cast<qint64>(root.value(QStringLiteral("code")).toDouble(-1));
    if (code >= 0 && code != 200) {
        const QString message = root.value(QStringLiteral("message")).toString().trimmed();
        return invalidResponse(message.isEmpty()
                                   ? QStringLiteral("Agnes 用量查询失败")
                                   : message);
    }

    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    if (data.isEmpty()) {
        return invalidResponse(QStringLiteral("Agnes 返回了空数据"));
    }
    const QJsonObject usage = data.value(QStringLiteral("usage")).toObject();
    if (usage.isEmpty()) {
        return invalidResponse(QStringLiteral("Agnes 未提供用量数据"));
    }

    AgnesSnapshot snapshot;
    // 固定顺序：文本 → 图像 → 视频，每个簇内：滚动窗口 → 每周 → 每日
    static const QList<QPair<QString, QString>> orderedKeys = {
        {QStringLiteral("text_generation"), QStringLiteral("windowed")},
        {QStringLiteral("text_generation"), QStringLiteral("weekly")},
        {QStringLiteral("text_generation"), QStringLiteral("daily")},
        {QStringLiteral("image_generation"), QStringLiteral("windowed")},
        {QStringLiteral("image_generation"), QStringLiteral("weekly")},
        {QStringLiteral("image_generation"), QStringLiteral("daily")},
        {QStringLiteral("video_generation"),  QStringLiteral("windowed")},
        {QStringLiteral("video_generation"),  QStringLiteral("weekly")},
        {QStringLiteral("video_generation"),  QStringLiteral("daily")},
    };
    for (const auto &entry : orderedKeys) {
        const QJsonValue clusterValue = usage.value(entry.first);
        if (!clusterValue.isObject()) {
            continue;
        }
        const QJsonObject cluster = clusterValue.toObject();
        const QJsonValue windowValue = cluster.value(entry.second);
        if (!windowValue.isObject()) {
            continue;
        }
        appendWindow(entry.first, entry.second, windowValue.toObject(), snapshot.plans);
    }

    if (snapshot.plans.isEmpty()) {
        snapshot.statusLabel = QStringLiteral("当前套餐未提供用量");
        snapshot.errorText = QStringLiteral("Agnes 用量字段均为空，可能未订阅 Coding Plan");
        return {true, {}, snapshot};
    }

    snapshot.statusLabel = QStringLiteral("可用");
    snapshot.errorText = QString();
    return {true, {}, snapshot};
}
