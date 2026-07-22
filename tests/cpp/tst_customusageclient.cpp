// SPDX-License-Identifier: GPL-2.0-or-later

#include "customusageclient.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

class CustomUsageClientTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void mapsExtractorVariablesToConfiguredPlans();
    void rejectsMissingOrNonNumericVariables();
    void exposesRefreshContract();
    void queriesAndMapsLocalJsonEndpoint();
};

QVariantMap definition()
{
    return {
        {QStringLiteral("catalogId"), QStringLiteral("custom")},
        {QStringLiteral("id"), QStringLiteral("token-hub")},
        {QStringLiteral("plans"), QVariantList{
             QVariantMap{{QStringLiteral("id"), QStringLiteral("five-hours")},
                         {QStringLiteral("planName"), QStringLiteral("5 小时")},
                         {QStringLiteral("unit"), QStringLiteral("次")},
                         {QStringLiteral("sourceType"), QStringLiteral("http-js")},
                         {QStringLiteral("usedVariable"), QStringLiteral("${fiveHourUsed}")},
                         {QStringLiteral("limitVariable"), QStringLiteral("${fiveHourLimit}")},
                         {QStringLiteral("resetVariable"), QStringLiteral("${fiveHourReset}")}},
             QVariantMap{{QStringLiteral("id"), QStringLiteral("weekly")},
                         {QStringLiteral("planName"), QStringLiteral("每周")},
                         {QStringLiteral("unit"), QStringLiteral("次")},
                         {QStringLiteral("sourceType"), QStringLiteral("http-js")},
                         {QStringLiteral("usedVariable"), QStringLiteral("${weeklyUsed}")},
                         {QStringLiteral("limitVariable"), QStringLiteral("${weeklyLimit}")},
                         {QStringLiteral("resetVariable"), QStringLiteral("${weeklyReset}")}},
         }},
    };
}

void CustomUsageClientTest::mapsExtractorVariablesToConfiguredPlans()
{
    const QVariantMap result{
        {QStringLiteral("fiveHourUsed"), 18.0},
        {QStringLiteral("fiveHourLimit"), 100.0},
        {QStringLiteral("fiveHourReset"), QStringLiteral("07-22 20:00")},
        {QStringLiteral("weeklyUsed"), 43.0},
        {QStringLiteral("weeklyLimit"), 200.0},
        {QStringLiteral("weeklyReset"), QStringLiteral("07-27 00:00")},
    };

    const QVariantMap snapshot = CustomUsageClient::snapshotFromResult(definition(), result);
    const QVariantList plans = snapshot.value(QStringLiteral("plans")).toList();

    QCOMPARE(snapshot.value(QStringLiteral("providerId")).toString(), QStringLiteral("token-hub"));
    QCOMPARE(snapshot.value(QStringLiteral("statusLabel")).toString(), QStringLiteral("可用"));
    QCOMPARE(plans.size(), 2);
    QCOMPARE(plans.at(0).toMap().value(QStringLiteral("planName")).toString(),
             QStringLiteral("5 小时"));
    QCOMPARE(plans.at(0).toMap().value(QStringLiteral("used")).toDouble(), 18.0);
    QCOMPARE(plans.at(0).toMap().value(QStringLiteral("resetText")).toString(),
             QStringLiteral("07-22 20:00"));
    QCOMPARE(plans.at(1).toMap().value(QStringLiteral("total")).toDouble(), 200.0);
}

void CustomUsageClientTest::rejectsMissingOrNonNumericVariables()
{
    const QVariantMap snapshot = CustomUsageClient::snapshotFromResult(
        definition(),
        {{QStringLiteral("fiveHourUsed"), QStringLiteral("18")},
         {QStringLiteral("fiveHourLimit"), 100.0}});
    const QVariantList plans = snapshot.value(QStringLiteral("plans")).toList();

    QCOMPARE(snapshot.value(QStringLiteral("statusLabel")).toString(),
             QStringLiteral("无有效数据"));
    QVERIFY(!snapshot.value(QStringLiteral("errorText")).toString().isEmpty());
    QCOMPARE(plans.at(0).toMap().value(QStringLiteral("isValid")).toBool(), false);
    QCOMPARE(plans.at(1).toMap().value(QStringLiteral("isValid")).toBool(), false);
}

void CustomUsageClientTest::exposesRefreshContract()
{
    const QMetaObject &metaObject = CustomUsageClient::staticMetaObject;
    QVERIFY(metaObject.indexOfProperty("snapshots") >= 0);
    QVERIFY(metaObject.indexOfProperty("loading") >= 0);
    QVERIFY(metaObject.indexOfMethod("refresh(QVariantList)") >= 0);
}

void CustomUsageClientTest::queriesAndMapsLocalJsonEndpoint()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        QTcpSocket *socket = server.nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
            if (!socket->readAll().contains("\r\n\r\n")) {
                return;
            }
            const QByteArray body = R"({"data":{"used":37,"limit":100,"reset":"07-22 20:00"}})";
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                          + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            socket->disconnectFromHost();
        });
    });

    QVariantMap provider = definition();
    provider[QStringLiteral("script")] = QStringLiteral(
        "({ request: { url: 'http://127.0.0.1:%1/usage', method: 'GET', headers: {} }, "
        "extractor: function(response) { return { "
        "fiveHourUsed: response.data.used, fiveHourLimit: response.data.limit, "
        "fiveHourReset: response.data.reset, weeklyUsed: response.data.used, "
        "weeklyLimit: response.data.limit, weeklyReset: response.data.reset }; } })")
            .arg(server.serverPort());

    CustomUsageClient client;
    QSignalSpy snapshotsChanged(&client, &CustomUsageClient::snapshotsChanged);
    client.refresh({provider});

    QTRY_COMPARE_WITH_TIMEOUT(snapshotsChanged.count(), 1, 8000);
    QCOMPARE(client.snapshots().size(), 1);
    const QVariantMap snapshot = client.snapshots().first().toMap();
    QCOMPARE(snapshot.value(QStringLiteral("statusLabel")).toString(), QStringLiteral("可用"));
    QCOMPARE(snapshot.value(QStringLiteral("plans")).toList().first().toMap()
                 .value(QStringLiteral("used")).toDouble(),
             37.0);
    QCOMPARE(snapshot.value(QStringLiteral("plans")).toList().first().toMap()
                 .value(QStringLiteral("resetText")).toString(),
             QStringLiteral("07-22 20:00"));
}

QTEST_GUILESS_MAIN(CustomUsageClientTest)

#include "tst_customusageclient.moc"
