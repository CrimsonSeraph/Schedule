#include "data/import_export/academic_affairs/DocConvertUtil.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#if defined(Q_OS_WIN)
#include <QSettings>
#endif

namespace Schedule {

    namespace {

        /** 单次转换的进程超时（毫秒）。首次启动 Word / LibreOffice 可能要十几秒，留足余量。 */
        constexpr int CONVERT_TIMEOUT_MS = 120000;

        /** 启动子进程本身的超时（毫秒）。 */
        constexpr int START_TIMEOUT_MS = 15000;

        /**
         * Word 的 `SaveAs` 格式常量：`wdFormatXMLDocument`。
         *
         * 注意不是 `wdFormatDocumentDefault`（16）——那个在 Word 2003 上会落到旧格式，
         * 12 才稳定产出 `.docx`。
         */
        constexpr int WORD_FORMAT_XML_DOCUMENT = 12;

        /**
         * 驱动 Word 的 VBScript。
         *
         * 约定：`WScript.Arguments(0)` 是源文件、`(1)` 是目标 `.docx`；
         * 退出码 0 表示成功。`Visible=False` + `DisplayAlerts=0` 是为了不在用户
         * 桌面上弹窗、也不因为“是否保留格式”之类的对话框卡住。
         */
        const char* const WORD_SAVE_SCRIPT =
            "Option Explicit\r\n"
            "Dim source_path, output_path, word, document, failed\r\n"
            "source_path = WScript.Arguments(0)\r\n"
            "output_path = WScript.Arguments(1)\r\n"
            "failed = False\r\n"
            "On Error Resume Next\r\n"
            "Set word = CreateObject(\"Word.Application\")\r\n"
            "If Err.Number <> 0 Then\r\n"
            "    WScript.StdErr.WriteLine \"CreateObject: \" & Err.Description\r\n"
            "    WScript.Quit 2\r\n"
            "End If\r\n"
            "Err.Clear\r\n"
            "word.Visible = False\r\n"
            "word.DisplayAlerts = 0\r\n"
            "Set document = word.Documents.Open(source_path, False, True)\r\n"
            "If Err.Number <> 0 Then\r\n"
            "    WScript.StdErr.WriteLine \"Open: \" & Err.Description\r\n"
            "    failed = True\r\n"
            "End If\r\n"
            "Err.Clear\r\n"
            "If Not failed Then\r\n"
            "    document.SaveAs output_path, 12\r\n"
            "    If Err.Number <> 0 Then\r\n"
            "        WScript.StdErr.WriteLine \"SaveAs: \" & Err.Description\r\n"
            "        failed = True\r\n"
            "    End If\r\n"
            "    Err.Clear\r\n"
            "    document.Close 0\r\n"
            "    Err.Clear\r\n"
            "End If\r\n"
            "word.Quit\r\n"
            "Err.Clear\r\n"
            "On Error GoTo 0\r\n"
            "If failed Then\r\n"
            "    WScript.Quit 3\r\n"
            "End If\r\n";

        /** @brief 统一设置错误。 */
        bool fail(QString* error_message, const QString& message) {
            if (error_message) {
                *error_message = message;
            }
            return false;
        }

        /** @return 由源文件推导出的 `.docx` 输出路径（与源文件同主名）。 */
        QString docx_path_for(const QString& source_path, const QString& output_dir) {
            return QDir(output_dir).filePath(QFileInfo(source_path).completeBaseName() + QStringLiteral(".docx"));
        }

        /**
         * @brief 起一个子进程并等到它结束。
         *
         * 子进程的输出按本地 8 位编码解释：`cscript` 与 `soffice` 在中文 Windows 上
         * 都按控制台代码页输出，这里只用于拼错误提示，不做严格解码。
         */
        bool run_process(const QString& program, const QStringList& arguments, QString* error_message) {
            QProcess process;
            process.setProgram(program);
            process.setArguments(arguments);
            process.setProcessChannelMode(QProcess::MergedChannels);
            process.start();

            if (!process.waitForStarted(START_TIMEOUT_MS)) {
                return fail(error_message, QStringLiteral("无法启动 %1：%2").arg(program, process.errorString()));
            }
            if (!process.waitForFinished(CONVERT_TIMEOUT_MS)) {
                process.kill();
                process.waitForFinished(START_TIMEOUT_MS);
                return fail(error_message,
                    QStringLiteral("%1 执行超时（超过 %2 秒）").arg(QFileInfo(program).fileName()).arg(CONVERT_TIMEOUT_MS / 1000));
            }
            if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
                const QString output = QString::fromLocal8Bit(process.readAll()).trimmed();
                const QString detail = output.isEmpty()
                    ? QStringLiteral("退出码 %1").arg(process.exitCode())
                    : output;
                return fail(error_message, QStringLiteral("%1 执行失败：%2").arg(QFileInfo(program).fileName(), detail));
            }
            return true;
        }

