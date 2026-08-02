// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArrayView>
#include <QList>
#include <QString>

struct DeepSeekBalance
{
    QString currency;
    double totalBalance = 0;
    double grantedBalance = 0;
    double toppedUpBalance = 0;
    bool isAvailable = true;
};

struct DeepSeekParseResult
{
    bool ok = false;
    QString errorMessage;
    QList<DeepSeekBalance> balances;
};

class DeepSeekResponseParser
{
public:
    // payload: HTTP 响应体；httpStatus: HTTP 状态码
    // 401/403 → ok=false, "鉴权失败 (HTTP xxx)"
    // 其他非 2xx → ok=false, "接口错误 (HTTP xxx)"
    // 2xx 且非法 JSON → ok=false, "DeepSeek 返回了无法识别的数据"
    static DeepSeekParseResult parse(QByteArrayView payload, int httpStatus);
};
