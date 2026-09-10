#include "data/ImportSource.h"

namespace Schedule {

    bool ImportSource::operator==(const ImportSource& other) const {
        return id == other.id && file_path == other.file_path && format == other.format && imported_at == other.imported_at && course_count == other.course_count && note == other.note;
    }

    bool ImportSource::operator!=(const ImportSource& other) const {
        return !(*this == other);
    }

} // namespace Schedule
