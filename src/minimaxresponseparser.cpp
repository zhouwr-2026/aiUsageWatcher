// SPDX-License-Identifier: GPL-2.0-or-later

#include "minimaxresponseparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cmath>

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
    if (!std::isfinite(number) || std::floor(number) != number) {
        return false;
    }

    result = static_cast<qint64>(number);
    return true;
}

bool readPercentage(const QJsonObject &object, const QString &key, int &result)
{
    qint64 value = 0;
    if (!readInteger(object, key, value) || value < 0 || value > 100) {
        return false;
    }
    result = static_cast<int>(value);
    return true;
}

QString modelLabel(const QString &modelName)
{
    if (modelName == QLatin1String("general")) {
        return QStringLiteral("通用模型");
    }
    if (modelName == QLatin1String("video")) {
        return QStringLiteral("视频模型");
    }
    return modelName;
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

bool appendQuota(const QJsonObject &model,
                 const QString &modelName,
                 const QString &suffix,
                 const QString &label,
                 const QString &statusKey,
                 const QString &remainingKey,
                 const QString &endKey,
                 QList<MiniMaxPlan> &plans)
{
    qint64 status = 0;
    if (!readInteger(model, statusKey, status)) {
        return false;
    }
    if (status == 3) {
        return true;
    }
    if (status != 1 && status != 2) {
        return false;
    }

    int remaining = 0;
    qint64 resetAt = 0;
    if (!readPercentage(model, remainingKey, remaining)
        || !readInteger(model, endKey, resetAt)
        || resetAt <= 0) {
        return false;
    }

    plans.push_back({
        modelName + QLatin1Char('-') + suffix,
        modelLabel(modelName) + QStringLiteral(" · ") + label,
        100 - remaining,
        100,
        resetAt,
    });
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
    if (!baseValue.isObject() || !modelsValue.isArray()) {
        return invalidResponse();
    }

    qint64 statusCode = 0;
    if (!readInteger(baseValue.toObject(), QStringLiteral("status_code"), statusCode)) {
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

        if (!appendQuota(model,
                         modelName,
                         QStringLiteral("interval"),
                         intervalLabel(model),
                         QStringLiteral("current_interval_status"),
                         QStringLiteral("current_interval_remaining_percent"),
                         QStringLiteral("end_time"),
                         snapshot.plans)
            || !appendQuota(model,
                            modelName,
                            QStringLiteral("weekly"),
                            QStringLiteral("每周"),
                            QStringLiteral("current_weekly_status"),
                            QStringLiteral("current_weekly_remaining_percent"),
                            QStringLiteral("weekly_end_time"),
                            snapshot.plans)) {
            return invalidResponse();
        }
    }

    snapshot.statusLabel = snapshot.plans.isEmpty() ? QStringLiteral("未订阅") : QStringLiteral("可用");
    return {true, {}, {}, snapshot};
}
