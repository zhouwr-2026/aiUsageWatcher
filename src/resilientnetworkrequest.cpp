// SPDX-License-Identifier: GPL-2.0-or-later

#include "resilientnetworkrequest.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QTimer>

ResilientNetworkRequest::ResilientNetworkRequest(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{
}

ResilientNetworkRequest::~ResilientNetworkRequest()
{
    teardownReply();
}

void ResilientNetworkRequest::get(const QNetworkRequest &request, Callback callback)
{
    m_request = request;
    m_body.clear();
    m_isPost = false;
    m_aborted = false;
    m_delivered = false;
    m_attempts = 0;
    m_callback = std::move(callback);
    executeOnce();
}

void ResilientNetworkRequest::post(const QNetworkRequest &request,
                                    const QByteArray &body,
                                    Callback callback)
{
    m_request = request;
    m_body = body;
    m_isPost = true;
    m_aborted = false;
    m_delivered = false;
    m_attempts = 0;
    m_callback = std::move(callback);
    executeOnce();
}

void ResilientNetworkRequest::abort()
{
    m_aborted = true;
    if (m_retryTimer) {
        m_retryTimer->stop();
        m_retryTimer->deleteLater();
        m_retryTimer = nullptr;
    }
    if (m_reply) {
        // QNetworkReply::abort() 会触发 finished()，我们在 onReplyFinished 里统一收口。
        m_reply->abort();
        return;
    }
    if (!m_delivered && m_callback) {
        deliver({Outcome::Aborted, 0, QNetworkReply::NoError, {},
                 QStringLiteral("请求已取消"), m_attempts, false});
    }
}

void ResilientNetworkRequest::executeOnce()
{
    Q_ASSERT(m_nam);
    if (!m_nam || m_aborted) {
        if (!m_delivered) {
            deliver({Outcome::Aborted, 0, QNetworkReply::NoError, {},
                     QStringLiteral("请求已取消"), m_attempts, false});
        }
        return;
    }

    ++m_attempts;
    QNetworkReply *reply = m_isPost
        ? m_nam->post(m_request, m_body)
        : m_nam->get(m_request);
    m_reply = reply;

    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
        if (received > maximumResponseBytes) {
            reply->setProperty("aiUsageWatcherResponseTooLarge", true);
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (m_reply == reply) {
            onReplyFinished();
        }
    });
}

void ResilientNetworkRequest::onReplyFinished()
{
    if (m_delivered || !m_reply) {
        return;
    }
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError error = reply->error();
    const bool tooLarge = reply->property("aiUsageWatcherResponseTooLarge").toBool();
    const int retryAfterMs = reply->rawHeader("Retry-After").toInt() * 1000;

    QByteArray payload;
    bool payloadTooLarge = tooLarge;
    if (!tooLarge && error == QNetworkReply::NoError && status >= 200 && status < 300) {
        payload = reply->read(maximumResponseBytes + 1);
        if (payload.size() > maximumResponseBytes) {
            payloadTooLarge = true;
        }
    }
    reply->deleteLater();

    // 主动 abort 的归一为 Aborted；不计入重试判定。
    if (m_aborted) {
        deliver({Outcome::Aborted, status, error, {},
                 QStringLiteral("请求已取消"), m_attempts, payloadTooLarge});
        return;
    }

    // 响应过大不可重试。
    if (payloadTooLarge) {
        deliver({Outcome::NonRetryableFailure, status, error, {},
                 QStringLiteral("响应过大，已拒绝处理"),
                 m_attempts, true});
        return;
    }

    // 2xx 成功。
    if (error == QNetworkReply::NoError && status >= 200 && status < 300) {
        deliver({Outcome::Success, status, error, payload, {}, m_attempts, false});
        return;
    }

    // 不可重试：401/403 等业务或认证错误。即使 NetworkError 报告为 ProtocolFailure
    // （服务端在拒绝时也可能报协议错误），HTTP 状态本身已经表达了"不要再来"的语义。
    if (!isRetryableHttpStatus(status)
        && !isRetryableNetworkError(error)
        && status != 0) {
        QString message = describeNetworkError(error);
        if (message.isEmpty()) {
            message = (status > 0)
                ? QStringLiteral("HTTP %1").arg(status)
                : QStringLiteral("请求失败");
        }
        deliver({Outcome::NonRetryableFailure, status, error, {},
                 message, m_attempts, false});
        return;
    }

    // HTTP 状态本身就是 4xx（且非 408/425/429），属于凭据/权限/资源问题，
    // 不应该被 retryable 网络错误带偏去重试。
    if (status >= 400 && status < 500
        && status != 408 && status != 425 && status != 429) {
        deliver({Outcome::NonRetryableFailure, status, error, {},
                 QStringLiteral("HTTP %1").arg(status),
                 m_attempts, false});
        return;
    }

    // 可重试；尝试次数耗尽则上报最终失败。
    if (m_attempts >= m_maxAttempts) {
        QString message = describeNetworkError(error);
        if (message.isEmpty()) {
            message = (status > 0)
                ? QStringLiteral("HTTP %1").arg(status)
                : QStringLiteral("请求失败");
        }
        deliver({Outcome::RetryableFailure, status, error, {},
                 message, m_attempts, false, retryAfterMs});
        return;
    }

    scheduleRetry();
}

