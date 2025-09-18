#include "DownloadDatabase.h"

namespace edm {

DownloadDatabase::DownloadDatabase() {
    db_ = std::make_unique<SQLite::Database>("edm.db", SQLite::OPEN_CREATE | SQLite::OPEN_READWRITE);
}


} // namespace edm