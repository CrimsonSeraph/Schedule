#include "core/adapter/SchoolAdapter.h"
#include "data/adapter/GenericSchoolAdapter.h"
#include "data/adapter/NetworkScheduleFetcher.h"

#include <QDir>
#include <QFile>
#include <QTest>

using Schedule::AdapterInfo;
using Schedule::AdapterSession;
using Schedule::GenericSchoolAdapter;
using Schedule::IScheduleFetcher;
using Schedule::LocalFileScheduleFetcher;
using Schedule::NetworkScheduleFetcher;
using Schedule::SchoolAdapterRegistry;

namespace {

    /** @return 样本文件目录（由 CMake 通过编译定义注入）。 */
    QString samples_dir() {
        return QStringLiteral(SCHEDULE_SAMPLES_DIR);
    }

    /**
     * @brief 测试替身：不联网、返回固定字节的抓取器。
     *
     * 这也正是 `IScheduleFetcher` 抽象存在的意义——适配器逻辑可以完全脱离网络测试。
     */
    class FakeFetcher : public IScheduleFetcher {
    public:
        FakeFetcher(QByteArray body, bool claims_network)
            : m_body(std::move(body))
            , m_claims_network(claims_network) {
        }

        bool supports(const QUrl& url) const override {
            Q_UNUSED(url);
            return m_claims_network;
        }

        QByteArray fetch(const QUrl& url, const AdapterSession& session, QString* error_message) const override {
            Q_UNUSED(url);
            Q_UNUSED(session);
            if (m_body.isEmpty()) {
                if (error_message) {
                    *error_message = QStringLiteral("假抓取器：没有内容");
                }
                return QByteArray();
            }
            if (error_message) {
                error_message->clear();
            }
            return m_body;
        }

    private:
        QByteArray m_body;
        bool m_claims_network;
    };

    /** @return 读取样本 JSON 的字节内容。 */
    QByteArray sample_json() {
        QFile file(QDir(samples_dir()).filePath(QStringLiteral("schedule_sample.json")));
        if (!file.open(QIODevice::ReadOnly)) {
            return QByteArray();
        }
        return file.readAll();
    }

    /** @return 构造一个使用假抓取器的适配器。 */
    std::unique_ptr<GenericSchoolAdapter> make_adapter(AdapterInfo info, QByteArray body, bool network = true) {
        std::vector<std::shared_ptr<IScheduleFetcher>> fetchers;
        fetchers.push_back(std::make_shared<FakeFetcher>(std::move(body), network));
        return std::make_unique<GenericSchoolAdapter>(std::move(info), std::move(fetchers));
    }

    /** @return 一个可用的适配器元信息。 */
    AdapterInfo make_info(bool requires_session = false) {
        AdapterInfo info;
        info.id = QStringLiteral("test-adapter");
        info.name = QStringLiteral("测试适配器");
        info.description = QStringLiteral("仅用于单元测试");
        info.schedule_url = QStringLiteral("https://example.invalid/schedule");
        info.login_url = QStringLiteral("https://example.invalid/login");
        info.requires_session = requires_session;
        return info;
    }

} // namespace

/**
 * @brief 教务适配器的单元测试：注册表、会话凭证、抓取与解析链路。
 *
 * **隐私断言**：测试中不出现任何密码字段；会话凭证只存在于 `AdapterSession` 内存对象中，
 * `clear()` 后必须立即为空。
 */
class TestSchoolAdapter : public QObject {
    Q_OBJECT

private slots:
    /** 元信息与会话凭证的基本语义。 */
    void validates_info_and_session();

    /** 注册表：注册 / 查找 / 下标访问 / 同 id 覆盖 / 清空。 */
    void manages_registry();

    /** 抓取器按协议认领地址。 */
    void fetchers_claim_matching_urls();

    /** 适配器能把 JSON 原文解析成课表。 */
    void parses_json_payload();

    /** 需要登录但会话为空时给出可操作的错误提示。 */
    void requires_session_when_configured();

