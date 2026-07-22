// SPDX-License-Identifier: GPL-2.0-or-later

#include "customusageclient.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMetaType>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <cmath>

#ifndef QUOTA_PILOT_SCRIPT_WORKER_PATH
#define QUOTA_PILOT_SCRIPT_WORKER_PATH "quota-pilot-script-worker"
#endif

namespace
{
constexpr qsizetype maximumResponseBytes = 1024 * 1024;

QString variableName(const QString &reference)
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(^\$\{([A-Za-z_$][A-Za-z0-9_$]*)\}$)"));
    const QRegularExpressionMatch match = pattern.match(reference.trimmed());
    return match.hasMatch() ? match.captured(1) : QString{};
}

bool isNumeric(const QVariant &value)
{
    const int type = value.metaType().id();
    return type == QMetaType::Double || type == QMetaType::Float
        || type == QMetaType::Int || type == QMetaType::UInt
        || type == QMetaType::LongLong || type == QMetaType::ULongLong;
}

bool hasHttpPlans(const QVariantMap &definition)
{
    const QVariantList plans = definition.value(QStringLiteral("plans")).toList();
    for (const QVariant &item : plans) {
        if (item.toMap().value(QStringLiteral("sourceType")).toString()
            == QLatin1String("http-js")) {
            return true;
        }
    }
    return false;
}

bool isAllowedUrl(const QUrl &url)
{
    if (!url.isValid() || url.host().isEmpty()) {
        return false;
    }
    if (url.scheme() == QLatin1String("https")) {
        return true;
    }
    if (url.scheme() != QLatin1String("http")) {
        return false;
    }
    if (url.host().compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    const QHostAddress address(url.host());
    return !address.isNull() && address.isLoopback();
}

QVariantMap planSnapshot(const QVariantMap &plan,
                         double used,
                         double total,
                         bool valid,
                         const QString &resetText = {},
                         const QString &reason = {})
{
    return {
        {QStringLiteral("planId"), plan.value(QStringLiteral("id")).toString()},
        {QStringLiteral("planName"), plan.value(QStringLiteral("planName")).toString()},
        {QStringLiteral("used"), used},
        {QStringLiteral("total"), total},
        {QStringLiteral("unit"), plan.value(QStringLiteral("unit")).toString()},
        {QStringLiteral("resetText"), resetText},
        {QStringLiteral("extraText"), QString()},
        {QStringLiteral("isValid"), valid},
        {QStringLiteral("invalidReason"), reason},
    };
}
}

CustomUsageClient::CustomUsageClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

QVariantList CustomUsageClient::snapshots() const
{
    return m_snapshots;
}

bool CustomUsageClient::loading() const
{
    return m_loading;
}

void CustomUsageClient::refresh(const QVariantList &definitions)
{
    if (m_loading) {
        m_pendingDefinitions = definitions;
        m_refreshPending = true;
        return;
    }
    beginRefresh(definitions);
}

void CustomUsageClient::beginRefresh(const QVariantList &definitions)
{
    m_snapshots.clear();
    m_jobs.clear();
    for (const QVariant &item : definitions) {
        const QVariantMap definition = item.toMap();
        if (definition.value(QStringLiteral("catalogId")).toString()
                == QLatin1String("custom")
            && hasHttpPlans(definition)) {
            m_jobs.enqueue(definition);
        }
    }

    if (!m_jobs.isEmpty()) {
        m_loading = true;
        Q_EMIT loadingChanged();
    }
    startNextProvider();
}

void CustomUsageClient::startNextProvider()
{
    if (m_jobs.isEmpty()) {
        finishRefresh();
        return;
    }

    m_currentDefinition = m_jobs.dequeue();
    const QString script = m_currentDefinition.value(QStringLiteral("script")).toString();
    if (script.trimmed().isEmpty()) {
        failCurrent(QStringLiteral("查询脚本不能为空"));
        return;
    }
    startWorker(WorkerStage::Request,
                {{QStringLiteral("mode"), QStringLiteral("request")},
                 {QStringLiteral("script"), script}});
}

void CustomUsageClient::startWorker(WorkerStage stage, const QJsonObject &task)
{
    QProcess *worker = new QProcess(this);
    m_worker = worker;
    m_workerStage = stage;
    worker->setProgram(QStringLiteral(QUOTA_PILOT_SCRIPT_WORKER_PATH));
    worker->setProcessChannelMode(QProcess::SeparateChannels);
    connect(worker,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this, worker](int, QProcess::ExitStatus) { handleWorkerFinished(worker); });
    connect(worker, &QProcess::errorOccurred, this, [this, worker](QProcess::ProcessError error) {
        if (m_worker == worker && error == QProcess::FailedToStart) {
            m_worker = nullptr;
            worker->deleteLater();
            failCurrent(QStringLiteral("脚本解析组件未安装"));
        }
    });
    worker->start();
    worker->write(QJsonDocument(task).toJson(QJsonDocument::Compact));
    worker->closeWriteChannel();
    QTimer::singleShot(3000, worker, [this, worker]() {
        if (m_worker == worker && worker->state() != QProcess::NotRunning) {
            worker->setProperty("quotaPilotTimedOut", true);
            worker->kill();
        }
    });
}

