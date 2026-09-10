#include "data/import_export/ImportManager.h"

#include "data/import_export/CsvScheduleIo.h"
#include "data/import_export/IcsScheduleIo.h"
#include "data/import_export/JsonScheduleIo.h"

#include <QFile>
#include <QFileInfo>

namespace Schedule {

    namespace {

        /** @return 两个冲突是否描述同一件事（用于“新引入的冲突”差集计算）。 */
        bool same_conflict(const Conflict& left, const Conflict& right) {
            return left.type == right.type && left.course_id_a == right.course_id_a && left.course_id_b == right.course_id_b && left.session_id_a == right.session_id_a && left.session_id_b == right.session_id_b && left.day_of_week == right.day_of_week && left.week == right.week;
        }

        /**
         * @brief 从“导入后的冲突”中剔除“导入前就存在的冲突”。
         *
         * 否则预览会把用户早已知道的历史冲突重复报一遍，淹没真正的新问题。
         */
        QList<Conflict> subtract_conflicts(const QList<Conflict>& after, const QList<Conflict>& before) {
            QList<Conflict> result;
            for (const Conflict& conflict : after) {
                bool existed = false;
                for (const Conflict& existing : before) {
                    if (same_conflict(existing, conflict)) {
                        existed = true;
                        break;
                    }
                }
                if (!existed) {
                    result.append(conflict);
                }
            }
            return result;
        }

        /** @brief 统一设置错误。 */
        bool fail(QString* error_message, const QString& message) {
            if (error_message) {
                *error_message = message;
            }
            return false;
        }

    } // namespace

    ImportManager::ImportManager() {
        register_importer(std::make_unique<JsonScheduleIo>());
        register_importer(std::make_unique<CsvScheduleIo>());
        register_importer(std::make_unique<IcsScheduleIo>());
    }

    ImportManager::~ImportManager() = default;

    void ImportManager::register_importer(std::unique_ptr<IScheduleImporter> importer) {
        if (!importer) {
            return;
        }
        const ScheduleFormat format = importer->format();
        for (auto& existing : m_importers) {
            if (existing && existing->format() == format) {
                existing = std::move(importer); // 替换：后注册者生效
                return;
            }
        }
        m_importers.push_back(std::move(importer));
    }

    QList<ScheduleFormat> ImportManager::supported_formats() const {
        QList<ScheduleFormat> formats;
        for (const auto& importer : m_importers) {
            if (importer) {
                formats.append(importer->format());
            }
        }
        return formats;
    }

    const IScheduleImporter* ImportManager::importer_for_format(ScheduleFormat format) const {
        for (const auto& importer : m_importers) {
            if (importer && importer->format() == format) {
                return importer.get();
            }
        }
        return nullptr;
    }

    const IScheduleImporter* ImportManager::importer_for_content(const QByteArray& data) const {
        const ScheduleFormat sniffed = format_from_content(data);
        if (sniffed != ScheduleFormat::Unknown) {
            if (const IScheduleImporter* importer = importer_for_format(sniffed)) {
                return importer;
            }
        }
        // 内容无法判定时退回 JSON（最严格的格式），由解析器给出明确错误
        return importer_for_format(ScheduleFormat::Json);
    }

    void ImportManager::set_conflict_options(const ConflictDetector::Options& options) {
        m_options = options;
    }

    ConflictDetector::Options ImportManager::conflict_options() const {
        return m_options;
    }

    QStringList ImportManager::strategy_names() {
        return QStringList{
            strategy_display_name(ImportStrategy::Merge),
            strategy_display_name(ImportStrategy::SkipDuplicates),
            strategy_display_name(ImportStrategy::Overwrite),
        };
    }

    ImportStrategy ImportManager::strategy_at(int index) {
        switch (index) {
        case 1:
            return ImportStrategy::SkipDuplicates;
        case 2:
            return ImportStrategy::Overwrite;
        default:
            return ImportStrategy::Merge;
        }
    }

    int ImportManager::index_of_strategy(ImportStrategy strategy) {
        switch (strategy) {
        case ImportStrategy::SkipDuplicates:
            return 1;
        case ImportStrategy::Overwrite:
            return 2;
        case ImportStrategy::Merge:
        default:
            return 0;
        }
    }

