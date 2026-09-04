// SPDX-License-Identifier: GPL-2.0-or-later
//
// KWallet worker —— Plasma 插件进程之外运行的独立可执行程序。
//
// 约束：
// - 一次进程只处理一个请求（read / save / clear），完成后立刻退出；
//   Plasma 进程内不再持有任何 KWallet::Wallet 实例，避免 kwalletd 阻塞拖死 plasmashell。
// - 通过受限 JSON 标准输入/输出通信，敏感字段不写入日志或 stderr。
// - 硬超时由 SIGALRM 兜底，父进程另有 QProcess 看门狗。
// - 旧 "AI Usage Watcher" 文件夹的条目按各 provider 现有语义一次性迁移到
//   AIQuotaPilot 文件夹，幂等。

#include <QCoreApplication>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QString>

#include <KWallet>

#include <signal.h>
#include <unistd.h>

namespace
{
// 所有客户端共用的钱包文件夹名（保持历史命名，向后兼容已有安装）。
const QString walletFolder = QStringLiteral("AIQuotaPilot");
// 项目改名之前的旧文件夹名（迁移来源）。
const QString legacyWalletFolder = QStringLiteral("AI Usage Watcher");

// Provider 标识 → KWallet 条目名称。保持与旧客户端一一对应，避免破坏现有用户数据。
QString entryNameFor(const QString &provider)
{
    if (provider == QLatin1String("minimax")) {
        return QStringLiteral("MiniMax API Key");
    }
    if (provider == QLatin1String("deepseek")) {
        return QStringLiteral("deepseek-api-key");
    }
    if (provider == QLatin1String("codexzh")) {
        return QStringLiteral("CodexZH API Key");
    }
    if (provider == QLatin1String("opencodego")) {
        return QStringLiteral("opencode-go-credential");
    }
    if (provider == QLatin1String("commandcode")) {
        return QStringLiteral("command-code-cookie");
    }
    if (provider == QLatin1String("agnes-ai")) {
        return QStringLiteral("agnes-ai-api-key");
    }
    return {};
}

// OpenCodeGo 把 workspaceId + cookie 序列化为 JSON 写入二进制条目，其余用密码条目。
bool providerUsesBinaryEntry(const QString &provider)
{
    return provider == QLatin1String("opencodego");
}

// 项目改名后需要把旧文件夹里的条目迁过来；保持原迁移触发条件。
bool providerHasMigration(const QString &provider)
{
    return provider == QLatin1String("minimax") || provider == QLatin1String("codexzh");
}

void writeJsonStdout(const QJsonObject &object)
{
    QFile output;
    if (!output.open(stdout, QIODevice::WriteOnly)) {
        return;
    }
    output.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    output.write("\n");
}

QJsonObject failure(const QString &errorCode)
{
    return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), errorCode}};
}

QJsonObject success(const QJsonValue &value = QJsonValue::Null, bool migrated = false)
{
    QJsonObject object{
        {QStringLiteral("ok"), true},
        {QStringLiteral("value"), value},
    };
    if (migrated) {
        object.insert(QStringLiteral("migrated"), true);
    }
    return object;
}

// 同步打开钱包并切到 AIQuotaPilot 文件夹；任何一步失败通过 errorCode 反馈给父进程。
bool openAndPrepare(KWallet::Wallet *&wallet, QString &errorCode)
{
    if (!KWallet::Wallet::isEnabled()) {
        errorCode = QStringLiteral("wallet_disabled");
        return false;
    }
    // 同步打开：worker 自己有 SIGALRM 兜底，父进程另有 QProcess 超时保护。
    KWallet::Wallet *opened = KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(),
                                                            0,
                                                            KWallet::Wallet::Synchronous);
    if (!opened || !opened->isOpen()) {
        if (opened) {
            opened->deleteLater();
        }
        errorCode = QStringLiteral("wallet_open_failed");
        return false;
    }
    if (!opened->hasFolder(walletFolder)) {
        if (!opened->createFolder(walletFolder)) {
            opened->deleteLater();
            errorCode = QStringLiteral("folder_create_failed");
            return false;
        }
    }
    if (!opened->setFolder(walletFolder)) {
        opened->deleteLater();
        errorCode = QStringLiteral("folder_select_failed");
        return false;
    }
    wallet = opened;
    return true;
}