void ResilientNetworkRequest::scheduleRetry()
{
    if (!m_retryTimer) {
        m_retryTimer = new QTimer(this);
        m_retryTimer->setSingleShot(true);
        connect(m_retryTimer, &QTimer::timeout, this, [this] {
            QTimer *timer = m_retryTimer;
            m_retryTimer = nullptr;
            timer->deleteLater();
            executeOnce();
        });
    }
    const int delay = computeDelayMs(m_attempts);
    m_retryTimer->start(delay);
}

void ResilientNetworkRequest::deliver(Result result)
{
    if (m_delivered) {
        return;
    }
    m_delivered = true;
    if (m_aborted) {
        result.outcome = Outcome::Aborted;
        if (result.errorMessage.isEmpty()) {
            result.errorMessage = QStringLiteral("请求已取消");
        }
    }
    Callback cb = std::move(m_callback);
    m_callback = nullptr;
    if (cb) {
        cb(std::move(result));
    }
}

int ResilientNetworkRequest::computeDelayMs(int attempt) const
{
    const int shift = qMin(qMax(0, attempt - 1), 5);
    const int multiplier = 1 << shift;
    int delay = m_baseDelayMs * multiplier;
    delay = qBound(m_baseDelayMs, delay, m_maxDelayMs);

    // ±30% 抖动，避免与同进程其它客户端对齐重试。
    const int jitterRange = delay * 3 / 10;
    if (jitterRange > 0) {
        const int jitter = QRandomGenerator::global()->bounded(-jitterRange, jitterRange + 1);
        delay = qMax(0, delay + jitter);
    }
    return delay;
}

void ResilientNetworkRequest::teardownReply()
{
    if (m_reply) {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->abort();
        reply->deleteLater();
    }
    if (m_retryTimer) {
        m_retryTimer->stop();
        m_retryTimer->deleteLater();
        m_retryTimer = nullptr;
    }
}

bool ResilientNetworkRequest::isRetryableNetworkError(QNetworkReply::NetworkError error)
{
    switch (error) {
    case QNetworkReply::NoError:
        return false;
    case QNetworkReply::TimeoutError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::SslHandshakeFailedError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::BackgroundRequestNotAllowedError:
    case QNetworkReply::TooManyRedirectsError:
    case QNetworkReply::InsecureRedirectError:
    case QNetworkReply::ProtocolFailure:           // 399
    case QNetworkReply::ServiceUnavailableError:   // 19
    case QNetworkReply::UnknownNetworkError:       // 99
    case QNetworkReply::UnknownContentError:       // 299
    case QNetworkReply::OperationCanceledError:    // 看门狗触发的取消
        return true;
    default:
        // 兜底：Qt 在不同版本会加入新的 enum 值；保守地视为可重试。
        return static_cast<int>(error) >= 100;
    }
}

bool ResilientNetworkRequest::isRetryableHttpStatus(int status)
{
    // 0 表示根本没有收到 HTTP 响应（连接/TLS/DNS 阶段就失败）。
    if (status == 0) {
        return true;
    }
    if (status == 408 || status == 425 || status == 429) {
        return true;
    }
    if (status >= 500 && status < 600) {
        return true;
    }
    return false;
}

QString ResilientNetworkRequest::describeNetworkError(QNetworkReply::NetworkError error)
{
    switch (error) {
    case QNetworkReply::NoError:
        return {};
    case QNetworkReply::TimeoutError:
        return QStringLiteral("请求超时");
    case QNetworkReply::HostNotFoundError:
        return QStringLiteral("无法解析服务地址");
    case QNetworkReply::ConnectionRefusedError:
        return QStringLiteral("服务拒绝连接");
    case QNetworkReply::RemoteHostClosedError:
        return QStringLiteral("服务关闭了连接");
    case QNetworkReply::SslHandshakeFailedError:
        return QStringLiteral("TLS 握手失败");
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
        return QStringLiteral("网络暂时不可用");
    case QNetworkReply::BackgroundRequestNotAllowedError:
        return QStringLiteral("后台请求受限");
    case QNetworkReply::TooManyRedirectsError:
        return QStringLiteral("重定向次数过多");
    case QNetworkReply::InsecureRedirectError:
        return QStringLiteral("不安全的重定向被拦截");
    case QNetworkReply::ProtocolFailure:
        return QStringLiteral("服务返回了无法识别的响应");
    case QNetworkReply::ServiceUnavailableError:
        return QStringLiteral("服务暂时不可用");
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::UnknownContentError:
        return QStringLiteral("网络暂时不可用");
    case QNetworkReply::OperationCanceledError:
        return QStringLiteral("请求已取消");
    default:
        // 兜底：避免直接抛出 enum 数值；用泛化文案。
        return QStringLiteral("网络暂时不可用");
    }
}