void CustomUsageClient::handleWorkerFinished(QProcess *worker)
{
    if (m_worker != worker) {
        worker->deleteLater();
        return;
    }
    m_worker = nullptr;
    const WorkerStage stage = m_workerStage;
    m_workerStage = WorkerStage::None;
    const bool timedOut = worker->property("quotaPilotTimedOut").toBool();
    const QByteArray output = worker->readAllStandardOutput();
    const bool cleanExit = worker->exitStatus() == QProcess::NormalExit
        && worker->exitCode() == 0;
    worker->deleteLater();

    if (timedOut) {
        failCurrent(QStringLiteral("查询脚本执行超时"));
        return;
    }
    if (!cleanExit || output.size() > 512 * 1024) {
        failCurrent(QStringLiteral("查询脚本执行失败"));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument resultDocument = QJsonDocument::fromJson(output, &parseError);
    if (parseError.error != QJsonParseError::NoError || !resultDocument.isObject()) {
        failCurrent(QStringLiteral("查询脚本返回格式无效"));
        return;
    }
    const QJsonObject envelope = resultDocument.object();
    if (!envelope.value(QStringLiteral("ok")).toBool()) {
        const QString error = envelope.value(QStringLiteral("error")).toString();
        failCurrent(error.isEmpty() ? QStringLiteral("查询脚本执行失败") : error);
        return;
    }

    if (stage == WorkerStage::Request && envelope.value(QStringLiteral("value")).isObject()) {
        startNetworkRequest(envelope.value(QStringLiteral("value")).toObject().toVariantMap());
        return;
    }
    if (stage == WorkerStage::Extract && envelope.value(QStringLiteral("value")).isObject()) {
        finishCurrent(snapshotFromResult(
            m_currentDefinition,
            envelope.value(QStringLiteral("value")).toObject().toVariantMap()));
        return;
    }
    failCurrent(QStringLiteral("查询脚本返回格式无效"));
}

void CustomUsageClient::startNetworkRequest(const QVariantMap &configuration)
{
    const QUrl url(configuration.value(QStringLiteral("url")).toString());
    if (url.host().compare(QStringLiteral("example.com"), Qt::CaseInsensitive) == 0) {
        failCurrent(QStringLiteral("请先在脚本中配置真实用量接口"));
        return;
    }
    if (!isAllowedUrl(url)) {
        failCurrent(QStringLiteral("请求地址必须使用 HTTPS（本机地址除外）"));
        return;
    }
    const QByteArray method = configuration.value(QStringLiteral("method"), QStringLiteral("GET"))
                                  .toString().trimmed().toUpper().toLatin1();
    static const QList<QByteArray> allowedMethods{
        QByteArrayLiteral("GET"), QByteArrayLiteral("POST"), QByteArrayLiteral("PUT"),
        QByteArrayLiteral("PATCH"), QByteArrayLiteral("DELETE")};
    if (!allowedMethods.contains(method)) {
        failCurrent(QStringLiteral("请求方法不受支持"));
        return;
    }

    QNetworkRequest request(url);
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);
    request.setRawHeader("Accept", "application/json");
    const QVariantMap headers = configuration.value(QStringLiteral("headers")).toMap();
    for (auto it = headers.cbegin(); it != headers.cend(); ++it) {
        const QByteArray name = it.key().toLatin1().trimmed();
        const QByteArray value = it.value().toString().toUtf8();
        if (name.isEmpty() || name.contains('\n') || name.contains('\r')
            || value.contains('\n') || value.contains('\r')) {
            failCurrent(QStringLiteral("请求头格式无效"));
            return;
        }
        request.setRawHeader(name, value);
    }
    const QByteArray body = configuration.value(QStringLiteral("body")).toString().toUtf8();
    if (body.size() > 64 * 1024) {
        failCurrent(QStringLiteral("请求体过大"));
        return;
    }

    QNetworkReply *reply = method == QByteArrayLiteral("GET")
        ? m_network->get(request)
        : m_network->sendCustomRequest(request, method, body);
    m_reply = reply;
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
        if (received > maximumResponseBytes) {
            reply->setProperty("quotaPilotResponseTooLarge", true);
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleNetworkFinished(reply);
    });
}

