#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace Schedule {

    /**
     * @brief 旧版 `.doc` → `.docx` 转换工具。
     *
     * ## 为什么需要它
     *
     * 教务系统「导出」出来的课表常常是 Word 97-2003 格式（`.doc`）。`.doc` 是 OLE
     * 复合文档，没有可用的开源解析路径；而 `.docx` 只是一个 zip，其中的
     * `word/document.xml` 可以直接读。于是“把 `.doc` 转成 `.docx`”就退化成
     * “调用一次现成的办公软件做格式转换”。
     *
     * ## 转换后端（按优先级）
     *
     * | 机器名 | 平台 | 做法 |
     * | --- | --- | --- |
     * | `microsoft-word` | Windows | `cscript` 驱动一段 VBScript，调用本机 Word 的 COM 接口 `SaveAs(..., 12)` |
     * | `libreoffice` | 全平台 | `soffice --headless --convert-to docx` |
     *
     * ### Word 后端为什么不用 `QAxObject`
     *
     * 本仓库使用的 Qt 套件（官方 MinGW / MSVC 安装包）**不包含 ActiveQt**
     * （`Qt6::AxContainer`），`QAxObject` 链接不上。改用 `QProcess` 驱动 `cscript`
     * 执行 VBScript：同样调用 Word 的 COM 接口，却不引入任何额外的 Qt 模块，
     * 在 MinGW 与 MSVC 下行为一致，也与 LibreOffice 后端共用同一套
     * 「起进程 → 等退出 → 校验产物」逻辑。若将来换用自带 ActiveQt 的 Qt 套件，
     * 只需通过 `set_backends()` 追加一个 `QAxObject` 版本的后端，本类其余部分不用动。
     *
     * ## 失败不静默
     *
     * 两个后端都不可用时，`convert_to_docx()` 返回的 `error_message` 会说明
     * **缺什么**（本机既没有 Word 也没有 LibreOffice）以及**怎么办**
     * （装一个 LibreOffice，或改用 `.docx` / 教务系统「导出为 docx」再导入）。
     */
    class DocConvertUtil {
    public:
        /** 单次转换的结果。 */
        struct Conversion {
            /** 是否成功。 */
            bool success = false;

            /** 产出的 `.docx` 绝对路径；失败时为空。 */
            QString output_path;

            /** 实际使用的后端机器名；失败时为空。 */
            QString backend;

            /** 失败原因（中文，面向用户）。 */
            QString error_message;
        };

        /**
         * @brief 一个可插拔的转换后端。
         *
         * 把后端做成数据而非硬编码，是为了让单元测试注入假后端：无需真装
         * Word / LibreOffice，就能覆盖「回退链」「全部不可用」这些分支。
         */
        struct Backend {
            /** 机器名，如 `microsoft-word`。 */
            QString name;

            /** 中文名，用于面向用户的提示。 */
            QString display_name;

            /** @return 本后端当前是否可用。 */
            std::function<bool()> available;

            /**
             * @brief 执行转换。
             * @param source_path   源 `.doc` 绝对路径
             * @param output_dir    输出目录（必须已存在且可写）
             * @param error_message 失败原因（中文）；成功时应保持为空
             * @return 产出的 `.docx` 绝对路径；失败返回空串
             */
            std::function<QString(const QString& source_path, const QString& output_dir, QString* error_message)> convert;
        };

        /** @return 内置后端列表（Word → LibreOffice），按优先级排列。 */
        static QList<Backend> default_backends();

        /** @return 当前可用后端的中文名清单；全部不可用时返回空。 */
        static QStringList available_backend_names();

        /**
         * @brief 把 `.doc` 转换为 `.docx`。
         *
         * @param source_path 源文件绝对路径
         * @param output_dir  输出目录；**必须显式给出**且由调用方持有生命周期
         *                    （例如调用方自己的 `QTemporaryDir`），
         *                    以免返回的路径在函数返回后就被删掉
         * @return 转换结果；`success == false` 时 `error_message` 为中文原因
         */
        static Conversion convert_to_docx(const QString& source_path, const QString& output_dir);

        /**
         * @brief 覆盖后端列表（**仅供测试**）。
         *
         * 调用方负责在测试结束时 `reset_backends()`，否则状态会泄漏到后续用例。
         */
        static void set_backends(QList<Backend> backends);

        /** @brief 恢复 `default_backends()`。 */
        static void reset_backends();

        /** @return LibreOffice 可执行文件路径；未找到返回空串。 */
        static QString find_libreoffice();

        /** @return Word 自动化脚本宿主（`cscript.exe`）路径；未找到返回空串。 */
        static QString find_script_host();

        /**
         * @brief 判断文件是否为 OOXML 包（即已经是 `.docx`）。
         *
         * 只读文件头 4 字节并比对 zip 魔数 `PK\x03\x04`，不打开 zip 本身。
         *
         * @param file_path 待检查文件
         * @return 是否像 `.docx`；文件不存在或不可读时返回 false
         */
        static bool is_ooxml_package(const QString& file_path);

    private:
        /** 执行单次转换并校验产物是否真的落盘。 */
        static Conversion run_backend(const Backend& backend, const QString& source_path, const QString& output_dir);

        /** @return 当前生效的后端列表（未被覆盖时即 `default_backends()`）。 */
        static QList<Backend>& active_backends();

        /** 当前生效的后端列表。 */
        static QList<Backend> s_backends;

        /** `set_backends()` 是否覆盖过默认值。 */
        static bool s_overridden;
    };

} // namespace Schedule
