#include "data/adapter/NetworkScheduleFetcher.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace Schedule {

    namespace {

        /** @brief 统一设置错误。 */
        bool fail(QString* error_message, const QString& message) {
            if (error_message) {
                *error_message = message;
            }
            return false;
        }

    } // namespace

    NetworkScheduleFetcher::NetworkScheduleFetcher(int timeout_ms)
        : m_timeout_ms(qMax(1000, timeout_ms)) {
    }

    bool NetworkScheduleFetcher::is_network_url(const QUrl& url) {
        const QString scheme = url.scheme().toLower();
        return scheme == QStringLiteral("http") || scheme == QStringLiteral("https");
    }

    bool NetworkScheduleFetcher::supports(const QUrl& url) const {
        return is_network_url(url);
    }

    QByteArray NetworkScheduleFetcher::fetch(const QUrl& url, const AdapterSession& session, QString* error_message) const {
        if (!is_network_url(url)) {
            fail(error_message, QStringLiteral("不是 http(s) 地址：%1").arg(url.toString()));
            return QByteArray();
        }

        // QNetworkAccessManager 必须在有事件循环的线程中创建/使用（即主线程）
        QNetworkAccessManager manager;

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader,
            session.user_agent.isEmpty() ? QByteArray("ScheduleApp/1.0") : session.user_agent);
        request.setRawHeader("Accept", "application/json, text/csv, text/calendar, text/html;q=0.8, */*;q=0.5");
        if (!session.is_empty()) {
            // 只发送 Cookie：不发送账号、密码或任何长期凭证
            request.setRawHeader("Cookie", session.cookie_header);
        }
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

        QNetworkReply* reply = manager.get(request);

        QEventLoop loop;
        QTimer timeout_timer;
        timeout_timer.setSingleShot(true);
        bool timed_out = false;

        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeout_timer, &QTimer::timeout, &loop, [&loop, &timed_out]() {
            timed_out = true;
            loop.quit();
        });

        timeout_timer.start(m_timeout_ms);
        loop.exec();

        QByteArray body;
        if (timed_out) {
            reply->abort();
            reply->deleteLater();
            fail(error_message, QStringLiteral("请求超时（%1 毫秒）：%2").arg(m_timeout_ms).arg(url.toString()));
            return body;
        }

        if (reply->error() != QNetworkReply::NoError) {
            const QString reason = reply->errorString();
            reply->deleteLater();
            fail(error_message, QStringLiteral("网络请求失败：%1（%2）").arg(reason, url.toString()));
            return body;
        }

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        body = reply->readAll();
        reply->deleteLater();

        if (status >= 400) {
            fail(error_message,
                QStringLiteral("教务系统返回 HTTP %1；若提示未登录，请重新在 WebView 中登录并更新 Cookie").arg(status));
            return QByteArray();
        }
        if (body.isEmpty()) {
            fail(error_message, QStringLiteral("教务系统返回内容为空（可能需要有效的登录 Cookie）"));
            return body;
        }

        if (error_message) {
            error_message->clear();
        }
        return body;
    }

} // namespace Schedule