    /** 未配置地址时提示去设置中填写。 */
    void requires_schedule_url();

    /** 抓取失败时透传错误。 */
    void propagates_fetch_errors();

    /** 本地文件抓取器可读取真实样本文件。 */
    void reads_local_sample_file();

    /** 可以按地址挑选匹配的抓取器（本地文件场景）。 */
    void picks_local_fetcher_for_file_url();
};

void TestSchoolAdapter::validates_info_and_session() {
    AdapterInfo info = make_info();
    QVERIFY(info.is_valid());

    AdapterInfo incomplete;
    QVERIFY(!incomplete.is_valid());
    incomplete.id = QStringLiteral("x");
    QVERIFY(!incomplete.is_valid());
    incomplete.name = QStringLiteral("名称");
    incomplete.schedule_url = QStringLiteral("https://example.invalid/s");
    QVERIFY(incomplete.is_valid());

    AdapterSession session;
    QVERIFY(session.is_empty());
    QCOMPARE(session.age_hours(), -1.0);

    session.cookie_header = "JSESSIONID=abc";
    session.created_at = QDateTime::currentDateTime();
    QVERIFY(!session.is_empty());
    QVERIFY(session.age_hours() >= 0.0);

    // 擦除后不得残留任何内容
    session.clear();
    QVERIFY(session.is_empty());
    QVERIFY(session.cookie_header.isEmpty());
    QVERIFY(!session.created_at.isValid());
}

void TestSchoolAdapter::manages_registry() {
    SchoolAdapterRegistry registry;
    QCOMPARE(registry.count(), 0);
    QVERIFY(registry.adapters().isEmpty());
    QVERIFY(registry.find(QStringLiteral("missing")) == nullptr);
    QVERIFY(registry.at(0) == nullptr);

    registry.register_adapter(make_adapter(make_info(), sample_json()));
    QCOMPARE(registry.count(), 1);
    QCOMPARE(registry.adapters().first().id, QStringLiteral("test-adapter"));
    QVERIFY(registry.find(QStringLiteral("test-adapter")) != nullptr);
    QVERIFY(registry.at(0) != nullptr);
    QVERIFY(registry.at(1) == nullptr);
    QVERIFY(registry.at(-1) == nullptr);

    // 同 id 覆盖而不是追加
    AdapterInfo renamed = make_info();
    renamed.name = QStringLiteral("测试适配器（改名）");
    registry.register_adapter(make_adapter(renamed, sample_json()));
    QCOMPARE(registry.count(), 1);
    QCOMPARE(registry.adapters().first().name, QStringLiteral("测试适配器（改名）"));

    registry.register_adapter(nullptr);
    QCOMPARE(registry.count(), 1);

    registry.clear();
    QCOMPARE(registry.count(), 0);
}

void TestSchoolAdapter::fetchers_claim_matching_urls() {
    NetworkScheduleFetcher network;
    LocalFileScheduleFetcher local;

    QVERIFY(network.supports(QUrl(QStringLiteral("https://example.invalid/a"))));
    QVERIFY(network.supports(QUrl(QStringLiteral("http://example.invalid/a"))));
    QVERIFY(!network.supports(QUrl::fromLocalFile(QStringLiteral("C:/tmp/a.json"))));

    QVERIFY(local.supports(QUrl::fromLocalFile(QStringLiteral("C:/tmp/a.json"))));
    QVERIFY(local.supports(QUrl(QStringLiteral("relative/a.json"))));
    QVERIFY(!local.supports(QUrl(QStringLiteral("https://example.invalid/a"))));
}

void TestSchoolAdapter::parses_json_payload() {
    auto adapter = make_adapter(make_info(), sample_json());

    AdapterSession session;
    QVERIFY(adapter->can_handle(QStringLiteral("https://example.invalid/schedule")));

    Schedule::ScheduleSnapshot snapshot;
    QString error;
    QVERIFY2(adapter->fetch_schedule(session, &snapshot, &error), qPrintable(error));
    QCOMPARE(snapshot.courses.size(), 4);
    QCOMPARE(snapshot.semester.total_weeks, 16);
    QCOMPARE(adapter->last_format(), QStringLiteral("json"));
}

