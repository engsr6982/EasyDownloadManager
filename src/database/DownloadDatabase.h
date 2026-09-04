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

    /// 获取数据库中任务总数
    int getTaskCount() const;

    /// 分页加载任务，按 lastTry 倒序（最新操作的任务在前）
    /// @param offset 起始偏移量
    /// @param limit  最大返回数量
    std::vector<std::shared_ptr<TaskModel>> getTasksRange(int offset, int limit) const;

    /// 从数据库中删除任务及其关联的 header 记录。
    /// @returns true 如果删除成功
    bool deleteTask(int id);
};

} // namespace edm
