// SPDX-License-Identifier: GPL-2.0-or-later

#include "minimaxresponseparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cmath>
#include <limits>

namespace
{
MiniMaxParseResult invalidResponse()
{
    return {
        false,
        QStringLiteral("invalid_response"),
        QStringLiteral("MiniMax 返回了无法识别的数据"),
        {},
    };
}

bool readInteger(const QJsonObject &object, const QString &key, qint64 &result)
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        return false;
    }

    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number
        || number < static_cast<double>(std::numeric_limits<qint64>::min())
        || number >= -static_cast<double>(std::numeric_limits<qint64>::min())) {
        return false;
    }

    result = static_cast<qint64>(number);
    return true;
}

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

QString intervalLabel(const QJsonObject &model)
{
    qint64 startAt = 0;
    qint64 endAt = 0;
    if (!readInteger(model, QStringLiteral("start_time"), startAt)
        || !readInteger(model, QStringLiteral("end_time"), endAt)
        || endAt <= startAt) {
        return QStringLiteral("当前周期");
    }

    constexpr qint64 hourMs = 60LL * 60LL * 1000LL;
    const qint64 duration = endAt - startAt;
    if (duration % hourMs != 0) {
        return QStringLiteral("当前周期");
    }
    return QString::number(duration / hourMs) + QStringLiteral(" 小时");
}

bool readOptionalReset(const QJsonObject &model, const QString &key, qint64 &resetAt)
{
    if (!model.contains(key) || model.value(key).isNull()) {
        resetAt = 0;
        return true;
    }
    return readInteger(model, key, resetAt) && resetAt >= 0;
}

bool appendIntervalQuota(const QJsonObject &model, QList<MiniMaxPlan> &plans)
{
    if (!model.contains(QStringLiteral("current_interval_remaining_percent"))
        || model.value(QStringLiteral("current_interval_remaining_percent")).isNull()) {
        return true;
    }
    double remaining = 0;
    qint64 resetAt = 0;
    if (!readPercentage(model, QStringLiteral("current_interval_remaining_percent"), remaining)) {
        return true;
    }
    readOptionalReset(model, QStringLiteral("end_time"), resetAt);
    plans.push_back({QStringLiteral("general-interval"),
                     intervalLabel(model),
                     100 - remaining,
                     100,
                     resetAt});
    return true;
}

bool appendWeeklyQuota(const QJsonObject &model, QList<MiniMaxPlan> &plans)
{
    if (!model.contains(QStringLiteral("current_weekly_status"))) {
        return true;
    }
    qint64 status = 0;
    if (!readInteger(model, QStringLiteral("current_weekly_status"), status) || status != 1) {
        return true;
    }
    if (!model.contains(QStringLiteral("current_weekly_remaining_percent"))
        || model.value(QStringLiteral("current_weekly_remaining_percent")).isNull()) {
        return true;
    }
    double remaining = 0;
    qint64 resetAt = 0;
    if (!readPercentage(model, QStringLiteral("current_weekly_remaining_percent"), remaining)) {
        return true;
    }
    readOptionalReset(model, QStringLiteral("weekly_end_time"), resetAt);
    plans.push_back({QStringLiteral("general-weekly"),
                     QStringLiteral("每周"),
                     100 - remaining,
                     100,
                     resetAt});
    return true;
}
}

MiniMaxParseResult MiniMaxResponseParser::parse(QByteArrayView payload)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload.toByteArray(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return invalidResponse();
    }

    const QJsonObject root = document.object();
    const QJsonValue baseValue = root.value(QStringLiteral("base_resp"));
    const QJsonValue modelsValue = root.value(QStringLiteral("model_remains"));

    if (!baseValue.isUndefined() && !baseValue.isNull()) {
        // status_code 必须存在且可解析；缺失或非零均按接口错误处理（与 cc-switch 参考实现一致）
        qint64 statusCode = 0;
        if (!baseValue.isObject()
            || !readInteger(baseValue.toObject(), QStringLiteral("status_code"), statusCode)) {
            return {
                false,
                QStringLiteral("api_error"),
                QStringLiteral("MiniMax 接口拒绝了本次请求"),
                {},
            };
        }
        if (statusCode != 0) {
            return {
                false,
                QStringLiteral("api_error"),
                QStringLiteral("MiniMax 接口拒绝了本次请求（%1）").arg(statusCode),
                {},
            };
        }
    }

    MiniMaxSnapshot snapshot;
    if (modelsValue.isArray()) {
        for (const QJsonValue &modelValue : modelsValue.toArray()) {
            if (!modelValue.isObject()) {
                continue;
            }
            const QJsonObject model = modelValue.toObject();
            if (model.value(QStringLiteral("model_name")).toString() != QLatin1String("general")) {
                continue;
            }
            appendIntervalQuota(model, snapshot.plans);
            appendWeeklyQuota(model, snapshot.plans);
        }
    }

    snapshot.statusLabel = snapshot.plans.isEmpty() ? QStringLiteral("未订阅") : QStringLiteral("可用");
    return {true, {}, {}, snapshot};
}