void TestSchoolAdapter::requires_session_when_configured() {
    auto adapter = make_adapter(make_info(true), sample_json());

    Schedule::ScheduleSnapshot snapshot;
    QString error;
    QVERIFY(!adapter->fetch_schedule(AdapterSession(), &snapshot, &error));
    QVERIFY(error.contains(QStringLiteral("登录")));
    QVERIFY(error.contains(QStringLiteral("Cookie")));

    // 提供 Cookie 后即可正常抓取
    AdapterSession session;
    session.cookie_header = "JSESSIONID=abc";
    session.created_at = QDateTime::currentDateTime();
    QVERIFY2(adapter->fetch_schedule(session, &snapshot, &error), qPrintable(error));
    QCOMPARE(snapshot.courses.size(), 4);
}

void TestSchoolAdapter::requires_schedule_url() {
    AdapterInfo info = make_info();
    info.schedule_url.clear();
    auto adapter = make_adapter(info, sample_json());

    Schedule::ScheduleSnapshot snapshot;
    QString error;
    QVERIFY(!adapter->fetch_schedule(AdapterSession(), &snapshot, &error));
    QVERIFY(error.contains(QStringLiteral("配置")));

    // 通过 set_endpoints 补上地址后即可用
    adapter->set_endpoints(QStringLiteral("https://example.invalid/schedule"), QString());
    QVERIFY2(adapter->fetch_schedule(AdapterSession(), &snapshot, &error), qPrintable(error));
    QCOMPARE(adapter->info().schedule_url, QStringLiteral("https://example.invalid/schedule"));
}

void TestSchoolAdapter::propagates_fetch_errors() {
    auto adapter = make_adapter(make_info(), QByteArray());

    Schedule::ScheduleSnapshot snapshot;
    QString error;
    QVERIFY(!adapter->fetch_schedule(AdapterSession(), &snapshot, &error));
    QVERIFY(!error.isEmpty());
}

void TestSchoolAdapter::reads_local_sample_file() {
    const QString path = QDir(samples_dir()).filePath(QStringLiteral("schedule_sample.csv"));
    QVERIFY2(QFile::exists(path), qPrintable(path));

    LocalFileScheduleFetcher fetcher;
    QString error;
    const QByteArray body = fetcher.fetch(QUrl::fromLocalFile(path), AdapterSession(), &error);
    QVERIFY2(!body.isEmpty(), qPrintable(error));
    QVERIFY(body.startsWith("课程名称") || body.startsWith(QByteArray::fromHex("EFBBBF")));

    // 不存在的文件必须给出明确错误
    QVERIFY(fetcher.fetch(QUrl::fromLocalFile(path + QStringLiteral(".missing")), AdapterSession(), &error).isEmpty());
    QVERIFY(error.contains(QStringLiteral("不存在")));
}

void TestSchoolAdapter::picks_local_fetcher_for_file_url() {
    // 同时提供网络与本地抓取器，适配器应能为 file:// 地址挑到本地实现
    std::vector<std::shared_ptr<IScheduleFetcher>> fetchers;
    fetchers.push_back(std::make_shared<NetworkScheduleFetcher>());
    fetchers.push_back(std::make_shared<LocalFileScheduleFetcher>());

    AdapterInfo info = make_info();
    info.schedule_url = QDir(samples_dir()).filePath(QStringLiteral("schedule_sample.json"));
    GenericSchoolAdapter adapter(info, fetchers);

    Schedule::ScheduleSnapshot snapshot;
    QString error;
    QVERIFY2(adapter.fetch_schedule(AdapterSession(), &snapshot, &error), qPrintable(error));
    QCOMPARE(snapshot.courses.size(), 4);
}

QTEST_GUILESS_MAIN(TestSchoolAdapter)

#include "tst_school_adapter.moc"
