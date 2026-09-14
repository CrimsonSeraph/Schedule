#include "data/import_export/academic_affairs/DocConvertUtil.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using Schedule::DocConvertUtil;

namespace {

    /**
     * @return 一份最小的 Word 版式 HTML（`.doc` 的常见真实形态）。
     *
     * 教务系统「导出」出的 `.doc` 多数就是这种“Word 另存为网页”的产物：
     * 声明了 `urn:schemas-microsoft-com:office:word` 命名空间，Word / LibreOffice
     * 都能当作文档打开。用它当输入，就不必把真实课表样本提交进仓库。
     */
    QByteArray word_html_document() {
        return QByteArray(
            "<html xmlns:w=\"urn:schemas-microsoft-com:office:word\">"
            "<head><meta charset=\"utf-8\"><title>t</title></head>"
            "<body><table><tr><td>1-2节</td><td>高等数学<br />张三 @A101<br />1-8 1,2</td></tr></table></body>"
            "</html>");
    }

    /** @brief 在临时目录里落一份 `.doc`，返回其路径。 */
    QString write_sample_doc(const QDir& directory) {
        const QString path = directory.filePath(QStringLiteral("sample.doc"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return QString();
        }
        file.write(word_html_document());
        file.close();
        return path;
    }

    /**
     * @return 一个可用的假后端：把源文件复制成 `.docx` 名字，模拟“转换成功”。
     *
     * @param name         机器名
     * @param display_name 中文名；面向用户的提示里出现的应该是它
     * @param succeeds     是否会成功
     */
    DocConvertUtil::Backend fake_backend(const QString& name, const QString& display_name, bool succeeds) {
        DocConvertUtil::Backend backend;
        backend.name = name;
        backend.display_name = display_name;
        backend.available = []() { return true; };
        backend.convert = [succeeds, name](const QString& source_path, const QString& output_dir, QString* error_message) {
            if (!succeeds) {
                if (error_message) {
                    *error_message = QStringLiteral("%1 故意失败").arg(name);
                }
                return QString();
            }
            const QString target = QDir(output_dir).filePath(QFileInfo(source_path).completeBaseName() + QStringLiteral(".docx"));
            if (!QFile::copy(source_path, target)) {
                QFile::remove(target);
                if (!QFile::copy(source_path, target)) {
                    if (error_message) {
                        *error_message = QStringLiteral("假后端复制失败");
                    }
                    return QString();
                }
            }
            return target;
        };
        return backend;
    }

} // namespace

/**
 * @brief `.doc` → `.docx` 转换工具测试。
 *
 * 覆盖两类行为：
 *  - **纯逻辑**：入参校验、后端优先级与回退、全部不可用时的提示文本
 *    （用注入的假后端，不需要真的装 Word / LibreOffice）；
 *  - **真实转换**：本机装了 Word 或 LibreOffice 时，拿一份 Word 版式 HTML
 *    实际转一次，确认产物是合法的 OOXML 包；两者都没有则 `QSKIP`。
 */
class TestDocConvert : public QObject {
    Q_OBJECT

private slots:
    /** 清理注入的假后端，避免状态泄漏到下一个用例。 */
    void cleanup();

    /** 内置后端的优先级是 Word → LibreOffice。 */
    void default_backends_are_ordered();

    /** 源文件不存在时明确失败，且不依赖任何后端。 */
    void missing_source_reports_error();

    /** 输出目录必须由调用方给出。 */
    void missing_output_dir_reports_error();

    /** 一个后端都没有时，提示要装什么、以及可以改用 .docx。 */
    void no_backend_explains_what_to_install();

    /** 首个可用后端失败时自动回退到下一个。 */
    void falls_back_to_next_backend();

    /** 注入的后端全部可用且首个成功时，直接采用首个。 */
    void uses_first_available_backend();

    /** OOXML 包识别：Word 版式 HTML 不是，转换产物是。 */
    void detects_ooxml_package();

    /** 本机有后端时真实转换一次。 */
    void converts_real_doc_when_backend_present();
};

void TestDocConvert::cleanup() {
    DocConvertUtil::reset_backends();
}

void TestDocConvert::default_backends_are_ordered() {
    const QList<DocConvertUtil::Backend> backends = DocConvertUtil::default_backends();
    QCOMPARE(backends.size(), 2);
    QCOMPARE(backends.at(0).name, QStringLiteral("microsoft-word"));
    QCOMPARE(backends.at(1).name, QStringLiteral("libreoffice"));
    for (const DocConvertUtil::Backend& backend : backends) {
        QVERIFY(!backend.display_name.isEmpty());
        QVERIFY(static_cast<bool>(backend.available));
        QVERIFY(static_cast<bool>(backend.convert));
    }
}

void TestDocConvert::missing_source_reports_error() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const DocConvertUtil::Conversion result =
        DocConvertUtil::convert_to_docx(directory.filePath(QStringLiteral("not-there.doc")), directory.path());
    QVERIFY(!result.success);
    QVERIFY(result.error_message.contains(QStringLiteral("不存在")));
    QVERIFY(result.output_path.isEmpty());
}

