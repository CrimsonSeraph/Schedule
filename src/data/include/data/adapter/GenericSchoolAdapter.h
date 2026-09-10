#pragma once

#include "core/adapter/SchoolAdapter.h"
#include "data/import_export/ImportManager.h"

#include <QString>

#include <memory>
#include <vector>

namespace Schedule {

    /**
     * @brief 本地文件抓取器：从磁盘读取“课表原文”。
     *
     * 用途：
     *  - 离线演示与单元测试：无需真实教务系统即可跑通“抓取 → 解析 → 预览 → 合并”全链路；
     *  - 手工流程：用户从浏览器另存课表页面后再由适配器解析。
     *
     * @note 该实现忽略会话凭证；`GenericSchoolAdapter` 会按 URL 协议自动选择它。
     */
    class LocalFileScheduleFetcher : public IScheduleFetcher {
    public:
        bool supports(const QUrl& url) const override;

        QByteArray fetch(const QUrl& url, const AdapterSession& session, QString* error_message = nullptr) const override;
    };

    /**
     * @brief 通用教务适配器：抓取课表原文后交给已有的导入器解析。
     *
     * 设计要点：
     *  - **不解析教务系统的页面结构**，而是把响应体当作 JSON / CSV / ICS 之一，
     *    复用 `ImportManager` 的格式嗅探与解析器——这样各校差异只需在教务系统侧
     *    导出一个标准格式（或写一个薄薄的页面转换），不必为每所学校写一个 C++ 适配器；
     *  - 按 URL 协议自动选择抓取器：`http/https` 用网络抓取器，其余按本地文件读取；
     *  - `requires_session` 为真时，若会话为空会给出明确错误，引导用户先在 WebView 登录。
     *
     * **隐私约束**：不接收、不保存、不记录任何密码；凭证只以 Cookie 形式存在于内存，
     * 使用完毕后由调用方 `AdapterSession::clear()` 擦除。
     */
    class GenericSchoolAdapter : public ISchoolAdapter {
    public:
        /**
         * @param info     适配器元信息（id / name / schedule_url / login_url / requires_session）
         * @param fetchers 抓取器列表；按顺序取第一个“协议匹配”的实现
         */
        GenericSchoolAdapter(AdapterInfo info, std::vector<std::shared_ptr<IScheduleFetcher>> fetchers);

        ~GenericSchoolAdapter() override;

        AdapterInfo info() const override;

        void set_endpoints(const QString& schedule_url, const QString& login_url) override;

        bool can_handle(const QString& url) const override;

        QString last_format() const override;

        bool fetch_schedule(const AdapterSession& session, ScheduleSnapshot* out_snapshot, QString* error_message = nullptr) const override;

    private:
        /** @return 能处理该 URL 的抓取器；没有则返回 nullptr。 */
        const IScheduleFetcher* fetcher_for(const QUrl& url) const;

        /** 适配器元信息。 */
        AdapterInfo m_info;

        /** 可用抓取器。 */
        std::vector<std::shared_ptr<IScheduleFetcher>> m_fetchers;

        /** 复用的导入器：负责格式嗅探与解析。 */
        mutable ImportManager m_import_manager;

        /** 上一次识别到的格式机器名（`json` / `csv` / `ics`）。 */
        mutable QString m_last_format;
    };

} // namespace Schedule
