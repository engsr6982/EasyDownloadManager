#pragma once
#include <functional>
#include <sqlitecpp/SQLiteCpp.h>

#include <mutex>

namespace edm {

struct TaskModel;
struct TaskHeaderModel;

class DownloadDatabase {
    std::unique_ptr<SQLite::Database> db_{nullptr};
    mutable std::mutex                mutex_{};

    void initTables() const;

public:
    explicit DownloadDatabase();

    void insertTask(std::shared_ptr<TaskModel>);

    void insertTaskHeader(std::shared_ptr<TaskHeaderModel> const& taskHeader);

    std::shared_ptr<TaskModel> getTaskById(int id) const;

    std::shared_ptr<TaskHeaderModel> getTaskHeaderById(int id) const;

    void forEachTask(std::function<bool(std::shared_ptr<TaskModel>)> const& callback) const;
};

} // namespace edm
