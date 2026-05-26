#include "DownloadDatabase.h"

#include "EdmConfig.h"
#include "model/TaskModel.h"

#include <random>

namespace edm {


DownloadDatabase::DownloadDatabase() {
    db_ = std::make_unique<SQLite::Database>(
        EdmConfig::getDatabasePath().toStdString(),
        SQLite::OPEN_CREATE | SQLite::OPEN_READWRITE
    );
    initTables();
}

void DownloadDatabase::initTables() const {
    db_->exec("PRAGMA foreign_keys = ON;"); // 开启外键关联约束

    // downloads 表
    {
        SQLite::Statement query(
            *db_,
            R"(
                CREATE TABLE IF NOT EXISTS downloads (
                    id INTEGER PRIMARY KEY,
                    url TEXT NOT NULL,
                    fileName TEXT NOT NULL,
                    fileSize INTEGER NOT NULL,       -- int64
                    category INTEGER NOT NULL,       -- Category 枚举
                    state INTEGER NOT NULL,          -- TaskState 枚举
                    bandLimit INTEGER NOT NULL,
                    threadCount INTEGER NOT NULL,
                    firstTry INTEGER NOT NULL,       -- int64 时间戳
                    lastTry INTEGER NOT NULL,        -- int64 时间戳
                    userAgent TEXT NOT NULL,
                    resumable INTEGER NOT NULL,      -- Resumable 枚举
                    pageUrl TEXT NOT NULL,
                    pageTitle TEXT NOT NULL,
                    mimeType TEXT NOT NULL,
                    errorMsg TEXT NOT NULL,
                    saveDir TEXT NOT NULL
                );
            )"
        );
        query.exec();
    }

    // headers 表
    {
        SQLite::Statement query(
            *db_,
            R"(
                CREATE TABLE IF NOT EXISTS headers (
                    id INTEGER PRIMARY KEY,   -- 关联 downloads.id
                    origin TEXT,
                    cookie TEXT,
                    referer TEXT,
                    FOREIGN KEY (id) REFERENCES downloads(id) ON DELETE CASCADE
                );
            )"
        );
        query.exec();
    }
}

void DownloadDatabase::insertTask(std::shared_ptr<TaskModel> task) {
    std::lock_guard   lock(mutex_);
    SQLite::Statement query(
        *db_,
        R"(
            INSERT OR REPLACE INTO downloads (
                id,
                url,
                fileName,
                fileSize,
                category,
                state,
                bandLimit,
                threadCount,
                firstTry,
                lastTry,
                userAgent,
                resumable,
                pageUrl,
                pageTitle,
                mimeType,
                errorMsg,
                saveDir
            ) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
        )"
    );

    int idx = 1;
    if (task->id > kInvalidTaskID) {
        query.bind(idx++, task->id);
    } else {
        query.bind(idx++); // 让 SQLite 自动分配 id
    }

    query.bind(idx++, task->url);
    query.bind(idx++, task->fileName);
    query.bind(idx++, task->fileSize);
    query.bind(idx++, static_cast<int>(task->category));
    query.bind(idx++, static_cast<int>(task->state));
    query.bind(idx++, task->bandLimit);
    query.bind(idx++, task->threadCount);
    query.bind(idx++, task->firstTry);
    query.bind(idx++, task->lastTry);
    query.bind(idx++, task->userAgent);
    query.bind(idx++, static_cast<int>(task->resumable));
    query.bind(idx++, task->pageUrl);
    query.bind(idx++, task->pageTitle);
    query.bind(idx++, task->mimeType);
    query.bind(idx++, task->errorMsg);
    query.bind(idx++, task->saveDir);

    query.exec();

    // 插入后更新 task->id
    if (task->id <= kInvalidTaskID) {
        task->id = static_cast<int>(db_->getLastInsertRowid());
    }
}

void DownloadDatabase::insertTaskHeader(std::shared_ptr<TaskHeaderModel> const& taskHeader) {
    std::lock_guard   lock(mutex_);
    SQLite::Statement query(
        *db_,
        R"(
            INSERT OR REPLACE INTO headers (
                id,
                origin,
                cookie,
                referer
            ) VALUES (?,?,?,?)
        )"
    );

    query.bind(1, taskHeader->id);
    query.bind(2, taskHeader->origin);
    query.bind(3, taskHeader->cookie);
    query.bind(4, taskHeader->referer);

    query.exec();
}