// 旧文件夹条目迁移到新文件夹。返回 true 表示本次真的把旧条目搬过来并删除了。
bool runMigration(KWallet::Wallet *wallet, const QString &entryName)
{
    if (!wallet->hasFolder(legacyWalletFolder)) {
        return false;
    }
    if (!wallet->setFolder(legacyWalletFolder)) {
        return false;
    }
    QString legacyKey;
    const int readResult = wallet->readPassword(entryName, legacyKey);
    if (!wallet->setFolder(walletFolder)) {
        legacyKey.fill(QChar(u'\0'));
        return false;
    }
    if (readResult != 0 || legacyKey.trimmed().isEmpty()) {
        legacyKey.fill(QChar(u'\0'));
        return false;
    }
    QString currentKey;
    const bool alreadyPresent = wallet->readPassword(entryName, currentKey) == 0
        && !currentKey.isEmpty();
    currentKey.fill(QChar(u'\0'));
    if (alreadyPresent) {
        legacyKey.fill(QChar(u'\0'));
        return false;
    }
    const bool wrote = wallet->writePassword(entryName, legacyKey) == 0;
    legacyKey.fill(QChar(u'\0'));
    if (!wrote) {
        return false;
    }
    if (wallet->setFolder(legacyWalletFolder)) {
        wallet->removeEntry(entryName);
        wallet->setFolder(walletFolder);
    }
    return true;
}

QJsonObject doRead(const QString &provider)
{
    const QString entryName = entryNameFor(provider);
    if (entryName.isEmpty()) {
        return failure(QStringLiteral("unknown_provider"));
    }
    KWallet::Wallet *wallet = nullptr;
    QString errorCode;
    if (!openAndPrepare(wallet, errorCode)) {
        return failure(errorCode);
    }

    bool migrated = false;
    if (providerHasMigration(provider)) {
        migrated = runMigration(wallet, entryName);
    }

    QJsonObject result;
    if (providerUsesBinaryEntry(provider)) {
        QByteArray payload;
        const int rc = wallet->readEntry(entryName, payload);
        wallet->deleteLater();
        if (rc != 0 || payload.isEmpty()) {
            return failure(QStringLiteral("not_found"));
        }
        result = success(QString::fromUtf8(payload), migrated);
    } else {
        QString value;
        const int rc = wallet->readPassword(entryName, value);
        wallet->deleteLater();
        if (rc != 0 || value.trimmed().isEmpty()) {
            value.fill(QChar(u'\0'));
            return failure(QStringLiteral("not_found"));
        }
        result = success(value, migrated);
        value.fill(QChar(u'\0'));
    }
    return result;
}

QJsonObject doSave(const QString &provider, const QString &value)
{
    const QString entryName = entryNameFor(provider);
    if (entryName.isEmpty()) {
        return failure(QStringLiteral("unknown_provider"));
    }
    KWallet::Wallet *wallet = nullptr;
    QString errorCode;
    if (!openAndPrepare(wallet, errorCode)) {
        return failure(errorCode);
    }

    bool ok = false;
    if (providerUsesBinaryEntry(provider)) {
        ok = wallet->writeEntry(entryName, value.toUtf8()) == 0;
    } else {
        ok = wallet->writePassword(entryName, value) == 0;
    }
    wallet->deleteLater();
    return ok ? success() : failure(QStringLiteral("io_error"));
}

QJsonObject doClear(const QString &provider)
{
    const QString entryName = entryNameFor(provider);
    if (entryName.isEmpty()) {
        return failure(QStringLiteral("unknown_provider"));
    }
    KWallet::Wallet *wallet = nullptr;
    QString errorCode;
    if (!openAndPrepare(wallet, errorCode)) {
        return failure(errorCode);
    }
    const int rc = wallet->hasEntry(entryName) ? wallet->removeEntry(entryName) : 0;
    wallet->deleteLater();
    return rc == 0 ? success() : failure(QStringLiteral("io_error"));
}

void alarmHandler(int)
{
    // SIGALRM 触发立即退出，避免钱包同步调用把 worker 拖死。
    // 退出码 2 与父进程"超时"语义对应。
    _exit(2);
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);

    // 硬超时：覆盖 kwalletd 完全卡死的情况；父进程另有 QProcess 看门狗抢先 kill。
    struct sigaction action = {};
    action.sa_handler = alarmHandler;
    sigaction(SIGALRM, &action, nullptr);
    alarm(5);

    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly)) {
        writeJsonStdout(failure(QStringLiteral("stdin_unreadable")));
        return 1;
    }
    const QByteArray payload = input.readAll();
    if (payload.size() > 64 * 1024) {
        writeJsonStdout(failure(QStringLiteral("request_too_large")));
        return 1;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        writeJsonStdout(failure(QStringLiteral("invalid_json")));
        return 1;
    }

    const QJsonObject request = document.object();
    const QString op = request.value(QStringLiteral("op")).toString();
    const QString provider = request.value(QStringLiteral("provider")).toString();

    QJsonObject response;
    if (op == QLatin1String("read")) {
        response = doRead(provider);
    } else if (op == QLatin1String("save")) {
        response = doSave(provider, request.value(QStringLiteral("value")).toString());
    } else if (op == QLatin1String("clear")) {
        response = doClear(provider);
    } else {
        response = failure(QStringLiteral("unknown_op"));
    }

    writeJsonStdout(response);
    return response.value(QStringLiteral("ok")).toBool() ? 0 : 1;
}
