// SPDX-License-Identifier: GPL-2.0-or-later

#include "deepseekresponseparser.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include <cmath>

namespace
{
DeepSeekParseResult failure(const QString &message)
{
    DeepSeekParseResult result;
    result.ok = false;
    result.errorMessage = message;
    return result;
}

bool readAmount(const QJsonValue &value, double &result)
{
    if (value.isDouble()) {
        const double number = value.toDouble();
        if (!std::isfinite(number) || number < 0) {
            return false;
        }
        result = number;
        return true;
    }
    if (value.isString()) {
        bool ok = false;
        const double number = value.toString().toDouble(&ok);
        if (!ok || !std::isfinite(number) || number < 0) {
            return false;
        }
        result = number;
        return true;
    }
    return false;
}

bool readBalance(const QJsonObject &object, DeepSeekBalance &balance)
{
    if (!readAmount(object.value(QStringLiteral("total_balance")), balance.totalBalance)) {
        return false;
    }
    readAmount(object.value(QStringLiteral("granted_balance")), balance.grantedBalance);
    readAmount(object.value(QStringLiteral("topped_up_balance")), balance.toppedUpBalance);
    balance.currency = object.value(QStringLiteral("currency"))
                           .toString(QStringLiteral("CNY"));
    return true;
}
} // namespace

DeepSeekParseResult DeepSeekResponseParser::parse(QByteArrayView payload, int httpStatus)
{
    if (httpStatus == 401 || httpStatus == 403) {
        return failure(QStringLiteral("鉴权失败 (HTTP %1)").arg(httpStatus));
    }
    if (httpStatus < 200 || httpStatus >= 300) {
        return failure(QStringLiteral("接口错误 (HTTP %1)").arg(httpStatus));
    }

    const QByteArray bytes(payload);
    const QJsonDocument document = QJsonDocument::fromJson(bytes);
    if (!document.isObject()) {
        return failure(QStringLiteral("DeepSeek 返回了无法识别的数据"));
    }
    const QJsonObject body = document.object();
    const bool isAvailable = body.value(QStringLiteral("is_available"))
                                 .toBool(true);

    DeepSeekParseResult result;
    result.ok = true;
    const QJsonArray infos = body.value(QStringLiteral("balance_infos"))
                                  .toArray();
    for (const QJsonValue &value : infos) {
        if (!value.isObject()) {
            return failure(QStringLiteral("DeepSeek 返回了无法识别的数据"));
        }
        DeepSeekBalance balance;
        balance.isAvailable = isAvailable;
        if (!readBalance(value.toObject(), balance)) {
            return failure(QStringLiteral("DeepSeek 返回了无法识别的数据"));
        }
        result.balances.append(balance);
    }
    return result;
}
