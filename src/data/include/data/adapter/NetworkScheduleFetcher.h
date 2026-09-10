#pragma once

#include "core/adapter/SchoolAdapter.h"

namespace Schedule {

    /**
     * @brief HTTP(S) 抓取器：用 `QNetworkAccessManager` 发起一次同步 GET。
     *
     * 设计取舍：
     *  - **同步 + 超时**：适配器导入由用户主动点击触发，属于一次性前台操作，
     *    同步实现让上层无需处理异步状态机；超时（默认 15 秒）保证界面不会卡死；
     *  - **必须在主线程调用**：内部会启动局部事件循环等待响应；
     *  - **不发送任何账号密码**：只附加调用方传入的 Cookie 请求头。
     *
     * @note 本类只负责“把字节取回来”，不解析内容——解析由 `GenericSchoolAdapter`
     *       交给 `ImportManager` 完成。
     */
    class NetworkScheduleFetcher : public IScheduleFetcher {
    public:
        /** @param timeout_ms 单次请求超时（毫秒） */
        explicit NetworkScheduleFetcher(int timeout_ms = 15000);

        /** @return 该 URL 是否为 http(s) 协议。 */
        static bool is_network_url(const QUrl& url);

        bool supports(const QUrl& url) const override;

        QByteArray fetch(const QUrl& url, const AdapterSession& session, QString* error_message = nullptr) const override;

    private:
        /** 单次请求超时（毫秒）。 */
        int m_timeout_ms;
    };

} // namespace Schedule