    int ImportManager::count_duplicates(const QList<Course>& incoming, const QList<Course>& existing, bool* matched_by_id) {
        int count = 0;
        if (matched_by_id) {
            *matched_by_id = false;
        }
        for (const Course& candidate : incoming) {
            bool duplicated = false;
            for (const Course& other : existing) {
                if (!candidate.id.isEmpty() && candidate.id == other.id) {
                    duplicated = true;
                    if (matched_by_id) {
                        *matched_by_id = true;
                    }
                    break;
                }
                // 没有 id（例如来自 CSV / ICS）时按“名称 + 代码”判定重复
                if (!candidate.name.isEmpty() && candidate.name == other.name && candidate.code == other.code) {
                    duplicated = true;
                    break;
                }
            }
            if (duplicated) {
                ++count;
            }
        }
        return count;
    }

    ImportPreview ImportManager::build_preview(const ScheduleSnapshot& parsed,
        ScheduleFormat format,
        const QString& source_name,
        const ScheduleSnapshot& current) const {
        ImportPreview preview;
        preview.is_valid = true;
        preview.format = format;
        preview.source_path = source_name;
        preview.semester = parsed.semester;
        preview.time_slots = parsed.time_slots;
        preview.courses = parsed.courses;

        // 导入目标是“当前学期”：若当前已有学期，则以它为准做周次 / 节次校验，
        // 并把文件的学期差异作为提示告知用户。
        const bool has_current_semester = current.semester.is_valid(nullptr);
        const Semester effective_semester = has_current_semester ? current.semester : parsed.semester;
        const QList<TimeSlot> effective_slots = !current.time_slots.isEmpty() ? current.time_slots : parsed.time_slots;

        if (has_current_semester && parsed.semester.is_valid(nullptr) && parsed.semester.id != current.semester.id) {
            preview.warnings.append(QStringLiteral("文件来自其它学期“%1”，课程将导入到当前学期“%2”")
                    .arg(parsed.semester.name, current.semester.name));
        }
        if (parsed.time_slots.isEmpty() && current.time_slots.isEmpty()) {
            preview.warnings.append(QStringLiteral("文件未包含作息表，将使用应用的默认作息时间"));
        }

        preview.duplicate_count = count_duplicates(parsed.courses, current.courses, nullptr);
        preview.new_course_count = qMax(0, static_cast<int>(parsed.courses.size()) - preview.duplicate_count);

        // 预览“导入后会怎样”：先模拟一次合并，再对新课表做冲突检测，
        // 最后减去导入前就存在的问题，得到“本次导入新引入的冲突”。
        ScheduleSnapshot simulated = current;
        if (!simulated.semester.is_valid(nullptr)) {
            simulated.semester = parsed.semester;
        }
        if (simulated.time_slots.isEmpty()) {
            simulated.time_slots = parsed.time_slots;
        }
        for (const Course& course : parsed.courses) {
            Course normalised = course;
            if (!simulated.semester.id.isEmpty()) {
                normalised.semester_id = simulated.semester.id;
            }
            const int index = simulated.index_of_course(normalised.id);
            if (index >= 0) {
                simulated.courses[index] = normalised;
            }
            else {
                simulated.courses.append(normalised);
            }
        }

        const QList<Conflict> before = ConflictDetector::detect(current.courses, effective_semester, effective_slots, m_options);
        const QList<Conflict> after = ConflictDetector::detect(simulated.courses, simulated.semester, simulated.time_slots, m_options);
        preview.conflicts = subtract_conflicts(after, before);

        return preview;
    }

    ImportPreview ImportManager::preview(const QString& file_path, const ScheduleSnapshot& current) const {
        ImportPreview preview;

        if (file_path.isEmpty()) {
            preview.error_message = QStringLiteral("导入路径为空");
            return preview;
        }
        QFile file(file_path);
        if (!file.exists()) {
            preview.error_message = QStringLiteral("文件不存在：%1").arg(file_path);
            return preview;
        }
        if (!file.open(QIODevice::ReadOnly)) {
            preview.error_message = QStringLiteral("无法读取文件 %1：%2").arg(file_path, file.errorString());
            return preview;
        }
        const QByteArray data = file.readAll();
        file.close();

        // 优先按内容嗅探：用户把 .csv 误存成 .json 时也能正确解析
        ScheduleFormat format = format_from_content(data);
        if (format == ScheduleFormat::Unknown) {
            format = format_from_extension(file_path);
        }

        const IScheduleImporter* importer = importer_for_format(format);
        if (!importer) {
            preview.error_message = QStringLiteral("不支持的文件格式：%1（已支持 %2）")
                                        .arg(QFileInfo(file_path).suffix(), supported_file_extensions().join(QStringLiteral(" / ")));
            return preview;
        }

        ScheduleSnapshot parsed;
        QString error;
        if (!importer->parse_data(data, file_path, &parsed, &error)) {
            preview.format = format;
            preview.source_path = file_path;
            preview.error_message = error;
            return preview;
        }

        return build_preview(parsed, format, file_path, current);
    }

