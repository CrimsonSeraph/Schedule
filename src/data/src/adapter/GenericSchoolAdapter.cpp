#include "data/adapter/GenericSchoolAdapter.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

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

    // ------------------------------------------------------------ 本地文件抓取器

    bool LocalFileScheduleFetcher::supports(const QUrl& url) const {
        const QString scheme = url.scheme().toLower();
        // http(s) 交给网络抓取器；其余（file://、纯路径）由本地文件抓取器认领
        return scheme != QStringLiteral("http") && scheme != QStringLiteral("https");
    }

    QByteArray LocalFileScheduleFetcher::fetch(const QUrl& url, const AdapterSession& session, QString* error_message) const {
        Q_UNUSED(session);

        // 兼容三种写法：`file:///path`、`/abs/path`、`relative/path`
        QString path = url.toLocalFile();
        if (path.isEmpty()) {
            path = url.toString();
        }
        if (path.isEmpty()) {
            fail(error_message, QStringLiteral("课表数据地址为空"));
            return QByteArray();
        }

        QFile file(path);
        if (!file.exists()) {
            fail(error_message, QStringLiteral("课表数据文件不存在：%1").arg(path));
            return QByteArray();
        }
        if (!file.open(QIODevice::ReadOnly)) {
            fail(error_message, QStringLiteral("无法读取课表数据文件 %1：%2").arg(path, file.errorString()));
            return QByteArray();
        }

        if (error_message) {
            error_message->clear();
        }
        return file.readAll();
    }

    // -------------------------------------------------------------- 通用适配器

    GenericSchoolAdapter::GenericSchoolAdapter(AdapterInfo info, std::vector<std::shared_ptr<IScheduleFetcher>> fetchers)
        : m_info(std::move(info))
        , m_fetchers(std::move(fetchers)) {
    }

    GenericSchoolAdapter::~GenericSchoolAdapter() = default;

    AdapterInfo GenericSchoolAdapter::info() const {
        return m_info;
    }

    void GenericSchoolAdapter::set_endpoints(const QString& schedule_url, const QString& login_url) {
        m_info.schedule_url = schedule_url.trimmed();
        m_info.login_url = login_url.trimmed();
    }

    QString GenericSchoolAdapter::last_format() const {
        return m_last_format;
    }

    bool GenericSchoolAdapter::can_handle(const QString& url) const {
        const QUrl candidate(url.trimmed());
        if (candidate.isEmpty()) {
            return false;
        }
        if (candidate.isLocalFile() || candidate.scheme().isEmpty()) {
            // 本地路径：所有适配器都能处理，但优先级最低
            return true;
        }
        // 网络地址：只要与当前配置同源即认为可以处理
        const QUrl configured(m_info.schedule_url);
        return !configured.isValid() || configured.host() == candidate.host();
    }

    const IScheduleFetcher* GenericSchoolAdapter::fetcher_for(const QUrl& url) const {
        for (const auto& fetcher : m_fetchers) {
            if (fetcher && fetcher->supports(url)) {
                return fetcher.get();
            }
        }
        return nullptr;
    }

    bool GenericSchoolAdapter::fetch_schedule(const AdapterSession& session, ScheduleSnapshot* out_snapshot, QString* error_message) const {
        if (!out_snapshot) {
            return fail(error_message, QStringLiteral("输出参数为空"));
        }
        if (!m_info.is_valid()) {
            return fail(error_message,
                QStringLiteral("适配器“%1”尚未配置课表数据地址，请在设置中填写").arg(m_info.name));
        }
        if (m_info.requires_session && session.is_empty()) {
            return fail(error_message,
                QStringLiteral("适配器“%1”需要先登录：请在浏览器/WebView 中登录后把 Cookie 粘贴到设置页").arg(m_info.name));
        }

        const QUrl url(m_info.schedule_url);
        const IScheduleFetcher* fetcher = fetcher_for(url);
        if (!fetcher) {
            return fail(error_message, QStringLiteral("没有可用的抓取器处理地址：%1").arg(m_info.schedule_url));
        }

        QString fetch_error;
        const QByteArray body = fetcher->fetch(url, session, &fetch_error);
        if (body.isEmpty()) {
            return fail(error_message, fetch_error.isEmpty() ? QStringLiteral("抓取课表失败") : fetch_error);
        }

        // 复用已有的导入器：按内容嗅探 JSON / CSV / ICS，避免为每所学校写解析代码
        const IScheduleImporter* importer = m_import_manager.importer_for_content(body);
        if (!importer) {
            return fail(error_message, QStringLiteral("无法识别教务系统返回的内容格式（支持 JSON / CSV / ICS）"));
        }
        m_last_format = format_to_string(importer->format());

        ScheduleSnapshot parsed;
        QString parse_error;
        if (!importer->parse_data(body, m_info.schedule_url, &parsed, &parse_error)) {
            return fail(error_message, QStringLiteral("解析教务系统返回内容失败：%1").arg(parse_error));
        }

        // 适配器来源的课表通常不带学期名，这里补一个可辨识的名字
        if (parsed.semester.name.trimmed().isEmpty()) {
            parsed.semester.name = QStringLiteral("%1 导入").arg(m_info.name);
        }

        *out_snapshot = parsed;
        if (error_message) {
            error_message->clear();
        }
        return true;
    }

} // namespace Schedule