        /** @brief 用一段 VBScript 驱动本机 Word 完成转换。失败返回空串。 */
        QString convert_with_word(const QString& source_path, const QString& output_dir, QString* error_message) {
            const QString script_host = DocConvertUtil::find_script_host();
            if (script_host.isEmpty()) {
                fail(error_message, QStringLiteral("本机没有 cscript.exe，无法驱动 Word"));
                return QString();
            }

            // 脚本放进系统临时目录：调用方给出的 output_dir 只应该留下 .docx，
            // 不该混进实现细节。文件名带 pid，避免并发调用互相踩。
            const QString script_path = QDir(QDir::tempPath())
                                            .filePath(QStringLiteral("schedule-doc-convert-%1.vbs")
                                                          .arg(QCoreApplication::applicationPid()));
            QFile script(script_path);
            if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                fail(error_message, QStringLiteral("无法写入临时脚本 %1").arg(script_path));
                return QString();
            }
            script.write(WORD_SAVE_SCRIPT);
            script.close();

            const QString output_path = docx_path_for(source_path, output_dir);
            const bool launched = run_process(script_host,
                {QStringLiteral("//nologo"),
                    QDir::toNativeSeparators(script_path),
                    QDir::toNativeSeparators(source_path),
                    QDir::toNativeSeparators(output_path)},
                error_message);
            QFile::remove(script_path);

