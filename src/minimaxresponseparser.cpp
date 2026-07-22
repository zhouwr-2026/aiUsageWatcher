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
    if (!model.contains(QStringLiteral("current_interval_remaining_percent"))) {
        return true;
    }
    double remaining = 0;
    qint64 resetAt = 0;
    if (!readPercentage(model, QStringLiteral("current_interval_remaining_percent"), remaining)
        || !readOptionalReset(model, QStringLiteral("end_time"), resetAt)) {
        return false;
    }
    plans.push_back({QStringLiteral("general-interval"),
                     intervalLabel(model),
                     100 - remaining,
                     100,
                     resetAt});
    return true;
}

bool appendWeeklyQuota(const QJsonObject &model, QList<MiniMaxPlan> &plans)
{
    qint64 status = 0;
    if (!readInteger(model, QStringLiteral("current_weekly_status"), status)
        || status != 1
        || !model.contains(QStringLiteral("current_weekly_remaining_percent"))) {
        return true;
    }
    double remaining = 0;
    qint64 resetAt = 0;
    if (!readPercentage(model, QStringLiteral("current_weekly_remaining_percent"), remaining)
        || !readOptionalReset(model, QStringLiteral("weekly_end_time"), resetAt)) {
        return false;
    }
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
    if (!modelsValue.isArray()) {
        return invalidResponse();
    }

    if (!baseValue.isUndefined() && !baseValue.isNull()) {
        qint64 statusCode = 0;
        if (!baseValue.isObject()
            || !readInteger(baseValue.toObject(), QStringLiteral("status_code"), statusCode)) {
            return invalidResponse();
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
    const QJsonArray models = modelsValue.toArray();
    for (const QJsonValue &modelValue : models) {
        if (!modelValue.isObject()) {
            return invalidResponse();
        }

        const QJsonObject model = modelValue.toObject();
        const QString modelName = model.value(QStringLiteral("model_name")).toString();
        if (modelName.isEmpty()) {
            return invalidResponse();
        }
        if (modelName != QLatin1String("general")) {
            continue;
        }

        if (!appendIntervalQuota(model, snapshot.plans)
            || !appendWeeklyQuota(model, snapshot.plans)) {
            return invalidResponse();
        }
    }

    snapshot.statusLabel = snapshot.plans.isEmpty() ? QStringLiteral("未订阅") : QStringLiteral("可用");
    return {true, {}, {}, snapshot};
}