    ImportPreview ImportManager::preview_data(const QByteArray& data, const QString& source_name, const ScheduleSnapshot& current) const {
        ImportPreview preview;
        if (data.isEmpty()) {
            preview.error_message = QStringLiteral("内容为空");
            return preview;
        }

        ScheduleFormat format = format_from_content(data);
        if (format == ScheduleFormat::Unknown) {
            preview.error_message = QStringLiteral("无法识别内容格式（支持 JSON / CSV / ICS）");
            return preview;
        }

        const IScheduleImporter* importer = importer_for_format(format);
        if (!importer) {
            preview.error_message = QStringLiteral("没有可用的 %1 导入器").arg(format_display_name(format));
            return preview;
        }

        ScheduleSnapshot parsed;
        QString error;
        if (!importer->parse_data(data, source_name, &parsed, &error)) {
            preview.format = format;
            preview.source_path = source_name;
            preview.error_message = error;
            return preview;
        }

        return build_preview(parsed, format, source_name, current);
    }

    ImportResult ImportManager::apply(const ImportPreview& preview, ImportStrategy strategy, ScheduleSnapshot* in_out_snapshot) const {
        ImportResult result;
        result.strategy = strategy;

        if (!in_out_snapshot) {
            result.error_message = QStringLiteral("目标快照为空");
            return result;
        }
        if (!preview.is_valid) {
            result.error_message = preview.error_message.isEmpty() ? QStringLiteral("导入预览无效") : preview.error_message;
            return result;
        }

        ScheduleSnapshot& snapshot = *in_out_snapshot;
        const auto assign_semester = [&snapshot](Course* course) {
            if (!snapshot.semester.id.isEmpty()) {
                course->semester_id = snapshot.semester.id;
            }
        };

        if (strategy == ImportStrategy::Overwrite) {
            snapshot.courses.clear();
            if (!preview.time_slots.isEmpty()) {
                snapshot.time_slots = preview.time_slots;
            }
            for (const Course& course : preview.courses) {
                Course normalised = course;
                assign_semester(&normalised);
                normalised.ensure_session_ids();
                snapshot.courses.append(normalised);
                ++result.imported_count;
            }
        }
        else {
            for (const Course& course : preview.courses) {
                Course normalised = course;
                assign_semester(&normalised);
                normalised.ensure_session_ids();

                int index = snapshot.index_of_course(normalised.id);
                if (index < 0 && strategy == ImportStrategy::SkipDuplicates && !normalised.name.isEmpty()) {
                    // 无 id（CSV / ICS）时按“名称 + 代码”判定重复
                    for (int i = 0; i < snapshot.courses.size(); ++i) {
                        if (snapshot.courses.at(i).name == normalised.name && snapshot.courses.at(i).code == normalised.code) {
                            index = i;
                            break;
                        }
                    }
                }

                if (index >= 0) {
                    if (strategy == ImportStrategy::SkipDuplicates) {
                        ++result.skipped_count;
                    }
                    else {
                        snapshot.courses[index] = normalised;
                        ++result.updated_count;
                    }
                    continue;
                }

                snapshot.courses.append(normalised);
                ++result.imported_count;
            }

            // 当前作息表为空时，采用文件提供的作息表，避免导入后无法计算上课时间
            if (snapshot.time_slots.isEmpty() && !preview.time_slots.isEmpty()) {
                snapshot.time_slots = preview.time_slots;
            }
        }

        result.success = true;
        result.semester = snapshot.semester;
        result.conflicts = ConflictDetector::detect(snapshot.courses, snapshot.semester, snapshot.time_slots, m_options);
        return result;
    }

} // namespace Schedule
