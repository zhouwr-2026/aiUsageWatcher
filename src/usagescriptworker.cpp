// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJSEngine>
#include <QJSValue>

#ifdef Q_OS_UNIX
#include <sys/resource.h>
#endif

namespace
{
void writeResult(const QJsonObject &result)
{
    QFile output;
    if (!output.open(stdout, QIODevice::WriteOnly)) {
        return;
    }
    output.write(QJsonDocument(result).toJson(QJsonDocument::Compact));
    output.write("\n");
}

QJsonObject failure(const QString &message)
{
    return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), message}};
}

QJsonObject success(const QJsonValue &value)
{
    return {{QStringLiteral("ok"), true}, {QStringLiteral("value"), value}};
}

void restrictProcess()
{
#ifdef Q_OS_UNIX
    const rlimit cpuLimit{2, 2};
    setrlimit(RLIMIT_CPU, &cpuLimit);
#endif
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    restrictProcess();

    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly)) {
        writeResult(failure(QStringLiteral("无法读取脚本任务")));
        return 1;
    }
    const QByteArray payload = input.read(512 * 1024 + 1);
    if (payload.size() > 512 * 1024) {
        writeResult(failure(QStringLiteral("脚本任务过大")));
        return 1;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        writeResult(failure(QStringLiteral("脚本任务格式无效")));
        return 1;
    }

    const QJsonObject task = document.object();
    const QString mode = task.value(QStringLiteral("mode")).toString();
    const QString script = task.value(QStringLiteral("script")).toString();
    if (script.trimmed().isEmpty() || script.size() > 256 * 1024) {
        writeResult(failure(QStringLiteral("查询脚本为空或过大")));
        return 1;
    }

    QJSEngine engine;
    const QJSValue configuration = engine.evaluate(script, QStringLiteral("usage-script.js"));
    if (configuration.isError() || !configuration.isObject()) {
        writeResult(failure(QStringLiteral("查询脚本无法解析")));
        return 1;
    }

    if (mode == QLatin1String("request")) {
        const QJSValue request = configuration.property(QStringLiteral("request"));
        const QJsonValue value = QJsonValue::fromVariant(request.toVariant());
        if (!value.isObject()) {
            writeResult(failure(QStringLiteral("脚本缺少有效的 request 对象")));
            return 1;
        }
        writeResult(success(value));
        return 0;
    }

    if (mode == QLatin1String("extract")) {
        const QJSValue extractor = configuration.property(QStringLiteral("extractor"));
        if (!extractor.isCallable()) {
            writeResult(failure(QStringLiteral("脚本缺少有效的 extractor 函数")));
            return 1;
        }
        const QJSValue response = engine.toScriptValue(task.value(QStringLiteral("response")).toVariant());
        const QJSValue extracted = extractor.call({response});
        if (extracted.isError()) {
            writeResult(failure(QStringLiteral("响应解析脚本执行失败")));
            return 1;
        }
        const QJsonValue value = QJsonValue::fromVariant(extracted.toVariant());
        if (!value.isObject()) {
            writeResult(failure(QStringLiteral("extractor 必须返回对象")));
            return 1;
        }
        writeResult(success(value));
        return 0;
    }

    writeResult(failure(QStringLiteral("未知脚本任务")));
    return 1;
}