            if (!launched) {
                return QString();
            }
            if (!DocConvertUtil::is_ooxml_package(output_path)) {
                fail(error_message, QStringLiteral("Word 未生成有效的 .docx 文件"));
                return QString();
            }
            return output_path;
        }

        /** @brief 用 LibreOffice 完成转换。失败返回空串。 */
        QString convert_with_libreoffice(const QString& source_path, const QString& output_dir, QString* error_message) {
            const QString program = DocConvertUtil::find_libreoffice();
            if (program.isEmpty()) {
                fail(error_message, QStringLiteral("本机没有找到 soffice 可执行文件"));
                return QString();
            }

            // 独立的用户配置目录：用户自己开着 LibreOffice 时，共用 profile 会让
            // headless 实例拿不到锁而静默失败。
            const QString profile_dir = QDir(QDir::tempPath())
                                            .filePath(QStringLiteral("schedule-lo-profile-%1")
                                                          .arg(QCoreApplication::applicationPid()));

            const QString output_path = docx_path_for(source_path, output_dir);
            const bool launched = run_process(program,
                {QStringLiteral("--headless"),
                    QStringLiteral("--norestore"),
                    QStringLiteral("--nolockcheck"),
                    QStringLiteral("--nodefault"),
                    QStringLiteral("-env:UserInstallation=%1").arg(QUrl::fromLocalFile(profile_dir).toString()),
                    QStringLiteral("--convert-to"),
                    QStringLiteral("docx"),
                    QStringLiteral("--outdir"),
                    QDir::toNativeSeparators(output_dir),
                    QDir::toNativeSeparators(source_path)},
                error_message);
            QDir(profile_dir).removeRecursively();

            if (!launched) {
                return QString();
            }
            if (!DocConvertUtil::is_ooxml_package(output_path)) {
                fail(error_message, QStringLiteral("LibreOffice 未生成有效的 .docx 文件"));
                return QString();
            }
            return output_path;
        }

        /** @return 本机是否装了 Word（看 COM 注册项，不真的去启动它）。 */
        bool word_backend_available() {
#if defined(Q_OS_WIN)
            // 实例化一次 Word 要一秒以上、还会留下后台进程；注册项检查是廉价的替身。
            const QSettings registry(QStringLiteral("HKEY_CLASSES_ROOT\\Word.Application"), QSettings::NativeFormat);
            if (registry.childGroups().contains(QStringLiteral("CLSID"), Qt::CaseInsensitive)) {
                return true;
            }
            return !QStandardPaths::findExecutable(QStringLiteral("WINWORD.EXE")).isEmpty();
#else
            return false;
#endif
        }

        /** @return 本机是否有 LibreOffice。 */
        bool libreoffice_backend_available() {
            return !DocConvertUtil::find_libreoffice().isEmpty();
        }

    } // namespace

    QList<DocConvertUtil::Backend> DocConvertUtil::s_backends;

    bool DocConvertUtil::s_overridden = false;

    QList<DocConvertUtil::Backend> DocConvertUtil::default_backends() {
        Backend word;
        word.name = QStringLiteral("microsoft-word");
        word.display_name = QStringLiteral("Microsoft Word");
        word.available = &word_backend_available;
        word.convert = &convert_with_word;

        Backend libreoffice;
        libreoffice.name = QStringLiteral("libreoffice");
        libreoffice.display_name = QStringLiteral("LibreOffice");
        libreoffice.available = &libreoffice_backend_available;
        libreoffice.convert = &convert_with_libreoffice;

        return QList<Backend>{word, libreoffice};
    }

    QList<DocConvertUtil::Backend>& DocConvertUtil::active_backends() {
        if (!s_overridden) {
            s_backends = default_backends();
        }
        return s_backends;
    }

    void DocConvertUtil::set_backends(QList<Backend> backends) {
        s_backends = std::move(backends);
        s_overridden = true;
    }

    void DocConvertUtil::reset_backends() {
        s_backends.clear();
        s_overridden = false;
    }

    QStringList DocConvertUtil::available_backend_names() {
        QStringList names;
        for (const Backend& backend : active_backends()) {
            if (backend.available && backend.available()) {
                names.append(backend.display_name);
            }
        }
        return names;
    }

    QString DocConvertUtil::find_libreoffice() {
        const QString from_path = QStandardPaths::findExecutable(QStringLiteral("soffice"));
        if (!from_path.isEmpty()) {
            return from_path;
        }

#if defined(Q_OS_WIN)
        const QStringList candidates = {
            QStringLiteral("C:/Program Files/LibreOffice/program/soffice.exe"),
            QStringLiteral("C:/Program Files (x86)/LibreOffice/program/soffice.exe"),
        };
#elif defined(Q_OS_MACOS)
        const QStringList candidates = {
            QStringLiteral("/Applications/LibreOffice.app/Contents/MacOS/soffice"),
        };
#else
        const QStringList candidates = {
            QStringLiteral("/usr/bin/soffice"),
            QStringLiteral("/usr/lib/libreoffice/program/soffice"),
            QStringLiteral("/snap/bin/libreoffice"),
        };
#endif
        for (const QString& candidate : candidates) {
            if (QFileInfo::exists(candidate)) {
                return candidate;
            }
        }
        return QStandardPaths::findExecutable(QStringLiteral("libreoffice"));
    }

    QString DocConvertUtil::find_script_host() {
#if defined(Q_OS_WIN)
        const QString from_path = QStandardPaths::findExecutable(QStringLiteral("cscript"));
        if (!from_path.isEmpty()) {
            return from_path;
        }
        const QString system_root = qEnvironmentVariable("SystemRoot");
        if (!system_root.isEmpty()) {
            const QString candidate = QDir(system_root).filePath(QStringLiteral("System32/cscript.exe"));
            if (QFileInfo::exists(candidate)) {
                return candidate;
            }
        }
#endif
        return QString();
    }

    bool DocConvertUtil::is_ooxml_package(const QString& file_path) {
        QFile file(file_path);
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        // zip 本地文件头魔数；.docx / .xlsx / .pptx 都是 zip 包
        const QByteArray magic = file.read(4);
        file.close();
        return magic.startsWith(QByteArray::fromHex("504B0304"));
    }

    DocConvertUtil::Conversion DocConvertUtil::run_backend(const Backend& backend,
        const QString& source_path,
        const QString& output_dir) {
        Conversion result;
        result.backend = backend.name;

        if (!backend.convert) {
            result.error_message = QStringLiteral("%1 后端未提供转换实现").arg(backend.display_name);
            return result;
        }
        QString backend_error;
        const QString produced = backend.convert(source_path, output_dir, &backend_error);
        if (produced.isEmpty()) {
            result.error_message = backend_error.isEmpty()
                ? QStringLiteral("%1 未能完成转换").arg(backend.display_name)
                : QStringLiteral("%1：%2").arg(backend.display_name, backend_error);
            return result;
        }
        if (!QFileInfo::exists(produced)) {
            result.error_message = QStringLiteral("%1 报告成功，但没有生成 %2").arg(backend.display_name, produced);
            return result;
        }

        result.success = true;
        result.output_path = produced;
        return result;
    }

    DocConvertUtil::Conversion DocConvertUtil::convert_to_docx(const QString& source_path, const QString& output_dir) {
        Conversion result;

        const QFileInfo source_info(source_path);
        if (!source_info.exists() || !source_info.isFile()) {
            result.error_message = QStringLiteral("文件不存在：%1").arg(source_path);
            return result;
        }
        if (output_dir.isEmpty()) {
            // 不给默认临时目录：产物路径必须活得比本函数久，这件事只能由调用方保证
            result.error_message = QStringLiteral("必须指定输出目录：转换产物需要由调用方持有生命周期");
            return result;
        }
        if (!QDir(output_dir).exists()) {
            result.error_message = QStringLiteral("输出目录不存在：%1").arg(output_dir);
            return result;
        }

        QStringList unavailable;
        QStringList failures;

        for (const Backend& backend : active_backends()) {
            if (!backend.available || !backend.available()) {
                unavailable.append(backend.display_name);
                continue;
            }
            Conversion attempt = run_backend(backend, source_path, output_dir);
            if (attempt.success) {
                return attempt;
            }
            // 后端装了但这次没成功：继续试下一个，并把原因累积进最终提示
            failures.append(attempt.error_message);
        }

        if (!failures.isEmpty()) {
            result.error_message = QStringLiteral("转换 %1 失败：%2。可改用 LibreOffice 重试，或直接导入 .docx 格式的课表。")
                                       .arg(source_info.fileName(), failures.join(QStringLiteral("；")));
            return result;
        }

        const QString missing = unavailable.isEmpty() ? QStringLiteral("Microsoft Word / LibreOffice")
                                                      : unavailable.join(QStringLiteral(" / "));
        result.error_message = QStringLiteral("本机没有可用的 .doc 转换后端（需要 %1）。"
                                              "请任选其一：安装 LibreOffice 后重试；或先用办公软件另存为 .docx 再导入。")
                                   .arg(missing);
        return result;
    }

} // namespace Schedule
