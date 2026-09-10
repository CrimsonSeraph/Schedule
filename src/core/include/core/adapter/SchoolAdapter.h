#pragma once

#include "core/model/ScheduleSnapshot.h"

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QUrl>

#include <memory>
#include <vector>

namespace Schedule {

    /**
     * @brief 教务适配器元信息。
     *
     * 适配器是**可选的本地导入器**：用户在界面上主动点击时才发起一次抓取，
     * 抓到内容后走与文件导入完全相同的“预览 → 冲突检测 → 合并”流程。
     */
    struct AdapterInfo {
        /** 唯一标识，如 `local-sample`、`generic-jwgl`。 */
        QString id;

        /** 展示名（界面下拉框使用）。 */
        QString name;

        /** 说明文本，应包含隐私提示（例如“仅本地主动触发，不保存密码”）。 */
        QString description;

        /** 课表数据来源地址：`http(s)://` 或本地文件路径。 */
        QString schedule_url;

        /** 需要用户先在 WebView 中登录的页面地址；为空表示无需登录。 */
        QString login_url;

        /** 是否需要会话（Cookie）。 */
        bool requires_session = false;

        /** 是否为实验性适配器（界面应提示用户自行确认数据正确性）。 */
        bool is_experimental = true;

        /** @return 元信息是否可用（至少要有 id、名称与数据地址）。 */
        bool is_valid() const;
    };

    /**
     * @brief 仅存在于**内存**中的适配器会话凭证。
     *
     * 硬性约束（本项目不做账号系统）：
     *  - **绝不接收、绝不保存明文密码**；
     *  - 凭证只来自 WebView 登录成功后由系统 Cookie 存储提供的 Cookie 串；
     *  - 任何情况下都不写入数据库、日志或导出文件，应用退出即丢失。
     */
    struct AdapterSession {
        /** Cookie 请求头，形如 `JSESSIONID=xxx; SERVERID=yyy`。 */
        QByteArray cookie_header;

        /** 可选的自定义 User-Agent（部分教务系统会校验）。 */
        QByteArray user_agent;

        /** 凭证创建时间（本地时间），用于提示用户凭证可能已过期。 */
        QDateTime created_at;

        /** @return 是否为空（没有任何 Cookie）。 */
        bool is_empty() const;

        /** @brief 立即擦除凭证内容（用户点击“退出登录”或导入完成后调用）。 */
        void clear();

        /** @return 距离创建时间的小时数；未设置时间返回 -1。 */
        double age_hours() const;
    };

    /**
     * @brief 数据抓取抽象：把“怎么拿到课表原文”从适配器逻辑中分离出来。
     *
     * 这样做的价值：
     *  - `core` 不依赖 `Qt6::Network`（只用到 `QUrl` / `QByteArray`）；
     *  - 单元测试可以注入返回固定字节的假实现，无需真实网络。
     */
    class IScheduleFetcher {
    public:
        virtual ~IScheduleFetcher() = default;

        /**
         * @brief 本抓取器是否认领该地址（按协议 / 路径形态判断）。
         *
         * 由实现自行声明能力，避免上层用 RTTI 猜类型。
         */
        virtual bool supports(const QUrl& url) const = 0;

        /**
         * @brief 抓取课表原文。
         * @param url           数据地址（http(s) 或本地文件）
         * @param session       会话凭证（可为空）
         * @param error_message 失败原因（面向用户的中文）
         * @return 响应体；失败返回空字节串
         */
        virtual QByteArray fetch(const QUrl& url, const AdapterSession& session, QString* error_message = nullptr) const = 0;
    };

    /**
     * @brief 教务适配器接口（**接口在 core，实现在 data，注册在 app**）。
     *
     * 生命周期约束：`fetch_schedule()` 只应在用户主动触发时调用一次，
     * 不做定时轮询、不做后台同步、不做增量合并——这三件事都属于“云同步”范畴，
     * 本项目明确不做。
     */
    class ISchoolAdapter {
    public:
        virtual ~ISchoolAdapter() = default;

        /** @return 适配器元信息。 */
        virtual AdapterInfo info() const = 0;

        /** @brief 更新数据地址 / 登录地址（由 app 层从设置中读取后注入）。 */
        virtual void set_endpoints(const QString& schedule_url, const QString& login_url) = 0;

        /** @return 是否能够处理该地址（用于“粘贴教务系统网址自动选择适配器”）。 */
        virtual bool can_handle(const QString& url) const = 0;

        /**
         * @return 上一次 `fetch_schedule()` 识别到的内容格式机器名（`json` / `csv` / `ics`）。
         *
         * 用字符串而非 `data` 层的枚举，是为了让接口留在 `core` 且不产生依赖方向问题。
         */
        virtual QString last_format() const = 0;

        /**
         * @brief 主动抓取并解析课表。
         *
         * @param session       会话凭证（仅内存；不接收密码）
         * @param out_snapshot  输出快照
         * @param error_message 失败原因（面向用户的中文）
         * @return 是否成功
         */
        virtual bool fetch_schedule(const AdapterSession& session, ScheduleSnapshot* out_snapshot, QString* error_message = nullptr) const = 0;
    };

    /**
     * @brief 适配器注册表：集中管理可用适配器。
     *
     * 纯逻辑、无网络与 UI 依赖；由 `app` 层在启动时构造具体适配器并注册。
     */
    class SchoolAdapterRegistry {
    public:
        SchoolAdapterRegistry() = default;

        ~SchoolAdapterRegistry();

        SchoolAdapterRegistry(const SchoolAdapterRegistry&) = delete;
        SchoolAdapterRegistry& operator=(const SchoolAdapterRegistry&) = delete;

        /** @brief 注册适配器（接管所有权）；同 id 重复注册时后注册者生效。 */
        void register_adapter(std::unique_ptr<ISchoolAdapter> adapter);

        /** @return 全部已注册适配器的元信息（注册顺序）。 */
        QList<AdapterInfo> adapters() const;

        /** @return 指定 id 的适配器；未找到返回 nullptr。 */
        const ISchoolAdapter* find(const QString& id) const;

        /** @return 第 index 个适配器；越界返回 nullptr。 */
        const ISchoolAdapter* at(int index) const;

        /** @return 已注册数量。 */
        int count() const;

        /** @brief 清空注册表。 */
        void clear();

    private:
        /** 已注册适配器。 */
        std::vector<std::unique_ptr<ISchoolAdapter>> m_adapters;
    };

} // namespace Schedule
