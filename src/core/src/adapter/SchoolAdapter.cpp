#include "core/adapter/SchoolAdapter.h"

namespace Schedule {

    bool AdapterInfo::is_valid() const {
        return !id.isEmpty() && !name.isEmpty() && !schedule_url.isEmpty();
    }

    bool AdapterSession::is_empty() const {
        return cookie_header.trimmed().isEmpty();
    }

    void AdapterSession::clear() {
        // 主动填充零再清空，尽量减少凭证在内存中的残留时间
        cookie_header.fill('\0');
        cookie_header.clear();
        user_agent.fill('\0');
        user_agent.clear();
        created_at = QDateTime();
    }

    double AdapterSession::age_hours() const {
        if (!created_at.isValid()) {
            return -1.0;
        }
        return created_at.secsTo(QDateTime::currentDateTime()) / 3600.0;
    }

    SchoolAdapterRegistry::~SchoolAdapterRegistry() = default;

    void SchoolAdapterRegistry::register_adapter(std::unique_ptr<ISchoolAdapter> adapter) {
        if (!adapter) {
            return;
        }
        const QString id = adapter->info().id;
        for (auto& existing : m_adapters) {
            if (existing && existing->info().id == id) {
                existing = std::move(adapter);
                return;
            }
        }
        m_adapters.push_back(std::move(adapter));
    }

    QList<AdapterInfo> SchoolAdapterRegistry::adapters() const {
        QList<AdapterInfo> result;
        for (const auto& adapter : m_adapters) {
            if (adapter) {
                result.append(adapter->info());
            }
        }
        return result;
    }

    const ISchoolAdapter* SchoolAdapterRegistry::find(const QString& id) const {
        for (const auto& adapter : m_adapters) {
            if (adapter && adapter->info().id == id) {
                return adapter.get();
            }
        }
        return nullptr;
    }

    const ISchoolAdapter* SchoolAdapterRegistry::at(int index) const {
        if (index < 0 || index >= static_cast<int>(m_adapters.size())) {
            return nullptr;
        }
        return m_adapters.at(static_cast<std::size_t>(index)).get();
    }

    int SchoolAdapterRegistry::count() const {
        return static_cast<int>(m_adapters.size());
    }

    void SchoolAdapterRegistry::clear() {
        m_adapters.clear();
    }

} // namespace Schedule
