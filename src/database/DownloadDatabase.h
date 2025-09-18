#pragma once
#include <sqlitecpp/SQLiteCpp.h>

namespace edm {


class DownloadDatabase {
    std::unique_ptr<SQLite::Database> db_{nullptr};

public:
    explicit DownloadDatabase();
};

} // namespace edm