void TestDocConvert::missing_output_dir_reports_error() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString source = write_sample_doc(QDir(directory.path()));
    QVERIFY(!source.isEmpty());

    // 不给输出目录：产物会随函数的临时目录一起消失，因此必须显式要求
    const DocConvertUtil::Conversion result = DocConvertUtil::convert_to_docx(source, QString());
    QVERIFY(!result.success);
    QVERIFY(result.error_message.contains(QStringLiteral("输出目录")));
}

void TestDocConvert::no_backend_explains_what_to_install() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString source = write_sample_doc(QDir(directory.path()));
    QVERIFY(!source.isEmpty());

    // 连一个后端都没有：提示里要出现「装什么」和「可以改用 .docx」
    DocConvertUtil::set_backends({});
    const DocConvertUtil::Conversion none = DocConvertUtil::convert_to_docx(source, directory.path());
    QVERIFY(!none.success);
    QVERIFY2(none.error_message.contains(QStringLiteral("LibreOffice")), qPrintable(none.error_message));
    QVERIFY2(none.error_message.contains(QStringLiteral(".docx")), qPrintable(none.error_message));
    QVERIFY(DocConvertUtil::available_backend_names().isEmpty());

    // 有后端但都不可用：提示里应点名缺的是哪一个
    DocConvertUtil::Backend unavailable = fake_backend(QStringLiteral("microsoft-word"), QStringLiteral("Microsoft Word"), true);
    unavailable.available = []() { return false; };
    DocConvertUtil::set_backends({unavailable});
    const DocConvertUtil::Conversion disabled = DocConvertUtil::convert_to_docx(source, directory.path());
    QVERIFY(!disabled.success);
    QVERIFY2(disabled.error_message.contains(QStringLiteral("Microsoft Word")), qPrintable(disabled.error_message));
}

void TestDocConvert::falls_back_to_next_backend() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString source = write_sample_doc(QDir(directory.path()));
    QVERIFY(!source.isEmpty());

    DocConvertUtil::set_backends({
        fake_backend(QStringLiteral("broken"), QStringLiteral("坏后端"), false),
        fake_backend(QStringLiteral("working"), QStringLiteral("好后端"), true),
    });

    const DocConvertUtil::Conversion result = DocConvertUtil::convert_to_docx(source, directory.path());
    QVERIFY2(result.success, qPrintable(result.error_message));
    QCOMPARE(result.backend, QStringLiteral("working"));
    QVERIFY(QFileInfo::exists(result.output_path));

    // 第一个后端失败的原因不能丢：否则用户无法判断是「没装」还是「装了但出错」
    DocConvertUtil::set_backends({fake_backend(QStringLiteral("broken"), QStringLiteral("坏后端"), false)});
    const DocConvertUtil::Conversion only_broken = DocConvertUtil::convert_to_docx(source, directory.path());
    QVERIFY(!only_broken.success);
    QVERIFY2(only_broken.error_message.contains(QStringLiteral("故意失败")), qPrintable(only_broken.error_message));
}

void TestDocConvert::uses_first_available_backend() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString source = write_sample_doc(QDir(directory.path()));
    QVERIFY(!source.isEmpty());

    DocConvertUtil::set_backends({
        fake_backend(QStringLiteral("first"), QStringLiteral("第一个"), true),
        fake_backend(QStringLiteral("second"), QStringLiteral("第二个"), true),
    });

    const DocConvertUtil::Conversion result = DocConvertUtil::convert_to_docx(source, directory.path());
    QVERIFY2(result.success, qPrintable(result.error_message));
    QCOMPARE(result.backend, QStringLiteral("first"));

    // 花括号初始化列表会被 QCOMPARE 宏当成多个参数，先落成一个具名变量
    const QStringList expected_names{QStringLiteral("第一个"), QStringLiteral("第二个")};
    QCOMPARE(DocConvertUtil::available_backend_names(), expected_names);
}

void TestDocConvert::detects_ooxml_package() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString source = write_sample_doc(QDir(directory.path()));
    QVERIFY(!source.isEmpty());

    // Word 版式 HTML 的 `.doc` 不是 zip 包
    QVERIFY(!DocConvertUtil::is_ooxml_package(source));
    QVERIFY(!DocConvertUtil::is_ooxml_package(directory.filePath(QStringLiteral("missing.docx"))));
}

void TestDocConvert::converts_real_doc_when_backend_present() {
    if (DocConvertUtil::available_backend_names().isEmpty()) {
        QSKIP("本机既没有 Microsoft Word 也没有 LibreOffice，跳过真实转换用例");
    }

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString source = write_sample_doc(QDir(directory.path()));
    QVERIFY(!source.isEmpty());

    const DocConvertUtil::Conversion result = DocConvertUtil::convert_to_docx(source, directory.path());
    QVERIFY2(result.success, qPrintable(result.error_message));
    QVERIFY(!result.backend.isEmpty());
    QCOMPARE(result.output_path, QDir(directory.path()).filePath(QStringLiteral("sample.docx")));
    QVERIFY(QFileInfo(result.output_path).size() > 0);

    // 真实产物的关键性质：是个 zip（OOXML 包），而不是被改了扩展名的 HTML
    QVERIFY(DocConvertUtil::is_ooxml_package(result.output_path));
}

QTEST_GUILESS_MAIN(TestDocConvert)

#include "tst_doc_convert.moc"