std::shared_ptr<TaskModel> DownloadDatabase::getTaskById(int id) const {
    std::lock_guard   lock(mutex_);
    SQLite::Statement query(
        *db_,
        R"(
            SELECT
                id,
                url,
                fileName,
                fileSize,
                category,
                state,
                bandLimit,
                threadCount,
                firstTry,
                lastTry,
                userAgent,
                resumable,
                pageUrl,
                pageTitle,
                mimeType,
                errorMsg,
                saveDir
            FROM downloads
            WHERE id=?
        )"
    );

    query.bind(1, id);

    if (!query.executeStep()) {
        return nullptr;
    }

    auto task = TaskModel ::make();

    int idx           = 0;
    task->id          = query.getColumn(idx++).getInt();
    task->url         = query.getColumn(idx++).getString();
    task->fileName    = query.getColumn(idx++).getString();
    task->fileSize    = query.getColumn(idx++).getInt64();
    task->category    = static_cast<Category>(query.getColumn(idx++).getInt());
    task->state       = static_cast<TaskState>(query.getColumn(idx++).getInt());
    task->bandLimit   = query.getColumn(idx++).getInt();
    task->threadCount = query.getColumn(idx++).getInt();
    task->firstTry    = query.getColumn(idx++).getInt64();
    task->lastTry     = query.getColumn(idx++).getInt64();
    task->userAgent   = query.getColumn(idx++).getString();
    task->resumable   = static_cast<Resumable>(query.getColumn(idx++).getInt());
    task->pageUrl     = query.getColumn(idx++).getString();
    task->pageTitle   = query.getColumn(idx++).getString();
    task->mimeType    = query.getColumn(idx++).getString();
    task->errorMsg    = query.getColumn(idx++).getString();
    task->saveDir     = query.getColumn(idx++).getString();

    return task;
}

std::shared_ptr<TaskHeaderModel> DownloadDatabase::getTaskHeaderById(int id) const {
    std::lock_guard   lock(mutex_);
    SQLite::Statement query(
        *db_,
        R"(
            SELECT
                id,
                origin,
                cookie,
                referer
            FROM headers
            WHERE id=?
        )"
    );

    query.bind(1, id);

    if (!query.executeStep()) {
        return nullptr;
    }

    auto taskHeader     = std::make_shared<TaskHeaderModel>();
    int  idx            = 0;
    taskHeader->id      = query.getColumn(idx++).getInt();
    taskHeader->origin  = query.getColumn(idx++).getString();
    taskHeader->cookie  = query.getColumn(idx++).getString();
    taskHeader->referer = query.getColumn(idx++).getString();
    return taskHeader;
}

void DownloadDatabase::forEachTask(std::function<bool(std::shared_ptr<TaskModel>)> const& callback) const {
    std::lock_guard   lock(mutex_);
    SQLite::Statement query(
        *db_,
        R"(
            SELECT
                id,
                url,
                fileName,
                fileSize,
                category,
                state,
                bandLimit,
                threadCount,
                firstTry,
                lastTry,
                userAgent,
                resumable,
                pageUrl,
                pageTitle,
                mimeType,
                errorMsg,
                saveDir
            FROM downloads
        )"
    );

    int idx{0};
    while (query.executeStep()) {
        auto task = TaskModel ::make();

        idx               = 0;
        task->id          = query.getColumn(idx++).getInt();
        task->url         = query.getColumn(idx++).getString();
        task->fileName    = query.getColumn(idx++).getString();
        task->fileSize    = query.getColumn(idx++).getInt64();
        task->category    = static_cast<Category>(query.getColumn(idx++).getInt());
        task->state       = static_cast<TaskState>(query.getColumn(idx++).getInt());
        task->bandLimit   = query.getColumn(idx++).getInt();
        task->threadCount = query.getColumn(idx++).getInt();
        task->firstTry    = query.getColumn(idx++).getInt64();
        task->lastTry     = query.getColumn(idx++).getInt64();
        task->userAgent   = query.getColumn(idx++).getString();
        task->resumable   = static_cast<Resumable>(query.getColumn(idx++).getInt());
        task->pageUrl     = query.getColumn(idx++).getString();
        task->pageTitle   = query.getColumn(idx++).getString();
        task->mimeType    = query.getColumn(idx++).getString();
        task->errorMsg    = query.getColumn(idx++).getString();
        task->saveDir     = query.getColumn(idx++).getString();

        // 如果回调返回 false，则提前停止遍历
        if (!callback(task)) {
            break;
        }
    }
}

} // namespace edm