void CustomUsageClient::handleNetworkFinished(QNetworkReply *reply)
{
    if (m_reply != reply) {
        reply->deleteLater();
        return;
    }
    m_reply = nullptr;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool tooLarge = reply->property("quotaPilotResponseTooLarge").toBool();
    const QByteArray payload = reply->read(maximumResponseBytes + 1);
    const QNetworkReply::NetworkError networkError = reply->error();
    reply->deleteLater();

    if (tooLarge || payload.size() > maximumResponseBytes) {
        failCurrent(QStringLiteral("服务响应过大，已停止解析"));
        return;
    }
    if (networkError != QNetworkReply::NoError || status < 200 || status >= 300) {
        failCurrent(status > 0
                        ? QStringLiteral("用量服务请求失败（HTTP %1）").arg(status)
                        : QStringLiteral("无法连接用量服务"));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument response = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError
        || (!response.isObject() && !response.isArray())) {
        failCurrent(QStringLiteral("用量服务未返回有效 JSON"));
        return;
    }
    const QJsonValue responseValue = response.isObject()
        ? QJsonValue(response.object()) : QJsonValue(response.array());
    startWorker(WorkerStage::Extract,
                {{QStringLiteral("mode"), QStringLiteral("extract")},
                 {QStringLiteral("script"),
                  m_currentDefinition.value(QStringLiteral("script")).toString()},
                 {QStringLiteral("response"), responseValue}});
}

QVariantMap CustomUsageClient::snapshotFromResult(const QVariantMap &definition,
                                                  const QVariantMap &result)
{
    QVariantList snapshots;
    int validCount = 0;
    const QVariantList plans = definition.value(QStringLiteral("plans")).toList();
    for (const QVariant &item : plans) {
        const QVariantMap plan = item.toMap();
        const QString sourceType = plan.value(QStringLiteral("sourceType")).toString();
        QVariant usedValue;
        QVariant totalValue;
        QString resetText;
        if (sourceType == QLatin1String("manual")) {
            usedValue = plan.value(QStringLiteral("manualUsed"));
            totalValue = plan.value(QStringLiteral("limit"));
            resetText = plan.value(QStringLiteral("resetText")).toString().trimmed();
        } else {
            usedValue = result.value(variableName(
                plan.value(QStringLiteral("usedVariable")).toString()));
            totalValue = result.value(variableName(
                plan.value(QStringLiteral("limitVariable")).toString()));
            const QString resetName = variableName(
                plan.value(QStringLiteral("resetVariable")).toString());
            if (!resetName.isEmpty()) {
                resetText = result.value(resetName).toString().trimmed();
            }
        }
        const double used = usedValue.toDouble();
        const double total = totalValue.toDouble();
        const bool valid = isNumeric(usedValue) && isNumeric(totalValue)
            && std::isfinite(used) && std::isfinite(total) && used >= 0 && total > 0;
        snapshots.push_back(planSnapshot(
            plan, used, total, valid, resetText,
            valid ? QString{} : QStringLiteral("脚本变量缺失或不是有效数字")));
        if (valid) {
            ++validCount;
        }
    }
    return {
        {QStringLiteral("providerId"), definition.value(QStringLiteral("id")).toString()},
        {QStringLiteral("statusLabel"), validCount > 0 ? QStringLiteral("可用")
                                                        : QStringLiteral("无有效数据")},
        {QStringLiteral("errorText"), validCount > 0 ? QString{}
                                                       : QStringLiteral("脚本未返回可用的限额变量")},
        {QStringLiteral("plans"), snapshots},
    };
}

QVariantMap CustomUsageClient::failedSnapshot(const QVariantMap &definition,
                                              const QString &message)
{
    QVariantList plans;
    const QVariantList definitions = definition.value(QStringLiteral("plans")).toList();
    for (const QVariant &item : definitions) {
        const QVariantMap plan = item.toMap();
        if (plan.value(QStringLiteral("sourceType")).toString() == QLatin1String("manual")) {
            const double used = plan.value(QStringLiteral("manualUsed")).toDouble();
            const double total = plan.value(QStringLiteral("limit")).toDouble();
            const bool valid = used >= 0 && total > 0;
            plans.push_back(planSnapshot(plan, used, total, valid));
        } else {
            plans.push_back(planSnapshot(plan, 0, 0, false, {}, message));
        }
    }
    return {
        {QStringLiteral("providerId"), definition.value(QStringLiteral("id")).toString()},
        {QStringLiteral("statusLabel"), QStringLiteral("查询失败")},
        {QStringLiteral("errorText"), message},
        {QStringLiteral("plans"), plans},
    };
}

void CustomUsageClient::failCurrent(const QString &message)
{
    finishCurrent(failedSnapshot(m_currentDefinition, message));
}

void CustomUsageClient::finishCurrent(const QVariantMap &snapshot)
{
    m_snapshots.push_back(snapshot);
    m_currentDefinition.clear();
    startNextProvider();
}

void CustomUsageClient::finishRefresh()
{
    Q_EMIT snapshotsChanged();
    if (m_loading) {
        m_loading = false;
        Q_EMIT loadingChanged();
    }
    if (m_refreshPending) {
        m_refreshPending = false;
        const QVariantList definitions = m_pendingDefinitions;
        m_pendingDefinitions.clear();
        beginRefresh(definitions);
    }
}
