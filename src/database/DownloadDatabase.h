#pragma once
#include <functional>
#include <sqlitecpp/SQLiteCpp.h>


namespace edm {

struct TaskModel;
struct TaskHeaderModel;

class DownloadDatabase {
    std::unique_ptr<SQLite::Database> db_{nullptr};

    void initTables() const;

public:
    explicit DownloadDatabase();

    void insertTask(TaskModel& task);

    void insertTaskHeader(TaskHeaderModel const& taskHeader);

    std::optional<TaskModel> getTaskById(int id) const;

    std::optional<TaskHeaderModel> getTaskHeaderById(int id) const;

    void forEachTask(std::function<bool(TaskModel const&)> const& callback) const;


#ifdef EDM_DEBUG
    void insertFakeTasks(int count);
#endif
};

} // namespace edm
