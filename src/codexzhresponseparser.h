// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArrayView>
#include <QString>
#include <QVariantMap>

struct CodexZhPlan
{
    QString planId;
    QString planName;
    double used = 0;
    double total = 100;
    QString extraText;
};

struct CodexZhSnapshot
{
    QString statusLabel;
    QString errorText;
    CodexZhPlan plan;
};

struct CodexZhParseResult
{
    bool ok = false;
    QString errorMessage;
    CodexZhSnapshot snapshot;
};

class CodexZhResponseParser
{
public:
    static CodexZhParseResult parse(QByteArrayView payload);
};
