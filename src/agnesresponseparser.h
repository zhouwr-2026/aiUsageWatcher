// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArrayView>
#include <QList>
#include <QString>

struct AgnesPlan
{
    QString planId;
    QString planName;
    QString unit;
    double used = 0;
    double total = 0;
    double percent = 0;
    QString resetText;
    qint64 resetAtMs = 0;
    QString extraText;
};

struct AgnesSnapshot
{
    QString statusLabel;
    QString errorText;
    QList<AgnesPlan> plans;
};

struct AgnesParseResult
{
    bool ok = false;
    QString errorMessage;
    AgnesSnapshot snapshot;
};

class AgnesResponseParser
{
public:
    static AgnesParseResult parse(QByteArrayView payload);
};
