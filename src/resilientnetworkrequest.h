// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArray>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QString>

#include <functional>

class QNetworkAccessManager;
class QTimer;

/**
 * 弹性网络请求：把"一次 HTTP 调用"封装为可重试、有退避与抖动的单元。
 *
 * 设计目标：
 * 1. 调用方只关心最终结果；重试、指数退避、抖动、响应大小守卫都收敛在此处。
 * 2. 把错误分为可重试与不可重试：
 *    - 可重试：DNS / 连接 / TLS / 超时 / ProtocolFailure(399) / 5xx / 429
 *    - 不可重试：401 / 403 / 业务层 status_code != 0 / 响应过大
 * 3. 重试期间不回调调用方，仅最后一次结果（成功或重试耗尽）触发回调，
 *    这样大多数瞬时抖动不会污染 UI。
 * 4. 退避：基础延迟 × 2^attempt，夹在 [base, max] 之间，再加 ±30% 抖动，
 *    避免与同进程其它 provider 同步重试。
 * 5. 客户端主动 abort()：回调 outcome = Aborted，且仅触发一次。
 *
 * 用法：
 *   auto *req = new ResilientNetworkRequest(m_network, this);
 *   QPointer<MyClass> self = this;
 *   req->get(createRequest(url), [self, req](Result r) {
 *       req->deleteLater();
 *       if (!self) return;
 *       if (r.outcome == Outcome::Success) { ... }
 *       else { setError(r.errorMessage); }
 *   });
 *
 * 线程模型：所有回调都发生在 QNetworkAccessManager 所在的线程（plasmashell
 * 主线程），无锁。
 */
class ResilientNetworkRequest : public QObject
{
    Q_OBJECT

public:
    enum class Outcome {
        Success,                ///< 2xx 且已读取完整 payload
        NonRetryableFailure,    ///< 401/403/响应过大等，不再尝试
        RetryableFailure,       ///< 重试已耗尽，仍失败
        Aborted,                ///< 调用方主动取消
    };

    struct Result {
        Outcome outcome = Outcome::Success;
        int httpStatus = 0;
        QNetworkReply::NetworkError networkError = QNetworkReply::NoError;
        QByteArray payload;
        QString errorMessage;
        int attempts = 0;
        bool responseTooLarge = false;
        int retryAfterMs = 0;
    };

    using Callback = std::function<void(Result)>;

    explicit ResilientNetworkRequest(QNetworkAccessManager *nam, QObject *parent = nullptr);
    ~ResilientNetworkRequest() override;

    /// 发起一次 GET；立即返回，回调异步触发。
    void get(const QNetworkRequest &request, Callback callback);

    /// 发起一次 POST；立即返回，回调异步触发。
    void post(const QNetworkRequest &request, const QByteArray &body, Callback callback);

    /// 主动取消。回调会以 Outcome::Aborted 触发一次（若尚未触发）。
    void abort();

    bool isRunning() const { return m_reply != nullptr || m_retryTimer != nullptr; }

    /// 总尝试次数（含首次），默认 3。最低 1。
    void setMaxAttempts(int attempts) { m_maxAttempts = qMax(1, attempts); }

    /// 退避基数（毫秒），默认 1000。
    void setBaseDelayMs(int ms) { m_baseDelayMs = qMax(0, ms); }

    /// 单次退避上限（毫秒），默认 8000。
    void setMaxDelayMs(int ms) { m_maxDelayMs = qMax(m_baseDelayMs, ms); }

    /// 分类工具：网络层错误是否可重试。
    static bool isRetryableNetworkError(QNetworkReply::NetworkError error);

    /// 分类工具：HTTP 状态码是否可重试。
    static bool isRetryableHttpStatus(int status);

    /// 把 NetworkError 翻译为面向用户的中文短语；errorMessage 为空时调用方应使用回退文案。
    static QString describeNetworkError(QNetworkReply::NetworkError error);

    /// 单次响应体上限；超过即视为 NonRetryableFailure。
    static constexpr qsizetype maximumResponseBytes = 1024 * 1024;

private:
    void executeOnce();
    void onReplyFinished();
    void scheduleRetry();
    void deliver(Result result);
    int computeDelayMs(int attempt) const;
    void teardownReply();

    QNetworkAccessManager *m_nam;
    QNetworkReply *m_reply = nullptr;
    QTimer *m_retryTimer = nullptr;

    QNetworkRequest m_request;
    QByteArray m_body;
    bool m_isPost = false;
    bool m_aborted = false;
    bool m_delivered = false;

    int m_maxAttempts = 3;
    int m_baseDelayMs = 1000;
    int m_maxDelayMs = 8000;
    int m_attempts = 0;
    Callback m_callback;
};
