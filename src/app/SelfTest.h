#pragma once

#include "engine/AppBridge.h"
#include "engine/ScheduleBridge.h"

#include <QGuiApplication>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

namespace Schedule {

    /**
     * @brief 桌面端自动化自检（`Schedule.exe --selftest`）。
     *
     * 仅在 `-DBUILD_SELFTEST=ON` 构建中存在，用于在**真实 QML 场景**下验证端到端链路：
     *
     * | 步骤 | 验证内容 |
     * | ---- | -------- |
     * | 1 | 点击设置页的“自检”按钮，验证 **C++ 侧显式连接**的 `Button::clicked` → `AppBridge::test_button_clicked()` 链路 |
     * | 2 | 从 `samples/schedule_sample.json` 生成导入预览，验证解析、冲突检测与重复统计 |
     * | 3 | 按“合并”策略应用导入，验证课表写入内存并落库 |
     * | 4 | 依次导出 JSON / CSV / ICS 到临时目录，验证**导出文件确实存在于指定目录** |
     * | 5 | 把刚导出的 JSON 再导入一次，验证预览识别出 4 门重复课程（往返一致性） |
     *
     * 全部步骤通过则退出码为 `0`，任一步失败为 `2`（便于 CI 判定）。
     */
    class SelfTest : public QObject {
        Q_OBJECT

    public:
        /**
         * @param root       QML 根对象
         * @param bridge    课表桥接（不持有所有权）
         * @param app_bridge 早期桥接对象（用于验证测试按钮链路）
         * @param app        应用对象（用于设置退出码）
         * @param parent     父对象
         */
        SelfTest(QObject* root,
            ScheduleBridge* bridge,
            AppBridge* app_bridge,
            QGuiApplication* app,
            QObject* parent = nullptr);

        ~SelfTest() override;

        /** @brief 启动自检流程（异步、按步骤延时执行，保证 QML 场景已就绪）。 */
        void start();

    private:
        /** @brief 安排下一步。 */
        void schedule_next_step(int delay_ms);

        /** @brief 执行下一步；失败则立即结束。 */
        void run_next_step();

        /** @brief 输出汇总并退出。 */
        void finish();

        /**
         * @brief 记录一条断言结果。
         * @param condition   是否通过
         * @param description 步骤描述（中文）
         * @return `condition` 原样返回，便于 `if (!check(...)) return;`
         */
        bool check(bool condition, const QString& description);

        /** @return 样本文件目录；找不到时返回空串。 */
        QString samples_directory() const;

        /** @return 自检使用的导出目录（临时目录下的固定子目录）。 */
        QString export_directory() const;

        /** @return 导入导出桥接（便捷访问）。 */
        class ImportExportBridge* import_export() const;

        /** QML 根对象。 */
        QPointer<QObject> m_root;

        /** 课表桥接；不持有所有权。 */
        ScheduleBridge* m_bridge = nullptr;

        /** 早期桥接对象；不持有所有权。 */
        AppBridge* m_app_bridge = nullptr;

        /** 应用对象；不持有所有权。 */
        QGuiApplication* m_app = nullptr;

        /** 自检步骤。 */
        std::vector<std::function<void()>> m_steps;

        /** 当前步骤下标。 */
        std::size_t m_step_index = 0;

        /** 是否已经结束。 */
        bool m_finished = false;

        /** 是否出现失败。 */
        bool m_failed = false;

        /** 测试按钮的点击是否触发了信号（验证 C++ 侧连接）。 */
        bool m_test_signal_received = false;

        /** 导出的三个文件路径，供后续步骤校验。 */
        QStringList m_exported_files;

        /** 通过的断言数量。 */
        int m_passed_checks = 0;
    };

} // namespace Schedule
