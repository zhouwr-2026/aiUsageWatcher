// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArrayView>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QVariantMap>

struct CodexZhUsageSegment
{
    QString kind;
    double used = 0;
    double usedPercent = 0;
    QString formattedUsed;
};

struct CodexZhPlan
{
    QString planId;
    QString planName;
    double used = 0;
    double total = 100;
    QString resetText;
    qint64 resetAtMs = 0;
    QString extraText;
    QList<CodexZhUsageSegment> usageSegments;
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

    // 返回 CodexZH 周限额的下一次本地自然周周一 00:00。
    static QString nextWeeklyResetText(const QDateTime &now);
};
