// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArrayView>
#include <QList>
#include <QString>

struct MiniMaxPlan
{
    QString planId;
    QString planName;
    double used = 0;
    double total = 100;
    qint64 resetAtMs = 0;
};

struct MiniMaxSnapshot
{
    QString statusLabel;
    QList<MiniMaxPlan> plans;
};

struct MiniMaxParseResult
{
    bool ok = false;
    QString errorCode;
    QString errorMessage;
    MiniMaxSnapshot snapshot;
};

class MiniMaxResponseParser
{
public:
    static MiniMaxParseResult parse(QByteArrayView payload);
};
